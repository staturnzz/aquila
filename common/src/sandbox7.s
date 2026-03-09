.global _sb_eval7_hook, _sb_eval7_hook_orig_addr, _sb_eval7_hook_vn_getpath
.global _sb_eval7_hook_memcmp, _sb_eval7_hook_len, _sb_eval7_trampoline
.global _sb_eval7_trampoline_hook_addr, _sb_eval7_trampoline_len
.align 2
.thumb

_sb_eval7_hook:
    pop     {r0, r1}
    push    {r0-r4, lr}
    sub     sp, #0x44
    ldr     r4, [r3, #0x20]
    cmp     r4, #1
    bne     L_actually_eval

    ldr     r4, [r3, #0x24]
    cmp     r4, #0
    beq     L_actually_eval

    ldr     r3, _sb_eval7_hook_vn_getpath
    mov     r1, sp
    mov     r0, #0x40
    add     r2, sp, #0x40
    str     r0, [r2]
    mov     r0, r4
    blx     r3

    cmp     r0, #28
    beq     L_enospc

    cmp     r0, #0
    bne     L_actually_eval


L_enospc:
    mov     r0, sp
    adr     r1, _var_mobile_path
    mov     r2, #19
    ldr     r3, _sb_eval7_hook_memcmp
    blx     r3
    cmp     r0, #0
    bne     L_allow
    
    mov     r0, sp
    adr     r1, _preferences_apple_path
    mov     r2, #49
    ldr     r3, _sb_eval7_hook_memcmp
    blx     r3
    cmp     r0, #0
    beq     L_actually_eval

    mov     r0, sp
    adr     r1, _preferences_path
    mov     r2, #39
    ldr     r3, _sb_eval7_hook_memcmp
    blx     r3
    cmp     r0, #0
    bne     L_actually_eval


L_allow:
    add     sp, #0x44
    pop     {r0}
    mov     r1, #0
    str     r1, [r0]
    mov     r1, #0x18
    str     r1, [r0, #4]
    pop     {r1-r4, pc}

L_actually_eval:
    add     sp, #0x44
    ldr     r0, [sp, #0x14]
    mov     lr, r0
    ldr     r1, _sb_eval7_hook_orig_addr
    mov     r9, r1

    pop     {r0-r4}
    add     sp, #4
    b       _jump_back

.align 2
_var_mobile_path:               .ascii "/private/var/mobile\0"
.align 2
_preferences_path:              .ascii "/private/var/mobile/Library/Preferences\0"
.align 2
_preferences_apple_path:        .ascii "/private/var/mobile/Library/Preferences/com.apple\0"
.align 2
_sb_eval7_hook_orig_addr:        .word 0x0
.align 2
_sb_eval7_hook_vn_getpath:       .word 0x0
.align 2
_sb_eval7_hook_memcmp:           .word 0x0

.align 2
_jump_back:
_sb_eval7_hook_end:
_sb_eval7_hook_len:              .word (_sb_eval7_hook_end - _sb_eval7_hook)
_padding1:                      .space 512

_sb_eval7_trampoline:
    push  {r0, r1}
    ldr   r0, _sb_eval7_trampoline_hook_addr
    bx    r0

_sb_eval7_trampoline_hook_addr:  .word 0x0
_sb_eval7_trampoline_end:
_sb_eval7_trampoline_len:        .word (_sb_eval7_trampoline_end - _sb_eval7_trampoline)
_padding2:                      .space 512
