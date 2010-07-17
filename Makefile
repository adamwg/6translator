all: 6translator

6translator: 6translator.c
	gcc -Wall -o 6translator 6translator.c

clean:
	-rm -f *~ 6translator *.o
