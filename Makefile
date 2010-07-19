CC = gcc
CFLAGS += -Wall

ifndef PREFIX
	PREFIX = /usr
endif

ifdef prefix
	PREFIX = ${prefix}
endif

all: 6translator

6translator: 6translator.c 6translator.h
	${CC} ${CFLAGS} -o 6translator 6translator.c

debug:
	@CFLAGS="-g -DDEBUG=1" make 6translator

install: 6translator
	install -D 6translator ${PREFIX}/bin/6translator

clean:
	-rm -f *~ 6translator *.o
