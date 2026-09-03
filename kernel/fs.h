/* =============================================================================
 * SENG21213-OS :: Simple File System (SFS)
 * File   : kernel/fs.h
 * Purpose: Lecture 12 §2-3. A flat, inode-based file system living on the
 *          1 MB RAM disk.
 *
 * Disk layout (each unit = 4 KB block):
 *   Block  0  : Superblock
 *   Block  1  : Block bitmap  (256 bits = 32 bytes, fits in one block)
 *   Block  2  : Inode bitmap  (32 bits  = 4 bytes,  fits in one block)
 *   Blocks 3-34: Inode table  (32 inodes × 128 bytes = 4 KB, 1 block)
 *                Note: all 32 inodes fit in 1 block (32 × 128 = 4096).
 *   Block  4  : Root directory (flat array of 32 dir_entry_t, 1 block)
 *   Blocks 5-255: Data blocks  (251 blocks = ~1004 KB usable data)
 *
 * (Blocks 3-4 are just indices 3 and 4; the table + dir each need only
 *  one 4 KB block for 32 entries each.)
 *
 * inode_t  : size, 8 direct block pointers -> max file = 8 × 4 KB = 32 KB
 * dir_entry_t : 28-char name + 4-byte inode number
 * ============================================================================*/
#ifndef FS_H
#define FS_H

#include "../include/types.h"

/* --- on-disk geometry --- */
#define FS_MAGIC         0x53465321u    /* 'SFS!' */
#define FS_MAX_INODES    32
#define FS_MAX_DIRECT    8              /* direct block pointers per inode  */
#define FS_MAX_FILE_SIZE (FS_MAX_DIRECT * 4096u)  /* 32 KB                 */
#define FS_NAME_LEN      28
#define FS_MAX_FILES     32            /* flat directory: 32 entries        */

/* Block indices */
#define FS_BLK_SUPER     0
#define FS_BLK_BMAP      1
#define FS_BLK_IMAP      2
#define FS_BLK_ITABLE    3
#define FS_BLK_DIR       4
#define FS_BLK_DATA      5             /* first usable data block           */

/* --- on-disk structures (must stay <= 4096 bytes for their block) --- */
typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint8_t  _pad[4076];    /* pad superblock to exactly 4096 bytes */
} __attribute__((packed)) superblock_t;

typedef struct {
    uint32_t size;                     /* file size in bytes               */
    uint32_t blocks[FS_MAX_DIRECT];    /* data-block indices (0 = unused)  */
    uint8_t  _pad[92];    /* pad inode to exactly 128 bytes (32×128=4096) */
} __attribute__((packed)) inode_t;

typedef struct {
    char     name[FS_NAME_LEN];        /* NUL-terminated file name         */
    uint32_t inode;                    /* inode number (0 = free slot)     */
} __attribute__((packed)) dir_entry_t;

/* --- file descriptor (in-memory, not on disk) --- */
#define FS_MAX_FDS   8

typedef struct {
    int      used;
    uint32_t inode;     /* inode number                                    */
    uint32_t offset;    /* current read/write byte position                */
} fd_t;

/* --- public API --- */
void  fs_mkfs(void);               /* format the RAM disk                  */
int   fs_open  (const char *name); /* returns fd >= 0, or -1               */
int   fs_creat (const char *name); /* create + open (touch); -1 on error   */
int   fs_read  (int fd, void *buf, uint32_t len);
int   fs_write (int fd, const void *buf, uint32_t len);
int   fs_close (int fd);
int   fs_unlink(const char *name);

/* Directory iteration for `ls` */
int   fs_ls    (dir_entry_t *out, int max); /* returns entry count         */

/* Helpers exposed to the shell */
int   fs_fsize (int fd);           /* current file size (bytes)            */
int   fs_seek  (int fd, uint32_t pos);

#endif /* FS_H */
