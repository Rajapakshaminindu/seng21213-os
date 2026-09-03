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
[GLOBAL irq_ignore_stub]

irq0_stub:
    pushad                  ; Save eax,ecx,edx,ebx,esp,ebp,esi,edi
    call irq0_c_handler     ; May switch_context() here — see kernel/pit.c
    popad                   ; Restore whichever process is now running
    iretd                   ; Pops EIP, CS, EFLAGS pushed by the CPU on entry

; -----------------------------------------------------------------------------
; Defensive catch-all for the masked IRQ vectors 0x21..0x2F.
; Stage 2 only services IRQ0 (the timer); every other line is masked in
; pic_remap(). If a device ever does interrupt anyway (e.g. a spurious IRQ,
; or a future stage unmasks a line before installing its handler), this stub
; acknowledges it on both PICs and returns cleanly instead of jumping through
; a null IDT gate — which would raise #GP -> #DF -> triple fault -> reboot.
; It deliberately does NOT touch the scheduler.
; -----------------------------------------------------------------------------
irq_ignore_stub:
    pushad
    mov  al, 0x20           ; EOI command byte
    out  0xA0, al           ; -> slave  PIC command port
    out  0x20, al           ; -> master PIC command port
    popad
    iretd
