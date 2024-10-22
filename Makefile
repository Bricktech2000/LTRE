ltrep:
	mkdir -p bin
	gcc -O2 -Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-unused-parameter -Wno-missing-field-initializers -std=c99 ltrep.c ltre.c -o bin/ltrep

compl:
	mkdir -p bin
	gcc -O2 -Wall -Wextra -Wpedantic -Wdangling-pointer=1 -Wno-sign-compare -Wno-unused-parameter -Wno-missing-field-initializers -std=c99 compl.c ltre.c -o bin/compl

test:
	mkdir -p bin
	gcc -O2 -Wall -Wextra -Wpedantic -Wno-missing-field-initializers -Wno-sign-compare -Wno-missing-field-initializers -std=c99 test.c ltre.c -o bin/test

clean:
	rm -rf bin
