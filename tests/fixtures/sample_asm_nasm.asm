; Simple x86 NASM example
; with multiple sections and constructs
section .text
    global _start

_start:
    mov eax, 1
    mov ebx, 0
    int 0x80

; Print a hello message to stdout
print_hello:
    push ebp
    mov ebp, esp
    mov eax, 4
    mov ebx, 1
    mov ecx, msg
    mov edx, len
    int 0x80
    pop ebp
    ret

%define BUFFER_SIZE 1024
%define MAX_RETRIES 3

%macro PROLOGUE 0
    push ebp
    mov ebp, esp
%endmacro

%macro EPILOGUE 0
    pop ebp
    ret
%endmacro

section .data
    msg db "Hello, World!", 10, 0
    len equ $ - msg

section .bss
    buffer resb 1024
    counter resb 4

; Struct-like construct
struc point
    .x resd 1
    .y resd 1
endstruc
