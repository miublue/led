CC = tcc
LIBS = -lncurses
PREFIX = /usr/local

all:
	${CC} -O2 -o led *.c -I. ${LIBS}

install: all
	mkdir -p ${PREFIX}/bin
	install -s led ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/led

.PHONY: all install uninstall
