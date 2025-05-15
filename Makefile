USECOLOR ?=

ifdef USECOLOR
	CFLAGS := -D_USE_COLOR
endif

all:
	tcc -O2 -o led *.c -I. -lncurses ${CFLAGS}

install: all
	install led /usr/local/bin

