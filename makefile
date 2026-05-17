CC = tcc
LIBS = -lncurses
PREFIX = /usr/local
CFLAGS = -Wall -Werror
USEMTM = 1
USEX11 = 1

ifdef USEMTM
	CFLAGS += -D_USE_MTM
endif

ifdef USEX11
	CFLAGS += -D_USE_X11
endif

all:
	${CC} -O2 -o led led.c -I. ${LIBS} ${CFLAGS}

install: all
	mkdir -p ${PREFIX}/bin
	install -s led ${PREFIX}/bin
	mkdir -p ${PREFIX}/share/man/man1
	install -m 644 led.1 ${PREFIX}/share/man/man1

uninstall:
	rm ${PREFIX}/bin/led
	rm ${PREFIX}/share/man/man1/led

.PHONY: all install uninstall
