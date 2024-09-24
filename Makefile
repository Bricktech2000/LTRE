ltrep:
	mkdir -p bin
	gcc -O2 -Wall -Wno-use-after-free -Werror -pedantic -std=c99 ltrep.c ltre.c -o bin/ltrep

compl:
	mkdir -p bin
	gcc -O2 -Wall -Wno-use-after-free -Wdangling-pointer=1 -Werror -pedantic -std=c99 compl.c ltre.c -o bin/compl

test:
	mkdir -p bin
	gcc -O2 -Wall -Wno-use-after-free -Werror -pedantic -std=c99 test.c ltre.c -o bin/test

clean:
	rm -rf bin
