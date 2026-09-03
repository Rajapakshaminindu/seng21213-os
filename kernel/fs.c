/* =============================================================================
 * SENG21213-OS :: Simple File System (SFS) implementation
 * File   : kernel/fs.c
 * Purpose: Lecture 12 §2-3.
 *
 * All on-disk I/O goes through ramdisk.c (rd_read_block / rd_write_block).
 * The in-memory file-descriptor table (fd_t fds[]) is never persisted.
 *
 * IMPORTANT: every function that previously allocated large buffers on the
 * stack (uint8_t buf[4096]) now uses a STATIC local buffer.  Thread stacks
 * are only 4 KB (PROC_STACK_SIZE), so a single 4 KB stack frame would
 * immediately overflow them.  FS operations are not re-entrant in this OS
 * (no concurrent FS threads), so static locals are safe.
 * ============================================================================*/
#include "fs.h"
#include "ramdisk.h"

/* ---- helpers (no libc) -------------------------------------------------- */
static void fs_memset(void *p, uint8_t v, uint32_t n) {
    uint8_t *b = (uint8_t *)p;
    while (n--) *b++ = v;
}
static void fs_memcpy(void *d, const void *s, uint32_t n) {
    uint8_t *dd = (uint8_t *)d;
    const uint8_t *ss = (const uint8_t *)s;
    while (n--) *dd++ = *ss++;
}
static int fs_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}
static uint32_t fs_strlen(const char *s) {
    uint32_t n = 0; while (s[n]) n++; return n;
}
static void fs_strncpy(char *d, const char *s, uint32_t n) {
    uint32_t i = 0;
    while (i < n - 1 && s[i]) { d[i] = s[i]; i++; }
    d[i] = '\0';
}

/* ---- in-memory state ---------------------------------------------------- */
static fd_t fds[FS_MAX_FDS];

/* ---- bitmap helpers ------------------------------------------------------ */
static uint8_t bmap[RD_BLOCK_SIZE];
static uint8_t imap[RD_BLOCK_SIZE];

static void bmap_load(void)  { rd_read_block(FS_BLK_BMAP, bmap); }
static void bmap_flush(void) { rd_write_block(FS_BLK_BMAP, bmap); }
static void imap_load(void)  { rd_read_block(FS_BLK_IMAP, imap); }
static void imap_flush(void) { rd_write_block(FS_BLK_IMAP, imap); }

static int  bmap_test(uint32_t b) { return (bmap[b>>3] >> (b&7)) & 1; }
static void bmap_set(uint32_t b)  { bmap[b>>3] |=  (1u << (b&7)); }
static void bmap_clr(uint32_t b)  { bmap[b>>3] &= ~(1u << (b&7)); }
static int  imap_test(uint32_t i) { return (imap[i>>3] >> (i&7)) & 1; }
static void imap_set(uint32_t i)  { imap[i>>3] |=  (1u << (i&7)); }
static void imap_clr(uint32_t i)  { imap[i>>3] &= ~(1u << (i&7)); }

static uint32_t alloc_block(void) {
    /* static: avoids 4 KB stack frame on 4 KB thread stack. */
    static uint8_t zero_blk[RD_BLOCK_SIZE];
    bmap_load();
    for (uint32_t b = FS_BLK_DATA; b < RD_TOTAL_BLOCKS; b++) {
        if (!bmap_test(b)) {
            bmap_set(b);
            bmap_flush();
            fs_memset(zero_blk, 0, RD_BLOCK_SIZE);
            rd_write_block(b, zero_blk);
            return b;
        }
    }
    return 0;
}

static void free_block(uint32_t b) {
    if (b < FS_BLK_DATA || b >= RD_TOTAL_BLOCKS) return;
    bmap_load();
    bmap_clr(b);
    bmap_flush();
}

static uint32_t alloc_inode(void) {
    imap_load();
    for (uint32_t i = 1; i < FS_MAX_INODES; i++) {
        if (!imap_test(i)) {
            imap_set(i);
            imap_flush();
            return i;
        }
    }
    return 0;
}

static void free_inode(uint32_t i) {
    imap_load();
    imap_clr(i);
    imap_flush();
}

/* ---- inode table helpers ------------------------------------------------ */
static uint8_t itable_buf[RD_BLOCK_SIZE];

static inode_t *itable_ptr(uint32_t ino) {
    return (inode_t *)(itable_buf + (ino - 1) * sizeof(inode_t));
}
static void itable_load(void)  { rd_read_block(FS_BLK_ITABLE, itable_buf); }
static void itable_flush(void) { rd_write_block(FS_BLK_ITABLE, itable_buf); }

/* ---- directory helpers -------------------------------------------------- */
static uint8_t dir_buf[RD_BLOCK_SIZE];
#define DIR_ENTRIES  (RD_BLOCK_SIZE / sizeof(dir_entry_t))

static void dir_load(void)  { rd_read_block(FS_BLK_DIR, dir_buf); }
static void dir_flush(void) { rd_write_block(FS_BLK_DIR, dir_buf); }

static dir_entry_t *dir_entry(uint32_t i) {
    return (dir_entry_t *)(dir_buf + i * sizeof(dir_entry_t));
}

static int dir_find(const char *name) {
    dir_load();
    for (uint32_t i = 0; i < DIR_ENTRIES; i++) {
        dir_entry_t *e = dir_entry(i);
        if (e->inode && fs_strcmp(e->name, name) == 0) return (int)i;
    }
    return -1;
}

static int dir_free_slot(void) {
    for (uint32_t i = 0; i < DIR_ENTRIES; i++) {
        if (dir_entry(i)->inode == 0) return (int)i;
    }
    return -1;
}

/* ---- superblock helpers ------------------------------------------------- */
static void sb_update(int delta_blocks, int delta_inodes) {
    superblock_t sb;
    rd_read_block(FS_BLK_SUPER, &sb);
    sb.free_blocks = (uint32_t)((int)sb.free_blocks + delta_blocks);
    sb.free_inodes = (uint32_t)((int)sb.free_inodes + delta_inodes);
    rd_write_block(FS_BLK_SUPER, &sb);
}

/* ---- public API --------------------------------------------------------- */

void fs_mkfs(void) {
    /* static: superblock_t is padded to 4096 bytes — too large for a thread
     * stack; zero_blk is another 4096. Using static avoids stack overflow. */
    static uint8_t      zero_blk[RD_BLOCK_SIZE];
    static superblock_t sb;

    fs_memset(zero_blk, 0, RD_BLOCK_SIZE);
    for (int b = 0; b <= 4; b++) rd_write_block((uint32_t)b, zero_blk);

    fs_memset(&sb, 0, sizeof(sb));
    sb.magic        = FS_MAGIC;
    sb.total_blocks = RD_TOTAL_BLOCKS;
    sb.total_inodes = FS_MAX_INODES;
    sb.free_blocks  = RD_TOTAL_BLOCKS - FS_BLK_DATA;
    sb.free_inodes  = FS_MAX_INODES - 1;
    rd_write_block(FS_BLK_SUPER, &sb);

    bmap_load();
    for (uint32_t b = 0; b <= 4; b++) bmap_set(b);
    bmap_flush();

    imap_load();
    imap_set(0);
    imap_flush();
}

int fs_creat(const char *name) {
    if (!name || fs_strlen(name) == 0 || fs_strlen(name) >= FS_NAME_LEN)
        return -1;
    dir_load();
    if (dir_find(name) >= 0) return -1;
    int slot = dir_free_slot();
    if (slot < 0) return -1;

    uint32_t ino = alloc_inode();
    if (!ino) return -1;

    itable_load();
    inode_t *ip = itable_ptr(ino);
    fs_memset(ip, 0, sizeof(inode_t));
    itable_flush();

    dir_load();
    dir_entry_t *e = dir_entry((uint32_t)slot);
    fs_strncpy(e->name, name, FS_NAME_LEN);
    e->inode = ino;
    dir_flush();
    sb_update(0, -1);

    for (int i = 0; i < FS_MAX_FDS; i++) {
        if (!fds[i].used) {
            fds[i].used   = 1;
            fds[i].inode  = ino;
            fds[i].offset = 0;
            return i;
        }
    }
    return -1;
}

int fs_open(const char *name) {
    dir_load();
    int idx = dir_find(name);
    if (idx < 0) return -1;
    uint32_t ino = dir_entry((uint32_t)idx)->inode;
    for (int i = 0; i < FS_MAX_FDS; i++) {
        if (!fds[i].used) {
            fds[i].used   = 1;
            fds[i].inode  = ino;
            fds[i].offset = 0;
            return i;
        }
    }
    return -1;
}

int fs_close(int fd) {
    if (fd < 0 || fd >= FS_MAX_FDS || !fds[fd].used) return -1;
    fds[fd].used = 0;
    return 0;
}

int fs_seek(int fd, uint32_t pos) {
    if (fd < 0 || fd >= FS_MAX_FDS || !fds[fd].used) return -1;
    itable_load();
    inode_t *ip = itable_ptr(fds[fd].inode);
    if (pos > ip->size) pos = ip->size;
    fds[fd].offset = pos;
    return 0;
}

int fs_fsize(int fd) {
    if (fd < 0 || fd >= FS_MAX_FDS || !fds[fd].used) return -1;
    itable_load();
    return (int)itable_ptr(fds[fd].inode)->size;
}

int fs_read(int fd, void *buf, uint32_t len) {
    /* static: avoids 4 KB on thread stack (PROC_STACK_SIZE = 4096). */
    static uint8_t blkbuf[RD_BLOCK_SIZE];
    if (fd < 0 || fd >= FS_MAX_FDS || !fds[fd].used) return -1;
    itable_load();
    inode_t *ip = itable_ptr(fds[fd].inode);
    uint32_t off  = fds[fd].offset;
    if (off >= ip->size) return 0;
    if (off + len > ip->size) len = ip->size - off;

    uint32_t done = 0;
    while (done < len) {
        uint32_t blk_idx = (off + done) / RD_BLOCK_SIZE;
        uint32_t blk_off = (off + done) % RD_BLOCK_SIZE;
        if (blk_idx >= FS_MAX_DIRECT || !ip->blocks[blk_idx]) break;
        rd_read_block(ip->blocks[blk_idx], blkbuf);
        uint32_t can = RD_BLOCK_SIZE - blk_off;
        if (can > len - done) can = len - done;
        fs_memcpy((uint8_t *)buf + done, blkbuf + blk_off, can);
        done += can;
    }
    fds[fd].offset += done;
    return (int)done;
}

int fs_write(int fd, const void *buf, uint32_t len) {
    /* static: avoids 4 KB on thread stack. */
    static uint8_t blkbuf[RD_BLOCK_SIZE];
    if (fd < 0 || fd >= FS_MAX_FDS || !fds[fd].used) return -1;
    if (len == 0) return 0;
    itable_load();
    inode_t *ip = itable_ptr(fds[fd].inode);
    uint32_t off  = fds[fd].offset;
    if (off + len > FS_MAX_FILE_SIZE) return -1;

    uint32_t done = 0;
    while (done < len) {
        uint32_t blk_idx = (off + done) / RD_BLOCK_SIZE;
        uint32_t blk_off = (off + done) % RD_BLOCK_SIZE;
        if (blk_idx >= FS_MAX_DIRECT) break;
        if (!ip->blocks[blk_idx]) {
            uint32_t nb = alloc_block();
            if (!nb) break;
            ip->blocks[blk_idx] = nb;
            sb_update(-1, 0);
            itable_flush();
            itable_load();
            ip = itable_ptr(fds[fd].inode);
        }
        rd_read_block(ip->blocks[blk_idx], blkbuf);
        uint32_t can = RD_BLOCK_SIZE - blk_off;
        if (can > len - done) can = len - done;
        fs_memcpy(blkbuf + blk_off, (const uint8_t *)buf + done, can);
        rd_write_block(ip->blocks[blk_idx], blkbuf);
        done += can;
    }
    fds[fd].offset += done;
    if (fds[fd].offset > ip->size) {
        ip->size = fds[fd].offset;
        itable_flush();
    }
    return (int)done;
}

int fs_unlink(const char *name) {
    dir_load();
    int idx = dir_find(name);
    if (idx < 0) return -1;
    uint32_t ino = dir_entry((uint32_t)idx)->inode;

    itable_load();
    inode_t *ip = itable_ptr(ino);
    for (int i = 0; i < FS_MAX_DIRECT; i++) {
        if (ip->blocks[i]) { free_block(ip->blocks[i]); ip->blocks[i] = 0; }
    }
    fs_memset(ip, 0, sizeof(inode_t));
    itable_flush();

    free_inode(ino);
    sb_update(0, 1);

    dir_entry_t *e = dir_entry((uint32_t)idx);
    fs_memset(e, 0, sizeof(dir_entry_t));
    dir_flush();

    for (int i = 0; i < FS_MAX_FDS; i++) {
        if (fds[i].used && fds[i].inode == ino) fds[i].used = 0;
    }
    return 0;
}

int fs_ls(dir_entry_t *out, int max) {
    dir_load();
    int count = 0;
    itable_load();
    for (uint32_t i = 0; i < DIR_ENTRIES && count < max; i++) {
        dir_entry_t *e = dir_entry(i);
        if (e->inode) {
            fs_memcpy(&out[count], e, sizeof(dir_entry_t));
            count++;
        }
    }
    return count;
}
