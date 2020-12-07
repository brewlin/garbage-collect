CC = gcc
SRCS = $(wildcard *.c) $(wildcard *.s)
BIN = gc

run: clean gcrun

clean:
	rm -f gc a.out
	rm -rf *.o
gcrun: $(SRCS)
	$(CC) -g -o gc  $(SRCS)
