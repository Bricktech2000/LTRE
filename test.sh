echo -e 00 'foo' | bin/ltrep 'a' >> test.act
echo -e 01 'bar' | bin/ltrep 'a' >> test.act
echo -e 02 'foo' | bin/ltrep 'a' - >> test.act
echo -e 03 'bar' | bin/ltrep 'a' - >> test.act
echo -e 04 'foo' | bin/ltrep -i 'a' >> test.act
echo -e 05 'bar' | bin/ltrep -i 'a' >> test.act
echo -e 06 'BAZ' | bin/ltrep -i 'a' >> test.act
echo -e 07 'foo' | bin/ltrep -v 'a' >> test.act
echo -e 08 'bar' | bin/ltrep -v 'a' >> test.act
echo -e 09 'foo' | bin/ltrep -vi 'a' >> test.act
echo -e 10 'bar' | bin/ltrep -vi 'a' >> test.act
echo -e 11 'BAZ' | bin/ltrep -vi 'a' >> test.act
echo -e 12 'BAZ' | bin/ltrep -v -i 'a' >> test.act
echo -e 13 'foo' | bin/ltrep -vxi '.*a' >> test.act
echo -e 14 'bar' | bin/ltrep -vxi '.*a' >> test.act
echo -e 15 'BAZ' | bin/ltrep -vxi '.*a' >> test.act
echo -e 16 'aa' | bin/ltrep -vxi '.*a' >> test.act
echo -e 17 'AA' | bin/ltrep -vxi '.*a' >> test.act
echo -e 18 'AA' | bin/ltrep -v -xi '.*a' >> test.act
echo -e 19 '-W' | bin/ltrep '\-W' >> test.act
echo -e 20 '-W' | bin/ltrep -F -- '-W' >> test.act
echo -e 21 '-W' | bin/ltrep -Fv -- '-W' >> test.act
echo -e 22 'a+' | bin/ltrep -Fi 'a+' >> test.act
echo -e 23 'A+' | bin/ltrep -Fi 'a+' >> test.act
echo -e 24 'ab\ncd' | bin/ltrep -n '' >> test.act
echo -e 25 'ab\ncd' | bin/ltrep -Hn '' >> test.act
echo -e 26 'ab\ncd\ne' | bin/ltrep -c '' >> test.act
echo -e 27 'ab\nac\ne' | bin/ltrep -c 'a' >> test.act
echo -e 28 'ab\ncd\ne' | bin/ltrep -nc '' >> test.act
echo -e 29 'ab\ncd\ne' | bin/ltrep -Hc '' >> test.act
echo -e 30 'ab\ncd\ne' | bin/ltrep -vc '' >> test.act
echo -e 31 'diff\t' | bin/ltrep 'diff\s' test.sh >> test.act
echo -e 32 'diff\t' | bin/ltrep 'diff\s' - test.sh >> test.act
echo -e 33 'diff\t' | bin/ltrep 'diff\s' test.sh - >> test.act
echo -e 34 'diff\t' | bin/ltrep -c 'diff\s' test.sh - >> test.act
echo -e 35 'diff\t' | bin/ltrep -Hn 'diff\s' test.sh - >> test.act
diff test.exp test.act
# cp test.act test.exp # for updating the test suite
rm test.act
