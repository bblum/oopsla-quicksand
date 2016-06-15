CC=gcc
CFLAGS=-Wall -Wextra -Werror -std=c99 -g
LDFLAGS=-lpthread

DEPS = common.h sync.h io.h pp.h job.h messaging.h xcalls.h time.h option.h array_list.h bug.h work.h signals.h
OBJ = main.o io.o pp.o job.o messaging.o time.o option.o bug.o work.o signals.o

all: quicksand

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

quicksand: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -f *.o quicksand
