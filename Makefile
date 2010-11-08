OBJS = obsfs.o
LIBS = -lfuse
CFLAGS = -D_FILE_OFFSET_BITS=64

all: obsfs

obsfs: $(OBJS)
	$(CC) $(CFLAGS) -o obsfs $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) obsfs
