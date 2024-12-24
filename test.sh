# when updating the test suite make sure to `<c-v>` `r0gvg<c-a>`

echo -n 00\  >> test.act; echo -e 'foo' | bin/ltrep 'a' >> test.act
echo -n 01\  >> test.act; echo -e 'bar' | bin/ltrep 'a' >> test.act
echo -n 02\  >> test.act; echo -e 'foo' | bin/ltrep 'a' - >> test.act
echo -n 03\  >> test.act; echo -e 'bar' | bin/ltrep 'a' - >> test.act

echo -n 04\  >> test.act; echo -e 'foo' | bin/ltrep -i 'a' >> test.act
echo -n 05\  >> test.act; echo -e 'bar' | bin/ltrep -i 'a' >> test.act
echo -n 06\  >> test.act; echo -e 'BAZ' | bin/ltrep -i 'a' >> test.act
echo -n 07\  >> test.act; echo -e 'foo' | bin/ltrep -v 'a' >> test.act
echo -n 08\  >> test.act; echo -e 'bar' | bin/ltrep -v 'a' >> test.act
echo -n 09\  >> test.act; echo -e 'foo' | bin/ltrep -vi 'a' >> test.act
echo -n 10\  >> test.act; echo -e 'bar' | bin/ltrep -vi 'a' >> test.act
echo -n 11\  >> test.act; echo -e 'BAZ' | bin/ltrep -vi 'a' >> test.act
echo -n 12\  >> test.act; echo -e 'BAZ' | bin/ltrep -v -i 'a' >> test.act
echo -n 13\  >> test.act; echo -e 'foo' | bin/ltrep -vxi '.*a' >> test.act
echo -n 14\  >> test.act; echo -e 'bar' | bin/ltrep -vxi '.*a' >> test.act
echo -n 15\  >> test.act; echo -e 'BAZ' | bin/ltrep -vxi '.*a' >> test.act
echo -n 16\  >> test.act; echo -e 'aa'  | bin/ltrep -vxi '.*a' >> test.act
echo -n 17\  >> test.act; echo -e 'AA'  | bin/ltrep -vxi '.*a' >> test.act
echo -n 18\  >> test.act; echo -e 'AA'  | bin/ltrep -v -xi '.*a' >> test.act

echo -n 19\  >> test.act; echo -e 'foo' | bin/ltrep -S 'a' >> test.act
echo -n 20\  >> test.act; echo -e 'bar' | bin/ltrep -S 'a' >> test.act
echo -n 21\  >> test.act; echo -e 'BAZ' | bin/ltrep -S 'a' >> test.act
echo -n 22\  >> test.act; echo -e 'foo' | bin/ltrep -S 'A' >> test.act
echo -n 23\  >> test.act; echo -e 'bar' | bin/ltrep -S 'A' >> test.act
echo -n 24\  >> test.act; echo -e 'BAZ' | bin/ltrep -S 'A' >> test.act

echo -n 25\  >> test.act; echo -e '-W' | bin/ltrep '\-W' >> test.act
echo -n 26\  >> test.act; echo -e '-W' | bin/ltrep -F -- '-W' >> test.act
echo -n 27\  >> test.act; echo -e '-W' | bin/ltrep -Fv -- '-W' >> test.act
echo -n 28\  >> test.act; echo -e 'a+' | bin/ltrep -Fi 'a+' >> test.act
echo -n 29\  >> test.act; echo -e 'A+' | bin/ltrep -Fi 'a+' >> test.act

echo -n 30\  >> test.act; echo -e 'ab\ncd' | bin/ltrep -n '' >> test.act
echo -n 31\  >> test.act; echo -e 'ab\ncd' | bin/ltrep -b '' >> test.act
echo -n 32\  >> test.act; echo -e 'ab\ncd' | bin/ltrep -Hn '' >> test.act
echo -n 33\  >> test.act; echo -e 'ab\ncd' | bin/ltrep -Hb '' >> test.act
echo -n 34\  >> test.act; echo -e 'ab\ncd' | bin/ltrep -Hnb '' >> test.act

echo -n 35\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -c '' >> test.act
echo -n 36\  >> test.act; echo -e 'ab\nac\ne' | bin/ltrep -c 'a' >> test.act
echo -n 37\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -nc '' >> test.act
echo -n 38\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -bc '' >> test.act
echo -n 39\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -Hc '' >> test.act
echo -n 40\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -vc '' >> test.act
echo -n 41\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -l '' >> test.act
echo -n 42\  >> test.act; echo -e 'ab\nac\ne' | bin/ltrep -l 'a' >> test.act
echo -n 43\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -nl '' >> test.act
echo -n 44\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -bl '' >> test.act
echo -n 45\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -Hl '' >> test.act
echo -n 46\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -vl '' >> test.act
echo -n 47\  >> test.act; echo -e 'ab\ncd\ne' | bin/ltrep -cl '' >> test.act
echo -n 48\  >> test.act; echo -e 'ab\nac\ne' | bin/ltrep -cl 'a' >> test.act

echo -n 49\  >> test.act; echo -e 'diff\t' | bin/ltrep 'diff\s' test.sh >> test.act
echo -n 50\  >> test.act; echo -e 'diff\t' | bin/ltrep 'diff\s' - test.sh >> test.act
echo -n 51\  >> test.act; echo -e 'diff\t' | bin/ltrep 'diff\s' test.sh - >> test.act
echo -n 52\  >> test.act; echo -e 'diff\t' | bin/ltrep -c 'diff\s' test.sh - >> test.act
echo -n 53\  >> test.act; echo -e 'diff\t' | bin/ltrep -l 'diff\s' test.sh - >> test.act
echo -n 54\  >> test.act; echo -e 'diff\t' | bin/ltrep -cl 'diff\s' test.sh - >> test.act
echo -n 55\  >> test.act; echo -e 'diff\t' | bin/ltrep -Hn 'diff\s' test.sh - >> test.act
echo -n 56\  >> test.act; echo -e 'diff\t' | bin/ltrep -Hb 'diff\s' test.sh - >> test.act

echo -n 57\  >> test.act; echo -e 'abaabb' | bin/ltrep -o 'b+' >> test.act
echo -n 58\  >> test.act; echo -e 'abbba'  | bin/ltrep -o 'b+' >> test.act
echo -n 59\  >> test.act; echo -e 'bar'    | bin/ltrep -ob 'a' >> test.act

echo -n 60\  >> test.act; echo -e 'bar' | bin/ltrep -xp 'a' >> test.act
echo -n 61\  >> test.act; echo -e 'bar' | bin/ltrep -px 'a' >> test.act
echo -n 62\  >> test.act; echo -e 'BAR' | bin/ltrep -is 'a' >> test.act
echo -n 63\  >> test.act; echo -e 'BAR' | bin/ltrep -si 'a' >> test.act
echo -n 64\  >> test.act; echo -e 'BAR' | bin/ltrep -Ss 'a' >> test.act
echo -n 65\  >> test.act; echo -e 'BAR' | bin/ltrep -sS 'a' >> test.act
echo -n 66\  >> test.act; echo -e 'bar' | bin/ltrep -Si 'A' >> test.act
echo -n 67\  >> test.act; echo -e 'bar' | bin/ltrep -iS 'A' >> test.act
echo -n 68\  >> test.act; echo -e 'bar' | bin/ltrep -nN 'a' >> test.act
echo -n 69\  >> test.act; echo -e 'bar' | bin/ltrep -Nn 'a' >> test.act
echo -n 70\  >> test.act; echo -e 'bar' | bin/ltrep -Hh 'a' >> test.act
echo -n 71\  >> test.act; echo -e 'bar' | bin/ltrep -hH 'a' >> test.act

diff test.exp test.act
# cp test.act test.exp # for updating the test suite
rm test.act
