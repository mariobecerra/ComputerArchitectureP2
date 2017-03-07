JUNK =  *.o

CC = gcc
CFLAGS = -g -Wall -Wno-unused -Wno-sign-compare

all:  sim

sim:  main.o cache.o
	$(CC) -o sim main.o cache.o -lm

main.o:  main.c cache.h
	$(CC) $(CFLAGS) -c main.c

cache.o:  cache.c cache.h
	$(CC) $(CFLAGS) -c cache.c

clean:
	rm -f $(JUNK) ./sim
