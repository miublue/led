CC = tcc
PREFIX = /usr/local

all:
	${CC} -O2 -o led *.c -I. -lncurses

install: all
	mkdir -p ${PREFIX}/bin
	install -s led ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/led

