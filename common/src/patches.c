#include "common.h"
#include "util.h"
#include "exploit.h"
#include "memory.h"
#include "patchfinder.h"
#include "patches.h"

static void *kernel_data = NULL;
static size_t kernel_data_size = 0xffe000;

int patch_page_table(patches_t *patches, uint32_t page) {
    if (kinfo->version[0] == 7) {
        uint32_t i = page >> 20;
        uint32_t j = (page >> 12) & 0xFF;

        uint32_t addr = patches->ios_7.tte_virt + (i << 2);
        uint32_t entry = kread32(addr);
        if (entry == 0) return -1;

        if ((entry & L1_PAGE_PROTO) == L1_PAGE_PROTO) {
            uint32_t page_entry = ((entry & L1_COARSE_PT) - patches->ios_7.tte_phys) + patches->ios_7.tte_virt;
            uint32_t addr2 = page_entry + (j << 2);
            uint32_t entry2 = kread32(addr2);
            if (entry2 != 0) {
                uint32_t new_entry2 = (entry2 & (~L2_PAGE_APX));
                kwrite32(addr2, new_entry2);
            }
        } else if ((entry & L1_SECT_PROTO) == L1_SECT_PROTO) {
            uint32_t new_entry = L1_PROTO_TTE(entry);
            new_entry &= ~L1_SECT_APX;
            kwrite32(addr, new_entry);
        }
    } else if (kinfo->version[0] == 6) {
        uint32_t idx = page >> 20;
        uint32_t entry = patches->ios_6.l1_page_table[idx];

        if (entry == 0) {
            entry = kread32(patches->ios_6.tte_virt + (idx * 4));
            patches->ios_6.l1_page_table[idx] = entry;
        }

        if ((entry & 0x3) == 2) {
            if ((idx << 20) == ((page >> 20) << 20)) {
                entry &= ~(1 << 15);
                kwrite32(patches->ios_6.tte_virt + (idx * 4), entry);
                goto done;
            }
        } else if ((entry & 0x3) == 1) {
            uint32_t page_table_pa = (entry >> 10) << 10;
            uint32_t page_table_va = (page_table_pa - patches->ios_6.kern_phys_base) + kinfo->kernel_base;

            int l2_idx = (page >> 12) & 0xff;
            uint32_t l2_entry = kread32(page_table_va + (l2_idx * 4));

            if ((l2_entry & 0x3) == 1 || (l2_entry & 0x2) == 2) {
                if (((idx << 20) + (l2_idx << 12)) == page) {
                    l2_entry &= ~(1 << 9);
                    kwrite32(page_table_va + (l2_idx * 4), l2_entry);
                    goto done;
                }
            }
        }
    }

done:
    usleep(100000);
    return 0;
}

void kwrite_buf_exec(patches_t *patches, uint32_t addr, void *data, size_t size) {
    if (addr == 0) return;
    uint32_t test_read = 0;

    if (MACH_PORT_VALID(kinfo->tfp0) && (kinfo->version[0] == 6 || kinfo->version[0] == 7)) {
        uint32_t page = (addr & ~0xfff);
        if (patch_page_table(patches, page) != 0) return;
        kwrite_buf(addr, data, size);
    } else {
        kwrite_buf(addr, data, size);
    }

    usleep(50000);
    for (int i = 0; i < 10; i++) {
        test_read = kread32(addr);
        usleep(10000);
    }
}

void kwrite16_exec(patches_t *patches, uint32_t addr, uint16_t val) {
    kwrite_buf_exec(patches, addr, &val, 2);
}

void kwrite32_exec(patches_t *patches, uint32_t addr, uint32_t val) {
    kwrite_buf_exec(patches, addr, &val, 4);
}

uint32_t find_patch_offset(uint32_t (*func)(uint32_t, uint8_t *, size_t)) {
    uint32_t addr = func(kinfo->kernel_base, kernel_data, kernel_data_size);
    if (addr <= 0xffff) return 0;
    if ((addr & 0xf0000000) == 0x80000000) return addr;
    return addr + kinfo->kernel_base;
}

void set_bootargs(patches_t *patches, const char *bootargs) {
    if (kinfo->version[0] < 6) return;
    size_t args_size = strlen(bootargs) + 1;
    size_t buf_size = (args_size + 3) / 4 * 4;
    
    char *buf = calloc(1, buf_size);
    strlcpy(buf, bootargs, buf_size);
    memset(buf + args_size, 0, buf_size - args_size);
    
    uint32_t str_addr = 0;
    if (kinfo->version[0] == 6) {
        str_addr = kread32(patches->ios_6.p_bootargs) + 0x38;
    } else {
        str_addr = kread32(patches->ios_7.bootargs) + 0x38;
    }

    kwrite_buf(str_addr, buf, args_size);
    free(buf);
    usleep(10000);
}

int patch_kernel(void) {
    patches_t *patches = calloc(1, sizeof(patches_t));
    kernel_data = calloc(1, kernel_data_size);
    kread_buf(kinfo->kernel_base, kernel_data, kernel_data_size);
    int status = -1;

    if (kinfo->version[0] == 7) {
        if ((patches->ios_7.proc_enforce = find_patch_offset(find_proc_enforce_ios_7)) == 0) goto done;
        if ((patches->ios_7.cs_enforcement_disable = find_patch_offset(find_cs_enforcement_disable_amfi_ios_7)) == 0) goto done;
        if ((patches->ios_7.bootargs = find_patch_offset(find_p_bootargs_ios_7)) == 0) goto done;
        if ((patches->ios_7.mount_common = find_patch_offset(find_mount_ios_7)) == 0) goto done;
        if ((patches->ios_7.pmap_location = find_patch_offset(find_pmap_location_ios_7)) == 0) goto done;
        if ((patches->ios_7.csops = find_patch_offset(find_csops_ios_7)) == 0) goto done;
        if ((patches->ios_7.sandbox_call_i_can_has_debugger = find_patch_offset(find_sandbox_call_i_can_has_debugger_ios_7)) == 0) goto done;
        if ((patches->ios_7.i_can_has_debugger_1 = find_patch_offset(find_i_can_has_debugger_1_ios_7)) == 0) goto done;
        if ((patches->ios_7.i_can_has_debugger_2 = find_patch_offset(find_i_can_has_debugger_2_ios_7)) == 0) goto done;
        if ((patches->ios_7.vm_fault_enter_patch = find_patch_offset(find_vm_fault_enter_patch_ios_7)) == 0) goto done;
        if ((patches->ios_7.vm_map_enter_patch = find_patch_offset(find_vm_map_enter_patch_ios_7)) == 0) goto done;
        if ((patches->ios_7.vm_map_protect_patch = find_patch_offset(find_vm_map_protect_patch_ios_7)) == 0) goto done;
        if ((patches->ios_7.tfp0_patch = find_patch_offset(find_tfp0_patch_ios_7)) == 0) goto done;
        if ((patches->ios_7.sb_patch = find_patch_offset(find_sb_patch_ios_7)) == 0) goto done;
        if ((patches->ios_7.vn_getpath = find_patch_offset(find_vn_getpath_ios_7)) == 0) goto done;
        if ((patches->ios_7.memcmp = find_patch_offset(find_memcmp_ios_7)) == 0) goto done;
        if ((patches->ios_7.container_required_patch = find_patch_offset(find_container_required_patch_ios_7)) == 0) goto done;
        if ((patches->ios_7.pmap_list = calloc(1, TTB_SIZE * sizeof(uint32_t))) == NULL) goto done;

        uint32_t kernel_pmap_store = kread32(patches->ios_7.pmap_location);
        patches->ios_7.tte_virt = kread32(kernel_pmap_store);
        patches->ios_7.tte_phys = kread32(kernel_pmap_store + 4);
        patches->ios_7.kern_phys_base = patches->ios_7.tte_phys - (patches->ios_7.tte_virt - kinfo->kernel_base);
        patches->ios_7.pmap_count = 0;

        if (MACH_PORT_VALID(kinfo->tfp0)) {
            for (uint32_t i = 0; i < TTB_SIZE; i++) {
                uint32_t addr = patches->ios_7.tte_virt + (i << 2);
                uint32_t entry = kread32(addr);
                if (entry == 0) continue;

                if ((entry & 0x3) == 1) {
                    uint32_t lvl_pg_addr = (entry & (~0x3ff)) - patches->ios_7.tte_phys + patches->ios_7.tte_virt;
                    for (int i = 0; i < 256; i++) {
                        uint32_t sladdr  = lvl_pg_addr + (i << 2);
                        uint32_t slentry = kread32(sladdr);
                        if (slentry == 0) continue;
                        
                        uint32_t new_entry = slentry & (~0x200);
                        if (slentry != new_entry) {
                            kwrite32(sladdr, new_entry);
                            patches->ios_7.pmap_list[patches->ios_7.pmap_count++] = sladdr;
                        }
                    }
                    continue;
                }
                
                if ((entry & L1_SECT_PROTO) == 2) {
                    uint32_t new_entry = L1_PROTO_TTE(entry);
                    new_entry &= ~L1_SECT_APX;
                    kwrite32(addr, new_entry);
                }
            }
        }

        set_bootargs(patches, "cs_enforcement_disable=1 amfi_get_out_of_my_way=1");
        usleep(100000);

        kwrite32(patches->ios_7.proc_enforce, 0);    
        kwrite32(patches->ios_7.cs_enforcement_disable, 1);
        kwrite32(patches->ios_7.cs_enforcement_disable-4, 1);
        kwrite32(patches->ios_7.i_can_has_debugger_1, 1);
        kwrite32(patches->ios_7.i_can_has_debugger_2, 1);
        kwrite32(patches->ios_7.container_required_patch, 'haxx');

        kwrite32_exec(patches, patches->ios_7.mount_common, 0x0501F025);
        kwrite32_exec(patches, patches->ios_7.tfp0_patch, 0xbf00bf00);
        kwrite32_exec(patches, patches->ios_7.sandbox_call_i_can_has_debugger, 0xbf00bf00);
        kwrite32_exec(patches, patches->ios_7.vm_fault_enter_patch, 0x2201bf00);
        kwrite32_exec(patches, patches->ios_7.vm_map_protect_patch, 0xbf00bf00);
        kwrite16_exec(patches, patches->ios_7.vm_map_enter_patch, 0x29ff);
        kwrite32_exec(patches, patches->ios_7.csops, 0xbf00bf00);

        uint8_t *sb_eval7_trampoline_addr = (uint8_t *)(((uint32_t)&sb_eval7_trampoline) & ~1);
        uint8_t *sb_eval7_hook_addr = (uint8_t *)(((uint32_t)&sb_eval7_hook) & ~1);
        uint8_t *trampoline = calloc(1, sb_eval7_trampoline_len);
        
        memcpy(trampoline, sb_eval7_trampoline_addr, sb_eval7_trampoline_len);
        uint32_t trampoline_offset = (int32_t)&sb_eval7_trampoline_hook_addr - (int32_t)sb_eval7_trampoline_addr;
        *(uint32_t*)(trampoline + trampoline_offset) = kinfo->kernel_base + 0x801;
        uint8_t *overwritten_inst = malloc(sb_eval7_trampoline_len + 4);
        kread_buf(patches->ios_7.sb_patch, overwritten_inst, sb_eval7_trampoline_len + 4);

        if (memcmp(overwritten_inst, trampoline, sb_eval7_trampoline_len) != 0) {
            uint32_t overwritten_inst_size = 0;
            uint16_t *current_inst = (uint16_t *)overwritten_inst;
            
            while (((int32_t)current_inst - (int32_t)overwritten_inst) < sb_eval7_trampoline_len) {
                if((*current_inst & 0xe000) == 0xe000 && (*current_inst & 0x1800) != 0x0) {
                    overwritten_inst_size += 4;
                    current_inst += 2;
                } else {
                    overwritten_inst_size += 2;
                    current_inst += 1;
                }
            }
            
            uint16_t bx_r9 = 0x4748;
            uint32_t hook_size = ((sb_eval7_hook_len + overwritten_inst_size + sizeof(bx_r9) + 3) / 4) * 4;
            uint8_t *hook_data = calloc(1, hook_size);
            
            memcpy(hook_data, sb_eval7_hook_addr, sb_eval7_hook_len);
            memcpy(hook_data + sb_eval7_hook_len, overwritten_inst, overwritten_inst_size);
            memcpy(hook_data + sb_eval7_hook_len + overwritten_inst_size, &bx_r9, sizeof(bx_r9));

            *((uint32_t*)(hook_data + ((int32_t)&sb_eval7_hook_orig_addr - (int32_t)sb_eval7_hook_addr))) = patches->ios_7.sb_patch + overwritten_inst_size + 1;
            *((uint32_t*)(hook_data + ((int32_t)&sb_eval7_hook_vn_getpath - (int32_t)sb_eval7_hook_addr))) = patches->ios_7.vn_getpath;
            *((uint32_t*)(hook_data + ((int32_t)&sb_eval7_hook_memcmp - (int32_t)sb_eval7_hook_addr))) = patches->ios_7.memcmp;

            kwrite_buf_exec(patches, (kinfo->kernel_base + 0x800), hook_data, hook_size);
            kwrite_buf_exec(patches, patches->ios_7.sb_patch, trampoline, sb_eval7_trampoline_len);
            free(hook_data);
        }
        
        usleep(100000);
        free(overwritten_inst);
        free(trampoline);
    } else if (kinfo->version[0] == 6) {
        if ((patches->ios_6.pmap_location = find_patch_offset(find_pmap_location)) == 0) goto done;
        if ((patches->ios_6.proc_enforce = find_patch_offset(find_proc_enforce)) == 0) goto done;
        if ((patches->ios_6.cs_enforcement_disable_amfi = find_patch_offset(find_cs_enforcement_disable_amfi)) == 0) goto done;
        if ((patches->ios_6.cs_enforcement_disable_kernel = find_patch_offset(find_cs_enforcement_disable_kernel)) == 0) goto done;
        if ((patches->ios_6.i_can_has_debugger_1 = find_patch_offset(find_i_can_has_debugger_1)) == 0) goto done;
        if ((patches->ios_6.i_can_has_debugger_2 = find_patch_offset(find_i_can_has_debugger_2)) == 0) goto done;
        if ((patches->ios_6.vm_map_enter_patch = find_patch_offset(find_vm_map_enter_patch)) == 0) goto done;
        if ((patches->ios_6.vm_map_protect_patch = find_patch_offset(find_vm_map_protect_patch)) == 0) goto done;
        if ((patches->ios_6.tfp0_patch = find_patch_offset(find_tfp0_patch)) == 0) goto done;
        if ((patches->ios_6.sb_patch = find_patch_offset(find_sb_patch)) == 0) goto done;
        if ((patches->ios_6.vn_getpath = find_patch_offset(find_vn_getpath)) == 0) goto done;
        if ((patches->ios_6.memcmp = find_patch_offset(find_memcmp)) == 0) goto done;
        if ((patches->ios_6.p_bootargs = find_patch_offset(find_p_bootargs)) == 0) goto done;

        uint32_t kernel_pmap_store = kread32(patches->ios_6.pmap_location);
        patches->ios_6.tte_virt = kread32(kernel_pmap_store);
        patches->ios_6.tte_phys = kread32(kernel_pmap_store + 4);
        patches->ios_6.l1_table_entries = kread32(kernel_pmap_store + 0x54);
        patches->ios_6.kern_phys_base =  patches->ios_6.tte_phys - (patches->ios_6.tte_virt - kinfo->kernel_base);
        patches->ios_6.l1_page_table = calloc(1, patches->ios_6.l1_table_entries * 4);

        kwrite8(patches->ios_6.proc_enforce, 0);
        kwrite8(patches->ios_6.cs_enforcement_disable_amfi, 1);
        kwrite8(patches->ios_6.cs_enforcement_disable_kernel, 1);
        kwrite8(patches->ios_6.i_can_has_debugger_1, 1);
        kwrite8(patches->ios_6.i_can_has_debugger_2, 1);

        kwrite16_exec(patches, patches->ios_6.vm_map_enter_patch, 0xbf00);
        kwrite16_exec(patches, patches->ios_6.vm_map_protect_patch, 0xe005);
        kwrite16_exec(patches, patches->ios_6.tfp0_patch, 0xe006);
        set_bootargs(patches, "cs_enforcement_disable=1");

        uint8_t *sb_eval_trampoline_addr = (uint8_t *)(((uint32_t)&sb_eval_trampoline) & ~1);
        uint8_t *sb_eval_hook_addr = (uint8_t *)(((uint32_t)&sb_eval_hook) & ~1);
        uint8_t *trampoline = calloc(1, sb_eval_trampoline_len);
        
        memcpy(trampoline, sb_eval_trampoline_addr, sb_eval_trampoline_len);
        uint32_t trampoline_offset = (int32_t)&sb_eval_trampoline_hook_addr - (int32_t)sb_eval_trampoline_addr;
        *(uint32_t*)(trampoline + trampoline_offset) = kinfo->kernel_base + 0x701;
        uint8_t *overwritten_inst = malloc(sb_eval_trampoline_len + 4);
        kread_buf(patches->ios_6.sb_patch, overwritten_inst, sb_eval_trampoline_len + 4);

        if (memcmp(overwritten_inst, trampoline, sb_eval_trampoline_len) != 0) {
            uint32_t overwritten_inst_size = 0;
            uint16_t *current_inst = (uint16_t *)overwritten_inst;
            
            while (((int32_t)current_inst - (int32_t)overwritten_inst) < sb_eval_trampoline_len) {
                if((*current_inst & 0xe000) == 0xe000 && (*current_inst & 0x1800) != 0x0) {
                    overwritten_inst_size += 4;
                    current_inst += 2;
                } else {
                    overwritten_inst_size += 2;
                    current_inst += 1;
                }
            }
            
            uint16_t bx_r9 = 0x4748;
            uint32_t hook_size = ((sb_eval_hook_len + overwritten_inst_size + sizeof(bx_r9) + 3) / 4) * 4;
            uint8_t *hook_data = calloc(1, hook_size);
            
            memcpy(hook_data, sb_eval_hook_addr, sb_eval_hook_len);
            memcpy(hook_data + sb_eval_hook_len, overwritten_inst, overwritten_inst_size);
            memcpy(hook_data + sb_eval_hook_len + overwritten_inst_size, &bx_r9, sizeof(bx_r9));

            *((uint32_t*)(hook_data + ((int32_t)&sb_eval_hook_orig_addr - (int32_t)sb_eval_hook_addr))) = patches->ios_6.sb_patch + overwritten_inst_size + 1;
            *((uint32_t*)(hook_data + ((int32_t)&sb_eval_hook_vn_getpath - (int32_t)sb_eval_hook_addr))) = patches->ios_6.vn_getpath;
            *((uint32_t*)(hook_data + ((int32_t)&sb_eval_hook_memcmp - (int32_t)sb_eval_hook_addr))) = patches->ios_6.memcmp;

            kwrite_buf_exec(patches, (kinfo->kernel_base + 0x700), hook_data, hook_size);
            kwrite_buf_exec(patches, patches->ios_6.sb_patch, trampoline, sb_eval_trampoline_len);
            free(hook_data);
        }
        
        usleep(100000);
        free(overwritten_inst);
        free(trampoline);
    } else if (kinfo->version[0] == 5) {
        if ((patches->ios_5.cs_enforcement = find_patch_offset(find_cs_enforcement_ios_5)) == 0) goto done;        
        if ((patches->ios_5.vm_map_enter_patch = find_patch_offset(find_vm_map_enter_patch_ios_5)) == 0) goto done;
        if ((patches->ios_5.tfp0_patch = find_patch_offset(find_tfp0_patch_ios_5)) == 0) goto done;
        if ((patches->ios_5.i_can_has_debugger = find_patch_offset(find_i_can_has_debugger_ios_5)) == 0) goto done;
        if ((patches->ios_5.amfi_patch = find_patch_offset(find_amfi_patch_ios_5)) == 0) goto done;
        if ((patches->ios_5.sb_patch = find_patch_offset(find_sb_patch_ios_5)) == 0) goto done;
        if ((patches->ios_5.signature_check = find_patch_offset(find_signature_check_ios_5)) == 0) goto done;
        patches->ios_5.amfi_kill_patch = find_patch_offset(find_amfi_kill_patch_ios_5);

        kwrite16_exec(patches, patches->ios_5.cs_enforcement, 0x2301);
        kwrite16_exec(patches, patches->ios_5.vm_map_enter_patch, 0x28ff);
        kwrite16_exec(patches, patches->ios_5.tfp0_patch, 0xe006);
        kwrite16_exec(patches, patches->ios_5.i_can_has_debugger, 0x2001);
        kwrite16_exec(patches, patches->ios_5.amfi_patch, 0x2000);
        kwrite16_exec(patches, patches->ios_5.sb_patch, 0x2301);
        kwrite16_exec(patches, patches->ios_5.sb_patch + 0x2, 0x2301);
        kwrite16_exec(patches, patches->ios_5.signature_check, 0x2000);

        if (patches->ios_5.amfi_kill_patch != 0) {
            kwrite16_exec(patches, patches->ios_5.amfi_kill_patch, 0x46c0);
        }
    } else if (kinfo->version[0] == 4) {
        if ((patches->ios_4.tfp0_patch = find_patch_offset(find_tfp0_patch_ios_4)) == 0) goto done;
        if ((patches->ios_4.vm_map_enter_patch = find_patch_offset(find_vm_map_enter_patch_ios_4)) == 0) goto done;
        if ((patches->ios_4.vm_map_protect_patch = find_patch_offset(find_vm_map_protect_patch_ios_4)) == 0) goto done;
        if ((patches->ios_4.i_can_has_debugger = find_patch_offset(find_i_can_has_debugger_ios_4)) == 0) goto done;
        if ((patches->ios_4.cs_enforcement = find_patch_offset(find_cs_enforcement_ios_4)) == 0) goto done;
        if ((patches->ios_4.proc_enforce = find_patch_offset(find_proc_enforce_ios_4)) == 0) goto done;
        if ((patches->ios_4.amfi_patch = find_patch_offset(find_amfi_patch_ios_4)) == 0) goto done;
        if ((patches->ios_4.sb_patch = find_patch_offset(find_sb_patch_ios_4)) == 0) goto done;

        kwrite8(patches->ios_4.i_can_has_debugger, 1);
        kwrite8(patches->ios_4.cs_enforcement, 1);
        kwrite8(patches->ios_4.proc_enforce, 0);

        kwrite16_exec(patches, patches->ios_4.tfp0_patch, 0xe00b);
        kwrite16_exec(patches, patches->ios_4.vm_map_enter_patch, 0x46c0);
        kwrite16_exec(patches, patches->ios_4.vm_map_protect_patch, 0x46c0);
        kwrite16_exec(patches, patches->ios_4.vm_map_protect_patch + 0x2, 0x46c0);
        kwrite16_exec(patches, patches->ios_4.amfi_patch, 0x2001);
        kwrite16_exec(patches, patches->ios_4.amfi_patch + 0x2, 0x4770);

        uint16_t reg = (kread16(patches->ios_4.sb_patch) & 0x000F);
        if (reg == 0) {
            kwrite16_exec(patches, patches->ios_4.sb_patch, 0x2001);
            kwrite16_exec(patches, patches->ios_4.sb_patch + 0x2, 0x2331);
        } else {
            kwrite16_exec(patches, patches->ios_4.sb_patch, 0x2301);
            kwrite16_exec(patches, patches->ios_4.sb_patch + 0x2, 0x2031);
        }
    }
    
    usleep(250000);
    sync();
    status = 0;

done:
    if (patches != NULL) free(patches);
    if (kernel_data != NULL) free(kernel_data);
    return status;
}
