ltrep:
	mkdir -p bin
	gcc -O2 -Wall -Werror -pedantic -std=c99 ltrep.c ltre.c -o bin/ltrep

test:
	mkdir -p bin
	gcc -O2 -Wall -Werror -pedantic -std=c99 test.c ltre.c -o bin/test

clean:
	rm -rf bin
