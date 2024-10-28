CC = gcc

CFLAGS = -pedantic -pthread -Wall -Werror --std=gnu99 -g

debug = -g

LIBDIR = /local/courses/csse2310/lib
LDFLAGS = -L$(LIBDIR) -lcsse2310a3 -lcsse2310a4 -lm

INCLUDEDIR = /local/courses/csse2310/include
CFLAGS += -I$(INCLUDEDIR)

.DEFAULT_GOAL := all
.PHONY: debug clean

all: auctionclient auctioneer

debug: CFLAGS += $(DEBUG)
debug: clean auctionclient

auctionclient.o: auctionclient.c
	$(CC) $(CFLAGS) -c $^ -o $@

auctionclient: auctionclient.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

auctioneer.o: auctioneer.c
	$(CC) $(CFLAGS) -c $^ -o $@

auctioneer: auctioneer.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
clean:
	rm -f autcioneer auctionclient *.o

#cleanall:
#	rm -f testuqwordladder *.o
#	rm -f *.stderr
#	rm -f *.stdout
#	rm -f *.exitstatus
#	rm -r tmp*
