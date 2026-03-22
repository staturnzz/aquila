.global _dsc_patch
.align 2
.thumb

_dsc_patch:
    push {r4, r5, r6, r7, lr}
    sub.w sp, sp, #0x410
    mov r4, r0
    mov r5, r1
    mov r6, r2
    ldr r0, [r1]
    add r1, sp, #8
    mov.w r2, #0x400
    add.w r3, sp, #0x408
    ldr r4, [pc, #0x4c]
    blx r4
    add r0, sp, #8
    adr r1, #0x54
    bl 2f
    cmp r0, #0
    bne 0f
    mov.w r0, #-1
    str r0, [r6]
    mov.w r0, #2
    b 1f
0:
    mov r0, r4
    mov r1, r5
    mov r2, r6
    ldr r3, [pc, #0x30]
    blx r3
1:
    add.w sp, sp, #0x410
    pop {r4, r5, r6, r7, pc}
2:
    ldrb r2, [r0]
    ldrb r3, [r1]
    cmp r2, r3
    bne 3f
    cmp r3, #0
    beq 4f
    add.w r0, r0, #1
    add.w r1, r1, #1
    b 2b

3:
    mov.w r0, #-1
    bx lr

4:
    mov.w r0, #0
    bx lr
    nop 

.long 0x41414141 // copyinstr ptr
.long 0x42424242 // stat/stat64 ptr
.long 0x00000000 // padding
.ascii "/System/Library/Caches/com.apple.dyld/enable-dylibs-to-override-cache\0"

_padding: .space 512
