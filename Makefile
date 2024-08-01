OUT = led

all:
	tcc -O2 -o $(OUT) led.c -lncurses

install: all
	install $(OUT) /usr/local/bin

