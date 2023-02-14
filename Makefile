CC 		 = gcc
CFLAGS = -g -Wall

p2: p2.o
	$(CC) p2.o -o p2

p2.o: src/p2.c
	$(CC) src/p2.c -c -o p2.o

clean:
	rm -rf *.o p2

