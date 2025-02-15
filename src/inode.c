// SPDX-License-Identifier: GPL-2.0-only
/*
 *  linux/fs/cominix/inode.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Copyright (C) 1996  Gertjan van Wingerde
 *	Minix V2 fs support.
 *
 *  Modified for 680x0 by Andreas Schwab
 *  Updated to filesystem version 3 by Daniel Aragones
 */

#include <linux/module.h>
#include "cominix.h"
#include "chunk_handler.h"
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/highuid.h>
#include <linux/mpage.h>
#include <linux/vfs.h>
#include <linux/writeback.h>

typedef u32 block_t;
struct buffer_head *load_block(struct super_block *sb, block_t block)
{
	BUG_ON(sb->s_blocksize_bits < 9);
	return sb_bread(sb, block);
}

//int cominix_write_inode(struct inode *inode,
		//struct writeback_control *wbc);
static int cominix_statfs(struct dentry *dentry, struct kstatfs *buf);
static int cominix_remount (struct super_block * sb, int * flags, char * data);

static void cominix_evict_inode(struct inode *inode)
{
	truncate_inode_pages_final(&inode->i_data);
	if (!inode->i_nlink) {
		inode->i_size = 0;
		cominix_truncate(inode);
	}
	invalidate_inode_buffers(inode);
	clear_inode(inode);
	if (!inode->i_nlink)
		cominix_free_inode(inode);
}

static void cominix_put_super(struct super_block *sb)
{
	int i;
	struct cominix_sb_info *sbi = cominix_sb(sb);

	if (!sb_rdonly(sb)) {
		if (sbi->s_version != MINIX_V3)	 /* s_state is now out from V3 sb */
			sbi->s_ms->s_state = sbi->s_mount_state;
		mark_buffer_dirty(sbi->s_sbh);
	}
	for (i = 0; i < sbi->s_imap_blocks; i++)
		brelse(sbi->s_imap[i]);
	for (i = 0; i < sbi->s_zmap_blocks; i++)
		brelse(sbi->s_zmap[i]);
	brelse (sbi->s_sbh);
	kfree(sbi->s_imap);
	sb->s_fs_info = NULL;
	kfree(sbi);
}

static struct kmem_cache * cominix_inode_cachep;

static struct inode *cominix_alloc_inode(struct super_block *sb)
{
	struct cominix_inode_info *ei;
	ei = alloc_inode_sb(sb, cominix_inode_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	return &ei->vfs_inode;
}

static void cominix_free_in_core_inode(struct inode *inode)
{
	kmem_cache_free(cominix_inode_cachep, cominix_i(inode));
}

static void init_once(void *foo)
{
	struct cominix_inode_info *ei = (struct cominix_inode_info *) foo;

	inode_init_once(&ei->vfs_inode);
}

static int __init init_inodecache(void)
{
	cominix_inode_cachep = kmem_cache_create("cominix_inode_cache",
					     sizeof(struct cominix_inode_info),
					     0, (SLAB_RECLAIM_ACCOUNT|
						SLAB_ACCOUNT),
					     init_once);
	if (cominix_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void destroy_inodecache(void)
{
	/*
	 * Make sure all delayed rcu free inodes are flushed before we
	 * destroy cache.
	 */
	rcu_barrier();
	kmem_cache_destroy(cominix_inode_cachep);
}

static const struct super_operations cominix_sops = {
	.alloc_inode	= cominix_alloc_inode,
	.free_inode	= cominix_free_in_core_inode,
	.write_inode	= cominix_write_inode,
	.evict_inode	= cominix_evict_inode,
	.put_super	= cominix_put_super,
	.statfs		= cominix_statfs,
	.remount_fs	= cominix_remount,
};

static int cominix_remount (struct super_block * sb, int * flags, char * data)
{
	struct cominix_sb_info * sbi = cominix_sb(sb);
	struct cominix_super_block * ms;

	sync_filesystem(sb);
	ms = sbi->s_ms;
	if ((bool)(*flags & SB_RDONLY) == sb_rdonly(sb))
		return 0;
	if (*flags & SB_RDONLY) {
		if (ms->s_state & MINIX_VALID_FS ||
		    !(sbi->s_mount_state & MINIX_VALID_FS))
			return 0;
		/* Mounting a rw partition read-only. */
		if (sbi->s_version != MINIX_V3)
			ms->s_state = sbi->s_mount_state;
		mark_buffer_dirty(sbi->s_sbh);
	} else {
	  	/* Mount a partition which is read-only, read-write. */
		if (sbi->s_version != MINIX_V3) {
			sbi->s_mount_state = ms->s_state;
			ms->s_state &= ~MINIX_VALID_FS;
		} else {
			sbi->s_mount_state = MINIX_VALID_FS;
		}
		mark_buffer_dirty(sbi->s_sbh);

		if (!(sbi->s_mount_state & MINIX_VALID_FS))
			printk("MINIX-fs warning: remounting unchecked fs, "
				"running fsck is recommended\n");
		else if ((sbi->s_mount_state & MINIX_ERROR_FS))
			printk("MINIX-fs warning: remounting fs with errors, "
				"running fsck is recommended\n");
	}
	return 0;
}

static bool cominix_check_superblock(struct super_block *sb)
{
	struct cominix_sb_info *sbi = cominix_sb(sb);

	if (sbi->s_imap_blocks == 0 || sbi->s_zmap_blocks == 0)
		return false;

	/*
	 * s_max_size must not exceed the block mapping limitation.  This check
	 * is only needed for V1 filesystems, since V2/V3 support an extra level
	 * of indirect blocks which places the limit well above U32_MAX.
	 */
	if (sbi->s_version == MINIX_V1 &&
	    sb->s_maxbytes > (7 + 512 + 512*512) * BLOCK_SIZE)
		return false;

	return true;
}

static int cminix_alloc_extra_super(struct super_block *sb, struct buffer_head *sb_bh)
{
	struct cominix_sb_info *sbi = cominix_sb(sb);
	struct buffer_head *bh = NULL;

	block_t esb_loc = sbi->extra_sb_location;
	if (esb_loc) {
		printk("Trying to alloc a new block for an already filled extra super block\n");
		BUG_ON(1);
	}
	esb_loc = cominix_new_block_sb(sb);
	if (WARN_ON(!esb_loc)) {
		printk("Couldn't allocate an extra super block.\n");
		return -1;
	}
	sbi->extra_sb_location = esb_loc;
	bh = load_block(sb, esb_loc);
	BUG_ON(!bh);
	memset(bh->b_data, 0, sb->s_blocksize);
	struct cminix_extra_super_block *esb = (void*)bh->b_data;
	esb->hashtable_location = sbi->hashtable;
	esb->hashtable_size = sbi->hashtable_size;
	esb->heap_brk = sbi->heap_brk;

	struct cominix3_super_block *m3s = (void*)sb_bh->b_data;
	m3s->s_pad0 = esb_loc & ((1 << 16) - 1);
	m3s->s_pad1 = esb_loc >> 16;
	mark_buffer_dirty(sb_bh);

	printk("RESETTING CHUNK HASHTABLE\n");
	chunk_reset_hashtable(sb);

	mark_buffer_dirty(bh);
	brelse(bh);
	return 0;
}

static int cminix_fill_extra_super(struct super_block *sb)
{
	struct cominix_sb_info *sbi = cominix_sb(sb);

	block_t esb_loc = sbi->extra_sb_location;
	struct buffer_head *bh = NULL;
	if (!esb_loc) {
		sbi->hashtable = 40LL << 20;
		sbi->hashtable_size = 32LL << 10;
		sbi->heap_brk = sbi->hashtable + sbi->hashtable_size;

		brelse(bh);
		return 0;
	}
	printk("esb_loc is %d\n", esb_loc);
	bh = load_block(sb, esb_loc);
	BUG_ON(!bh || IS_ERR(bh));
	struct cminix_extra_super_block *esb = (void *)bh->b_data;
	sbi->hashtable = esb->hashtable_location;
	printk("hashtable location is %lld kb (sector no. %lld)\n", esb->hashtable_location >> 10, esb->hashtable_location >> 9);
	sbi->hashtable_size = esb->hashtable_size;
	printk("hashtable size is %d kb\n", esb->hashtable_size >> 10);
	sbi->heap_brk = esb->heap_brk;
	printk("hashtable break is %lld kb\n", esb->heap_brk >> 10);
	printk("heap size is %lld kb\n", (esb->heap_brk - esb->hashtable_location - esb->hashtable_size)>> 10);
	brelse(bh);
	return 0;
	
}

static int cominix_fill_super(struct super_block *s, void *data, int silent)
{
	struct buffer_head *bh;
	struct buffer_head **map;
	struct cominix_super_block *ms;
	struct cominix3_super_block *m3s = NULL;
	unsigned long i, block;
	struct inode *root_inode;
	struct cominix_sb_info *sbi;
	int ret = -EINVAL;

	sbi = kzalloc(sizeof(struct cominix_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	s->s_fs_info = sbi;

	BUILD_BUG_ON(32 != sizeof (struct cominix_inode));
	BUILD_BUG_ON(64 != sizeof(struct cominix2_inode));

	if (!sb_set_blocksize(s, BLOCK_SIZE))
		goto out_bad_hblock;

	if (!(bh = sb_bread(s, 1)))
		goto out_bad_sb;

	ms = (struct cominix_super_block *) bh->b_data;
	sbi->s_ms = ms;
	sbi->s_sbh = bh;
	sbi->s_mount_state = ms->s_state;
	sbi->s_ninodes = ms->s_ninodes;
	sbi->s_nzones = ms->s_nzones;
	sbi->s_imap_blocks = ms->s_imap_blocks;
	sbi->s_zmap_blocks = ms->s_zmap_blocks;
	sbi->s_firstdatazone = ms->s_firstdatazone;
	sbi->s_log_zone_size = ms->s_log_zone_size;
	s->s_maxbytes = ms->s_max_size;
	s->s_magic = ms->s_magic;
	if (!(*(__u16 *)(bh->b_data + 24) == MINIX3_SUPER_MAGIC))
		goto out_no_fs;

	m3s = (struct cominix3_super_block *) bh->b_data;
	s->s_magic = m3s->s_magic;
	sbi->s_imap_blocks = m3s->s_imap_blocks;
	sbi->s_zmap_blocks = m3s->s_zmap_blocks;
	sbi->s_firstdatazone = m3s->s_firstdatazone;
	sbi->s_log_zone_size = m3s->s_log_zone_size;
	if (WARN_ON(sbi->s_log_zone_size != 0)) {
		printk("CMINIX doesn't support the zone size being different from block size.\n");
		goto out_release;
	}
	s->s_maxbytes = m3s->s_max_size;
	sbi->s_ninodes = m3s->s_ninodes;
	sbi->s_nzones = m3s->s_zones;
	sbi->s_dirsize = 64;
	sbi->s_namelen = 60;
	sbi->s_version = MINIX_V3;
	sbi->s_mount_state = MINIX_VALID_FS;
	sb_set_blocksize(s, m3s->s_blocksize);
	printk("blocksize is %lld bytes\n", m3s->s_blocksize);
	s->s_max_links = MINIX2_LINK_MAX;

	if (!cominix_check_superblock(s))
		goto out_illegal_sb;

	int alloc_new_esb = false;
	block_t *esb_loc = &sbi->extra_sb_location;
	*esb_loc = (m3s->s_pad1 << 16) + m3s->s_pad0;
	if (*esb_loc == 0) {
		printk("Previous extra-superblock not found. Will make new.");
		alloc_new_esb = true;
	}

	ret = cminix_fill_extra_super(s);
	if (ret)
		goto out_illegal_sb;
	u64 new_nzones = sbi->hashtable >> s->s_blocksize_bits;
	//nzones is the total number of blocks on the whole disk
	sbi->max_brk = sbi->s_nzones * s->s_blocksize;
	BUG_ON(new_nzones > sbi->s_nzones);
	sbi->s_nzones = new_nzones;

	/*
	 * Allocate the buffer map to keep the superblock small.
	 */
	i = (sbi->s_imap_blocks + sbi->s_zmap_blocks) * sizeof(bh);
	map = kzalloc(i, GFP_KERNEL);
	if (!map)
		goto out_no_map;
	sbi->s_imap = &map[0];
	sbi->s_zmap = &map[sbi->s_imap_blocks];

	block=2;
	for (i=0 ; i < sbi->s_imap_blocks ; i++) {
		if (!(sbi->s_imap[i]=sb_bread(s, block)))
			goto out_no_bitmap;
		block++;
	}
	for (i=0 ; i < sbi->s_zmap_blocks ; i++) {
		if (!(sbi->s_zmap[i]=sb_bread(s, block)))
			goto out_no_bitmap;
		block++;
	}

	cominix_set_bit(0,sbi->s_imap[0]->b_data);
	cominix_set_bit(0,sbi->s_zmap[0]->b_data);

	/* Apparently cominix can create filesystems that allocate more blocks for
	 * the bitmaps than needed.  We simply ignore that, but verify it didn't
	 * create one with not enough blocks and bail out if so.
	 */
	block = cominix_blocks_needed(sbi->s_ninodes, s->s_blocksize);
	if (sbi->s_imap_blocks < block) {
		printk("MINIX-fs: file system does not have enough "
				"imap blocks allocated.  Refusing to mount.\n");
		goto out_no_bitmap;
	}

	block = cominix_blocks_needed(
			(sbi->s_nzones - sbi->s_firstdatazone + 1),
			s->s_blocksize);
	if (sbi->s_zmap_blocks < block) {
		printk("MINIX-fs: file system does not have enough "
				"zmap blocks allocated.  Refusing to mount.\n");
		goto out_no_bitmap;
	}

	/* set up enough so that it can read an inode */
	s->s_op = &cominix_sops;
	s->s_time_min = 0;
	s->s_time_max = U32_MAX;
	root_inode = cominix_iget(s, MINIX_ROOT_INO);
	if (IS_ERR(root_inode)) {
		ret = PTR_ERR(root_inode);
		goto out_no_root;
	}

	ret = -ENOMEM;
	s->s_root = d_make_root(root_inode);
	if (!s->s_root)
		goto out_no_root;

	if (!sb_rdonly(s)) {
		if (sbi->s_version != MINIX_V3) /* s_state is now out from V3 sb */
			ms->s_state &= ~MINIX_VALID_FS;
		mark_buffer_dirty(bh);
	}
	if (!(sbi->s_mount_state & MINIX_VALID_FS))
		printk("MINIX-fs: mounting unchecked file system, "
			"running fsck is recommended\n");
	else if (sbi->s_mount_state & MINIX_ERROR_FS)
		printk("MINIX-fs: mounting file system with errors, "
			"running fsck is recommended\n");

	if (alloc_new_esb)
		cminix_alloc_extra_super(s, bh);

	return 0;

out_no_root:
	if (!silent)
		printk("MINIX-fs: get root inode failed\n");
	goto out_freemap;

out_no_bitmap:
	printk("MINIX-fs: bad superblock or unable to read bitmaps\n");
out_freemap:
	for (i = 0; i < sbi->s_imap_blocks; i++)
		brelse(sbi->s_imap[i]);
	for (i = 0; i < sbi->s_zmap_blocks; i++)
		brelse(sbi->s_zmap[i]);
	kfree(sbi->s_imap);
	goto out_release;

out_no_map:
	ret = -ENOMEM;
	if (!silent)
		printk("MINIX-fs: can't allocate map\n");
	goto out_release;

out_illegal_sb:
	if (!silent)
		printk("MINIX-fs: bad superblock\n");
	goto out_release;

out_no_fs:
	if (!silent)
		printk("VFS: Can't find a Minix filesystem V3 "
		       "on device %s.\n", s->s_id);
out_release:
	brelse(bh);
	goto out;

out_bad_hblock:
	printk("MINIX-fs: blocksize too small for device\n");
	goto out;

out_bad_sb:
	printk("MINIX-fs: unable to read superblock\n");
out:
	s->s_fs_info = NULL;
	kfree(sbi);
	return ret;
}

static int cominix_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;
	struct cominix_sb_info *sbi = cominix_sb(sb);
	u64 id = huge_encode_dev(sb->s_bdev->bd_dev);
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = (sbi->s_nzones - sbi->s_firstdatazone) << sbi->s_log_zone_size;
	buf->f_bfree = cominix_count_free_blocks(sb);
	buf->f_bavail = buf->f_bfree;
	buf->f_files = sbi->s_ninodes;
	buf->f_ffree = cominix_count_free_inodes(sb);
	buf->f_namelen = sbi->s_namelen;
	buf->f_fsid = u64_to_fsid(id);

	return 0;
}

static int cominix_get_block(struct inode *inode, sector_t block,
		    struct buffer_head *bh_result, int create)
{
	return V2_cominix_get_block(inode, block, bh_result, create);
}

static int cominix_writepages(struct address_space *mapping,
		struct writeback_control *wbc)
{	
	return mpage_writepages(mapping, wbc, cominix_get_block);
	//char namebuf[] = "!FILENAME ERROR!";
	////int buflen = sizeof(namebuf)/sizeof(char);
	//char *name = NULL;// = dentry_path(mapping->host->i_dentry, namebuf, buflen);
	//if (!name)
	//	name = namebuf;
	//printk(KERN_ALERT "MINIX tried to writeback pages from file '%s'", name);
	//return -EIO;
}

static int cominix_read_folio(struct file *file, struct folio *folio)
{
	return block_read_full_folio(folio, cominix_get_block);
}

int cominix_prepare_chunk(struct folio *folio, loff_t pos, unsigned len)
{
	return __block_write_begin(folio, pos, len, cominix_get_block);
}

static void cominix_write_failed(struct address_space *mapping, loff_t to)
{
	struct inode *inode = mapping->host;

	if (to > inode->i_size) {
		truncate_pagecache(inode, inode->i_size);
		cominix_truncate(inode);
	}
}

static int cominix_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len,
			struct folio **foliop, void **fsdata)
{
	int ret;

	ret = block_write_begin(mapping, pos, len, foliop, cominix_get_block);
	if (unlikely(ret))
		cominix_write_failed(mapping, pos + len);

	return ret;
}

static sector_t cominix_bmap(struct address_space *mapping, sector_t block)
{
	return generic_block_bmap(mapping,block,cominix_get_block);
}

static const struct address_space_operations cominix_aops = {
	.dirty_folio	= block_dirty_folio,
	.invalidate_folio = block_invalidate_folio,
	.read_folio = cominix_read_folio,
	.writepages = cominix_writepages,
	.write_begin = cominix_write_begin,
	.write_end = generic_write_end,
	.migrate_folio = buffer_migrate_folio,
	.bmap = cominix_bmap,
	.direct_IO = noop_direct_IO
};

static const struct inode_operations cominix_symlink_inode_operations = {
	.get_link	= page_get_link,
	.getattr	= cominix_getattr,
};

void cominix_set_inode(struct inode *inode, dev_t rdev)
{
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &cominix_file_inode_operations;
		inode->i_fop = &cominix_file_operations;
		inode->i_mapping->a_ops = &cominix_aops;
		if (inode_is_chunked(inode)) {
			inode->i_fop = &chunked_file_operations;
			inode->i_mapping->a_ops = &empty_aops;
		}
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &cominix_dir_inode_operations;
		inode->i_fop = &cominix_dir_operations;
		inode->i_mapping->a_ops = &cominix_aops;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &cominix_symlink_inode_operations;
		inode_nohighmem(inode);
		inode->i_mapping->a_ops = &cominix_aops;
	} else
		init_special_inode(inode, inode->i_mode, rdev);
}


/*
 * The cominix V2 function to read an inode.
 */
static struct inode *V2_cominix_iget(struct inode *inode)
{
	struct buffer_head * bh;
	struct cominix2_inode * raw_inode;
	struct cominix_inode_info *cominix_inode = cominix_i(inode);
	int i;

	raw_inode = cominix_V2_raw_inode(inode->i_sb, inode->i_ino, &bh);
	if (!raw_inode) {
		iget_failed(inode);
		return ERR_PTR(-EIO);
	}
	if (raw_inode->i_nlinks == 0) {
		printk("MINIX-fs: deleted inode referenced: %lu\n",
		       inode->i_ino);
		brelse(bh);
		iget_failed(inode);
		return ERR_PTR(-ESTALE);
	}
	inode->i_mode = raw_inode->i_mode;
	i_uid_write(inode, raw_inode->i_uid);
	i_gid_write(inode, raw_inode->i_gid);
	set_nlink(inode, raw_inode->i_nlinks);
	inode->i_size = raw_inode->i_size;
	inode_set_mtime(inode, raw_inode->i_mtime, 0);
	inode_set_atime(inode, raw_inode->i_atime, 0);
	inode_set_ctime(inode, raw_inode->i_ctime, 0);
	inode->i_blocks = 0;
	for (i = 0; i < 10; i++)
		cominix_inode->u.i2_data[i] = raw_inode->i_zone[i];
	cominix_set_inode(inode, old_decode_dev(raw_inode->i_zone[0]));
	brelse(bh);
	unlock_new_inode(inode);
	return inode;
}

/*
 * The global function to read an inode.
 */
struct inode *cominix_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;

	return V2_cominix_iget(inode);
}

/*
 * The cominix V2 function to synchronize an inode.
 */
static struct buffer_head * V2_cominix_update_inode(struct inode * inode)
{
	struct buffer_head * bh;
	struct cominix2_inode * raw_inode;
	struct cominix_inode_info *cominix_inode = cominix_i(inode);
	int i;

	raw_inode = cominix_V2_raw_inode(inode->i_sb, inode->i_ino, &bh);
	if (!raw_inode)
		return NULL;
	raw_inode->i_mode = inode->i_mode;
	raw_inode->i_uid = fs_high2lowuid(i_uid_read(inode));
	raw_inode->i_gid = fs_high2lowgid(i_gid_read(inode));
	raw_inode->i_nlinks = inode->i_nlink;
	raw_inode->i_size = inode->i_size;
	raw_inode->i_mtime = inode_get_mtime_sec(inode);
	raw_inode->i_atime = inode_get_atime_sec(inode);
	raw_inode->i_ctime = inode_get_ctime_sec(inode);
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		raw_inode->i_zone[0] = old_encode_dev(inode->i_rdev);
	else for (i = 0; i < 10; i++)
		raw_inode->i_zone[i] = cominix_inode->u.i2_data[i];
	mark_buffer_dirty(bh);
	return bh;
}
int cominix_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	int err = 0;
	struct buffer_head *bh;

	bh = V2_cominix_update_inode(inode);
	if (!bh)
		return -EIO;
	if (wbc->sync_mode == WB_SYNC_ALL && buffer_dirty(bh)) {
		sync_dirty_buffer(bh);
		if (buffer_req(bh) && !buffer_uptodate(bh)) {
			printk("IO error syncing cominix inode [%s:%08lx]\n",
				inode->i_sb->s_id, inode->i_ino);
			err = -EIO;
		}
	}
	brelse (bh);
	return err;
}

int cominix_getattr(struct mnt_idmap *idmap, const struct path *path,
		  struct kstat *stat, u32 request_mask, unsigned int flags)
{
	struct super_block *sb = path->dentry->d_sb;
	struct inode *inode = d_inode(path->dentry);

	generic_fillattr(&nop_mnt_idmap, request_mask, inode, stat);
	stat->blocks = (sb->s_blocksize / 512) * V2_cominix_blocks(stat->size, sb);
	stat->blksize = sb->s_blocksize;
	return 0;
}

/*
 * The function that is called for file truncation.
 */
void cominix_truncate(struct inode * inode)
{
	if (inode_is_chunked(inode)) {
		/* if the size increases then that will
		cause a kernel panic when the ll_search_left
		returns a NULL, so I don't need to worry about it.
		(The BUG() is already happening I mean) */
		/* if the size decreases then I'm already kind of handling that with the pos,
		except that the read might return a little bit more, but its an odd edge case
		that I'm not worried about. */
		WARN_ON(inode->i_size);
		printk("Chunked inode is being removed. TODO: delete the linked list in the node.\n");
		return;
	}
	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode)))
		return;
	V2_cominix_truncate(inode);
}

static struct dentry *cominix_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, cominix_fill_super);
}

static void info_then_kill(struct super_block *sb)
{
	//for (u64 i = 0; i < 100; i++){
	//	blockoff_t search = chunk_search_hashtable(sb, i);	
	//	if (search) {
	//		printk("chunk hash %lld found at %lld", i, search);
	//		struct buffer_head *bh = NULL;
	//		ssize_t bytes_left = 0;
	//		struct chunk *chunk 
	//			= load_blockoff(sb, search, &bytes_left, &bh);
	//		if (chunk->hash != i) {
	//			printk("hash doesn't match! ondisk hash is %lld\n", chunk->hash);
	//		}
	//		printk("size is %d\n", chunk->length);
	//		printk("next is %lld\n", chunk->next);
	//		brelse(bh);
	//	}
	//}
	kill_block_super(sb);
}

struct file_system_type cominix_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "cominix",
	.mount		= cominix_mount,
	//.kill_sb	= kill_block_super,
	.kill_sb	= info_then_kill,
	.fs_flags	= FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("cominix");

static int __init init_cominix_fs(void)
{
	cminix_proc_init();
	int err = init_inodecache();
	if (err)
		goto out1;
	err = register_filesystem(&cominix_fs_type);
	if (err)
		goto out;
	return 0;
out:
	destroy_inodecache();
out1:
	return err;
}

static void __exit exit_cominix_fs(void)
{
	cminix_proc_clean();
        unregister_filesystem(&cominix_fs_type);
	destroy_inodecache();
}

module_init(init_cominix_fs)
module_exit(exit_cominix_fs)
MODULE_LICENSE("GPL");


