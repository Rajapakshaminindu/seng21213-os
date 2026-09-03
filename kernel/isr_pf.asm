; =============================================================================
; SENG21213-OS :: Page-Fault (#PF, vector 0x0E) ISR trampoline
; File   : kernel/isr_pf.asm
; Purpose: Low-level entry point registered in the IDT for the page-fault
;          exception (Lecture 11 §4). Unlike IRQ0, the CPU pushes an ERROR
;          CODE onto the stack before the return frame, so the stub must
;          discard it before IRET. It passes CR2 (the faulting linear
;          address) and the error code to the C handler, which may install a
;          mapping on the fly (demand paging); on return the faulting
;          instruction is automatically re-executed.
; =============================================================================
[BITS 32]
[EXTERN pf_c_handler]
[GLOBAL pf_stub]

pf_stub:
    pushad                  ; Save eax,ecx,edx,ebx,esp,ebp,esi,edi
    mov  eax, [esp+32]      ; Error code sits 32 bytes above (pushad frame)
    push eax                ; arg2 = error code
    mov  eax, cr2           ; CR2 = faulting linear address
    push eax                ; arg1 = cr2
    call pf_c_handler       ; pf_c_handler(cr2, err) — may map a page
    add  esp, 8             ; Discard the two arguments
    popad                   ; Restore registers
    add  esp, 4             ; Discard the CPU-pushed error code
    iretd                   ; Re-executes the faulting instruction
