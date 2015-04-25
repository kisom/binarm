/*
 * Copyright (c) 2015 Kyle Isom <kyle@tyrfingr.is>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#define XOPEN_SOURCE	600


#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>


static int		tmod = 0;
static struct termios	torig;
const  size_t		fragmax = 32;


static int
stdin_nobuf(void)
{
	struct termios	t;

	if (tmod) {
		return 0;
	}

	if (-1 == tcgetattr(0, &t)) {
		perror("tcgetattr");
		return -1;
	}

	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	t.c_iflag &= ~ICRNL;
	t.c_lflag &= ~(ICANON | ECHO);
	t.c_lflag |= ISIG;

	if (-1 == tcsetattr(0, TCSAFLUSH, &t)) {
		return -1;
	}

	tmod = 1;
	return 0;
}


static int
stdin_buf(void)
{
	if (!tmod) {
		return 0;
	}

	if (-1 == tcsetattr(0, TCSAFLUSH, &torig)) {
		return -1;
	}

	tmod = 0;
	return 0;
}


static int
hexdigit(char c)
{
	if ((c >= 0x30) && (c <= 0x39)) {
		return 1;
	}

	if ((c >= 0x61) && (c <= 0x66)) {
		return 1;
	}

	return 0;
}

static int
readnum(size_t *n)
{
	char	buf[9];
	int	i = 0;
	char	ch;

	memset(buf, 0, 9);
	while (0x20 == (ch = fgetc(stdin))) printf("%c", ch);

	while (hexdigit(ch) && (i <= 8)) {
		printf("%c", ch);
		buf[i++] = ch;
		ch = fgetc(stdin);
	}

	if ((0x20 != ch) && (0x0a != ch) && (0x0d != ch)) {
		printf("%x\n", ch);
		return -1;
	}
	printf("%c", ch);

	*n = strtoul(buf, NULL, 0x10);
	return 0;
}


static void
printhex(uint8_t *buf, size_t l, size_t off)
{
	size_t	i;
	long    n = (long)l;

	printf("\n");
	l = 0;
	while (n >= 0) {
		printf("%08lx | ", off+l);
		for (i = 0; i < 8 && --n >= 0; i++) {
			printf("%02x ", buf[l++]);
		}

		if (n < 0) {
			break;
		}
		printf(" ");

		for (i = 0; i < 8 && --n >= 0; i++) {
			printf("%02x ", buf[l++]);
		}
		printf("\n");
	}

	if ((l % 16) != 0) {
		printf("\n");
	}
}


static uint8_t
gethexdigit(char c)
{
	uint8_t	v;

	if ((c >= 0x30) && (c <= 0x39)) {
		v = c - 0x30;
	} else if ((c >= 0x61) && (c <= 0x66)) {
		v = c - 0x61 + 0xa;
	}

	return v;
}


static void
writehex(uint8_t *buf, size_t lim)
{
	size_t	off = 0;
	char	prev;
	char	ch;
	uint8_t	i = 0;

	printf("\nW> ");

	ch = fgetc(stdin);
	while ((0xa != ch) && ((0x20 == ch) || hexdigit(ch))) {
		printf("%c", ch);
		if ((0x20 == ch) && (1 == i)) {
			printf("\nINVALID BYTE\n");
			break;
		} else if (0x20 != ch) {
			if (i) {
				ch = gethexdigit(ch);
				i = ((prev << 4) + ch);
				buf[off++] = i;
				i = 0;
			} else {
				prev = gethexdigit(ch);
				i++;
			}
		}

		if (off == lim) {
			break;
		}
		ch = fgetc(stdin);
	}

	printf("\nWROTE %lu BYTE(S)\n", (long unsigned)off);
}


static int
read_fragment(uint8_t *frag, size_t fraglen)
{
	size_t	i = 0, nib = 0;
	char	ch;
	char	prev;

	printf("\nF> ");
	memset(frag, 0, fragmax+1);
	while (0x20 == (ch = fgetc(stdin))) printf("%c", ch);

	while (((0x20 == ch) || hexdigit(ch)) && (i < fraglen)) {
		printf("%c", ch);
		if (0x20 != ch) {
			if (nib) {
				frag[i++] = (prev<<4)+gethexdigit(ch);
				nib = 0;
			} else {
				nib++;
				prev = gethexdigit(ch);
			}
		}
		ch = fgetc(stdin);
	}

	if (i != fraglen && ((0x20 != ch) && (0x0a != ch) && (0x0d != ch))) {
		memset(frag, 0, fragmax+1);
		printf("%c\n", ch);
		return -1;
	}


	printf("%c", ch);
	return 0;
}


static ssize_t
find_frag(uint8_t *f, size_t l, size_t foff, uint8_t *frag, size_t fraglen)
{
	size_t	i = 0;
	size_t	off = 0;
	size_t  start = 0;
	uint8_t	found = 0;

	while (!found && (i < l)) {
		if (frag[off] != f[i]) {
			off = 0;
		} else {
			if (off == (fraglen-1)) {
				found = 1;
				break;
			} else if (off == 0) {
				start = i;
			}
			off++;
		}

		i++;
	}

	if (found) {
		printf("FOUND STARTING AT +%lx (%lx)\n",
		    (long unsigned)start,
		    (long unsigned)start+foff);
		return start+foff;
	} else {
		printf("NOT FOUND\n");
		return -1;
	}
}

static int
processor(uint8_t *f, int fd, size_t l)
{
	uint8_t	frag[33];
	size_t	start, off;
	ssize_t	scan;
	int	rc = EXIT_SUCCESS;
	int	stop = 0;
	char	ch;

	if (stdin_nobuf()) {
		return EXIT_FAILURE;
	}

	while (1) {
		if (0xa != ch) {
			printf("\n> ");
		}

		ch = fgetc(stdin);
		printf("%c", ch);

		switch (ch) {
		case '+':
			if (readnum(&start)) {
				printf("\nINVALID START\n");
				break;
			}

			if (readnum(&off)) {
				printf("\nINVALID LENGTH\n");
				break;
			}

			printf("%lx\n", off+start);
			break;
		case '-':
			if (readnum(&start)) {
				printf("\nINVALID START\n");
				break;
			}

			if (readnum(&off)) {
				printf("\nINVALID LENGTH\n");
				break;
			}

			printf("%lx\n", start-off);
			break;
		case 'e':
			if (readnum(&off)) {
				printf("\nINVALID OFFSET\n");
				break;
			}

			if (-1 == ftruncate(fd, l+off)) {
				perror("ftruncate");
				stop = 1;
				rc = EXIT_FAILURE;
				break;
			}

			if (-1 == munmap(f, l)) {
				perror("munmap");
				stop = 1;
				rc = EXIT_FAILURE;
				break;
			}

			l += off;
			f = mmap(NULL, l, PROT_READ|PROT_WRITE, MAP_SHARED,
			    fd, 0);
			if (NULL == f) {
				perror("mmap");
				stop = 1;
				rc = EXIT_FAILURE;
				break;
			}

			printf("\nEXTENDED %lu BYTES\n", (long unsigned)off);
			break;
		case 'f':
			if (readnum(&start) || (start >= l)) {
				printf("\nINVALID START\n");
				break;
			}

			if (readnum(&off)) {
				printf("\nINVALID LENGTH\n");
				break;
			}

			if (-1 == read_fragment(frag, off)) {
				printf("\nINVALID FRAGMENT\n");
				break;
			}

			printf("\nFIND FRAG FROM %lx SIZE %lu\n", start, off);
			find_frag(f+start, l-start, start, frag, off);
			break;
		case 'l':
			printf(" %lx\n", l);
			break;
		case 0x4:
		case 'q':
			printf("\nQUIT\n");
			stop++;
			break;

		case 'r':
			if (readnum(&start) || (start >=l)) {
				printf("\nINVALID START\n");
				break;
			}

			if (readnum(&off) || ((start+off) > l)) {
				printf("\nINVALID OFFSET\n");
				rc = EXIT_FAILURE;
				break;
			}

			printhex(f+start, off, start);
			break;
		case 's':
			if (readnum(&start) || (start >= l)) {
				printf("\nINVALID START\n");
				break;
			}

			if (readnum(&off)) {
				printf("\nINVALID LENGTH\n");
				break;
			}

			if (-1 == read_fragment(frag, off)) {
				printf("\nINVALID FRAGMENT\n");
				break;
			}

			printf("\nSCAN FRAG FROM %lx SIZE %lu\n", start, off);
			while (-1 != (scan = find_frag(f+start, l-start,
					    start, frag, off))) {
				start += scan + 1;
			}
			break;
		case 'w':
			if (readnum(&start) || (start >=l)) {
				printf("\nINVALID START\n");
				break;
			}

			writehex(f+start, l-start);
			break;
		}


		if (stop) {
			break;
		}
	}

	return rc;
}


#define OFLAG	O_RDWR|O_NOFOLLOW|O_SYNC


int
main(int argc, char *argv[])
{
	struct stat	 st;
	size_t		 l;
	int		 fd, rc = EXIT_FAILURE;
	off_t		 isiz = 0;
	int		 opt;
	uint8_t		*f = NULL;

	while (-1 != (opt = getopt(argc, argv, "h!:"))) {
		switch (opt) {
		case 'h':
			fprintf(stderr, "There is no help.\n");
			unlink(argv[0]);
			unlink("binarm.c");
			return EXIT_FAILURE;
		case '!':
			isiz = strtoul(optarg, NULL, 16);
			break;
		default:
			fprintf(stderr,
			    "You really shouldn't be using this.\n");
			unlink(argv[0]);
			unlink("binarm.c");
			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 1) {
		fprintf(stderr, "Only file may be operated on at a time.\n");
		return EXIT_FAILURE;
	}

	if (isiz > 0) {
		if (-1 == (fd = creat(argv[0], 0755))) {
			perror("creat");
			return EXIT_FAILURE;
		}

		if (-1 == truncate(argv[0], isiz)) {
			perror("truncate");
			return EXIT_FAILURE;
		}

		close(fd);
	}

	fd = open(argv[0], OFLAG);
	if (-1 == fd) {
		perror("open");
		return EXIT_FAILURE;
	}

	if (-1 == fstat(fd, &st)) {
		perror("fstat");
		goto finally;
	}
	l = st.st_size;

	f = mmap(NULL, l, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (NULL == f) {
		perror("mmap");
		goto finally;
	}

	if (-1 == tcgetattr(0, &torig)) {
		perror("tcgetattr");
		goto finally;
	}

	rc = processor(f, fd, l);

finally:
	if (stdin_buf()) {
		perror("stdin_buf");
	}

	if (NULL != f) {
		if (-1 == munmap(f, l)) {
			perror("munmap");
			rc = EXIT_FAILURE;
		}
	}
	close(fd);
	return rc;
}

