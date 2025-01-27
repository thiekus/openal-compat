BINS=oalcompat.so

all: $(BINS)

%.so: %.c
	$(CC) $(CFLAGS) -m32 -shared -nostartfiles -g -fPIC -Wall -o $@ $^ -ldl

clean:
	rm -f $(BINS)
