#include "cominix.h"
#include "chunk_handler.h"
#include <linux/string.h>
#include <linux/buffer_head.h>
#include <linux/sched.h>

/* TO CONSIDER
- i can add a hashtable lookup function that only looks up the literal array
	(instead of also searching through the linked list)
	since that code is common in a few places.
*/

static DEFINE_MUTEX(brk_lock);
static DEFINE_MUTEX(chunk_edge_write_lock);

static block_t block_no(struct super_block *sb, blockoff_t off)
{
	u8 log = sb->s_blocksize_bits;
	return off >> log;
}

static sector_t sector_no(struct super_block *sb, blockoff_t off)
{
	return off >> 9;
}

static u64 inblock_offset(struct super_block *sb, blockoff_t off)
{
	u8 log = sb->s_blocksize_bits;
	BUG_ON(log > 30);
	return off & ((1L << log) - 1);
}

static void update_brk_on_disk(struct super_block *sb, ssize_t new_brk)
{
	struct buffer_head *bh = load_block(sb, cominix_sb(sb)->extra_sb_location);
	struct cminix_extra_super_block *esb = (void *)bh->b_data;
	esb->heap_brk = new_brk;
	mark_buffer_dirty(bh);
	brelse(bh);
}

static blockoff_t chunk_alloc(struct super_block *sb, ssize_t size)
{
	BUG_ON(size <= 0);
	mutex_lock(&brk_lock);

	blockoff_t *brk = &cominix_sb(sb)->heap_brk;
	u8 log = sb->s_blocksize_bits;

	//it's fine that that we don't release the lock
	//because brk is in an invalid state anyway
	BUG_ON(*brk <= cominix_sb(sb)->s_nzones << log);

	//so the chunk metadata doesnt straddle a boundary
	if (block_no(sb, *brk) != block_no(sb, *brk + sizeof(struct chunk_head) - 1))
		*brk = (block_no(sb, *brk) + 1) << log;
	blockoff_t new = *brk;
	*brk += sizeof(struct chunk_head) + size;
	
	//printk("break increased by %lld kb, is now at %lld mb %lld kb\n", size >> 10, *brk >> 20, (*brk >> 10) & ((1<<10)-1));
	blockoff_t max_brk = cominix_sb(sb)->max_brk;
	//printk("max break is %lld mb %lld kb\n", max_brk >> 20, (max_brk >> 10) & ((1<<10)-1));
	if (*brk >= max_brk) {
		printk("Heap ran out of space. Giving up.\n");
		BUG();
	}

	mutex_unlock(&brk_lock);

	update_brk_on_disk(sb, *brk);
	return new;
}

void *load_blockoff(struct super_block *sb, blockoff_t off, ssize_t *bytes_left, struct buffer_head **bh)
{
	BUG_ON(!off);
	*bh = NULL;
	*bh = sb_bread(sb, block_no(sb, off));
	if (!*bh || IS_ERR(*bh)) {
		printk("UNABLE TO LOAD block %llx\n", block_no(sb, off));
		BUG();
	}
	void *ptr = (*bh)->b_data;
	BUG_ON(!ptr || IS_ERR(ptr));
	ptr += inblock_offset(sb, off);
	*bytes_left = sb->s_blocksize - inblock_offset(sb, off);
	return ptr;
}

static u64 hashtable_hash(struct super_block *sb, u64 chunk_hash)
{
	return chunk_hash % (cominix_sb(sb)->hashtable_size / sizeof(blockoff_t));
}

blockoff_t chunk_search_hashtable(struct super_block *sb, u64 chunk_hash)
{
	struct cominix_sb_info *msi = cominix_sb(sb);
	u64 index = hashtable_hash(sb, chunk_hash);	
	blockoff_t off = msi->hashtable + index*sizeof(blockoff_t);
	ssize_t bytes_left = 0;
	struct buffer_head *bh = NULL;
	blockoff_t *hash_table_entry = load_blockoff(sb, off, &bytes_left, &bh);
	//printk("location location %llx\n", off);
	blockoff_t chunk_location = 0;

	BUG_ON(bytes_left < sizeof(blockoff_t));
	BUG_ON(IS_ERR(hash_table_entry));
	chunk_location = *hash_table_entry;
	brelse(bh);
	while (chunk_location) {
		printk("INSPECTING CHUNK %llx", chunk_location);
		struct chunk *chunk = load_blockoff(sb, chunk_location, &bytes_left, &bh);
		//i can check length here as well if i'd like
		if (chunk->hash == chunk_hash) {
			brelse(bh);
			return chunk_location;
		}
		chunk_location = chunk->next;
		brelse(bh);
	}
	//printk("Done\n");
	return 0;
}

//releases *bh and replaces it with the next block
static inline char *get_next_block(struct super_block *sb, struct buffer_head **bh, int dirty)
{
	block_t next_blockno = (*bh)->b_blocknr + 1;
	u64 off = inblock_offset(sb, (*bh)->b_size);
	if (off)
		printk("why off not zero? off = %llx; bsize %lx and sbsize %lx and bits %lx", off, (*bh)->b_size, sb->s_blocksize, 1L << sb->s_blocksize_bits);
	if (dirty) 
		mark_buffer_dirty(*bh);
	brelse(*bh);
	*bh = sb_bread(sb, next_blockno);
	BUG_ON(!*bh);
	return (*bh)->b_data;
}

//if data null then fill with zeroes
static int write_data_storage(struct super_block *sb, blockoff_t storage, char *data, ssize_t length)
{
	ssize_t remaining = length;
	ssize_t bytes_left = 0;
	struct buffer_head *bh = NULL;
	char *first_block = load_blockoff(sb, storage, &bytes_left, &bh);

	char *cursor = data;
	ssize_t to_write = min(bytes_left, remaining);
	mutex_lock(&chunk_edge_write_lock);
	if (data) 
		memcpy(first_block, cursor, to_write);
	else
		memset(first_block, 0, to_write);
	mutex_unlock(&chunk_edge_write_lock);
	remaining -= to_write;
	cursor += to_write;
	while(remaining > 0) {	
		char *next_block = get_next_block(sb, &bh, 1); //dirty
		to_write = min((ssize_t)sb->s_blocksize, remaining);
		if (to_write < sb->s_blocksize)
			mutex_lock(&chunk_edge_write_lock);
		if (data)
			memcpy(next_block, cursor, to_write);
		else
			memset(next_block, 0, to_write);
		if (to_write < sb->s_blocksize)
			mutex_unlock(&chunk_edge_write_lock);
		remaining -= to_write;
		cursor += to_write;
	}
	mark_buffer_dirty(bh);
	brelse(bh);
	return 0;
}

static int read_data_storage(struct super_block *sb, blockoff_t storage, char __user *data, ssize_t length)
{
	BUG_ON(!data);
	ssize_t remaining = length;
	ssize_t bytes_left = 0;
	struct buffer_head *bh = NULL;
	char *first_block = load_blockoff(sb, storage, &bytes_left, &bh);

	char __user *cursor = data;
	ssize_t to_read = min(bytes_left, remaining);
	int ret = copy_to_user(cursor, first_block, to_read);
	BUG_ON(ret);
	remaining -= to_read;
	cursor += to_read;
	while(remaining > 0) {	
		char *next_block = get_next_block(sb, &bh, 0); //not dirty
		to_read = min((ssize_t)sb->s_blocksize, remaining);
		int ret = copy_to_user(cursor, next_block, to_read);
		if (ret) {
			printk("bytes not transferred =  %d", ret);
			BUG_ON(ret);
		}
		remaining -= to_read;
		cursor += to_read;
	}
	brelse(bh);
	return 0;
}

static int copy_chunk_into_storage(struct super_block *sb, blockoff_t storage, struct chunk *metadata, char *data)
{
	write_data_storage(sb, storage, (char*)metadata, sizeof(struct chunk_head));
	storage += sizeof(struct chunk_head);
	write_data_storage(sb, storage, data, metadata->length);
	return 0;
}

//really this should be in the mkfs utility
int chunk_reset_hashtable(struct super_block *sb)
{
	struct cominix_sb_info *msi = cominix_sb(sb);
	msi->heap_brk = msi->hashtable + msi->hashtable_size;
	printk("HASHTABLE IS %lld kb\nAND SIZE IS %lld kb\n", msi->hashtable / 1024, msi->hashtable_size / 1024);
	//zeroes out the hashtable
	return write_data_storage(sb, msi->hashtable, NULL, msi->hashtable_size);
}

//has been searched beforehand so we know it isn't part of the table
blockoff_t chunk_fill_hashtable(struct super_block *sb, struct chunk *metadata, char *data)
{
	struct cominix_sb_info *msi = cominix_sb(sb);
	blockoff_t fresh_chunk = chunk_alloc(sb, metadata->length);
	u64 index = hashtable_hash(sb, metadata->hash);
	blockoff_t off = msi->hashtable + index*sizeof(blockoff_t);
	ssize_t bytes_left = 0;
	struct buffer_head *bh = NULL;

	blockoff_t *hash_table_entry = load_blockoff(sb, off, &bytes_left, &bh);
	metadata->next = *hash_table_entry;
	*hash_table_entry = fresh_chunk;
	mark_buffer_dirty(bh);
	brelse(bh);

	copy_chunk_into_storage(sb, fresh_chunk, metadata, data);
	
	return fresh_chunk;
}

int chunk_copy_into_buffer(struct super_block *sb, 
	struct chunk_entry *chunk, 
	char __user *buf, ssize_t count, off_t pos)
{
	//printk("CHUNK SIZE IS %lld kb (and %lld bytes)", chunk->size >> 10, chunk->size & ((1<<10) - 1));
	ssize_t to_read = min(count, (ssize_t)chunk->size - (ssize_t)pos);
	if (to_read <= 0)
		return 0;
	blockoff_t loc = chunk->location + sizeof(struct chunk_head) + pos;
	read_data_storage(sb, loc, buf, to_read);
	return to_read;
}

