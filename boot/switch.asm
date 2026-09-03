; =============================================================================
; SENG21213-OS :: Context-switch stub
; File   : boot/switch.asm
; Purpose: void switch_context(uint32_t *old_esp_store, uint32_t new_esp);
;          Saves the four callee-saved registers of the CURRENTLY running
;          process onto its own stack, records the resulting stack pointer
;          into *old_esp_store, then switches ESP to new_esp and restores
;          that process's registers before returning.
;
;          A brand-new process's stack is pre-built by create_process()
;          (kernel/process.c) to look exactly like what this routine
;          expects to find: [edi][esi][ebx][ebp][entry_point], so the
;          final `ret` below "returns" straight into the new process's
;          entry function the very first time it runs.  (Lecture 9 §4)
; =============================================================================
[BITS 32]
[GLOBAL switch_context]

switch_context:
    push ebp
    push ebx
    push esi
    push edi

    ; Stack now: [edi][esi][ebx][ebp][ret-addr][old_esp_store][new_esp]
    ;             esp+0                esp+16    esp+20         esp+24
    mov  eax, [esp + 20]     ; eax = old_esp_store
    mov  [eax], esp          ; *old_esp_store = current esp

    mov  eax, [esp + 24]     ; eax = new_esp
    mov  esp, eax            ; switch stacks

    pop  edi
    pop  esi
    pop  ebx
    pop  ebp
    ret                      ; Returns into the new process's saved context
