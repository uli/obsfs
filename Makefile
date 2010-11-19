OBJS = obsfs.o cache.o util.o
LIBS = -lfuse -lcurl -lexpat
CFLAGS = -g -Wall -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE

all: obsfs

obsfs: $(OBJS)
	$(CC) $(CFLAGS) -o obsfs $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) obsfs

obsfs.o cache.o: cache.h obsfs.h util.h
util.o: util.h
