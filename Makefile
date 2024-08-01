all:
	tcc -O2 -o led *.c -lncurses

install: all
	install led /usr/local/bin

