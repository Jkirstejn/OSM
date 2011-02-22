CC=gcc
CFLAGS=-g -pedantic -std=c99 -Wall -Wextra

main: bintree.o dlist.o main.c
	$(CC) $(CFLAGS) bintree.o dlist.o main.c -o main

bintree.o: bintree.c bintree.h
	$(CC) $(CFLAGS) -c bintree.c

dlist.o: dlist.c dlist.h
	$(CC) $(CFLAGS) -c dlist.c

clean:
	rm -f *.o
	rm -f bintree
	rm -f dlist
	rm -f main

all: main

