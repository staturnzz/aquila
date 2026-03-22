#ifndef patchfinder_h
#define patchfinder_h

#include "common.h"

uint32_t find_pmap_location(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_proc_enforce(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_cs_enforcement_disable_amfi(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_cs_enforcement_disable_kernel(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_i_can_has_debugger_1(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_i_can_has_debugger_2(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_vm_map_enter_patch(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_vm_map_protect_patch(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_tfp0_patch(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_sb_patch(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_vn_getpath(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_memcmp(uint32_t region, uint8_t* kdata, size_t ksize);
uint32_t find_p_bootargs(uint32_t region, uint8_t* kdata, size_t ksize);

uint32_t find_cs_enforcement_ios_5(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_vm_map_enter_patch_ios_5(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_tfp0_patch_ios_5(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_i_can_has_debugger_ios_5(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_amfi_patch_ios_5(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_amfi_kill_patch_ios_5(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_sb_patch_ios_5(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_signature_check_ios_5(uint32_t region, uint8_t *kdata, size_t ksize);

uint32_t find_tfp0_patch_ios_4(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_vm_map_enter_patch_ios_4(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_vm_map_protect_patch_ios_4(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_i_can_has_debugger_ios_4(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_cs_enforcement_ios_4(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_proc_enforce_ios_4(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_amfi_patch_ios_4(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_sb_patch_ios_4(uint32_t region, uint8_t *kdata, size_t ksize);

uint32_t find_proc_enforce_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_cs_enforcement_disable_amfi_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_p_bootargs_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_mount_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_pmap_location_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_csops_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_sandbox_call_i_can_has_debugger_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint16_t *find_PE_reboot_on_panic_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_i_can_has_debugger_1_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_i_can_has_debugger_2_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_vm_fault_enter_patch_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_vm_map_enter_patch_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_vm_map_protect_patch_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_tfp0_patch_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_sb_patch_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_vn_getpath_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_memcmp_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_container_required_patch_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_sysent_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);
uint32_t find_copyinstr_ios_7(uint32_t region, uint8_t *kdata, size_t ksize);

#endif /* patchfinder_h */
