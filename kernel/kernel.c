/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 4 – RAM Disk File System)
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
 *     6. Spawns kernel threads that demonstrate the myglobal race
 *        condition (with and without a mutex) and a bounded-buffer
 *        producer/consumer using three semaphores (Lecture 10)
 *
 * ASSIGNMENT MILESTONES  (what YOU will add in later lectures)
 *   Lecture  9  – Process Management  →  process.h / process.c / scheduler.c  [DONE]
 *   Lecture 10  – Threads             →  thread.h / thread.c / mutex.c /
 *                                        semaphore.c                          [DONE]
 *   Lecture 11  – Memory Management   →  pmm.h/pmm.c vmm.h/vmm.c kheap.c  [DONE]
 *   Lecture 12  – File System        →  ramdisk.h/c fs.h/c              [DONE]
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
#include "thread.h"
#include "mutex.h"
#include "semaphore.h"
#include "pmm.h"
#include "vmm.h"
#include "kheap.h"
#include "ramdisk.h"
#include "fs.h"
#include "../include/types.h"

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_meminfo(void);
static void cmd_ls(void);
static void cmd_touch(const char *args);
static void cmd_cat(const char *args);
static void cmd_write_cmd(const char *args);
static void cmd_rm(const char *args);
static void cmd_ls(void);
static void cmd_touch(const char *args);
static void cmd_cat(const char *args);
static void cmd_write_cmd(const char *args);
static void cmd_rm(const char *args);
static void cmd_ls(void);
static void cmd_touch(const char *args);
static void cmd_cat(const char *args);
static void cmd_write_cmd(const char *args);
static void cmd_rm(const char *args);
static void cmd_version(void);
static void cmd_colour(const char *args);
static void cmd_halt(void);
static void cmd_ps(void);
static void cmd_kill(const char *args);
static void cmd_threads(void);

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

/* Append helpers used to build the fixed-position demo status lines */
static void k_append(char *line, int *k, const char *s) {
    while (*s) line[(*k)++] = *s++;
}

static void k_append_num(char *line, int *k, uint32_t n, int width) {
    char b[12];
    k_utoa_pad(n, b, width);
    k_append(line, k, b);
}

/* Append an 8-digit hex value with a 0x prefix (used for addresses) */
static void k_append_hex(char *line, int *k, uint32_t v) {
    char b[9];
    for (int i = 7; i >= 0; i--) {
        uint32_t n = (v >> (i * 4)) & 0xF;
        b[7 - i] = (char)(n < 10 ? '0' + n : 'A' + (n - 10));
    }
    b[8] = '\0';
    k_append(line, k, "0x");
    k_append(line, k, b);
}

/* Sleep ~n PIT ticks (10 ms each). hlt idles the CPU until the next IRQ
 * instead of burning cycles in a tight spin. */
static void k_delay_ticks(uint32_t n) {
    uint32_t start = pit_ticks;
    while (pit_ticks - start < n) { __asm__ __volatile__("hlt"); }
}

/* Overwrite one reserved demo row (blanking it first so shrinking text
 * never leaves stale characters behind). */
static void row_put(int row, const char *msg, vga_color_t fg) {
    char blank[81];
    for (int i = 0; i < 80; i++) blank[i] = ' ';
    blank[80] = '\0';
    vga_put_at(row, 0, blank, fg, VGA_BLACK);
    vga_put_at(row, 1, msg, fg, VGA_BLACK);
}

/* Redraw the separator line of the reserved demo area (used by `clear`) */
static void redraw_status_separator(void) {
    for (int c = 0; c < 80; c++) {
        vga_put_at(VGA_SHELL_ROWS, c, "-", VGA_DARK_GREY, VGA_BLACK);
    }
    vga_put_at(VGA_SHELL_ROWS, 2, " Scheduler + Threads + Memory + FS Demo (L09-L12) ",
               VGA_DARK_GREY, VGA_BLACK);
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
    vga_puts_color("  Stage 4: RAM Disk File System", VGA_LIGHT_CYAN, VGA_BLACK);

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
    vga_puts("Threads & Sync      – kernel threads, mutex, semaphore  [DONE]\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   – physical page allocator, virtual memory  [DONE]\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         – RAM disk, inode FS, POSIX API      [DONE]\n");
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
    vga_puts("  meminfo - [L11] Physical memory: E820 + frames + heap\n");
    vga_puts("  ls      - [L12] List files on the RAM disk\n");
    vga_puts("  touch   - [L12] Create a file: touch <name>\n");
    vga_puts("  cat     - [L12] Print file contents: cat <name>\n");
    vga_puts("  write   - [L12] Append text: write <name> <text>\n");
    vga_puts("  rm      - [L12] Remove a file: rm <name>\n");
    vga_puts("  ls      - [L12] List files on the RAM disk\n");
    vga_puts("  touch   - [L12] Create a file: touch <name>\n");
    vga_puts("  cat     - [L12] Print file contents: cat <name>\n");
    vga_puts("  write   - [L12] Append text: write <name> <text>\n");
    vga_puts("  rm      - [L12] Remove a file: rm <name>\n");
    vga_puts("  ls      - [L12] List files on the RAM disk\n");
    vga_puts("  touch   - [L12] Create a file: touch <name>\n");
    vga_puts("  cat     - [L12] Print file contents: cat <name>\n");
    vga_puts("  write   - [L12] Append text: write <name> <text>\n");
    vga_puts("  rm      - [L12] Remove a file: rm <name>\n");
    vga_puts("  ls      - [L12] List files on the RAM disk\n");
    vga_puts("  touch   - [L12] Create a file: touch <name>\n");
    vga_puts("  cat     - [L12] Print file contents: cat <name>\n");
    vga_puts("  write   - [L12] Append text: write <name> <text>\n");
    vga_puts("  rm      - [L12] Remove a file: rm <name>\n");
    vga_puts("  version - Show kernel name and version\n");
    vga_puts("  colour  - Change text colour: colour <fg> <bg> (0-15)\n");
    vga_puts("  halt    - Disable interrupts and halt the CPU\n");
    vga_puts("  ps      - [L09] List all processes (PID, state, name)\n");
    vga_puts("  kill    - [L09] Terminate a process: kill <pid>\n");
    vga_puts("  threads - [L10] List kernel threads (TID, owner, state)\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  (all stages complete!)\n\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
    redraw_status_separator();   /* Keep the demo area frame visible */
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ----------------------------------------------\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  Scheduler    : Round-robin, 100 Hz PIT tick (Lecture 9)\n");
    vga_puts("  Threads      : Kernel threads, mutex, semaphore (Lecture 10)\n");
    vga_puts("  Memory       : Bitmap PMM, paging VMM, kmalloc/kfree (Lecture 11)\n");
    vga_puts("  File System  : RAM disk, inode FS, POSIX API (Lecture 12)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 - Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

/* Lecture 11 deliverable: real memory info from the E820 map + PMM bitmap */
static void cmd_meminfo(void) {
    char line[80];
    int  k;
    vga_puts_color("\n  Physical Memory (BIOS E820 + PMM bitmap)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ----------------------------------------------\n");

    k = 0; k_append(line, &k, "  E820 entries : ");
    k_append_num(line, &k, (uint32_t)pmm_e820_count(), 1);
    line[k] = '\0'; vga_puts(line); vga_puts("\n");

    for (int i = 0; i < pmm_e820_count(); i++) {
        const e820_entry_t *e = pmm_e820_entry(i);
        k = 0; k_append(line, &k, "    base=");
        k_append_hex(line, &k, (uint32_t)e->base);
        k_append(line, &k, " len=");
        k_append_hex(line, &k, (uint32_t)e->length);
        k_append(line, &k, " type=");
        k_append_num(line, &k, e->type, 1);
        k_append(line, &k, e->type == E820_USABLE ? " (usable)" : "");
        line[k] = '\0'; vga_puts(line); vga_puts("\n");
    }

    k = 0; k_append(line, &k, "  Frames  total=");
    k_append_num(line, &k, pmm_total_frames(), 6);
    k_append(line, &k, " used=");
    k_append_num(line, &k, pmm_used_frames(), 6);
    k_append(line, &k, " free=");
    k_append_num(line, &k, pmm_free_frames(), 6);
    line[k] = '\0'; vga_puts(line); vga_puts("\n");

    k = 0; k_append(line, &k, "  Memory  total=");
    k_append_num(line, &k, pmm_total_frames() * 4 / 1024, 5);
    k_append(line, &k, " MB  used=");
    k_append_num(line, &k, pmm_used_frames() * 4, 6);
    k_append(line, &k, " KB  free=");
    k_append_num(line, &k, pmm_free_frames() * 4 / 1024, 5);
    k_append(line, &k, " MB");
    line[k] = '\0'; vga_puts(line); vga_puts("\n");

    k = 0; k_append(line, &k, "  Heap    used=");
    k_append_num(line, &k, kheap_used(), 6);
    k_append(line, &k, " B of ");
    k_append_num(line, &k, kheap_capacity(), 6);
    k_append(line, &k, " B");
    line[k] = '\0'; vga_puts(line); vga_puts("\n\n");
}

static void cmd_version(void) {
    vga_puts_color("\n  SENG21213-OS  v0.5-stage4\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  Stage 4: RAM Disk File System\n\n");
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
        case PROC_BLOCKED:    return "BLOCKED";
        case PROC_TERMINATED: return "TERMINATED";
        default:               return "UNUSED";
    }
}

static void cmd_ps(void) {
    vga_puts_color("\n  PID  STATE       NAME\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ----------------------------\n");
    for (int i = 0; i < process_table_max(); i++) {
        pcb_t *p = process_table_get(i);
        if (!p || p->is_thread) continue;   /* Threads: see `threads` */
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

/* Lecture 10 deliverable: list every kernel thread with its owner process */
static void cmd_threads(void) {
    vga_puts_color("\n  TID  OWNER  STATE       NAME\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ----------------------------------------\n");
    int shown = 0;
    for (int i = 0; i < process_table_max(); i++) {
        pcb_t *p = process_table_get(i);
        if (!p || !p->is_thread) continue;
        vga_printf("  %d    %d      %s", p->pid, p->owner_pid,
                   state_name(p->state));
        vga_puts("\t");
        vga_puts(p->name);
        vga_puts("\n");
        shown++;
    }
    if (!shown) vga_puts("  (no threads alive)\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Stage 1 demo processes: two processes running concurrently, each printing
 * a counter at a different rate, proving the round-robin scheduler is
 * preempting and resuming them correctly. They write into the reserved
 * status rows via vga_put_at() so they never disturb the shell cursor.
 * --------------------------------------------------------------------------*/
static void demo_task_a(void) {
    /* IMPORTANT (Lecture 9 §4 pitfall): switch_context() only saves/restores
     * 4 callee-saved registers, not EFLAGS. The very first time a freshly
     * created context is switched into (always triggered from inside the
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
 * Stage 2 demo 1 (Lecture 10 §4): the classic `myglobal` race condition.
 * Two racer threads do read-delay-write increments of a shared counter.
 * Phase 1 runs them WITHOUT a mutex — the ~20 ms delay sits right inside
 * the read-modify-write window, so the 10 ms timer preemption guarantees
 * lost updates. Phase 2 repeats with mutex_lock()/mutex_unlock() around
 * the critical section and the result is exactly correct. Both results
 * stay on screen side by side so the difference is unmistakable.
 * --------------------------------------------------------------------------*/
#define RACE_ITERS  15
#define RACE_EXPECT (2 * RACE_ITERS)

static volatile int myglobal;          /* The shared variable (L10 §4) */
static mutex_t      race_mutex;
static sem_t        race_join;         /* Racers signal completion here */

static void racer_unlocked(void *arg) {
    (void)arg;
    for (int i = 0; i < RACE_ITERS; i++) {
        int v = myglobal;              /* READ .......................... */
        k_delay_ticks(2);              /* ~20 ms race window             */
        myglobal = v + 1;              /* WRITE — lost if preempted above */
    }
    sem_signal(&race_join);
}

static void racer_locked(void *arg) {
    (void)arg;
    for (int i = 0; i < RACE_ITERS; i++) {
        mutex_lock(&race_mutex);       /* Critical section begins */
        int v = myglobal;
        k_delay_ticks(2);
        myglobal = v + 1;
        mutex_unlock(&race_mutex);     /* Critical section ends */
    }
    sem_signal(&race_join);
}

static void race_coordinator(void *arg) {
    (void)arg;
    char line[80];
    int  k;

    row_put(VGA_SHELL_ROWS + 3,
            " Race 1/2 (NO mutex): 2 threads x 15 increments running...",
            VGA_YELLOW);
    myglobal = 0;
    thread_create(racer_unlocked, 0, "racer1");
    thread_create(racer_unlocked, 0, "racer2");
    sem_wait(&race_join);
    sem_wait(&race_join);
    int nolock_result = myglobal;

    row_put(VGA_SHELL_ROWS + 3,
            " Race 2/2 (mutex)   : 2 threads x 15 increments running...",
            VGA_YELLOW);
    myglobal = 0;
    thread_create(racer_locked, 0, "racer3");
    thread_create(racer_locked, 0, "racer4");
    sem_wait(&race_join);
    sem_wait(&race_join);

    k = 0;
    k_append(line, &k, " Race myglobal: no-mutex=");
    k_append_num(line, &k, (uint32_t)nolock_result, 2);
    k_append(line, &k, "/");
    k_append_num(line, &k, RACE_EXPECT, 2);
    k_append(line, &k, " LOST | mutex=");
    k_append_num(line, &k, (uint32_t)myglobal, 2);
    k_append(line, &k, "/");
    k_append_num(line, &k, RACE_EXPECT, 2);
    k_append(line, &k, " OK");
    line[k] = '\0';
    row_put(VGA_SHELL_ROWS + 3, line, VGA_LIGHT_GREEN);
    /* Coordinator done — thread_exit() happens automatically on return */
}

/* ---------------------------------------------------------------------------
 * Stage 2 demo 2 (Lecture 10 §5): bounded-buffer producer/consumer using
 * THREE semaphores — empty slots, full slots, and a binary mutex semaphore
 * guarding the ring buffer itself. The consumer verifies that every item
 * arrives exactly once and in order; any corruption (lost/duplicated/out
 * of order item) is counted and reported on the demo row.
 * --------------------------------------------------------------------------*/
#define PC_ITEMS 40
#define PC_BUF   8

static int          pc_ring[PC_BUF];
static volatile int pc_in, pc_out;
static sem_t        pc_empty, pc_full, pc_mux;
static volatile int pc_produced, pc_consumed, pc_corrupt;

/* Consumer-only status writer (single writer => no torn lines) */
static void pc_update_row(void) {
    char line[80];
    int  k = 0;
    if (pc_consumed >= PC_ITEMS) {
        k_append(line, &k, " ProdCons: ");
        k_append_num(line, &k, (uint32_t)pc_consumed, 2);
        k_append(line, &k, "/");
        k_append_num(line, &k, PC_ITEMS, 2);
        k_append(line, &k, " items, corrupted=");
        k_append_num(line, &k, (uint32_t)pc_corrupt, 1);
        k_append(line, &k, pc_corrupt ? "  BAD!" : "  OK - no corruption");
    } else {
        k_append(line, &k, " ProdCons: produced=");
        k_append_num(line, &k, (uint32_t)pc_produced, 2);
        k_append(line, &k, " consumed=");
        k_append_num(line, &k, (uint32_t)pc_consumed, 2);
        k_append(line, &k, " buf=");
        k_append_num(line, &k, (uint32_t)pc_full.count, 1);
        k_append(line, &k, "/");
        k_append_num(line, &k, PC_BUF, 1);
    }
    line[k] = '\0';
    row_put(VGA_SHELL_ROWS + 4, line, VGA_WHITE);
}

static void producer_fn(void *arg) {
    (void)arg;
    for (int n = 1; n <= PC_ITEMS; n++) {
        sem_wait(&pc_empty);           /* Wait for a free slot */
        sem_wait(&pc_mux);             /* Exclusive access to the ring */
        pc_ring[pc_in] = n;
        pc_in = (pc_in + 1) % PC_BUF;
        sem_signal(&pc_mux);
        sem_signal(&pc_full);          /* One more full slot */
        pc_produced++;
        k_delay_ticks(1);              /* ~10 ms between items */
    }
}

static void consumer_fn(void *arg) {
    (void)arg;
    for (int n = 1; n <= PC_ITEMS; n++) {
        sem_wait(&pc_full);            /* Wait for an available item */
        sem_wait(&pc_mux);
        int v = pc_ring[pc_out];
        pc_out = (pc_out + 1) % PC_BUF;
        sem_signal(&pc_mux);
        sem_signal(&pc_empty);         /* One more empty slot */
        if (v != n) pc_corrupt++;      /* Out-of-order / lost / duplicated */
        pc_consumed++;
        pc_update_row();
        k_delay_ticks(2);              /* Slower than producer: fills buffer */
    }
    pc_update_row();                   /* Final verdict line */
}

/* ---------------------------------------------------------------------------
 * Stage 3 (Lecture 11): memory-management demos, written into the reserved
 * status rows so they never disturb the shell cursor.
 *   row +5 : PMM frame statistics + the required alloc/free-100 leak test
 *   row +6 : VMM paging: map a dynamic VA, write/read, translate VA->PA
 *   row +7 : demand paging (#PF services the first touch) + kmalloc/kfree
 * --------------------------------------------------------------------------*/
static volatile uint32_t pf_faults;    /* #PF count serviced by the handler  */
static uint32_t          demand_page;  /* VA deliberately left unmapped      */

/* Page-fault handler (vector 0x0E), reached via pf_stub in isr_pf.asm.
 * If the fault is the deliberate demand-paging first touch, allocate a frame
 * and map it on the fly; IRET then re-runs the faulting instruction and it
 * succeeds. Any other fault is a kernel bug -> report and halt. */
void pf_c_handler(uint32_t cr2, uint32_t err) {
    uint32_t page = cr2 & ~0xFFFu;
    if (page == demand_page) {
        uint32_t f = pmm_alloc_frame();
        vmm_map_page(page, f, VMM_PRESENT | VMM_WRITE);
        pf_faults++;
        return;                       /* retry the faulting instruction */
    }
    (void)err;
    row_put(VGA_SHELL_ROWS + 7, " UNEXPECTED PAGE FAULT -> halted", VGA_LIGHT_RED);
    for (;;) { __asm__ __volatile__("cli; hlt"); }
}

static void mem_demo(void *arg) {
    (void)arg;
    __asm__ __volatile__("sti");
    char line[80];
    int  k;

    /* --- PMM: the required alloc-then-free-100-frames leak test --- */
    uint32_t before = pmm_used_frames();
    uint32_t hold[100];
    for (int i = 0; i < 100; i++) hold[i] = pmm_alloc_frame();
    uint32_t mid = pmm_used_frames();
    for (int i = 0; i < 100; i++) pmm_free_frame(hold[i]);
    uint32_t after = pmm_used_frames();
    int leak_ok = (after == before) && (mid == before + 100);

    k = 0;
    k_append(line, &k, " PMM: ");
    k_append_num(line, &k, pmm_total_frames() * 4 / 1024, 4);
    k_append(line, &k, "MB tot used=");
    k_append_num(line, &k, pmm_used_frames() * 4, 6);
    k_append(line, &k, "KB free=");
    k_append_num(line, &k, pmm_free_frames() * 4 / 1024, 4);
    k_append(line, &k, "MB | leak100: ");
    k_append(line, &k, leak_ok ? "delta=0 OK" : "LEAK!");
    line[k] = '\0';
    row_put(VGA_SHELL_ROWS + 5, line, VGA_LIGHT_GREEN);

    /* --- VMM: map a dynamic page, write/read through it, translate --- */
    uint32_t va = VMM_DYN_BASE + 0x2000;
    uint32_t f  = pmm_alloc_frame();
    vmm_map_page(va, f, VMM_PRESENT | VMM_WRITE);
    volatile uint32_t *p = (volatile uint32_t *)va;
    p[0] = 0xCAFEBABE; p[1023] = 0x12345678;
    int rw_ok = (p[0] == 0xCAFEBABE && p[1023] == 0x12345678);
    uint32_t pa = vmm_translate(va);
    int tr_ok = (pa == f);
    vmm_unmap_page(va);
    pmm_free_frame(f);

    k = 0;
    k_append(line, &k, " VMM: paging ON | VA 0xC0002000 -> PA ");
    k_append_hex(line, &k, pa);
    k_append(line, &k, (rw_ok && tr_ok) ? "  rw+xlat OK" : "  FAIL");
    line[k] = '\0';
    row_put(VGA_SHELL_ROWS + 6, line, VGA_LIGHT_CYAN);

    /* --- Demand paging: first touch faults, handler maps, retry OK --- */
    demand_page = VMM_DYN_BASE;          /* this page starts unmapped */
    pf_faults   = 0;
    volatile uint32_t *d = (volatile uint32_t *)demand_page;
    *d = 0xDEADBEEF;                     /* #PF -> handler maps -> retry */
    int dp_ok = (*d == 0xDEADBEEF && pf_faults == 1);

    /* --- kheap: alloc / write / verify / free, expect used back to 0 --- */
    uint32_t *a = (uint32_t *)kmalloc(1024);
    uint32_t *b = (uint32_t *)kmalloc(64);
    uint32_t *c = (uint32_t *)kmalloc(3000);
    int kh_rw = (a && b && c);
    if (kh_rw) {
        for (int i = 0; i < 256; i++) a[i] = (uint32_t)i;
        for (int i = 0; i < 16;  i++) b[i] = (uint32_t)(0xA0 + i);
        for (int i = 0; i < 750; i++) c[i] = (uint32_t)i * 3;
        kh_rw = (a[255] == 255 && b[15] == 0xAF && c[749] == 749 * 3);
    }
    uint32_t used_mid = kheap_used();
    kfree(a); kfree(b); kfree(c);
    int kh_ok = kh_rw && (used_mid > 0) && (kheap_used() == 0);

    k = 0;
    k_append(line, &k, " DemandPG: faults=");
    k_append_num(line, &k, pf_faults, 1);
    k_append(line, &k, dp_ok ? " OK | kheap rw OK, freed used=" : " BAD | kheap used=");
    k_append_num(line, &k, kheap_used(), 1);
    k_append(line, &k, kh_ok ? "B OK" : "B BAD");
    line[k] = '\0';
    row_put(VGA_SHELL_ROWS + 7, line, VGA_YELLOW);
}

/* ---------------------------------------------------------------------------
 * Stage 4 (Lecture 12): File system shell commands
 * --------------------------------------------------------------------------*/
static void cmd_ls(void) {
    dir_entry_t entries[FS_MAX_FILES];
    int n = fs_ls(entries, FS_MAX_FILES);
    if (n == 0) {
        vga_puts("  (empty)\n");
        return;
    }
    vga_puts_color("\n  NAME                          SIZE (B)\n",
                   VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------------------\n");
    for (int i = 0; i < n; i++) {
        int fd = fs_open(entries[i].name);
        int sz = (fd >= 0) ? fs_fsize(fd) : 0;
        if (fd >= 0) fs_close(fd);
        /* Print name then pad to column 32 */
        vga_puts("  ");
        vga_puts(entries[i].name);
        uint32_t nlen = k_strlen(entries[i].name);
        for (uint32_t p = nlen; p < 28; p++) vga_puts(" ");
        vga_printf("%d\n", sz);
    }
    vga_puts("\n");
}

static void cmd_touch(const char *args) {
    const char *name = k_ltrim(args);
    if (k_strlen(name) == 0) {
        vga_puts_color("  Usage: touch <name>\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    int fd = fs_creat(name);
    if (fd < 0) {
        vga_puts_color("  Error: could not create file (exists or disk full)\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    fs_close(fd);
    vga_printf("  Created: %s\n", name);
}

static void cmd_cat(const char *args) {
    const char *name = k_ltrim(args);
    if (k_strlen(name) == 0) {
        vga_puts_color("  Usage: cat <name>\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    int fd = fs_open(name);
    if (fd < 0) {
        vga_puts_color("  Error: file not found\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    char rbuf[129];
    int  got;
    vga_puts("\n");
    while ((got = fs_read(fd, rbuf, 128)) > 0) {
        rbuf[got] = '\0';
        vga_puts(rbuf);
    }
    vga_puts("\n");
    fs_close(fd);
}

static void cmd_write_cmd(const char *args) {
    const char *p = k_ltrim(args);
    /* Expect: <name> <text> */
    char name[FS_NAME_LEN];
    uint32_t ni = 0;
    while (*p && *p != ' ' && ni < FS_NAME_LEN - 1) name[ni++] = *p++;
    name[ni] = '\0';
    p = k_ltrim(p);
    if (ni == 0 || k_strlen(p) == 0) {
        vga_puts_color("  Usage: write <name> <text>\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    /* Open existing or create */
    int fd = fs_open(name);
    if (fd < 0) fd = fs_creat(name);
    if (fd < 0) {
        vga_puts_color("  Error: cannot open/create file\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    /* Append: seek to end */
    fs_seek(fd, (uint32_t)fs_fsize(fd));
    uint32_t len = k_strlen(p);
    fs_write(fd, p, len);
    fs_write(fd, "\n", 1);
    fs_close(fd);
    vga_printf("  Wrote %d bytes to %s\n", (int)(len + 1), name);
}

static void cmd_rm(const char *args) {
    const char *name = k_ltrim(args);
    if (k_strlen(name) == 0) {
        vga_puts_color("  Usage: rm <name>\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    if (fs_unlink(name) < 0) {
        vga_puts_color("  Error: file not found\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    vga_printf("  Removed: %s\n", name);
}

/* ---------------------------------------------------------------------------
 * Stage 4 (Lecture 12): FS demo — runs at boot, writes 5 files, reads back,
 * deletes them, and prints a status line in the reserved demo row.
 * --------------------------------------------------------------------------*/
static void fs_demo(void *arg) {
    (void)arg;
    __asm__ __volatile__("sti");
    char line[80]; int k = 0;

    /* Create + write 5 files */
    const char *names[5] = {"alpha","beta","gamma","delta","epsilon"};
    const char *data [5] = {"Hello from alpha!","Beta content.",
                             "Gamma data.","Delta says hi.","Epsilon!"};
    int ok = 1;
    for (int i = 0; i < 5; i++) {
        int fd = fs_creat(names[i]);
        if (fd < 0) { ok = 0; break; }
        fs_write(fd, data[i], k_strlen(data[i]));
        fs_close(fd);
    }

    /* Read back and verify */
    int verified = 0;
    if (ok) {
        verified = 1;
        for (int i = 0; i < 5; i++) {
            int fd = fs_open(names[i]);
            if (fd < 0) { verified = 0; break; }
            char buf[64]; int got = fs_read(fd, buf, 63);
            fs_close(fd);
            buf[got] = '\0';
            uint32_t dl = k_strlen(data[i]);
            if ((uint32_t)got != dl) { verified = 0; break; }
            for (uint32_t j = 0; j < dl; j++) {
                if (buf[j] != data[i][j]) { verified = 0; break; }
            }
        }
    }

    /* Delete all 5 */
    int deleted = 0;
    for (int i = 0; i < 5; i++) {
        if (fs_unlink(names[i]) == 0) deleted++;
    }

    k = 0;
    k_append(line, &k, " FS: create 5 files+write+readback+delete | ");
    k_append(line, &k, (ok && verified && deleted == 5) ? "ALL OK" : "FAIL");
    line[k] = '\0';
    row_put(VGA_SHELL_ROWS + 8, line, VGA_LIGHT_GREEN);
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
        if (k_strcmp(cmd, "mem")     == 0 ||
            k_strcmp(cmd, "meminfo") == 0 ||
            k_strcmp(cmd, "free")    == 0) { cmd_meminfo(); continue; }
        if (k_strcmp(cmd, "version") == 0) { cmd_version(); continue; }
        if (k_strcmp(cmd, "halt")    == 0) { cmd_halt();    continue; }
        if (k_strcmp(cmd, "ps")      == 0) { cmd_ps();      continue; }
        if (k_strcmp(cmd, "threads") == 0) { cmd_threads(); continue; }

        if (k_strcmp(cmd, "ls")    == 0) { cmd_ls();           continue; }

        if (k_strncmp(cmd, "touch ", 6) == 0) {
            cmd_touch(cmd + 6); continue;
        }
        if (k_strcmp(cmd, "touch") == 0) {
            cmd_touch(""); continue;
        }

        if (k_strncmp(cmd, "cat ", 4) == 0) {
            cmd_cat(cmd + 4); continue;
        }
        if (k_strcmp(cmd, "cat") == 0) {
            cmd_cat(""); continue;
        }

        if (k_strncmp(cmd, "write ", 6) == 0) {
            cmd_write_cmd(cmd + 6); continue;
        }
        if (k_strcmp(cmd, "write") == 0) {
            cmd_write_cmd(""); continue;
        }

        if (k_strncmp(cmd, "rm ", 3) == 0) {
            cmd_rm(cmd + 3); continue;
        }
        if (k_strcmp(cmd, "rm") == 0) {
            cmd_rm(""); continue;
        }

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

        /* No more TODO stubs: all stages implemented */
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

    /* --- Lecture 11: physical + virtual memory managers ---
     * pmm_init() parses the E820 map the bootloader parked at 0x8000.
     * vmm_init() builds the page tables and sets CR0.PG (identity-mapped, so
     * existing code keeps working). kheap_init() wires the heap window. */
    pmm_init();
    vmm_init();
    kheap_init();

    /* --- Lecture 12: RAM disk + file system --- */
    rd_init();
    fs_mkfs();

    /* --- Lecture 9: bring up interrupt-driven process management --- */
    process_init();
    scheduler_init();

    pcb_t *shell_proc = create_process(shell_task,  "shell");
    pcb_t *proc_a      = create_process(demo_task_a, "proc_a");
    pcb_t *proc_b      = create_process(demo_task_b, "proc_b");

    scheduler_add(shell_proc);
    scheduler_add(proc_a);
    scheduler_add(proc_b);

    /* --- Lecture 10: synchronisation objects + kernel threads --- */
    mutex_init(&race_mutex, "race");
    sem_init(&race_join, 0,        "race_join");
    sem_init(&pc_empty,  PC_BUF,   "pc_empty");
    sem_init(&pc_full,   0,        "pc_full");
    sem_init(&pc_mux,    1,        "pc_mux");

    /* All three threads belong to the shell process (they share its
     * address space — Lecture 10 §2). */
    thread_create_in(shell_proc->pid, race_coordinator, 0, "race_coord");
    thread_create_in(shell_proc->pid, producer_fn,      0, "producer");
    thread_create_in(shell_proc->pid, consumer_fn,      0, "consumer");
    thread_create_in(shell_proc->pid, mem_demo,         0, "mem_demo");
    thread_create_in(shell_proc->pid, fs_demo,          0, "fs_demo");

    /* Draw a one-time separator + label for the reserved demo status area
     * (rows VGA_SHELL_ROWS..VGA_ROWS-1). This is written once, directly via
     * vga_put_at(), and is never touched by the shell's scrolling logic. */
    redraw_status_separator();

    pic_remap();          /* Remap IRQ0-15 to vectors 0x20-0x2F */
    idt_init();            /* Install irq0_stub at vector 0x20  */
    pit_init(100);          /* 100 Hz -> 10 ms tick               */

    __asm__ __volatile__("sti"); /* Enable interrupts - scheduler is now live */

    scheduler_start();      /* Switches into the shell process; never returns */

    /* Should never reach here */
    __asm__ __volatile__("hlt");
}
