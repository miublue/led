CC = tcc
PREFIX = /usr/local
USEMTM = 1
CFLAGS =

ifdef USEMTM
	CFLAGS += -D_USE_MTM
endif

all:
	${CC} -O2 -o led *.c -I. -lncurses ${CFLAGS}

install: all
	mkdir -p ${PREFIX}/bin
	install led ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/led

