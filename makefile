CC = tcc
USEMTM ?= 1
CFLAGS ?=

ifdef USEMTM
	CFLAGS += -D_USE_MTM
endif

all:
	${CC} -O2 -o led *.c -I. -lncurses ${CFLAGS}

install: all
	install led /usr/local/bin

