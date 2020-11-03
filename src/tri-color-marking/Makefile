CC = gcc
SRCS = $(wildcard *.c)
BIN = gc

all: clean gc

clean:
	rm -f gc a.out
	rm -rf *.o

gc: $(SRCS)
	$(CC) -g -o gc $(SRCS) ../../header/gc.c -I ../../header

