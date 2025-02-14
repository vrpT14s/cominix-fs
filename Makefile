obj-m += cominix.o
cominix-objs := bitmap.o itree_v2.o namei.o file.o dir.o chunk_handler.o gear_table.o inode.o
kernel_version = "6.12.10-arch1-1"

all:
	make -C /lib/modules/$(kernel_version)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
