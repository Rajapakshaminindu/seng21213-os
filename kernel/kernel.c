/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 1 – Process Table & Scheduler)
 * File   : kernel/kernel.c
 *
 * PURPOSE
 *   This is the heart of your operating system. It now:
 *     1. Initialises VGA text-mode display
 *     2. Initialises the keyboard driver
 *     3. Prints a splash screen
 *     4. Sets up the IDT, remaps the PIC, and programs the PIT at 100 Hz
 *     5. Creates the shell and two demo processes, then hands control to
 *        the round-robin scheduler (Lecture 9)
 *
 * ASSIGNMENT MILESTONES  (what YOU will add in later lectures)
 *   Lecture  9  – Process Management  →  process.h / process.c / scheduler.c  [DONE]
 *   Lecture 10  – Threads             →  thread.h  / thread.c
 *   Lecture 11  – Memory Management   →  pmm.h     / pmm.c / vmm.c
 *   Lecture 12  – File System         →  fs.h      / fs.c
 *
 * CODING CONVENTION
 *   - Prefix kernel-internal functions with k_ (e.g. k_strcmp)
 *   - All driver APIs live in their own .h/.c pair
 *   - NEVER call malloc – use the PMM you build in Lecture 11
 * ============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "process.h"
#include "scheduler.h"
#include "../include/types.h"

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_version(void);
static void cmd_colour(const char *args);
static void cmd_halt(void);
static void cmd_ps(void);
static void cmd_kill(const char *args);

/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* Skip leading spaces */
static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* Minimal ASCII-to-int (no libc atoi in a freestanding kernel) */
static int k_atoi(const char *s) {
    int result = 0;
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 1: Process Table & Round-Robin Scheduler", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering – Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath – only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones:\n");
    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Process Management  – PCB, ready queue, round-robin scheduler  [DONE]\n");
    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Threads & Sync      – kernel threads, mutex, semaphore\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   – physical page allocator, virtual memory\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         – RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ----------------------------------------------\n");
    vga_puts("  help    - Show this help message\n");
    vga_puts("  clear   - Clear the screen\n");
    vga_puts("  about   - About this OS and course\n");
    vga_puts("  echo    - Echo text to screen\n");
    vga_puts("  mem     - Memory map (stub)\n");
    vga_puts("  version - Show kernel name and version\n");
    vga_puts("  colour  - Change text colour: colour <fg> <bg> (0-15)\n");
    vga_puts("  halt    - Disable interrupts and halt the CPU\n");
    vga_puts("  ps      - [L09] List all processes (PID, state, name)\n");
    vga_puts("  kill    - [L09] Terminate a process: kill <pid>\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  threads - [L10] List kernel threads\n");
    vga_puts("  free    - [L11] Show free memory\n");
    vga_puts("  ls      - [L12] List files\n");
    vga_puts("  cat     - [L12] Print file contents\n\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ----------------------------------------------\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  Scheduler    : Round-robin, 100 Hz PIT tick (Lecture 9)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 - Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void) {
    /* Stage 0/1 stub – students implement the real PMM in Lecture 11 */
    vga_puts_color("\n  Memory Map (stub - implement PMM in Lecture 11)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ----------------------------------------------\n");
    vga_puts("  0x00000000 - 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n");
    vga_puts("  0x00100000 - 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n");
    vga_puts("  0x00F00000 - 0x00FFFFFF  :  BIOS / ROM area\n");
    vga_puts("  0xB8000    - 0xBFFFF     :  VGA frame buffer\n");
    vga_puts_color("\n  TODO: Use BIOS int 0x15, EAX=0xE820 to get real memory map\n\n",
                   VGA_YELLOW, VGA_BLACK);
}

static void cmd_version(void) {
    vga_puts_color("\n  SENG21213-OS  v0.2-stage1\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  Stage 1: Process Table & Round-Robin Scheduler\n\n");
}

static void cmd_colour(const char *args) {
    const char *p = k_ltrim(args);
    int fg = k_atoi(p);

    while (*p >= '0' && *p <= '9') p++;
    p = k_ltrim(p);
    int bg = k_atoi(p);

    if (fg < 0 || fg > 15 || bg < 0 || bg > 15) {
        vga_puts_color("  Usage: colour <fg> <bg>   (values 0-15)\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    vga_set_color((vga_color_t)fg, (vga_color_t)bg);
    vga_puts("  Colour changed.\n");
}

static void cmd_halt(void) {
    vga_puts_color("\n  System halted. It is now safe to close QEMU.\n",
                   VGA_YELLOW, VGA_BLACK);
    __asm__ __volatile__("cli");
    __asm__ __volatile__("hlt");
}

static const char *state_name(proc_state_t s) {
    switch (s) {
        case PROC_READY:      return "READY";
        case PROC_RUNNING:    return "RUNNING";
        case PROC_TERMINATED: return "TERMINATED";
        default:               return "UNUSED";
    }
}

static void cmd_ps(void) {
    vga_puts_color("\n  PID  STATE       NAME\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ----------------------------\n");
    for (int i = 0; i < process_table_max(); i++) {
        pcb_t *p = process_table_get(i);
        if (!p) continue;
        vga_printf("  %d    %s", p->pid, state_name(p->state));
        /* pad simply then print name */
        vga_puts("\t");
        vga_puts(p->name);
        vga_puts("\n");
    }
    vga_puts("\n");
}

static void cmd_kill(const char *args) {
    const char *p = k_ltrim(args);
    if (k_strlen(p) == 0) {
        vga_puts_color("  Usage: kill <pid>\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    int pid = k_atoi(p);
    process_kill(pid);
    vga_printf("  Process %d marked TERMINATED.\n", pid);
}

/* ---------------------------------------------------------------------------
 * Demo processes (Lecture 9 deliverable): two processes running
 * concurrently, each printing a character at a different rate, proving
 * the round-robin scheduler is preempting and resuming them correctly.
 * --------------------------------------------------------------------------*/
/* Tiny unsigned-int-to-string helper (no libc). Pads the tail with spaces
 * so a shrinking number never leaves stale digits behind on screen. */
static void k_utoa_pad(uint32_t n, char *out, int width) {
    char digits[12];
    int  i = 0;
    if (n == 0) { digits[i++] = '0'; }
    while (n > 0) { digits[i++] = (char)('0' + (n % 10)); n /= 10; }

    int j = 0;
    while (i > 0) out[j++] = digits[--i];
    while (j < width) out[j++] = ' ';
    out[j] = '\0';
}

static void demo_task_a(void) {
    /* IMPORTANT (Lecture 9 §4 pitfall): switch_context() only saves/restores
     * 4 callee-saved registers, not EFLAGS. The very first time a freshly
     * created process is switched into (always triggered from inside the
     * IRQ0 ISR the first time), it lands here via a plain `ret`, having
     * skipped the `iretd` that would normally restore IF=1. Without this
     * explicit sti, interrupts would stay disabled forever and the PIT
     * would never tick again, freezing the whole scheduler after exactly
     * one switch. This sti is a harmless no-op on any later re-entry. */
    __asm__ __volatile__("sti");
    uint32_t last  = pit_ticks;
    uint32_t count = 0;
    char     line[48];
    char     numbuf[12];
    for (;;) {
        if (pit_ticks - last >= 20) {   /* ~200 ms at 100 Hz */
            last = pit_ticks;
            k_utoa_pad(count++, numbuf, 6);
            line[0] = '\0';
            /* Manual concat (no libc strcat in a freestanding kernel) */
            const char *prefix = "Process A [200ms] tick #";
            int k = 0, m = 0;
            while (prefix[m]) line[k++] = prefix[m++];
            m = 0;
            while (numbuf[m]) line[k++] = numbuf[m++];
            line[k] = '\0';
            vga_put_at(VGA_SHELL_ROWS + 1, 2, line, VGA_LIGHT_GREEN, VGA_BLACK);
        }
    }
}

static void demo_task_b(void) {
    __asm__ __volatile__("sti"); /* See demo_task_a() for why this is required */
    uint32_t last  = pit_ticks;
    uint32_t count = 0;
    char     line[48];
    char     numbuf[12];
    for (;;) {
        if (pit_ticks - last >= 50) {   /* ~500 ms at 100 Hz */
            last = pit_ticks;
            k_utoa_pad(count++, numbuf, 6);
            line[0] = '\0';
            const char *prefix = "Process B [500ms] tick #";
            int k = 0, m = 0;
            while (prefix[m]) line[k++] = prefix[m++];
            m = 0;
            while (numbuf[m]) line[k++] = numbuf[m++];
            line[k] = '\0';
            vga_put_at(VGA_SHELL_ROWS + 2, 2, line, VGA_LIGHT_CYAN, VGA_BLACK);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

static void shell_run(void) {
    __asm__ __volatile__("sti"); /* See demo_task_a() for why this is required */
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        /* Trim leading whitespace */
        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        /* Dispatch */
        if (k_strcmp(cmd, "help")  == 0) { cmd_help();  continue; }
        if (k_strcmp(cmd, "clear") == 0) { cmd_clear(); continue; }
        if (k_strcmp(cmd, "about") == 0) { cmd_about(); continue; }
        if (k_strcmp(cmd, "mem")   == 0) { cmd_mem();   continue; }
        if (k_strcmp(cmd, "version") == 0) { cmd_version(); continue; }
        if (k_strcmp(cmd, "halt")    == 0) { cmd_halt();    continue; }
        if (k_strcmp(cmd, "ps")      == 0) { cmd_ps();      continue; }

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        if (k_strncmp(cmd, "colour ", 7) == 0) {
            cmd_colour(cmd + 7);
            continue;
        }

        if (k_strncmp(cmd, "kill ", 5) == 0) {
            cmd_kill(cmd + 5);
            continue;
        }

        /* Milestone stubs */
        if (k_strcmp(cmd, "threads") == 0 ||
            k_strcmp(cmd, "free")    == 0 ||
            k_strcmp(cmd, "ls")      == 0 ||
            k_strcmp(cmd, "cat")     == 0) {
            vga_puts_color("  [TODO] This command is not yet implemented.\n",
                           VGA_YELLOW, VGA_BLACK);
            vga_puts("  Implement it as part of your lecture assignment.\n");
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

/* Wraps shell_run() so it matches the void(*)(void) process entry signature. */
static void shell_task(void) {
    shell_run();
    for (;;) { __asm__ __volatile__("hlt"); } /* Should never reach here */
}

/* ---------------------------------------------------------------------------
 * Kernel entry point – called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
void kernel_main(void) {
    vga_init();
    kb_init();
    print_splash();

    /* --- Lecture 9: bring up interrupt-driven process management --- */
    process_init();
    scheduler_init();

    pcb_t *shell_proc = create_process(shell_task,  "shell");
    pcb_t *proc_a      = create_process(demo_task_a, "proc_a");
    pcb_t *proc_b      = create_process(demo_task_b, "proc_b");

    scheduler_add(shell_proc);
    scheduler_add(proc_a);
    scheduler_add(proc_b);

    /* Draw a one-time separator + label for the reserved demo status area
     * (rows VGA_SHELL_ROWS..VGA_ROWS-1). This is written once, directly via
     * vga_put_at(), and is never touched by the shell's scrolling logic. */
    for (int c = 0; c < 80; c++) {
        vga_put_at(VGA_SHELL_ROWS, c, "-", VGA_DARK_GREY, VGA_BLACK);
    }
    vga_put_at(VGA_SHELL_ROWS, 2, " Scheduler Demo (Lecture 9) ", VGA_DARK_GREY, VGA_BLACK);

    pic_remap();          /* Remap IRQ0-15 to vectors 0x20-0x2F */
    idt_init();            /* Install irq0_stub at vector 0x20  */
    pit_init(100);          /* 100 Hz -> 10 ms tick               */

    __asm__ __volatile__("sti"); /* Enable interrupts - scheduler is now live */

    scheduler_start();      /* Switches into the shell process; never returns */

    /* Should never reach here */
    __asm__ __volatile__("hlt");
}
