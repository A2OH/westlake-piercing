.text
.globl _start
_start:
    sub  sp, sp, #64
    stp  x0, x1, [sp]
    stp  x2, x8, [sp, #16]
    mov  w9, #0x524c
    movk w9, #0x3d58, lsl #16      // "LRX=" little-endian
    str  w9, [sp, #32]
    stur x30, [sp, #36]            // 8 raw bytes of caller return addr
    mov  x0, #2                    // fd = stderr
    add  x1, sp, #32
    mov  x2, #12                   // "LRX=" + 8 bytes
    mov  x8, #64                   // __NR_write
    svc  #0
    ldp  x0, x1, [sp]
    ldp  x2, x8, [sp, #16]
    add  sp, sp, #64
    sub  sp, sp, #64               // displaced ThrowStackOverflowError[0]
