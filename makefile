CC = tcc
LIBS = -lncurses
PREFIX = /usr/local
CFLAGS = -Wall -Werror

ifdef USEMTM
	CFLAGS += -D_USE_MTM
endif

all:
	${CC} -O2 -o led *.c -I. ${LIBS} ${CFLAGS}

install: all
	mkdir -p ${PREFIX}/bin
	install -s led ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/led

.PHONY: all install uninstall
