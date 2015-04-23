BINARY ARMAGEDDON

If you have to use this, it's probably the end of the world as we know
it --- do you feel fine?

Commands are single lower-case letters, numbers are hexadecimal. Arguments
are separated by spaces, commands are terminated with spaces or newlines.

COMMANDS:

	e offset	Extend file +offset bytes.
	q		Quit. This should be the first command you enter.
	r start offset	Read (and print) a hex dump of offset bytes from
			the file starting at start.
	w start		Start writing to the file at start.


EXAMPLE:
	Don't try this at home.

	$ cat test.txt
	hello world
	$ ls -l test.txt
	-rw-rw-r-- 1 kyle kyle 12 Apr 22 22:28 test.txt
	$ ./binarm test.txt
	
	> r 0 0c 
	68 65 6c 6c 6f 20 77 6f  72 6c 64 0a 
	
	
	> e 08
	EXTENDED 8 BYTES
	
	> w 0b
	W> 21 0a 0a 48 49 21 0a
	WROTE 7 BYTES
	
	> r 0 12 
	68 65 6c 6c 6f 20 77 6f  72 6c 64 21 0a 0a 48 49 
	21 0a 
	
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

