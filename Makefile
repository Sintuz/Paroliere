CFLAGS=-Wall -Wextra -pedantic -std=c99 -lSDL2 -lSDL2_ttf -lSDL2_mixer

all: main
main: main.c
	gcc -o main $< ${CFLAGS}

.PHONY: clean
clean:
	rm main