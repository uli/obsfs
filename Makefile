OBJS = obsfs.o cache.o util.o status.o rc.o
LIBS = -lfuse -lcurl -lexpat $(shell pkg-config glib-2.0 --libs) $(shell pkg-config bzip2 --libs)
CFLAGS = -g -Wall -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE $(shell pkg-config glib-2.0 --cflags)

all: obsfs

obsfs: $(OBJS)
	$(CC) $(CFLAGS) -o obsfs $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) obsfs

cache.o: cache.h obsfs.h util.h
obsfs.o: cache.h obsfs.h util.h status.h rc.h
status.o: status.h
util.o: util.h
rc.c: rc.h
