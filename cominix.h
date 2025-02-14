/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FS_COMINIX_H
#define FS_COMINIX_H

#include <linux/fs.h>
#include <linux/pagemap.h>
#include "cominix_fs.h"
#include <linux/buffer_head.h>

#define INODE_VERSION(inode)	cominix_sb(inode->i_sb)->s_version
#define MINIX_V1		0x0001		/* original cominix fs */
#define MINIX_V2		0x0002		/* cominix V2 fs */
#define MINIX_V3		0x0003		/* cominix V3 fs */

typedef u32 block_t;
typedef u64 blockoff_t;

/*
 * cominix fs inode data in memory
 */
struct cominix_inode_info {
	union {
		__u16 i1_data[16];
		__u32 i2_data[16];
	} u;
	struct inode vfs_inode;
};


/*
 * cominix super-block data in memory
 */
struct cominix_sb_info {
	unsigned long s_ninodes;
	unsigned long s_nzones;
	unsigned long s_imap_blocks;
	unsigned long s_zmap_blocks;
	unsigned long s_firstdatazone;
	unsigned long s_log_zone_size;
	int s_dirsize;
	int s_namelen;
	struct buffer_head ** s_imap;
	struct buffer_head ** s_zmap;
	struct buffer_head * s_sbh;
	struct cominix_super_block * s_ms;
	unsigned short s_mount_state;
	unsigned short s_version;
	block_t extra_sb_location;
	blockoff_t hashtable;
	blockoff_t hashtable_size;
	blockoff_t heap_brk;
};

extern struct inode *cominix_iget(struct super_block *, unsigned long);
extern struct cominix_inode *cominix_V1_raw_inode(struct super_block *, ino_t, struct buffer_head **);
extern struct cominix2_inode *cominix_V2_raw_inode(struct super_block *, ino_t, struct buffer_head **);
extern struct inode * cominix_new_inode(const struct inode *, umode_t);
extern void cominix_free_inode(struct inode * inode);
extern unsigned long cominix_count_free_inodes(struct super_block *sb);
extern int cominix_new_block(struct inode * inode);
extern int cominix_new_block_sb(struct super_block *sb);
extern void cominix_free_block(struct inode *inode, unsigned long block);
extern unsigned long cominix_count_free_blocks(struct super_block *sb);
extern int cominix_getattr(struct mnt_idmap *, const struct path *,
		struct kstat *, u32, unsigned int);
extern int cominix_prepare_chunk(struct folio *folio, loff_t pos, unsigned len);
extern void V1_cominix_truncate(struct inode *);
extern void V2_cominix_truncate(struct inode *);
extern void cominix_truncate(struct inode *);
extern void cominix_set_inode(struct inode *, dev_t);
extern int V1_cominix_get_block(struct inode *, long, struct buffer_head *, int);
extern int V2_cominix_get_block(struct inode *, long, struct buffer_head *, int);
extern unsigned V1_cominix_blocks(loff_t, struct super_block *);
extern unsigned V2_cominix_blocks(loff_t, struct super_block *);

struct cominix_dir_entry *cominix_find_entry(struct dentry*, struct folio**);
int cominix_add_link(struct dentry*, struct inode*);
int cominix_delete_entry(struct cominix_dir_entry*, struct folio*);
int cominix_make_empty(struct inode*, struct inode*);
int cominix_empty_dir(struct inode*);
int cominix_set_link(struct cominix_dir_entry *de, struct folio *page,
		struct inode *inode);
struct cominix_dir_entry *cominix_dotdot(struct inode*, struct folio**);
ino_t cominix_inode_by_name(struct dentry*);

extern const struct inode_operations cominix_file_inode_operations;
extern const struct inode_operations cominix_dir_inode_operations;
extern const struct file_operations cominix_file_operations;
extern const struct file_operations cominix_dir_operations;


int cominix_write_inode(struct inode *inode, struct writeback_control *wbc);

struct buffer_head *load_block(struct super_block *sb, block_t block);
struct chunk_entry {
	u64 location;
	u64 size;
};

static inline struct cominix_sb_info *cominix_sb(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct cominix_inode_info *cominix_i(struct inode *inode)
{
	return container_of(inode, struct cominix_inode_info, vfs_inode);
}

static inline unsigned cominix_blocks_needed(unsigned bits, unsigned blocksize)
{
	return DIV_ROUND_UP(bits, blocksize * 8);
}

void __init cminix_proc_init(void);
void cminix_proc_clean(void);
extern struct file_system_type cominix_fs_type;
extern const struct file_operations chunked_file_operations;
static inline u32 *get_mark_indirect(struct inode *inode)
{
	return &cominix_i(inode)->u.i2_data[9];
}
static inline u32 *get_zones(struct inode *inode)
{
	return cominix_i(inode)->u.i2_data;
}


#if defined(CONFIG_MINIX_FS_NATIVE_ENDIAN) && \
	defined(CONFIG_MINIX_FS_BIG_ENDIAN_16BIT_INDEXED)

#error Minix file system byte order broken

#elif defined(CONFIG_MINIX_FS_NATIVE_ENDIAN)

/*
 * big-endian 32 or 64 bit indexed bitmaps on big-endian system or
 * little-endian bitmaps on little-endian system
 */

#define cominix_test_and_set_bit(nr, addr)	\
	__test_and_set_bit((nr), (unsigned long *)(addr))
#define cominix_set_bit(nr, addr)		\
	__set_bit((nr), (unsigned long *)(addr))
#define cominix_test_and_clear_bit(nr, addr) \
	__test_and_clear_bit((nr), (unsigned long *)(addr))
#define cominix_test_bit(nr, addr)		\
	test_bit((nr), (unsigned long *)(addr))
#define cominix_find_first_zero_bit(addr, size) \
	find_first_zero_bit((unsigned long *)(addr), (size))

#elif defined(CONFIG_MINIX_FS_BIG_ENDIAN_16BIT_INDEXED)

/*
 * big-endian 16bit indexed bitmaps
 */

static inline int cominix_find_first_zero_bit(const void *vaddr, unsigned size)
{
	const unsigned short *p = vaddr, *addr = vaddr;
	unsigned short num;

	if (!size)
		return 0;

	size >>= 4;
	while (*p++ == 0xffff) {
		if (--size == 0)
			return (p - addr) << 4;
	}

	num = *--p;
	return ((p - addr) << 4) + ffz(num);
}

#define cominix_test_and_set_bit(nr, addr)	\
	__test_and_set_bit((nr) ^ 16, (unsigned long *)(addr))
#define cominix_set_bit(nr, addr)	\
	__set_bit((nr) ^ 16, (unsigned long *)(addr))
#define cominix_test_and_clear_bit(nr, addr)	\
	__test_and_clear_bit((nr) ^ 16, (unsigned long *)(addr))

static inline int cominix_test_bit(int nr, const void *vaddr)
{
	const unsigned short *p = vaddr;
	return (p[nr >> 4] & (1U << (nr & 15))) != 0;
}

#else

/*
 * little-endian bitmaps
 */

#define cominix_test_and_set_bit	__test_and_set_bit_le
#define cominix_set_bit		__set_bit_le
#define cominix_test_and_clear_bit	__test_and_clear_bit_le
#define cominix_test_bit	test_bit_le
#define cominix_find_first_zero_bit	find_first_zero_bit_le

#endif

#endif /* FS_MINIX_H */
