#include "common.h"
#include "util.h"
#include "patchfinder.h"

static uint32_t bit_range(uint32_t x, int start, int end) {
    x = (x << (31 - start)) >> (31 - start);
    x = (x >> end);
    return x;
}

static uint32_t ror(uint32_t x, int places) {
    return (x >> places) | (x << (32 - places));
}

static int thumb_expand_imm_c(uint16_t imm12) {
    if (bit_range(imm12, 11, 10) == 0) {
        switch (bit_range(imm12, 9, 8)) {
        case 0:
            return bit_range(imm12, 7, 0);
        case 1:
            return (bit_range(imm12, 7, 0) << 16) | bit_range(imm12, 7, 0);
        case 2:
            return (bit_range(imm12, 7, 0) << 24) | (bit_range(imm12, 7, 0) << 8);
        case 3:
            return (bit_range(imm12, 7, 0) << 24) | (bit_range(imm12, 7, 0) << 16) | (bit_range(imm12, 7, 0) << 8) | bit_range(imm12, 7, 0);
        default:
            return 0;
        }
    } else {
        uint32_t unrotated_value = 0x80 | bit_range(imm12, 6, 0);
        return ror(unrotated_value, bit_range(imm12, 11, 7));
    }
}

static int insn_is_32bit(uint16_t * i) {
    return ( * i & 0xe000) == 0xe000 && ( * i & 0x1800) != 0x0;
}

static int insn_is_bl(uint16_t * i) {
    if (( * i & 0xf800) == 0xf000 && ( * (i + 1) & 0xd000) == 0xd000)
        return 1;
    else if (( * i & 0xf800) == 0xf000 && ( * (i + 1) & 0xd001) == 0xc000)
        return 1;
    else
        return 0;
}

static uint32_t insn_bl_imm32(uint16_t * i) {
    uint16_t insn0 = * i;
    uint16_t insn1 = * (i + 1);
    uint32_t s = (insn0 >> 10) & 1;
    uint32_t j1 = (insn1 >> 13) & 1;
    uint32_t j2 = (insn1 >> 11) & 1;
    uint32_t i1 = ~(j1 ^ s) & 1;
    uint32_t i2 = ~(j2 ^ s) & 1;
    uint32_t imm10 = insn0 & 0x3ff;
    uint32_t imm11 = insn1 & 0x7ff;
    uint32_t imm32 = (imm11 << 1) | (imm10 << 12) | (i2 << 22) | (i1 << 23) | (s ? 0xff000000 : 0);
    return imm32;
}

static int insn_is_b_conditional(uint16_t * i) {
    return ( * i & 0xF000) == 0xD000 && ( * i & 0x0F00) != 0x0F00 && ( * i & 0x0F00) != 0xE;
}

static int insn_is_b_unconditional(uint16_t * i) {
    if (( * i & 0xF800) == 0xE000)
        return 1;
    else if (( * i & 0xF800) == 0xF000 && ( * (i + 1) & 0xD000) == 9)
        return 1;
    else
        return 0;
}

static int insn_is_ldr_literal(uint16_t * i) {
    return ( * i & 0xF800) == 0x4800 || ( * i & 0xFF7F) == 0xF85F;
}

static int insn_ldr_literal_rt(uint16_t * i) {
    if (( * i & 0xF800) == 0x4800)
        return ( * i >> 8) & 7;
    else if (( * i & 0xFF7F) == 0xF85F)
        return ( * (i + 1) >> 12) & 0xF;
    else
        return 0;
}

static int insn_ldr_literal_imm(uint16_t * i) {
    if (( * i & 0xF800) == 0x4800)
        return ( * i & 0xF) << 2;
    else if (( * i & 0xFF7F) == 0xF85F)
        return ( * (i + 1) & 0xFFF) * ((( * i & 0x0800) == 0x0800) ? 1 : -1);
    else
        return 0;
}

static int insn_is_ldr_imm(uint16_t * i) {
    uint8_t opA = bit_range( * i, 15, 12);
    uint8_t opB = bit_range( * i, 11, 9);
    return opA == 6 && (opB & 4) == 4;
}

static int insn_ldr_imm_rt(uint16_t * i) {
    return ( * i & 7);
}

static int insn_ldr_imm_rn(uint16_t * i) {
    return (( * i >> 3) & 7);
}

static int insn_ldr_imm_imm(uint16_t * i) {
    return (( * i >> 6) & 0x1F);
}

static int insn_is_ldrb_imm(uint16_t * i) {
    return ( * i & 0xF800) == 0x7800;
}

static int insn_ldrb_imm_rt(uint16_t * i) {
    return ( * i & 7);
}

static int insn_ldrb_imm_rn(uint16_t * i) {
    return (( * i >> 3) & 7);
}

static int insn_ldrb_imm_imm(uint16_t * i) {
    return (( * i >> 6) & 0x1F);
}


static int insn_is_add_reg(uint16_t * i) {
    if (( * i & 0xFE00) == 0x1800)
        return 1;
    else if (( * i & 0xFF00) == 0x4400)
        return 1;
    else if (( * i & 0xFFE0) == 0xEB00)
        return 1;
    else
        return 0;
}

static int insn_add_reg_rd(uint16_t * i) {
    if (( * i & 0xFE00) == 0x1800)
        return ( * i & 7);
    else if (( * i & 0xFF00) == 0x4400)
        return ( * i & 7) | (( * i & 0x80) >> 4);
    else if (( * i & 0xFFE0) == 0xEB00)
        return ( * (i + 1) >> 8) & 0xF;
    else
        return 0;
}

static int insn_add_reg_rn(uint16_t * i) {
    if (( * i & 0xFE00) == 0x1800)
        return (( * i >> 3) & 7);
    else if (( * i & 0xFF00) == 0x4400)
        return ( * i & 7) | (( * i & 0x80) >> 4);
    else if (( * i & 0xFFE0) == 0xEB00)
        return ( * i & 0xF);
    else
        return 0;
}

static int insn_add_reg_rm(uint16_t * i) {
    if (( * i & 0xFE00) == 0x1800)
        return ( * i >> 6) & 7;
    else if (( * i & 0xFF00) == 0x4400)
        return ( * i >> 3) & 0xF;
    else if (( * i & 0xFFE0) == 0xEB00)
        return * (i + 1) & 0xF;
    else
        return 0;
}

static int insn_is_movt(uint16_t * i) {
    return ( * i & 0xFBF0) == 0xF2C0 && ( * (i + 1) & 0x8000) == 0;
}

static int insn_movt_rd(uint16_t * i) {
    return ( * (i + 1) >> 8) & 0xF;
}

static int insn_movt_imm(uint16_t * i) {
    return (( * i & 0xF) << 12) | (( * i & 0x0400) << 1) | (( * (i + 1) & 0x7000) >> 4) | ( * (i + 1) & 0xFF);
}

static int insn_is_mov_imm(uint16_t * i) {
    if (( * i & 0xF800) == 0x2000)
        return 1;
    else if (( * i & 0xFBEF) == 0xF04F && ( * (i + 1) & 0x8000) == 0)
        return 1;
    else if (( * i & 0xFBF0) == 0xF240 && ( * (i + 1) & 0x8000) == 0)
        return 1;
    else
        return 0;
}

static int insn_mov_imm_rd(uint16_t * i) {
    if (( * i & 0xF800) == 0x2000)
        return ( * i >> 8) & 7;
    else if (( * i & 0xFBEF) == 0xF04F && ( * (i + 1) & 0x8000) == 0)
        return ( * (i + 1) >> 8) & 0xF;
    else if (( * i & 0xFBF0) == 0xF240 && ( * (i + 1) & 0x8000) == 0)
        return ( * (i + 1) >> 8) & 0xF;
    else
        return 0;
}

static int insn_mov_imm_imm(uint16_t * i) {
    if (( * i & 0xF800) == 0x2000)
        return * i & 0xF;
    else if (( * i & 0xFBEF) == 0xF04F && ( * (i + 1) & 0x8000) == 0)
        return thumb_expand_imm_c((( * i & 0x0400) << 1) | (( * (i + 1) & 0x7000) >> 4) | ( * (i + 1) & 0xFF));
    else if (( * i & 0xFBF0) == 0xF240 && ( * (i + 1) & 0x8000) == 0)
        return (( * i & 0xF) << 12) | (( * i & 0x0400) << 1) | (( * (i + 1) & 0x7000) >> 4) | ( * (i + 1) & 0xFF);
    else
        return 0;
}

static int insn_is_cmp_imm(uint16_t * i) {
    if (( * i & 0xF800) == 0x2800)
        return 1;
    else if (( * i & 0xFBF0) == 0xF1B0 && ( * (i + 1) & 0x8F00) == 0x0F00)
        return 1;
    else
        return 0;
}

static int insn_cmp_imm_rn(uint16_t * i) {
    if (( * i & 0xF800) == 0x2800)
        return ( * i >> 8) & 7;
    else if (( * i & 0xFBF0) == 0xF1B0 && ( * (i + 1) & 0x8F00) == 0x0F00)
        return * i & 0xF;
    else
        return 0;
}

static int insn_cmp_imm_imm(uint16_t * i) {
    if (( * i & 0xF800) == 0x2800)
        return * i & 0xFF;
    else if (( * i & 0xFBF0) == 0xF1B0 && ( * (i + 1) & 0x8F00) == 0x0F00)
        return thumb_expand_imm_c((( * i & 0x0400) << 1) | (( * (i + 1) & 0x7000) >> 4) | ( * (i + 1) & 0xFF));
    else
        return 0;
}

static int insn_is_and_imm(uint16_t * i) {
    return ( * i & 0xFBE0) == 0xF000 && ( * (i + 1) & 0x8000) == 0;
}

static int insn_and_imm_rn(uint16_t * i) {
    return * i & 0xF;
}

static int insn_and_imm_rd(uint16_t * i) {
    return ( * (i + 1) >> 8) & 0xF;
}

static int insn_and_imm_imm(uint16_t * i) {
    return thumb_expand_imm_c((( * i & 0x0400) << 1) | (( * (i + 1) & 0x7000) >> 4) | ( * (i + 1) & 0xFF));
}

static int insn_is_push(uint16_t * i) {
    if (( * i & 0xFE00) == 0xB400)
        return 1;
    else if ( * i == 0xE92D)
        return 1;
    else if ( * i == 0xF84D && ( * (i + 1) & 0x0FFF) == 0x0D04)
        return 1;
    else
        return 0;
}

static int insn_push_registers(uint16_t * i) {
    if (( * i & 0xFE00) == 0xB400)
        return ( * i & 0x00FF) | (( * i & 0x0100) << 6);
    else if ( * i == 0xE92D)
        return * (i + 1);
    else if ( * i == 0xF84D && ( * (i + 1) & 0x0FFF) == 0x0D04)
        return 1 << (( * (i + 1) >> 12) & 0xF);
    else
        return 0;
}

static int insn_is_preamble_push(uint16_t * i) {
    return insn_is_push(i) && (insn_push_registers(i) & (1 << 14)) != 0;
}

static int insn_is_str_imm(uint16_t * i) {
    if (( * i & 0xF800) == 0x6000)
        return 1;
    else if (( * i & 0xF800) == 0x9000)
        return 1;
    else if (( * i & 0xFFF0) == 0xF8C0)
        return 1;
    else if (( * i & 0xFFF0) == 0xF840 && ( * (i + 1) & 0x0800) == 0x0800)
        return 1;
    else
        return 0;
}

static int insn_str_imm_postindexed(uint16_t * i) {
    if (( * i & 0xF800) == 0x6000)
        return 1;
    else if (( * i & 0xF800) == 0x9000)
        return 1;
    else if (( * i & 0xFFF0) == 0xF8C0)
        return 1;
    else if (( * i & 0xFFF0) == 0xF840 && ( * (i + 1) & 0x0800) == 0x0800)
        return ( * (i + 1) >> 10) & 1;
    else
        return 0;
}

static int insn_str_imm_wback(uint16_t * i) {
    if (( * i & 0xF800) == 0x6000)
        return 0;
    else if (( * i & 0xF800) == 0x9000)
        return 0;
    else if (( * i & 0xFFF0) == 0xF8C0)
        return 0;
    else if (( * i & 0xFFF0) == 0xF840 && ( * (i + 1) & 0x0800) == 0x0800)
        return ( * (i + 1) >> 8) & 1;
    else
        return 0;
}

static int insn_str_imm_imm(uint16_t * i) {
    if (( * i & 0xF800) == 0x6000)
        return ( * i & 0x07C0) >> 4;
    else if (( * i & 0xF800) == 0x9000)
        return ( * i & 0xFF) << 2;
    else if (( * i & 0xFFF0) == 0xF8C0)
        return ( * (i + 1) & 0xFFF);
    else if (( * i & 0xFFF0) == 0xF840 && ( * (i + 1) & 0x0800) == 0x0800)
        return ( * (i + 1) & 0xFF);
    else
        return 0;
}

static int insn_str_imm_rt(uint16_t * i) {
    if (( * i & 0xF800) == 0x6000)
        return ( * i & 7);
    else if (( * i & 0xF800) == 0x9000)
        return ( * i >> 8) & 7;
    else if (( * i & 0xFFF0) == 0xF8C0)
        return ( * (i + 1) >> 12) & 0xF;
    else if (( * i & 0xFFF0) == 0xF840 && ( * (i + 1) & 0x0800) == 0x0800)
        return ( * (i + 1) >> 12) & 0xF;
    else
        return 0;
}

static int insn_str_imm_rn(uint16_t * i) {
    if (( * i & 0xF800) == 0x6000)
        return ( * i >> 3) & 7;
    else if (( * i & 0xF800) == 0x9000)
        return 13;
    else if (( * i & 0xFFF0) == 0xF8C0)
        return ( * i & 0xF);
    else if (( * i & 0xFFF0) == 0xF840 && ( * (i + 1) & 0x0800) == 0x0800)
        return ( * i & 0xF);
    else
        return 0;
}

static uint16_t * find_last_insn_matching(uint32_t region, uint8_t *kdata, size_t ksize, uint16_t * current_instruction, int( * match_func)(uint16_t * )) {
    while ((uintptr_t) current_instruction > (uintptr_t) kdata) {
        if (insn_is_32bit(current_instruction - 2) && !insn_is_32bit(current_instruction - 3)) {
            current_instruction -= 2;
        } else {
            --current_instruction;
        }

        if (match_func(current_instruction)) {
            return current_instruction;
        }
    }
    return NULL;
}

static uint32_t find_pc_rel_value(uint32_t region, uint8_t *kdata, size_t ksize, uint16_t * insn, int reg) {
    int found = 0;
    uint16_t * current_instruction = insn;
    while ((uintptr_t) current_instruction > (uintptr_t) kdata) {
        if (insn_is_32bit(current_instruction - 2)) {
            current_instruction -= 2;
        } else {
            --current_instruction;
        }

        if (insn_is_mov_imm(current_instruction) && insn_mov_imm_rd(current_instruction) == reg) {
            found = 1;
            break;
        }

        if (insn_is_ldr_literal(current_instruction) && insn_ldr_literal_rt(current_instruction) == reg) {
            found = 1;
            break;
        }
    }

    if (!found) return 0;
    uint32_t value = 0;

    while ((uintptr_t) current_instruction < (uintptr_t) insn) {
        if (insn_is_mov_imm(current_instruction) && insn_mov_imm_rd(current_instruction) == reg) {
            value = insn_mov_imm_imm(current_instruction);
        } else if (insn_is_ldr_literal(current_instruction) && insn_ldr_literal_rt(current_instruction) == reg) {
            value = * (uint32_t * )(kdata + (((((uintptr_t) current_instruction - (uintptr_t) kdata) + 4) & 0xFFFFFFFC) + insn_ldr_literal_imm(current_instruction)));
        } else if (insn_is_movt(current_instruction) && insn_movt_rd(current_instruction) == reg) {
            value |= insn_movt_imm(current_instruction) << 16;
        } else if (insn_is_add_reg(current_instruction) && insn_add_reg_rd(current_instruction) == reg) {
            if (insn_add_reg_rm(current_instruction) != 15 || insn_add_reg_rn(current_instruction) != reg) {
                return 0;
            }
            value += ((uintptr_t) current_instruction - (uintptr_t) kdata) + 4;
        }
        current_instruction += insn_is_32bit(current_instruction) ? 2 : 1;
    }
    return value;
}

static uint16_t * find_literal_ref(uint32_t region, uint8_t *kdata, size_t ksize, uint16_t * insn, uint32_t address) {
    uint16_t * current_instruction = insn;
    uint32_t value[16];
    memset(value, 0, sizeof(value));

    while ((uintptr_t) current_instruction < (uintptr_t)(kdata + ksize)) {
        if (insn_is_mov_imm(current_instruction)) {
            value[insn_mov_imm_rd(current_instruction)] = insn_mov_imm_imm(current_instruction);
        } else if (insn_is_ldr_literal(current_instruction)) {
            uintptr_t literal_address = (uintptr_t) kdata + ((((uintptr_t) current_instruction - (uintptr_t) kdata) + 4) & 0xFFFFFFFC) + insn_ldr_literal_imm(current_instruction);
            if (literal_address >= (uintptr_t) kdata && (literal_address + 4) <= ((uintptr_t) kdata + ksize)) {
                value[insn_ldr_literal_rt(current_instruction)] = * (uint32_t * )(literal_address);
            }
        } else if (insn_is_movt(current_instruction)) {
            value[insn_movt_rd(current_instruction)] |= insn_movt_imm(current_instruction) << 16;
        } else if (insn_is_add_reg(current_instruction)) {
            int reg = insn_add_reg_rd(current_instruction);
            if (insn_add_reg_rm(current_instruction) == 15 && insn_add_reg_rn(current_instruction) == reg) {
                value[reg] += ((uintptr_t) current_instruction - (uintptr_t) kdata) + 4;
                if (value[reg] == address) {
                    return current_instruction;
                }
            }
        }
        current_instruction += insn_is_32bit(current_instruction) ? 2 : 1;
    }
    return NULL;
}

struct find_search_mask {
    uint16_t mask;
    uint16_t value;
};

static uint16_t *find_with_search_mask(uint32_t region, uint8_t *kdata, size_t ksize, int num_masks,
    const struct find_search_mask *masks) {
    uint16_t * end = (uint16_t * )(kdata + ksize - (num_masks *sizeof(uint16_t)));
    uint16_t * cur;
    for (cur = (uint16_t * ) kdata; cur <= end; ++cur) {
        int matched = 1;
        int i;
        for (i = 0; i < num_masks; ++i) {
            if (( * (cur + i) & masks[i].mask) != masks[i].value) {
                matched = 0;
                break;
            }
        }
        if (matched) return cur;
    }
    return NULL;
}

uint32_t find_pmap_location(uint32_t region, uint8_t *kdata, size_t ksize) {
    uint8_t *pmap_map_bd = memmem(kdata, ksize, "\"pmap_map_bd\"", sizeof("\"pmap_map_bd\""));
    if (!pmap_map_bd) return 0;

    uint16_t * ptr = find_literal_ref(region, kdata, ksize, (uint16_t *)kdata, (uintptr_t)pmap_map_bd - (uintptr_t)kdata);
    if (!ptr) return 0;

    const uint8_t search_function_end[] = { 0xF0, 0xBD };
    ptr = memmem(ptr, ksize - ((uintptr_t)ptr - (uintptr_t)kdata), search_function_end, sizeof(search_function_end));
    if (!ptr) return 0;

    uint16_t *bl = find_last_insn_matching(region, kdata, ksize, ptr, insn_is_bl);
    if (!bl) return 0;

    uint16_t *ldr_r2 = NULL;
    uint16_t *current_instruction = bl;
    while ((uintptr_t) current_instruction > (uintptr_t) kdata) {
        if (insn_is_32bit(current_instruction - 2) && !insn_is_32bit(current_instruction - 3)) {
            current_instruction -= 2;
        } else {
            --current_instruction;
        }

        if (insn_ldr_imm_rt(current_instruction) == 2 && insn_ldr_imm_imm(current_instruction) == 0) {
            ldr_r2 = current_instruction;
            break;
        } else if (insn_is_b_conditional(current_instruction) || insn_is_b_unconditional(current_instruction)) {
            break;
        }
    }

    if (ldr_r2) return find_pc_rel_value(region, kdata, ksize, ldr_r2, insn_ldr_imm_rn(ldr_r2));
    uint32_t imm32 = insn_bl_imm32(bl);
    uint32_t target = ((uintptr_t)bl - (uintptr_t)kdata) + 4 + imm32;
    if (target > ksize) return 0;

    int found = 0;
    int rd = 0;
    current_instruction = (uint16_t * )(kdata + target);
    while ((uintptr_t) current_instruction < (uintptr_t)(kdata + ksize)) {
        if (insn_is_add_reg(current_instruction) && insn_add_reg_rm(current_instruction) == 15) {
            found = 1;
            rd = insn_add_reg_rd(current_instruction);
            current_instruction += insn_is_32bit(current_instruction) ? 2 : 1;
            break;
        }
        current_instruction += insn_is_32bit(current_instruction) ? 2 : 1;
    }

    if (!found) return 0;
    return find_pc_rel_value(region, kdata, ksize, current_instruction, rd);
}

uint32_t find_proc_enforce(uint32_t region, uint8_t *kdata, size_t ksize) {
    uint8_t *proc_enforce_description = memmem(kdata, ksize, "Enforce MAC policy on process operations", sizeof("Enforce MAC policy on process operations"));
    if (!proc_enforce_description) return 0;

    uint32_t proc_enforce_description_address = region + ((uintptr_t) proc_enforce_description - (uintptr_t) kdata);
    uint8_t *proc_enforce_description_ptr = memmem(kdata, ksize, & proc_enforce_description_address, sizeof(proc_enforce_description_address));
    if (!proc_enforce_description_ptr) return 0;

    uint32_t *proc_enforce_ptr = (uint32_t *)(proc_enforce_description_ptr - (5 * sizeof(uint32_t)));
    return *proc_enforce_ptr - region;
}

uint32_t find_cs_enforcement_disable_amfi(uint32_t region, uint8_t *kdata, size_t ksize) {
    const uint8_t search_function[] = {
        0x20, 0x68, 0x40, 0xF4, 0x40, 0x70,
        0x20, 0x60, 0x00, 0x20, 0x90, 0xBD
    };
    uint8_t *ptr = memmem(kdata, ksize, search_function, sizeof(search_function));
    if (!ptr) return 0;

    uint16_t *ldrb = find_last_insn_matching(region, kdata, ksize, (uint16_t * ) ptr, insn_is_ldrb_imm);
    if (!ldrb) return 0;

    if (insn_ldrb_imm_imm(ldrb) != 0 || insn_ldrb_imm_rt(ldrb) > 12) return 0;
    return find_pc_rel_value(region, kdata, ksize, ldrb, insn_ldrb_imm_rn(ldrb));
}

uint32_t find_cs_enforcement_disable_kernel(uint32_t region, uint8_t *kdata, size_t ksize) {
    const struct find_search_mask search_masks[] = {
        {
            0xF8FF,
            0x2800
        }, // CMP Rx, #0
        {
            0xFFFF,
            0xBF04
        }, // ITT EQ
        {
            0xFFF0,
            0xF080
        }, // EOR Rx, Ry, #1
        {
            0xF0FF,
            0x0001
        },
        {
            0xF8FF,
            0x2800
        }, // CMP Rx, #0
        {
            0xFF00,
            0xD100
        } // BNE x
    };

    uint16_t *insn = find_with_search_mask(region, kdata, ksize, sizeof(search_masks) / sizeof(*search_masks), search_masks);
    if (!insn) return 0;

    if (!insn_is_cmp_imm(insn) || insn_cmp_imm_imm(insn) != 0) return 0;
    int rn = insn_cmp_imm_rn(insn);

    uint16_t *ldr_rn = NULL;
    uint16_t *ldr = insn;
    while (!ldr_rn) {
        ldr = find_last_insn_matching(region, kdata, ksize, ldr, insn_is_ldr_imm);
        if (!ldr) return 0;

        if (insn_ldr_imm_rt(ldr) == rn && insn_ldr_imm_imm(ldr) == 0) {
            ldr_rn = ldr;
            break;
        }
    }

    return find_pc_rel_value(region, kdata, ksize, ldr_rn, insn_ldr_imm_rn(ldr_rn));
}

uint32_t find_i_can_has_debugger_1(uint32_t region, uint8_t *kdata, size_t ksize) {
    const struct find_search_mask search_masks[] = {
        {
            0xFFF0,
            0xF000
        }, // AND Rx, Ry, #4
        {
            0xF0FF,
            0x0004
        },
        {
            0xFFFF,
            0x2000
        }, // MOV R0, #0
        {
            0xF8FF,
            0x2800
        }, // CMP Rx, #0
        {
            0xFFFF,
            0xBF08
        }, // IT EQ
        {
            0xFFFF,
            0x2001
        } // MOVEQ R0, #1
    };

    uint16_t *insn = find_with_search_mask(region, kdata, ksize, sizeof(search_masks) / sizeof( * search_masks), search_masks);
    if (!insn) return 0;

    uint16_t *cmp = find_last_insn_matching(region, kdata, ksize, insn, insn_is_cmp_imm);
    if (!cmp) return 0;

    int rn = insn_cmp_imm_rn(cmp);
    uint16_t *ldr_rn = NULL;
    uint16_t *ldr = cmp;
    while (!ldr_rn) {
        ldr = find_last_insn_matching(region, kdata, ksize, ldr, insn_is_ldr_imm);
        if (!ldr) return 0;

        if (insn_ldr_imm_rt(ldr) == rn && insn_ldr_imm_imm(ldr) == 0) {
            ldr_rn = ldr;
            break;
        }
    }
    return find_pc_rel_value(region, kdata, ksize, ldr_rn, insn_ldr_imm_rn(ldr_rn));
}

uint32_t find_i_can_has_debugger_2(uint32_t region, uint8_t *kdata, size_t ksize) {
    const struct find_search_mask search_masks[] = {
        {
            0xFFF0,
            0xF000
        }, // AND Rx, Ry, #4
        {
            0xF0FF,
            0x0004
        },
        {
            0xFFFF,
            0x2000
        }, // MOV R0, #0
        {
            0xF8FF,
            0x2800
        }, // CMP Rx, #0
        {
            0xFFFF,
            0xBF08
        }, // IT EQ
        {
            0xFFFF,
            0x2001
        } // MOVEQ R0, #1
    };

    uint16_t *insn = find_with_search_mask(region, kdata, ksize, sizeof(search_masks) / sizeof( * search_masks), search_masks);
    if (!insn) return 0;

    if (!insn_is_and_imm(insn) || !insn_is_cmp_imm(insn + 3) || insn_and_imm_rd(insn) != insn_cmp_imm_rn(insn + 3) || insn_and_imm_imm(insn) != 4) return 0;
    int rn = insn_and_imm_rn(insn);

    uint16_t *ldr_rn = NULL;
    uint16_t *ldr = insn;
    while (!ldr_rn) {
        ldr = find_last_insn_matching(region, kdata, ksize, ldr, insn_is_ldr_imm);
        if (!ldr)
            return 0;

        if (insn_ldr_imm_rt(ldr) == rn && insn_ldr_imm_imm(ldr) == 0) {
            ldr_rn = ldr;
            break;
        }
    }

    rn = insn_ldr_imm_rn(ldr_rn);
    ldr = ldr_rn;
    ldr_rn = NULL;

    while (!ldr_rn) {
        ldr = find_last_insn_matching(region, kdata, ksize, ldr, insn_is_ldr_imm);
        if (!ldr) return 0;

        if (insn_ldr_imm_rt(ldr) == rn && insn_ldr_imm_imm(ldr) == 0) {
            ldr_rn = ldr;
            break;
        }
    }

    uint32_t p_i_can_has_debugger_2 = find_pc_rel_value(region, kdata, ksize, ldr_rn, insn_ldr_imm_rn(ldr_rn));
    uint32_t i_can_has_debugger_2 = *(uint32_t *)(kdata + p_i_can_has_debugger_2);
    return i_can_has_debugger_2 - region;
}

uint32_t find_vm_map_enter_patch(uint32_t region, uint8_t *kdata, size_t ksize) {
    const struct find_search_mask search_masks[] = {
        {
            0xFFF0,
            0xF000
        }, // AND Rx, Ry, #6
        {
            0xF0FF,
            0x0006
        },
        {
            0xF8FF,
            0x2806
        } // CMP Rx, #6
    };

    uint16_t *insn = find_with_search_mask(region, kdata, ksize, sizeof(search_masks) / sizeof( * search_masks), search_masks);
    if (!insn) return 0;
    insn += 3;
    return ((uintptr_t) insn) - ((uintptr_t) kdata);
}

uint32_t find_vm_map_protect_patch(uint32_t region, uint8_t *kdata, size_t ksize) {
    const uint8_t search[] = {
        0x08, 0xBF, 0x10,
        0xF0, 0x80, 0x4F
    };
    uint16_t *insn = memmem(kdata, ksize, search, sizeof(search));
    if (!insn) return 0;

    insn += 3;
    return ((uintptr_t) insn) - ((uintptr_t) kdata);
}

uint32_t find_tfp0_patch(uint32_t region, uint8_t *kdata, size_t ksize) {
    const uint8_t search[] = {
        0x02, 0x46, 0x30, 0x46,
        0x21, 0x46, 0x53, 0x46
    };

    uint16_t *fn = memmem(kdata, ksize, search, sizeof(search));
    if (!fn) return 0;

    uint16_t *fn_start = find_last_insn_matching(region, kdata, ksize, fn, insn_is_preamble_push);
    if (!fn_start) return 0;
    int found = 0;

    uint16_t * current_instruction = fn_start;
    while ((uintptr_t) current_instruction < (uintptr_t) fn) {
        if (insn_is_cmp_imm(current_instruction) && insn_cmp_imm_imm(current_instruction) == 0) {
            found = 1;
            break;
        }
        current_instruction += insn_is_32bit(current_instruction) ? 2 : 1;
    }

    if (!found) return 0;
    found = 0;

    while ((uintptr_t) current_instruction < (uintptr_t) fn) {
        if (insn_is_b_conditional(current_instruction) || insn_is_b_unconditional(current_instruction)) {
            found = 1;
            break;
        }
        current_instruction += insn_is_32bit(current_instruction) ? 2 : 1;
    }

    if (!found) return 0;
    return ((uintptr_t) current_instruction) - ((uintptr_t) kdata);
}

uint32_t find_sb_patch(uint32_t region, uint8_t *kdata, size_t ksize) {
    uint8_t *control_name = memmem(kdata, ksize, "control_name", sizeof("control_name"));
    if (!control_name) return 0;

    uint16_t * ref = find_literal_ref(region, kdata, ksize, (uint16_t * ) kdata, (uintptr_t) control_name - (uintptr_t) kdata);
    if (!ref) return 0;

    uint16_t *fn_start = ref;
    while (1) {
        fn_start = find_last_insn_matching(region, kdata, ksize, fn_start, insn_is_push);
        if (!fn_start) return 0;

        uint16_t registers = insn_push_registers(fn_start);
        if ((registers & (1 << 14)) != 0 || (registers & (1 << 0 | 1 << 1)) == (1 << 0 | 1 << 1)) break;
    }

    return ((uintptr_t) fn_start) - ((uintptr_t) kdata);
}

uint32_t find_vn_getpath(uint32_t region, uint8_t *kdata, size_t ksize) {
    const uint8_t search[] = {
        0x01, 0x20, 0xCD, 0xE9, 0x00, 0x01,
        0x28, 0x46, 0x41, 0x46, 0x32, 0x46,
        0x23, 0x46
    };
    uint16_t *fn = memmem(kdata, ksize, search, sizeof(search));
    if (!fn) return 0;

    uint16_t *fn_start = find_last_insn_matching(region, kdata, ksize, fn, insn_is_preamble_push);
    if (!fn_start) return 0;
    return ((uintptr_t) fn_start | 1) - ((uintptr_t) kdata);
}

uint32_t find_memcmp(uint32_t region, uint8_t *kdata, size_t ksize) {
    const uint8_t search[] = {
        0x00, 0x23, 0x62, 0xB1, 0x91, 0xF8,
        0x00, 0x90, 0x03, 0x78, 0x4B, 0x45,
        0x09, 0xD1, 0x01, 0x3A, 0x00, 0xF1,
        0x01, 0x00, 0x01, 0xF1, 0x01, 0x01,
        0x4F, 0xF0, 0x00, 0x03, 0xF2, 0xD1,
        0x18, 0x46, 0x70, 0x47, 0xA3, 0xEB,
        0x09, 0x03, 0x18, 0x46, 0x70, 0x47
    };

    void *ptr = memmem(kdata, ksize, search, sizeof(search)) + 1;
    if (!ptr) return 0;
    return ((uintptr_t) ptr | 1) - ((uintptr_t) kdata);
}

uint32_t find_p_bootargs(uint32_t region, uint8_t *kdata, size_t ksize) {
    uint8_t *pixel_format = memmem(kdata, ksize, "BBBBBBBBGGGGGGGGRRRRRRRR", sizeof("BBBBBBBBGGGGGGGGRRRRRRRR"));
    if (!pixel_format) return 0;

    uint16_t *ref = find_literal_ref(region, kdata, ksize, (uint16_t * ) kdata, (uintptr_t) pixel_format - (uintptr_t) kdata);
    if (!ref) return 0;

    uint16_t *fn_start = find_last_insn_matching(region, kdata, ksize, ref, insn_is_preamble_push);
    if (!fn_start) return 0;

    int found = 0;
    uint16_t *current_instruction = fn_start;
    while ((uintptr_t) current_instruction < (uintptr_t) ref) {
        if (insn_is_mov_imm(current_instruction) && insn_mov_imm_imm(current_instruction) == 1) {
            found = 1;
            break;
        }

        current_instruction += insn_is_32bit(current_instruction) ? 2 : 1;
    }

    if (!found) return 0;
    int reg = insn_mov_imm_rd(current_instruction);

    found = 0;
    while ((uintptr_t) current_instruction < (uintptr_t) ref) {
        if (insn_is_str_imm(current_instruction) && insn_str_imm_imm(current_instruction) == 0 &&
            insn_str_imm_postindexed(current_instruction) == 1 && insn_str_imm_wback(current_instruction) == 0 &&
            insn_str_imm_rt(current_instruction) == reg) {
            found = 1;
            break;
        }
        current_instruction += insn_is_32bit(current_instruction) ? 2 : 1;
    }

    uint32_t pe_state = find_pc_rel_value(region, kdata, ksize, current_instruction, insn_str_imm_rn(current_instruction));
    if (!pe_state) return 0;
    return pe_state + 0x70;
}
