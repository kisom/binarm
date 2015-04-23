CFLAGS =
RELEASE =	$(shell uname -s|tr A-Z a-z).$(shell uname -m)

.PHONY: all
all: binarm

binarm: binarm.c
	$(CC) $(CFLAGS) -o $@ binarm.c

.PHONY: release
release: binarm.$(RELEASE)

binarm.$(RELEASE): binarm.c
	$(CC) -static -o $@ binarm.c
	strip --strip-unneeded -x -X $@

clean:
	rm -f binarm binarm.$(RELEASE) *.o *core*

