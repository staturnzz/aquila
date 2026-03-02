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

#endif /* patchfinder_h */
