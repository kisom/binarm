BINARY ARMAGEDDON

If you have to use this, it's probably the end of the world as we know
it --- do you feel fine?

Commands are single lower-case letters, numbers are hexadecimal. Arguments
are separated by spaces, commands are terminated with spaces or
newlines. If the file doesn't exist, it can be created with the -!  <size>
flag. As with all other numbers in this program, this size is hexadecimal.

COMMANDS:

	e offset	Extend file +offset bytes.
	f offset size	Find a size sequence of bytes starting at
			offset. The maximum size is 32 bytes.
	i start		Insert a byte at start. This will create a hole
			at start, setting the byte to 0, and shift the
			remaining data to the right one byte.
	l		Print the current image size.
	q		Quit. This should be the first command you enter.
	r start offset	Read (and print) a hex dump of offset bytes from
			the file starting at start. If offset is 0, the
			remainder of the file (after start) is read. For
			example, `r 0 0` will read the entire file.
	s offset size	Scan for all instances of the size-sized
			fragment starting at offset.
	w start		Start writing to the file at start.


EXAMPLE:
	Don't try this at home.

	$ cat test.txt
	hello world
	$ ls -l test.txt
	-rw-rw-r-- 1 kyle kyle 12 Apr 22 22:28 test.txt
	$ ./binarm test.txt
	
	> r 0 0c 
	00000000 | 68 65 6c 6c 6f 20 77 6f  72 6c 64 0a 
	
	
	> e 08
	EXTENDED 8 BYTES
	
	> w 0b
	W> 21 0a 0a 48 49 21 0a
	WROTE 7 BYTE(S)
	
	> r 0 12 
	00000000 | 68 65 6c 6c 6f 20 77 6f  72 6c 64 21 0a 0a 48 49 
	00000010 | 21 0a 
	

	> f 2 3
	F> 77 6f 72

	FIND FRAG FROM 2 SIZE 3
	FOUND STARTING AT +4 (6)

	> s 0 1
	F> 6c
	SCAN FRAG FROM 0 SIZE 1
	FOUND STARTING AT +0 (0)
	FOUND STARTING AT +0 (1)
	FOUND STARTING AT +0 (3)
	FOUND STARTING AT +0 (7)
	NOT FOUND

	> q
	QUIT
	$ cat test.txt 
	hello world!
	
	HI!
	$ ls -l test.txt 
	-rw-rw-r-- 1 kyle kyle 20 Apr 22 22:32 test.txt
	$ 

There is no backspace. The only undo for a write is to start writing at
the same offset with the bytes you intended.

Alternatively, in the spirit of [1], armageddon may be unleashed on the
example client auth program. The major difference from the post is that
this example was built and run on an AMD64 machine ([1] uses a 32-bit
machine); accordingly, instead of the instruction 01442418, the target
instruction is 0145fc. The relevant instruction look up is found in [2].

First, build the (incorrect) client auth program:

	$ cat test1.c
	/*
	 * client-auth1.c
	 */
	
	#include <stdio.h>
	
	int
	main(void)
	{
	        int             i = 0;
	        unsigned        sum = 0xffff0000;
	        char            c;
	
	        while (0xa != (c = getchar())) {
	                i++;
	                sum += c * i;
	        }
	        printf("%010u\n", sum);
	}
	$ make test1 
	cc     test1.c   -o test1

Run it with the challenge input; this is the wrong input, but it's
useful to immediately compare the output of the modified version to
ensure something useful was changed.

	$ ./test1
	sptjpgzczykbgwogqmbrhsg
	4294931782

It's not strictly required, but it's useful to find main as a starting
point for exploring the program. It's mapped 0x40057d, which places it
at an offset of 0x57d in the image.

	$ readelf -s test1 | grep main$
	    61: 000000000040057d    75 FUNC    GLOBAL DEFAULT   13 main

Invoke binarm: find the desired 3-byte instruction starting from the
main offset in the image. It was found 24 bytes from the search point at
an absolute offset of 0x5a1 bytes in the image.

	$ ./binarm test1
	
	> f 57d 3
	F> 0145fc
	FIND FRAG FROM 57d SIZE 3
	FOUND STARTING AT +24 (5a1)
	
A visual confirmation shows the target instruction where expected; the
alignment isn't required, but it makes finding the instruction a little
easier.

	> r 570 64
	00000570 | e9 7b ff ff ff 0f 1f 00  e9 73 ff ff ff 55 48 89 
	00000580 | e5 48 83 ec 10 c7 45 f8  00 00 00 00 c7 45 fc 00 
	00000590 | 00 ff ff eb 0f 83 45 f8  01 0f be 45 f7 0f af 45 
	000005a0 | f8 01 45 fc e8 c7 fe ff  ff 88 45 f7 80 7d f7 0a 
	000005b0 | 75 e3 8b 45 fc 89 c6 bf  54 06 40 00 b8 00 00 00 
	000005c0 | 00 e8 8a fe ff ff c9 c3  0f 1f 84 00 00 00 00 00 
	000005d0 | 41 57 41 89 
	
The next step is to replace the single byte, a visual check on the
update, and exit.

	> w 5a1
	W> 29
	WROTE 1 BYTE(S)
	
	> r 5a0 32
	000005a0 | f8 29 45 fc e8 c7 fe ff  ff 88 45 f7 80 7d f7 0a 
	000005b0 | 75 e3 8b 45 fc 89 c6 bf  54 06 40 00 b8 00 00 00 
	000005c0 | 00 e8 8a fe ff ff c9 c3  0f 1f 84 00 00 00 00 00 
	000005d0 | 41 57 
	
	> q
	QUIT

The modified program can now be tested. A quick visual check shows the
output has changed, and the answer is the same answer from [1].

	$ ./test1                        
	sptjpgzczykbgwogqmbrhsg
	4294871738


[1] "Introduction to Patching Binaries with the HT Editor"
    http://kyleisom.net/blog/2013/10/19/intro-to-ht/
[2] http://ref.x86asm.net/coder64.html

