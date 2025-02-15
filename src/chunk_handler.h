#include "cominix.h"
typedef u64 blockoff_t;
#define HASHTABLE_SIZE (32L << 10) //32 kb

extern blockoff_t hashtable;
extern blockoff_t global_brk;

//i use this to get the size...
struct chunk_head {
	u64 hash;
	u32 length;
	u16 refcount;
	u16 flags;
	blockoff_t next;
};

struct chunk {
	u64 hash;
	u32 length;
	u16 refcount;
	u16 flags;
	blockoff_t next;
	char data[];
};

int chunk_reset_hashtable(struct super_block *sb);
blockoff_t chunk_search_hashtable(struct super_block *sb, u64 chunk_hash);
//only fill if you know it isn't already in the table
blockoff_t chunk_fill_hashtable(struct super_block *sb, 
			struct chunk *metadata, char *data);

//pos is relative to the chunk
int chunk_copy_into_buffer(struct super_block *sb, 
	struct chunk_entry *chunk, 
	char *buf, ssize_t count, off_t pos);

void *load_blockoff(struct super_block *sb, blockoff_t off, ssize_t *bytes_left, struct buffer_head **bh);
