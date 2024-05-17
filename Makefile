build:
	mkdir -p bin
	gcc -O2 -Wall -Werror -pedantic -std=c99 main.c ltre.c -o bin/ltre

clean:
	rm -rf bin
