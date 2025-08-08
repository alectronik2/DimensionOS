section .text

%macro ISR_NO_ERR_CODE 1
global isr%1
isr%1:
	push 0
	push %1
	jmp service_interrupt
%endmacro

%macro ISR_ERR_CODE 1
global isr%1
isr%1:
	push %1
	jmp service_interrupt
%endmacro

%macro ISR_FILL 0
	%assign i 33
	%rep 256 - 33
		ISR_NO_ERR_CODE i
		%assign i i+1
	%endrep
%endmacro

ISR_NO_ERR_CODE 0
ISR_NO_ERR_CODE 1
ISR_NO_ERR_CODE 2
ISR_NO_ERR_CODE 3
ISR_NO_ERR_CODE 4
ISR_NO_ERR_CODE 5
ISR_NO_ERR_CODE 6
ISR_NO_ERR_CODE 7
ISR_ERR_CODE 8
ISR_NO_ERR_CODE 9
ISR_ERR_CODE 10
ISR_ERR_CODE 11
ISR_ERR_CODE 12
ISR_ERR_CODE 13
ISR_ERR_CODE 14
ISR_NO_ERR_CODE 15
ISR_NO_ERR_CODE 16
ISR_ERR_CODE 17
ISR_NO_ERR_CODE 18
ISR_NO_ERR_CODE 19
ISR_NO_ERR_CODE 20
ISR_NO_ERR_CODE 21
ISR_NO_ERR_CODE 22
ISR_NO_ERR_CODE 23
ISR_NO_ERR_CODE 24
ISR_NO_ERR_CODE 25
ISR_NO_ERR_CODE 26
ISR_NO_ERR_CODE 27
ISR_NO_ERR_CODE 28
ISR_NO_ERR_CODE 29
ISR_ERR_CODE 30
ISR_NO_ERR_CODE 31
ISR_NO_ERR_CODE 32

ISR_FILL

extern handle_interrupt

service_interrupt:
	cli
    push rax
	push rbx
	push rcx
	push rdx
	push rbp
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15

	mov rdi, rsp
	call handle_interrupt

	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rbp
	pop rdx
	pop rcx
	pop rbx
	pop rax

	sti

	add rsp, 16
	iretq

section .text

global switch_context

; void switch_context(task_context_t *old_ctx, task_context_t *new_ctx)
; rdi = old_ctx, rsi = new_ctx
switch_context:
    ; Save current task's context to old_ctx
    mov [rdi + 0],  rax    ; rax
    mov [rdi + 8],  rbx    ; rbx
    mov [rdi + 16], rcx    ; rcx
    mov [rdi + 24], rdx    ; rdx
    mov [rdi + 32], rsi    ; rsi (overwritten by new_ctx, but we save original)
    mov rax, rdi           ; save old rdi in rax temporarily
    mov [rdi + 40], rax    ; rdi (original value before function call)
    mov [rdi + 48], rbp    ; rbp
    mov [rdi + 56], rsp    ; rsp
    mov [rdi + 64], r8     ; r8
    mov [rdi + 72], r9     ; r9
    mov [rdi + 80], r10    ; r10
    mov [rdi + 88], r11    ; r11
    mov [rdi + 96], r12    ; r12
    mov [rdi + 104], r13   ; r13
    mov [rdi + 112], r14   ; r14
    mov [rdi + 120], r15   ; r15
    
    ; Save rip (return address from stack)
    mov rax, [rsp]
    mov [rdi + 128], rax   ; rip
    
    ; Save rflags
    pushfq
    pop rax
    mov [rdi + 136], rax   ; rflags
    
    ; Save segment registers
    mov ax, cs
    mov [rdi + 144], ax    ; cs
    mov ax, ss
    mov [rdi + 146], ax    ; ss
    mov ax, ds
    mov [rdi + 148], ax    ; ds
    mov ax, es
    mov [rdi + 150], ax    ; es
    mov ax, fs
    mov [rdi + 152], ax    ; fs
    mov ax, gs
    mov [rdi + 154], ax    ; gs

    ; Load new task's context from new_ctx (rsi)
    mov rax, [rsi + 0]     ; rax
    mov rbx, [rsi + 8]     ; rbx
    mov rcx, [rsi + 16]    ; rcx
    mov rdx, [rsi + 24]    ; rdx
    ; Skip rsi for now, we need it to access new_ctx
    mov rdi, [rsi + 40]    ; rdi
    mov rbp, [rsi + 48]    ; rbp
    mov rsp, [rsi + 56]    ; rsp
    mov r8,  [rsi + 64]    ; r8
    mov r9,  [rsi + 72]    ; r9
    mov r10, [rsi + 80]    ; r10
    mov r11, [rsi + 88]    ; r11
    mov r12, [rsi + 96]    ; r12
    mov r13, [rsi + 104]   ; r13
    mov r14, [rsi + 112]   ; r14
    mov r15, [rsi + 120]   ; r15
    
    ; Load rflags
    mov r8, [rsi + 136]    ; rflags
    push r8
    popfq
    
    ; Load segment registers
    mov r8w, [rsi + 148]   ; ds
    mov ds, r8w
    mov r8w, [rsi + 150]   ; es
    mov es, r8w
    mov r8w, [rsi + 152]   ; fs
    mov fs, r8w
    mov r8w, [rsi + 154]   ; gs
    mov gs, r8w
    
    ; Push new rip onto stack for return
    mov r8, [rsi + 128]    ; rip
    push r8
    
    ; Finally load rsi
    mov rsi, [rsi + 32]    ; rsi
    
    ; Jump to new task
    ret