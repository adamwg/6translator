CC = gcc
CFLAGS += -Wall

all: 6translator

6translator: 6translator.c 6translator.h
	${CC} ${CFLAGS} -o 6translator 6translator.c

debug:
	@CFLAGS="-g -DDEBUG=1" make 6translator

clean:
	-rm -f *~ 6translator *.o
