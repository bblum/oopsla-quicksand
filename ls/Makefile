CC=gcc
CFLAGS=-Wall -Wextra -Werror -std=c99 -g
#LDFLAGS=-lpthread

DEPS = # .h files...
OBJ = landslide-aec.o

all: landslide-aec

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

landslide-aec: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -f *.o landslide-aec
