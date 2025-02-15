obj-m += src/
module-objs += src/bitmap.o src/itree_v2.o src/namei.o src/file.o src/dir.o src/chunk_handler.o src/gear_table.o
kernel_version = "6.12.10-arch1-1"

disk=80megs.img
disksize=80 #in megabytes

all:
	make -C /lib/modules/$(kernel_version)/build M=$(PWD) modules
	mv src/cominix.ko .

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

insmod:
	make all
	sudo insmod cominix.ko

rmmod:
	sudo rmmod cominix
	make clean

#wipes any data previous on disk
mkfs:
	mkfs.minix -3 $(disk)

diskimage:
	dd if=/dev/zero of=$(disk) bs=1M count=$(disksize)
	make mkfs

mount:
	make insmod
	sudo mount -t cominix -o loop $(disk) mnt

umount:
	sudo umount mnt

#reinserts the module too
remount:
	make umount
	make rmmod
	make mount

