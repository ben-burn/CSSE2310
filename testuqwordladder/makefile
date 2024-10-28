CC = gcc

CFLAGS = -pedantic -Wall -Werror --std=gnu99 -g

debug = -g

LIBDIR = /local/courses/csse2310/lib
LDFLAGS = -L$(LIBDIR) -lcsse2310a3

INCLUDEDIR = /local/courses/csse2310/include
CFLAGS += -I$(INCLUDEDIR)

.DEFAULT_GOAL := testuqwordladder
.PHONY: debug clean

debug: CFLAGS += $(DEBUG)
debug: clean testuqwordladder

program.o: testuqwordladder.c
	$(CC) $(CFLAGS) -c $^ -o $@

program: testuqwordladder.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f testuqwordladder *.o

cleanall:
	rm -f testuqwordladder *.o
	rm -f *.stderr
	rm -f *.stdout
	rm -f *.exitstatus
	rm -r tmp*
