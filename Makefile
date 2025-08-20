CC = tcc
USECOLOR ?= 1
USEMTM ?= 1
CFLAGS ?=

ifdef USECOLOR
	CFLAGS += -D_USE_COLOR
endif

ifdef USEMTM
	CFLAGS += -D_USE_MTM
endif

all:
	${CC} -O2 -o led *.c -I. -lncurses ${CFLAGS}

install: all
	install led /usr/local/bin

uninstall:
	rm /usr/local/bin/led

