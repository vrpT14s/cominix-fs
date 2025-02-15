This is an extension to the minix file system adding CDC-based deduplication. This tries to remove duplicate data between files. Check the other readme for more technical information (it explains the source code too). As an example, here on this 80 MB disk image we have 5 files each with more than 30 MB, so more than 150 MB in total. The trick is that 4 of these files are quite similar, so we end up storing their common data only once. You can interact with this like any other filesystem but there are three ways that you can notice it's different. Firstly you have to preallocate some space to hold chunks. For this specific disk I chose 40 MB for normal files and 40 MB for chunks. A file can't be half in the chunked area and half in the normal area, so that means even though the disk is 80 MB the largest file you can store is 40 MB (well, slightly less than that when accounting for metadata). Secondly, the chunked files are read-only. You can remove them but their chunks remain in the chunked area so you can have some garbage remaining there. Thirdly, to make a file chunked you have to run a special command (specifically you send the full path of the file to a proc entry). Chunking doesn't happen automatically. All three of these issues were simplifications to make it easy enough for me to implement.

# The demo disk
```console
$ uname -r #kernel version
6.12.10-arch1-1
$ make mount
(...)
$ cd mnt
$ ls
1.txt  2.txt  3.txt  4.txt  a.txt
$ ls -lah
total 5.0K
drwxr-xr-x 2 user user  512 Feb 15 17:11 .
drwxr-xr-x 5 user user 4.0K Feb 15 21:33 ..
-r--r--r-- 1 user user  31M Feb 15 14:42 1.txt
-r--r--r-- 1 user user  30M Feb 15 14:45 2.txt
-r--r--r-- 1 user user  30M Feb 15 14:50 3.txt
-r--r--r-- 1 user user  34M Feb 15 14:53 4.txt
-rw-r--r-- 1 user user  37M Feb 15 14:54 a.txt
$ du --apparent-size -h #the sum of the file sizes
159M    .
$ lsblk /dev/loop0
NAME  MAJ:MIN RM SIZE RO TYPE MOUNTPOINTS
loop0   7:0    0  80M  0 loop /home/user/cominix-fs/mnt
$ diff 1.txt 2.txt
0a1
> first difference
65536a65538
> second difference
196607a196610
> 30% diff
321126a321130
> 49 = 7*7
465306,465307d465309
< 0719990 37a5 1662 11be 4b95 9006 b746 d665 48fc
< 07199a0 721d bba2 98c9 b6b0 4cc3 7c78 7e8e 0dc3
589825,589826d589826
< 0900000 9a29 18f4 1081 7cc0 27a3 3a3e d895 c74d
< 0900010 9dda 09ef 4c4b ef2d 5f19 c4bf e561 7781
622593a622594
> almost at the end (95%)
$ diff 1.txt 2.txt | wc -c
387
$ diff 1.txt 3.txt | wc -c
464
$ diff 1.txt 4.txt | wc -c
3276831
$ diff 1.txt a.txt | wc -c
72089642
```

1.txt and a.txt were just 30 megabytes of some random data. I created them like this.
```console
$ head -c 12m /dev/random | hexdump > 1.txt
(...)
$ sudo ../chunk 1.txt
(...)
$ cat 1.txt > 2.txt 
$ vim 2.txt
$ sudo ../chunk 2.txt 
(...)
$ #repeat the last three lines for 3 and 4 too
(...)
$ head -c 12m /dev/random | hexdump > a.txt
```
The other three are just copies with some random messages I added at random positions. I also appended a few megabytes of random hex to 4.txt.


# Remaining issues
* The three issues I mentioned in the last paragraph
* Minix doesn't implement r/w/x permissions (i.e. ACL's) and I don't either and this can be a bit troublesome in some places. To get around this I set the chunked files to read-only by hand in my chunk shell command. You can still write to these files accidently if you're root though. This can truncate the file, which won't remove any of the data from disk but will set the ```i_size``` parameter in the inode to 0, which makes it look like it's empty when you read from it.
* In general chunking is slower than it needs to be. In one place I read a file a byte at a time. We don't have a context switch since it's a kernel calling another kernel function but it's still wasteful. I did it this way because if I made it too complicated at the beginning it would be hard to fix the mistakes I was making along the way.
* The general I/O efficiency is bad. I use buffer heads because they're very simple and that's what Minix-fs used. It would be nice to use bio's instead and it would actually simplify things when I'm reading and writing to my chunk area. I'd be interested to read what iomap is about but I don't think I know enough about memory management and the page cache yet. It's pretty hard to find documentation about it too.
* There's almost no error handling. If you chunk too many files, it just ```BUG()```'s and causes a kernel panic. It doesn't hurt your system but you will have to reboot because you can't unmount normally because the process never closes the file it was working on, so the system will always think that mountpoint is busy.
* Similarly there's almost no configuration. Currently the 40 MB value is completely fixed. Well there is a way you can change it by editing the disk before you mount it the first time, with a userspace program, but I haven't made that program yet. I got around making my own ```mkfs.minix``` userspace program with a slight hack.
* I need to check the locking more. I haven't thought about it enough to see if it's right and I've definitely not tested it. Since the chunks are immutable once written I don't need to lock anything for that. I have a mutex for increasing the heap size (since it can only increase it acts like a stack) but I think a spinlock would be better there. I grab the write lock for the inode when I want to chunk it but I'm not sure exactly if that's how you're supposed to use it. I saw how the writing was happening for the generic function minix was using and they did grab that lock and release it so it's probably right but I'd like to check. Also, if you mount multiple cominix filesystems at the same time, the locks are all shared because they're globals. In my defense, that's how also minix module did it. It wouldn't be hard to make separate locks for each superblock but it'd increase the complexity enough for me that it's not worth it. This might be able to cause a deadlock in extremely edge cases. I don't think it can but I'm not entirely sure.
* There might be a better hash than md5. I just used it because it was somewhat fast and it's already present in the kernel, so I didn't need to bring it in myself.

These are all the overarching problems with this program that I can think of. Well, maybe a last point is that I have hacks so I didn't need to make my own mkfs, and break (limited) compatibility with minix. For example, instead of having some kind of flag bit, I just set the first block ptr in the inode to be -1, and the last one too, so I know if the kernel ever tries to treat it like a normal file it'll have a problem. Well, technically if your disk is large enough, specifically 2^32 - 1 blocks large, then yes that'd be a problem. To get around that I had initially set it to 1 since I know no file would be pointing to the superblock (the superblock is always the first block, after the zeroeth block being the boot block). The problem was that if I forgot to check if it's 1, then the kernel might end up writing file data to my superblock and corrupting it. That did happen once which was why I changed it.

Using a procfs entry might be kind of a hack. I was initially considering using attributes somehow but that's pretty confusing, and is also a hack. One problem with using procfs is you'll need to have root privileges to write data to it. I think I can just change the permission bits of it though and it'll work. I don't think sysfs was meant for something like this. I want something like ioctl, really. Apparently ext4 has its own ioctl so that might be a better way to do it. One reason I avoided it was that you'd have to write and compile a userspace program because you can't directly ioctl in your shell (with default shell programs I mean), while with the proc entry you just have to write to it.

# Previous mistakes/bugs
This was pretty interesting so I've been making a list of these. To my knowledge there aren't any bugs left in the program. There are some untreated error scenarios where I cause a BUG() on purpose, that isn't what I'm referring to.

-- I'll write this later

# This project moving on
I feel happy about where this is right now (Well, specifically that it has no obvious bugs). I felt like I learnt about design and complexity from it, and obviously some details about the kernel. Initially I was trying to optimize everything and that's understandable, but I think the focus should be on designing it so that it can be upgraded and optimized later, where we replace the flesh and the muscles but keep the skeleton and the relative placements constant. In reality you make mistakes and misunderstand things so if you try to optimize from the beginning, debugging will be impossible, because you have no skeleton to work on top of. Of course making this kind of structure does limit the possible communication and efficiencies that can be made between organs, but I'd rather start with an overly rigid skeleton and break bones and make exceptions when necessary instead of starting with nothing at all. But yes, I am saying here that I will probably not making the best program I could theoretically be making. 

I probably won't do anything with this except for fixing bugs if I find any. I might come back if I want to learn more about memory management.

Also, I forgot to mention this, but there are definitely lots of ways to make the source code easier to understand. As it stands it's hard to read, and I comment out some sections that really I should have deleted, and didn't bother to make a linked_list.c file and just did everything in the header. Some places are slightly wrong but doesn't affect the compiled code. I may or may not fix it, I'm not sure. I think the reason my code is hard to read is because I haven't read enough of other people's source code to understand what's easy to understand and what's not. So I won't fix things until I'm more experienced with that.
