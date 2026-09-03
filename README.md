# SENG21213-OS — Stage 0: Kernel Foundations

> **Course**: SENG 21213 – Computer Architecture & Operating Systems  
> **Year**: 2nd Year, Software Engineering  
> **Assignment**: Build your own x86 Operating System

---

## What Is This?

This is **Stage 0** of your semester-long OS assignment. Over 5 lecture milestones
(Lectures 8–12), your team will transform this minimal kernel into a functioning
operating system with process management, threading, memory management, and a
file system.

```
seng21213-os/
├── boot/
│   └── boot.asm          ← MBR Bootloader (NASM, 16-bit → 32-bit transition)
├── kernel/
│   ├── kernel_entry.asm  ← Protected-mode entry, calls kernel_main()
│   ├── kernel.c          ← Main kernel: shell loop, command dispatch
│   ├── vga.c / vga.h     ← VGA 80×25 text-mode driver
│   ├── keyboard.c / .h   ← PS/2 keyboard polling driver
├── include/
│   └── types.h           ← Primitive types (no libc!)
├── linker.ld             ← Linker script (kernel at 0x10000)
├── Makefile              ← Build system
├── Dockerfile            ← Reproducible build environment
└── README.md             ← You are here
```

---

## Milestone Schedule

| Lecture | Milestone | Files to Add |
|---------|-----------|-------------|
| L08 | ✅ Stage 0 – Boot + VGA + Shell | *Given to you* |
| L09 | Process Management | `kernel/process.c`, `kernel/scheduler.c` |
| L10 | Threads & Synchronisation | `kernel/thread.c`, `kernel/mutex.c` |
| L11 | Memory Management | `kernel/pmm.c`, `kernel/vmm.c` |
| L12 | File System | `kernel/fs.c`, `kernel/ramdisk.c` |

---

## Quick Start
## Stage 0 Status — v0.1-stage0

**Status: Complete.** Kernel boots cleanly in QEMU with zero build warnings.

### Shell commands implemented

| Command | Description |
|---|---|
| `help` | List all available commands |
| `clear` | Clear the screen |
| `echo <text>` | Print arguments back to the screen |
| `version` | Print kernel name and version string |
| `colour <fg> <bg>` | Change text colour (0-15 for each) |
| `halt` | Disable interrupts and halt the CPU |
| `about` | Course/build info (extension) |
| `mem` | Memory map stub (extension, real PMM comes in L11) |

### How to test

```bash
make clean && make
make run
```
Inside the QEMU window, try:
```
help
version
colour 2 0
echo hello world
clear
halt
```
### Notes

- Fixed a GCC 15.2 / C23 compatibility issue: `bool` is now a reserved keyword,
  which conflicted with the freestanding `typedef uint8_t bool;` in `include/types.h`.
  Resolved by adding `-std=gnu99` to the Makefile CFLAGS.
- Added `version`, `colour`, and `halt` shell commands (required by the assignment
  spec) to `kernel/kernel.c`, since the provided starter kit only shipped
  `help`, `clear`, `about`, `echo`, and `mem`.

---

## Stage 1 Status — v0.2-stage1

**Status: Complete.** Process table, round-robin scheduler, and a fully
interrupt-driven timer are all working. Verified via QEMU screendumps
showing the shell and two background demo processes running concurrently.

### What was added

| Component | Files |
|---|---|
| Port I/O helpers | `include/io.h` |
| 8259 PIC remap (IRQ0-15 → vectors 0x20-0x2F) | `kernel/pic.h`, `kernel/pic.c` |
| 256-entry IDT + interrupt gate for IRQ0 | `kernel/idt.h`, `kernel/idt.c` |
| i8253 PIT programmed at 100 Hz | `kernel/pit.h`, `kernel/pit.c` |
| IRQ0 ISR trampoline (`pushad`/`call`/`popad`/`iretd`) | `kernel/isr_irq0.asm` |
| Minimal 4-register context switch (`switch_context`) | `boot/switch.asm` |
| Process Control Block + process table | `kernel/process.h`, `kernel/process.c` |
| Round-robin ready-queue scheduler | `kernel/scheduler.h`, `kernel/scheduler.c` |
| `ps` / `kill <pid>` shell commands | `kernel/kernel.c` |
| Two background demo processes (proc_a @ ~200ms, proc_b @ ~500ms) | `kernel/kernel.c` |
| Cursor-independent VGA write for background output | `vga_put_at()` in `kernel/vga.c` |

### Design notes

- The scheduler runs three processes: the interactive shell plus two demo
  tasks that each print an incrementing tick counter at a different rate,
  proving that the timer-driven round-robin scheduler is actually
  preempting and resuming every process correctly.
- `switch_context()` deliberately saves/restores only 4 callee-saved
  registers (`ebp`/`ebx`/`esi`/`edi`) plus a raw `esp` swap, instead of a
  full interrupt frame. A process being switched away from mid-interrupt
  has its own call chain (`irq0_stub` → `irq0_c_handler` → `scheduler_tick`
  → `switch_context`) suspended on its own stack; switching back to it via
  `ret` naturally unwinds that exact chain, eventually reaching the ISR's
  `popad; iretd` to fully resume it.
- **Pitfall found & fixed:** because that minimal switch never touches
  `EFLAGS`, the very first time a freshly-created process is scheduled in
  (always triggered from inside the IRQ0 ISR), it would otherwise inherit
  `IF=0` (interrupts disabled) forever, since it jumps straight into its
  entry function via `ret`, bypassing the `iretd` that would normally
  restore `EFLAGS`. This froze the scheduler after exactly one switch.
  Fixed by adding an explicit `sti` as the first instruction of every
  process entry function (a harmless no-op on later resumes).
- The bottom 3 VGA rows are permanently reserved for the scheduler demo
  status area (`VGA_RESERVED_ROWS` in `kernel/vga.h`); the shell's own
  scroll region is capped at `VGA_SHELL_ROWS` so shell output can never
  scroll into, or be corrupted by, the background processes' output.

### How to test

```bash
make clean && make
make run
```
Inside the QEMU window, try `help`, `ps`, `kill <pid>`, and watch the
"Process A" / "Process B" tick counters at the bottom of the screen
increment concurrently with normal shell use.

---

### Option A: Docker (Recommended for all platforms)

```bash
# 1. Install Docker Desktop (Windows/Mac) or Docker Engine (Linux)
# 2. Build the image once:
docker build -t seng21213-os-builder .

# 3. Build the OS:
docker run --rm -v "$(pwd)":/os seng21213-os-builder

# 4. Run in QEMU (install QEMU locally):
qemu-system-i386 -drive format=raw,file=seng21213-os.img -m 32M
```

### Option B: Native Linux/WSL2

```bash
# Ubuntu/Debian
sudo apt install nasm gcc gcc-multilib binutils qemu-system-x86 make

# Build
make all

# Run
make run
```

### Option C: macOS (Homebrew)

```bash
brew install nasm x86_64-elf-binutils qemu

# You also need an i686-elf-gcc cross-compiler:
# See: https://wiki.osdev.org/GCC_Cross-Compiler
make all
make run
```

---

## Understanding the Boot Process

```
Power On
  │
  ▼
BIOS (firmware in ROM)
  │  Loads 512-byte MBR from disk sector 1 into RAM at 0x7C00
  ▼
boot/boot.asm  (Real Mode, 16-bit)
  │  Prints "Loading SENG21213-OS..."
  │  Reads 64 sectors (kernel) from disk into RAM at 0x10000
  │  Sets up GDT (Global Descriptor Table)
  │  Switches CPU to 32-bit Protected Mode
  │  Far-jumps to 0x10000
  ▼
kernel/kernel_entry.asm  (Protected Mode, 32-bit)
  │  Calls kernel_main()
  ▼
kernel/kernel.c  →  kernel_main()
  │  vga_init()     – set up text display
  │  kb_init()      – set up keyboard
  │  print_splash() – welcome screen
  │  shell_run()    – interactive shell (infinite loop)
  ▼
Your code from here...
```

---

## Building Lecture 9: Process Management

When you reach Lecture 9, you'll add process support. Here's the interface to implement:

```c
/* kernel/process.h  — you write this! */

#define MAX_PROCESSES    16
#define STACK_SIZE     4096

typedef enum { READY, RUNNING, BLOCKED, TERMINATED } proc_state_t;

typedef struct pcb {
    uint32_t      pid;
    proc_state_t  state;
    uint32_t      esp;          /* Saved stack pointer */
    uint32_t      eip;          /* Saved instruction pointer */
    uint32_t      stack[STACK_SIZE / 4];
    struct pcb   *next;         /* For linked-list ready queue */
} pcb_t;

void   process_init(void);
pcb_t *process_create(void (*entry)(void));
void   process_yield(void);        /* Trigger context switch */
void   process_exit(void);
void   scheduler_tick(void);       /* Called by timer IRQ (Lecture 10) */
```

---

## Debugging Tips

```bash
# Debug with GDB
make run-debug
# In another terminal:
gdb
(gdb) target remote :1234
(gdb) set architecture i386
(gdb) symbol-file build/kernel.elf
(gdb) break kernel_main
(gdb) continue

# Inspect the disk image
xxd seng21213-os.img | head -32    # View MBR
xxd seng21213-os.img | grep -c aa55  # Verify boot signature
```

---

## Key Learning Resources

| Topic | Reference |
|-------|-----------|
| x86 Protected Mode | Intel IA-32 Manual, Vol 3, Chapter 3 |
| VGA Text Mode | OSDev Wiki: Text UI |
| Interrupts / IDT | Stallings Ch.1; OSDev: IDT |
| Process Management | Stallings Ch.3–4 (your lecture notes) |
| Memory Management | Stallings Ch.7–8 (your lecture notes) |
| OSDev community | https://wiki.osdev.org |

---

## Assessment Rubric (per milestone)

| Criterion | Weight |
|-----------|--------|
| Code compiles and kernel boots in QEMU | 30% |
| Feature implementation (correct behaviour) | 40% |
| Code quality and comments | 20% |
| Lab demo and viva questions | 10% |

---

*Happy hacking! Remember: every commercial OS started exactly like this.*
