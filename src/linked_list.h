static
int entries_per_block(struct super_block *sb)
{
	return sb->s_blocksize / sizeof(struct chunk_entry) - 1;
}

static
block_t *ptr_next(struct super_block *sb, char *node)
{
	return (block_t *)(node + sb->s_blocksize) - 1;
}

static
block_t ll_alloc_new_block(struct super_block *sb)
{
	int new = cominix_new_block_sb(sb);
	BUG_ON(!new);
	return (block_t)new;
}

static
void zero_out_block(struct super_block *sb, block_t block)
{
	struct buffer_head *bh = load_block(sb, block);
	memset(bh->b_data, 0, sb->s_blocksize);
	brelse(bh);
}


int ll_append(struct super_block *sb, block_t *end, ssize_t *ll_size, struct chunk_entry new_entry);
int ll_append(struct super_block *sb, block_t *end, ssize_t *ll_size, struct chunk_entry new_entry)
{
	struct buffer_head *bh = load_block(sb, *end);
	int i = *ll_size % entries_per_block(sb);
	if (i == 0 && *ll_size != 0) {
		*end = ll_alloc_new_block(sb);

		*ptr_next(sb, bh->b_data) = *end;
		mark_buffer_dirty(bh);
		brelse(bh);

		bh = load_block(sb, *end);
		memset(bh->b_data, 0, sb->s_blocksize);
	}
	BUG_ON(!end || !bh);
	struct chunk_entry *arr = (void *)bh->b_data;
	arr[i] = new_entry;
	(*ll_size)++;

	mark_buffer_dirty(bh);
	brelse(bh);
	return 0;
}

struct chunk_entry ll_search_left(struct super_block *sb, block_t head, ssize_t pos, ssize_t *accum);
//should get tco'd or RIP my 8kb kernel stack
struct chunk_entry ll_search_left(struct super_block *sb, block_t head, ssize_t pos, ssize_t *accum)
{
	BUG_ON(pos < *accum);
	struct buffer_head *bh = load_block(sb, head);
	struct chunk_entry *arr = (void *)bh->b_data;
	for (int i = 0; i < entries_per_block(sb); i++) {
		if (arr[i].size == 0)
			break;
		//printk("%ld < %ld ?", pos, *accum + (ssize_t)arr[i].size);
		if (pos < *accum + (ssize_t)arr[i].size) {
			//printk("TRUE\n");
			brelse(bh);
			return arr[i];
		}
		//printk("iter %d: FALSE\n", i);
		*accum += arr[i].size;
	}
	block_t next = *ptr_next(sb, bh->b_data);
	brelse(bh);
	if (!next)
		return (struct chunk_entry) {0, 0}; //was too large
	return ll_search_left(sb, next, pos, accum);
}
