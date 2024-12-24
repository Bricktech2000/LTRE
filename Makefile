ltrep:
	mkdir -p bin
	gcc -O2 -Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-parentheses -Wno-missing-field-initializers -Wno-unused-parameter -std=c99 ltrep.c ltre.c -o bin/ltrep

compl:
	mkdir -p bin
	gcc -O2 -Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-parentheses -Wno-missing-field-initializers -Wno-unused-parameter -Wdangling-pointer=1 -std=c99 compl.c ltre.c -o bin/compl

equiv:
	mkdir -p bin
	gcc -O2 -Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-parentheses -Wno-missing-field-initializers -Wno-unused-parameter -Wdangling-pointer=1 -std=c99 equiv.c ltre.c -o bin/equiv

test:
	mkdir -p bin
	gcc -O2 -Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-parentheses -Wno-missing-field-initializers -std=c99 test.c ltre.c -o bin/test

clean:
	rm -rf bin
