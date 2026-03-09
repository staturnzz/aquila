#ifndef patches_h
#define patches_h

#include "common.h"

#define TTB_SIZE                4096
#define L1_SECT_S_BIT           (1 << 16)
#define L1_SECT_PROTO           (1 << 1)
#define L1_SECT_AP_URW          (1 << 10) | (1 << 11)
#define L1_SECT_APX             (1 << 15)
#define L1_SECT_DEFPROT         (L1_SECT_AP_URW | L1_SECT_APX)
#define L1_SECT_SORDER          (0)
#define L1_SECT_DEFCACHE        (L1_SECT_SORDER)
#define L1_PROTO_TTE(entry)     (entry | L1_SECT_S_BIT | L1_SECT_DEFPROT | L1_SECT_DEFCACHE)
#define L1_PAGE_PROTO           (1 << 0)
#define L1_COARSE_PT            (0xFFFFFC00)
#define PT_SIZE                 256
#define L2_PAGE_APX             (1 << 9)
#define BOOTARGS_STR            "cs_enforcement_disable=1 amfi_get_out_of_my_way=1"

typedef struct {
    struct {
        uint32_t proc_enforce;
        uint32_t cs_enforcement_disable;
        uint32_t bootargs;
        uint32_t mount_common;
        uint32_t pmap_location;
        uint32_t csops;
        uint32_t sandbox_call_i_can_has_debugger;
        uint32_t i_can_has_debugger_1;
        uint32_t i_can_has_debugger_2;
        uint32_t vm_fault_enter_patch;
        uint32_t vm_map_enter_patch;
        uint32_t vm_map_protect_patch;
        uint32_t tfp0_patch;
        uint32_t sb_patch;
        uint32_t vn_getpath;
        uint32_t memcmp;
        uint32_t container_required_patch;
        uint32_t tte_virt;
        uint32_t tte_phys;
        uint32_t pmap_count;
        uint32_t *pmap_list;
        uint32_t kern_phys_base;
    } ios_7;
    struct {
        uint32_t pmap_location;
        uint32_t proc_enforce;
        uint32_t cs_enforcement_disable_amfi;
        uint32_t cs_enforcement_disable_kernel;
        uint32_t i_can_has_debugger_1;
        uint32_t i_can_has_debugger_2;
        uint32_t vm_map_enter_patch;
        uint32_t vm_map_protect_patch;
        uint32_t tfp0_patch;
        uint32_t sb_patch;
        uint32_t vn_getpath;
        uint32_t memcmp;
        uint32_t p_bootargs;
        uint32_t tte_virt;
        uint32_t tte_phys;
        uint32_t l1_table_entries;
        uint32_t *l1_page_table;
        uint32_t kern_phys_base;
    } ios_6;
    struct {
        uint32_t cs_enforcement;
        uint32_t vm_map_enter_patch;
        uint32_t tfp0_patch;
        uint32_t i_can_has_debugger;
        uint32_t amfi_patch;
        uint32_t amfi_kill_patch;
        uint32_t sb_patch;
        uint32_t signature_check;
    } ios_5;
    struct {
        uint32_t tfp0_patch;
        uint32_t vm_map_enter_patch;
        uint32_t vm_map_protect_patch;
        uint32_t i_can_has_debugger;
        uint32_t cs_enforcement;
        uint32_t proc_enforce;
        uint32_t amfi_patch;
        uint32_t sb_patch;
    } ios_4;
} patches_t;

extern void sb_eval7_trampoline(void);
extern void sb_eval7_hook(void);
extern uint32_t sb_eval7_trampoline_hook_addr;
extern uint32_t sb_eval7_trampoline_len;
extern uint32_t sb_eval7_hook_orig_addr;
extern uint32_t sb_eval7_hook_vn_getpath;
extern uint32_t sb_eval7_hook_memcmp;
extern uint32_t sb_eval7_hook_len;

extern void sb_eval_trampoline(void);
extern void sb_eval_hook(void);
extern uint32_t sb_eval_trampoline_hook_addr;
extern uint32_t sb_eval_trampoline_len;
extern uint32_t sb_eval_hook_orig_addr;
extern uint32_t sb_eval_hook_vn_getpath;
extern uint32_t sb_eval_hook_memcmp;
extern uint32_t sb_eval_hook_len;

int patch_kernel(void);

#endif /* patches_h */