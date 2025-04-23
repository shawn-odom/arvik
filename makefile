CC = gcc
DEBUG = -g3 -O0
LDFLAGS = -lz
CFLAGS = -Wall -Wshadow -Wunreachable-code -Wredundant-decls \
	-Wmissing-declarations -Wold-style-definition -Wmissing-prototypes \
	-Wdeclaration-after-statement -Wextra -Werror -Wno-return-local-addr -Wunsafe-loop-optimizations \
	-Wuninitialized

PROG = arvik

GIT_COMMIT_MSG = "Auto commit message"

all: $(PROG)

$(PROG) : $(PROG).o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(PROG).o : $(PROG).c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(PROG) *.o *~ \#*

TAR_FILE = ${LOGNAME}_lab2.tar.gz
tar:
	rm -f $(TAR_FILE)
	tar cvaf $(TAR_FILE) *.[ch] [Mm]akefile
	tar tvaf $(TAR_FILE)

git:
	@if [ ! -d .git ] ; then git init; git remote add origin git@github.com:shawn-odom/arvik.git; fi
	git add *.[ch] ?akefile
	git commit -m $(GIT_COMMIT_MSG)
	git branch -M main
	git push -u origin main

