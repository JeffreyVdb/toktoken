/* GAS (GNU Assembler) example for x86-64
 * Demonstrates labels, macros, constants, and sections
 */
    .text
    .globl main

main:
    pushq %rbp
    movq %rsp, %rbp
    call helper
    xorl %eax, %eax
    popq %rbp
    ret

; Helper function with preceding comment
.type helper, @function
helper:
    movl $42, %eax
    ret

.set MAX_COUNT, 256
.equ STACK_SIZE, 4096

.macro save_regs
    pushq %rbx
    pushq %r12
    pushq %r13
.endm

.macro restore_regs
    popq %r13
    popq %r12
    popq %rbx
.endm

.data
message:
    .asciz "Hello from GAS"

.bss
    .lcomm buffer, 1024
    .lcomm scratch, 256
