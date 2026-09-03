; =============================================================================
; SENG21213-OS :: IRQ0 (Timer) ISR trampoline
; File   : kernel/isr_irq0.asm
; Purpose: Low-level entry point registered in the IDT for interrupt vector
;          0x20 (IRQ0 after PIC remap). Saves the interrupted process's full
;          register state, calls the C handler (which may context-switch),
;          then restores whichever process is now current and returns via
;          IRET.                                          (Lecture 9 §2-3)
; =============================================================================
[BITS 32]
[EXTERN irq0_c_handler]
[GLOBAL irq0_stub]

irq0_stub:
    pushad                  ; Save eax,ecx,edx,ebx,esp,ebp,esi,edi
    call irq0_c_handler     ; May switch_context() here — see kernel/pit.c
    popad                   ; Restore whichever process is now running
    iretd                   ; Pops EIP, CS, EFLAGS pushed by the CPU on entry
