extern u64 gear_table[256];

ssize_t cdc_get_chunk_size(struct file *filp, loff_t pos);
ssize_t cdc_get_chunk_size(struct file *filp, loff_t pos)
{
	u64 MaskS = 0x0003590703530000LL;
	//u64 MaskA = 0x0003590703530000LL;
	u64 MaskL = 0x0003590703530000LL;
	u64 MinSize = 2LL << 10;
	u64 MaxSize = 64LL << 10;
	u64 NormalSize = 8LL << 10;
	u64 fp = 0; //fingerprint

	u32 i = MinSize;
	
	u64 n = filp->f_inode->i_size - pos;
	if (n <= MinSize)
		return n;

	if (n >= MaxSize)
		n = MaxSize;
	else if (n <= NormalSize)
		NormalSize = n;
	
	u8 b = 0;
	
	pos += i;
	for (; i < NormalSize; i++) {
		kernel_read(filp, &b, 1, &pos);
		fp = (fp << 1) + gear_table[b];
		if (!(fp & MaskS))
			return i;
	}

	for (; i < n; i++) {
		kernel_read(filp, &b, 1, &pos);
		fp = (fp << 1) + gear_table[b];
		if (!(fp & MaskL))
			return i;
	}
	return i;
}
