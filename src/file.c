// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/fs/cominix/file.c
 *
 *  Copyright (C) 1991, 1992 Linus Torvalds
 *
 *  cominix regular file handling primitives
 */

#include "cominix.h"
#include "chunk_handler.h"
#include "cdc.h"
#include "md5_wrapper.h"
#include <linux/writeback.h>
#include <linux/buffer_head.h>
#include "linked_list.h"

static block_t get_list_head(struct inode *inode)
{
	return i_data(inode)[1];
}

void dump_head(struct super_block *sb, block_t block);
static ssize_t 
chunked_file_read(struct file *filp,
		char __user *buf,
		size_t count,
		loff_t *pos)
{
	//printk("pos %lld < size %lld", *pos, filp->f_inode->i_size);
	if (*pos >= filp->f_inode->i_size)
		return 0;
	struct inode *inode = filp->f_inode;
	struct super_block *sb = inode->i_sb;
	BUG_ON(!inode_is_chunked(inode));

	ssize_t found_pos = 0;
	//dump_head(sb, zones[1]);
	struct chunk_entry chunk 
		= ll_search_left(sb, get_list_head(inode), *pos, &found_pos);
	BUG_ON(!chunk.location);
	BUG_ON(!chunk.size);
	off_t in_chunk_offset = *pos - found_pos;
	int ret = chunk_copy_into_buffer(sb,
			&chunk, buf, count, in_chunk_offset);
	if (ret > 0)
		*pos += ret;
	return ret;
}


//static int check_filp(struct file *filp)
//{
//	fmode_t mode = filp->f_mode & (FMODE_READ | FMODE_WRITE);	
//	switch(mode) {
//	case FMODE_WRITE:
//		printk("FILE IS WRITE ONLY\n");
//		break;
//	case FMODE_READ:
//		printk("FILE IS READ ONLY\n");
//		break;
//	case (FMODE_READ|FMODE_WRITE):
//		printk("FILE IS R/W\n");
//		break;
//	default:
//		BUG_ON(1);
//	}
//	return 0;
//}

static void print_hash(char *digest)
{
	printk(KERN_INFO "hash: ");
	for (int i = 0; i < 16; i++)
		printk(KERN_CONT "%x ", digest[i]);
	printk(KERN_CONT "\n");//flush
}

void dump_head(struct super_block *sb, block_t block)
{
#if 0
	struct buffer_head *bh = sb_bread(sb, block);
	printk("dumping block %x", block);
	for (int i = 0; i < 20; i++) {
		struct chunk_entry chunk = ((struct chunk_entry *)bh->b_data)[i];
		printk("loc %llx size %lld\n", chunk.location, chunk.size);
	}
	brelse(bh);
#endif
}

static void print_heap_info(struct super_block *sb)
{
	struct cominix_sb_info *msi = cominix_sb(sb);
	printk("Heap size is %lld KB.\n", (msi->heap_brk - (msi->hashtable + msi->hashtable_size)) >> 10);
	long free_blocks = cominix_count_free_blocks(sb);
	printk("%d free blocks (%lld kb).\n", free_blocks, free_blocks * sb->s_blocksize >> 10);
}


static int
chunk_and_replace(struct file *filp)
{
	struct super_block *sb = filp->f_inode->i_sb;

	block_t head = ll_alloc_new_block(sb);
	zero_out_block(sb, head);
	block_t end = head;
	ssize_t ll_size = 0;

	ssize_t chunk_size = 0; 
	filp->f_pos = 0;
	while (filp->f_pos < filp->f_inode->i_size) {
		chunk_size = cdc_get_chunk_size(filp, filp->f_pos);
		if (WARN_ON(chunk_size < 0)){
			printk("chunk_size was %ld\n", chunk_size);
			return -1;
		}
		BUG_ON(chunk_size == 0);
		//printk("DO YOU EVEN SEE ME?\n");
		char *buf = kmalloc(chunk_size, GFP_KERNEL);
		BUG_ON(!buf || IS_ERR(buf));
		ssize_t read = 0;
		ssize_t to_read = chunk_size;
		ssize_t pos = 0;
		while (to_read > 0) {
			char *cursor = &buf[pos];
			read = kernel_read(filp, cursor, chunk_size, &filp->f_pos);
			if (read < 0) {
				printk("ERROR OF READ IS %ld\n", -read);
				return -1;
			}
			to_read -= read;
			pos += read;
		}
		BUG_ON(pos != chunk_size);
		char digest[16] = {0};
		int ret = md5_hash(buf, chunk_size, digest);
		BUG_ON(ret);
		struct chunk metadata = {
			.hash = *(u64 *)digest, //throws away lower half
			.length = (u32)chunk_size,
			.refcount = 0,
			.flags = 0,
			.next = 0,
		};
		blockoff_t location = chunk_search_hashtable(sb, metadata.hash);
		if (location) {
			printk("COLLISION");
			print_hash(digest);
		} else
			location = chunk_fill_hashtable(sb, &metadata, buf);
		BUG_ON(!location);
		BUG_ON(!chunk_size);

		ll_append(sb, &end, &ll_size, (struct chunk_entry){location, chunk_size});
		dump_head(sb, end);
	}
	BUG_ON(filp->f_pos != filp->f_inode->i_size);
	loff_t fsize = filp->f_inode->i_size;
	print_heap_info(sb);

	//truncate doesn't mean remove all data, it means change to fit the f_size
	//(increasing too)
	filp->f_inode->i_size = 0; //IMPORTANT!!
	cominix_truncate(filp->f_inode); 
	filp->f_inode->i_size = fsize;

	print_heap_info(sb);
	switch_inode_to_chunked(filp->f_inode);
	u32 *zones = i_data(filp->f_inode);

	BUG_ON(head >= 1L << 32);
	BUG_ON(ll_size >= 1L << 32);
	BUG_ON(end >= 1L << 32);
	zones[1] = (u32) head;
	zones[2] = (u32) ll_size;
	zones[3] = (u32) end;
	
	struct writeback_control wbc;
	wbc.sync_mode = WB_SYNC_NONE;
	int ret = cominix_write_inode(filp->f_inode, &wbc);
	BUG_ON(ret);

	//sets the file operations
	cominix_set_inode(filp->f_inode, 0);

	print_heap_info(sb);
	return 0;

}

static ssize_t fail_write (struct file *filp, 
		    const char __user *buf, 
		    size_t count, 
		    loff_t *pos)
{
	char namebuf[] = "!FILENAME ERROR!";
	int buflen = sizeof(namebuf)/sizeof(char);
	char *name = NULL;//d_path(&filp->f_path, namebuf, buflen);
	int err = -EIO;
	if (!name)
		name = namebuf;
	printk(KERN_WARNING "MINIX: Attempted write of %ld bytes to %s at offset %lld\n", count, name, *pos);

	char *mybuf = kmalloc(count+1, GFP_KERNEL);
	if (!mybuf)
		return err;
	int ret = copy_from_user(mybuf, buf, count);
	if (ret)
		return err;
	mybuf[count] = 0;
	printk(KERN_WARNING "MINIX: Contents of write: '%s'", mybuf);
	kfree(mybuf);
	return err;
}

const struct inode_operations cominix_file_inode_operations = {};
const struct file_operations cominix_file_operations = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter, 
	.write_iter	= generic_file_write_iter, //change this to check and write?
	//.mmap		= generic_file_mmap, //same?
	.fsync		= generic_file_fsync,
	//.splice_read	= filemap_splice_read,
};
const struct file_operations chunked_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= chunked_file_read,
	//.write		= fail_write,
};

#include <linux/proc_fs.h>

static 
int cminix_proc_open(struct inode *inode, struct file *file)
{
	printk("CMINIX PROC OPENED\n");
	return 0;
}

static 
ssize_t cminix_proc_write(struct file *proc_file,
		const char __user *buf, size_t count, loff_t *pos)
{
	char *buf_copy = kmalloc(count+1, GFP_KERNEL);
	if (!buf_copy || IS_ERR(buf_copy))
		return -1;
	int ret = copy_from_user(buf_copy, buf, count);
	if (WARN_ON(ret))
		return -1;
	buf_copy[count] = '\0';
	if (buf_copy[count - 1] == '\n')
		buf_copy[count - 1] = '\0';
	printk("CMINIX PROC WRITE: %ld bytes were written to proc. CONTENTS:", count);
	printk("%s\n", buf_copy);
	printk("DONE.\n");
	
	struct file *filp = filp_open(buf_copy, O_RDWR, 0);
	if (!filp || IS_ERR(filp)) {
		printk("Couldn't open file '%s'\n", buf_copy);
		goto open_err;
	}
	if (filp->f_inode->i_sb->s_type != &cominix_fs_type) {
		printk("Chunking a file from another file system is not implemented.");
		printk("(Other file system is %s)\n", filp->f_inode->i_sb->s_type->name);
		goto general_err;
	}
	if (!S_ISREG(filp->f_inode->i_mode)) {
		printk("Attempted to chunk non-file. (Was it a directory?)\n");
		goto general_err;
	}
	inode_lock(filp->f_inode);
	if (filp->f_inode->i_fop != &cominix_file_operations) {
		if (filp->f_inode->i_fop == &chunked_file_operations)
			printk("'%s' has already been chunked.\n", buf_copy);
		else
			printk("ERROR?! Unknown file ops pointer.\n");
		goto general_err;
	}
	printk("Proceeding with chunking '%s'.\n", buf_copy);
	kfree(buf_copy);
	chunk_and_replace(filp);
	inode_unlock(filp->f_inode);

	filp_close(filp, NULL);
	return count;
general_err:
	filp_close(filp, NULL);
open_err:
	kfree(buf_copy);
	return -1;
}

static const struct proc_ops cminix_proc_ops = {
	.proc_open = cminix_proc_open,
	.proc_write = cminix_proc_write,
};

void __init cminix_proc_init(void)
{
	//i could make a separate dir for each bdev
	struct proc_dir_entry *base = proc_mkdir("fs/cominix", NULL);
	if (!base)
		return;
	proc_create("chunker", 0, base, &cminix_proc_ops);
}

void cminix_proc_clean(void);
void cminix_proc_clean(void)
{
	remove_proc_subtree("fs/cominix", NULL);
}

