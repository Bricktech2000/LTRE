# when updating the test suite make sure to <c-v> ... r0gvg<c-a>

rm test.act
true

#else   000 -v, -p, -i, -S
echo $? 001 >> test.act; echo -e 'bb' | bin/ltrep -i 'Aa' >> test.act
echo $? 002 >> test.act; echo -e 'aa' | bin/ltrep -i 'Aa' >> test.act
echo $? 003 >> test.act; echo -e 'Aa' | bin/ltrep -i 'Aa' >> test.act
echo $? 004 >> test.act; echo -e 'AA' | bin/ltrep -i 'Aa' >> test.act
echo $? 005 >> test.act; echo -e 'bb' | bin/ltrep -v 'Aa' >> test.act
echo $? 006 >> test.act; echo -e 'aa' | bin/ltrep -v 'Aa' >> test.act
echo $? 007 >> test.act; echo -e 'Aa' | bin/ltrep -v 'Aa' >> test.act
echo $? 008 >> test.act; echo -e 'AA' | bin/ltrep -v 'Aa' >> test.act
echo $? 009 >> test.act; echo -e 'bb' | bin/ltrep -vi 'Aa' >> test.act
echo $? 010 >> test.act; echo -e 'aa' | bin/ltrep -vi 'Aa' >> test.act
echo $? 011 >> test.act; echo -e 'Aa' | bin/ltrep -vi 'Aa' >> test.act
echo $? 012 >> test.act; echo -e 'AA' | bin/ltrep -vi 'Aa' >> test.act
echo $? 013 >> test.act; echo -e 'bb' | bin/ltrep -vp 'a' >> test.act
echo $? 014 >> test.act; echo -e 'aa' | bin/ltrep -vp 'a' >> test.act
echo $? 015 >> test.act; echo -e 'Aa' | bin/ltrep -vp 'a' >> test.act
echo $? 016 >> test.act; echo -e 'AA' | bin/ltrep -vp 'a' >> test.act
echo $? 017 >> test.act; echo -e 'bb' | bin/ltrep -vpi 'a' >> test.act
echo $? 018 >> test.act; echo -e 'aa' | bin/ltrep -vpi 'a' >> test.act
echo $? 019 >> test.act; echo -e 'Aa' | bin/ltrep -vpi 'a' >> test.act
echo $? 020 >> test.act; echo -e 'AA' | bin/ltrep -vpi 'a' >> test.act
echo $? 021 >> test.act; echo -e 'bb' | bin/ltrep -S 'Aa' >> test.act
echo $? 022 >> test.act; echo -e 'aa' | bin/ltrep -S 'Aa' >> test.act
echo $? 023 >> test.act; echo -e 'Aa' | bin/ltrep -S 'Aa' >> test.act
echo $? 024 >> test.act; echo -e 'AA' | bin/ltrep -S 'Aa' >> test.act
echo $? 025 >> test.act; echo -e 'bb' | bin/ltrep -S 'aa' >> test.act
echo $? 026 >> test.act; echo -e 'aa' | bin/ltrep -S 'aa' >> test.act
echo $? 027 >> test.act; echo -e 'Aa' | bin/ltrep -S 'aa' >> test.act
echo $? 028 >> test.act; echo -e 'AA' | bin/ltrep -S 'aa' >> test.act
#else   029 -o
echo $? 030 >> test.act; echo -e 'abaabb' | bin/ltrep -o 'b+' >> test.act
echo $? 031 >> test.act; echo -e 'abbba'  | bin/ltrep -o 'b+' >> test.act
echo $? 032 >> test.act; echo -e 'aaaa'   | bin/ltrep -o 'a+' >> test.act
echo $? 033 >> test.act; echo -e 'bar'    | bin/ltrep -ob 'a' >> test.act
echo $? 034 >> test.act; echo -e 'bar'    | bin/ltrep -ok 'a' >> test.act
#else   035 -F, [--]
echo $? 036 >> test.act; echo -e '-W'  | bin/ltrep '\-W' >> test.act
echo $? 037 >> test.act; echo -e '-W'  | bin/ltrep -F -- '-W' >> test.act
echo $? 038 >> test.act; echo -e '-W'  | bin/ltrep -Fv -- '-W' >> test.act
echo $? 039 >> test.act; echo -e 'a+'  | bin/ltrep -Fi 'a+' >> test.act
echo $? 040 >> test.act; echo -e 'A+'  | bin/ltrep -Fi 'a+' >> test.act
echo $? 041 >> test.act; echo -e 'a++' | bin/ltrep -Fp 'a+' >> test.act
echo $? 042 >> test.act; echo -e 'A++' | bin/ltrep -Fp 'a+' >> test.act
#else   043 -H, -n, -k, -b, -T
echo $? 044 >> test.act; echo -e 'ab\ncd' | bin/ltrep '%' >> test.act
echo $? 045 >> test.act; echo -e 'ab\ncd' | bin/ltrep -b '%' >> test.act
echo $? 046 >> test.act; echo -e 'ab\ncd' | bin/ltrep -k '%' >> test.act
echo $? 047 >> test.act; echo -e 'ab\ncd' | bin/ltrep -kb '%' >> test.act
echo $? 048 >> test.act; echo -e 'ab\ncd' | bin/ltrep -n '%' >> test.act
echo $? 049 >> test.act; echo -e 'ab\ncd' | bin/ltrep -nb '%' >> test.act
echo $? 050 >> test.act; echo -e 'ab\ncd' | bin/ltrep -nk '%' >> test.act
echo $? 051 >> test.act; echo -e 'ab\ncd' | bin/ltrep -nkb '%' >> test.act
echo $? 052 >> test.act; echo -e 'ab\ncd' | bin/ltrep -H '%' >> test.act
echo $? 053 >> test.act; echo -e 'ab\ncd' | bin/ltrep -Hb '%' >> test.act
echo $? 054 >> test.act; echo -e 'ab\ncd' | bin/ltrep -Hk '%' >> test.act
echo $? 055 >> test.act; echo -e 'ab\ncd' | bin/ltrep -Hkb '%' >> test.act
echo $? 056 >> test.act; echo -e 'ab\ncd' | bin/ltrep -Hn '%' >> test.act
echo $? 057 >> test.act; echo -e 'ab\ncd' | bin/ltrep -Hnb '%' >> test.act
echo $? 058 >> test.act; echo -e 'ab\ncd' | bin/ltrep -Hnk '%' >> test.act
echo $? 059 >> test.act; echo -e 'ab\ncd' | bin/ltrep -Hnkb '%' >> test.act
echo $? 060 >> test.act; echo -e 'ab\ncd' | bin/ltrep -T '%' >> test.act
echo $? 061 >> test.act; echo -e 'ab\ncd' | bin/ltrep -HnkbT '%' >> test.act
#else   062 -c, -l, -L
echo $? 063 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -c '%' >> test.act
echo $? 064 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -c 'a.' >> test.act
echo $? 065 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -vc 'a.' >> test.act
echo $? 066 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -Hc '%' >> test.act
echo $? 067 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -nc '%' >> test.act
echo $? 068 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -kc '%' >> test.act
echo $? 069 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -bc '%' >> test.act
echo $? 070 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -l '%' >> test.act
echo $? 071 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -l 'a.' >> test.act
echo $? 072 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -vl 'a.' >> test.act
echo $? 073 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -Hl '%' >> test.act
echo $? 074 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -nl '%' >> test.act
echo $? 075 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -kl '%' >> test.act
echo $? 076 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -bl '%' >> test.act
echo $? 077 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -L '' >> test.act
echo $? 078 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -L 'a.' >> test.act
echo $? 079 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -vL 'a.' >> test.act
echo $? 080 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -HL '' >> test.act
echo $? 081 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -nL '' >> test.act
echo $? 082 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -kL '' >> test.act
echo $? 083 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -bL '' >> test.act
echo $? 084 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -cl '%' >> test.act
echo $? 085 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -cl 'a.' >> test.act
echo $? 086 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -cl '' >> test.act
echo $? 087 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -cL '%' >> test.act
echo $? 088 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -cL 'a.' >> test.act
echo $? 089 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -cL '' >> test.act
echo $? 090 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -lL '%' >> test.act
echo $? 091 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -lL 'a.' >> test.act
echo $? 092 >> test.act; echo -e 'ab\ncd\nae' | bin/ltrep -lL '' >> test.act
#else   093 [files...], -0
echo $? 094 >> test.act; echo -e 'rm\t' | bin/ltrep 'rm\s%' >> test.act
echo $? 095 >> test.act; echo -e 'rm\t' | bin/ltrep 'rm\s%' - >> test.act
echo $? 096 >> test.act; echo -e 'rm\t' | bin/ltrep 'rm\s%' test.sh >> test.act
echo $? 097 >> test.act; echo -e 'rm\t' | bin/ltrep 'rm\s%' - test.sh >> test.act
echo $? 098 >> test.act; echo -e 'rm\t' | bin/ltrep 'rm\s%' test.sh - >> test.act
echo $? 099 >> test.act; echo -e 'rm\t' | bin/ltrep -c 'rm\s%' test.sh - >> test.act
echo $? 100 >> test.act; echo -e 'rm\t' | bin/ltrep -l 'rm\s%' test.sh - >> test.act
echo $? 101 >> test.act; echo -e 'rm\t' | bin/ltrep -L 'rm\s%' test.sh - >> test.act
echo $? 102 >> test.act; echo -e 'rm\t' | bin/ltrep -Hc 'rm\s%' test.sh - >> test.act
echo $? 103 >> test.act; echo -e 'rm\t' | bin/ltrep -Hnk 'rm\s%' test.sh - >> test.act
echo $? 104 >> test.act; echo -e 'rm\t' | bin/ltrep -HbT 'rm\s%' test.sh - >> test.act
echo $? 105 >> test.act; echo -e 'rm\t' | bin/ltrep -0 'rm\s%' test.sh - >> test.act
echo $? 106 >> test.act; echo -e 'rm\t' | bin/ltrep -0c 'rm\s%' test.sh - >> test.act
echo $? 107 >> test.act; echo -e 'rm\t' | bin/ltrep -0l 'rm\s%' test.sh - >> test.act
echo $? 108 >> test.act; echo -e 'rm\t' | bin/ltrep -0L 'rm\s%' test.sh - >> test.act
echo $? 109 >> test.act; echo -e 'rm\t' | bin/ltrep -0Hc 'rm\s%' test.sh - >> test.act
echo $? 110 >> test.act; echo -e 'rm\t' | bin/ltrep -0Hnk 'rm\s%' test.sh - >> test.act
echo $? 111 >> test.act; echo -e 'rm\t' | bin/ltrep -0HbT 'rm\s%' test.sh - >> test.act
#else   112 soft errors, -q
echo $? 113 >> test.act; echo -e 'a' | bin/ltrep 'a' - >> test.act 2> /dev/null
echo $? 114 >> test.act; echo -e 'a' | bin/ltrep 'a' err >> test.act 2> /dev/null
echo $? 115 >> test.act; echo -e 'a' | bin/ltrep 'a' - err >> test.act 2> /dev/null
echo $? 116 >> test.act; echo -e 'a' | bin/ltrep 'a' err - >> test.act 2> /dev/null
echo $? 117 >> test.act; echo -e 'a' | bin/ltrep 'b' - >> test.act 2> /dev/null
echo $? 118 >> test.act; echo -e 'a' | bin/ltrep 'b' err >> test.act 2> /dev/null
echo $? 119 >> test.act; echo -e 'a' | bin/ltrep 'b' - err >> test.act 2> /dev/null
echo $? 120 >> test.act; echo -e 'a' | bin/ltrep 'b' err - >> test.act 2> /dev/null
echo $? 121 >> test.act; echo -e 'a' | bin/ltrep -q 'a' - >> test.act 2> /dev/null
echo $? 122 >> test.act; echo -e 'a' | bin/ltrep -q 'a' err >> test.act 2> /dev/null
echo $? 123 >> test.act; echo -e 'a' | bin/ltrep -q 'a' - err >> test.act 2> /dev/null
echo $? 124 >> test.act; echo -e 'a' | bin/ltrep -q 'a' err - >> test.act 2> /dev/null
echo $? 125 >> test.act; echo -e 'a' | bin/ltrep -q 'b' - >> test.act 2> /dev/null
echo $? 126 >> test.act; echo -e 'a' | bin/ltrep -q 'b' err >> test.act 2> /dev/null
echo $? 127 >> test.act; echo -e 'a' | bin/ltrep -q 'b' - err >> test.act 2> /dev/null
echo $? 128 >> test.act; echo -e 'a' | bin/ltrep -q 'b' err - >> test.act 2> /dev/null
#else   129 NUL bytes, -z
echo $? 130 >> test.act; echo -en '\0\n\n'     | bin/ltrep -c '%' >> test.act
echo $? 131 >> test.act; echo -en 'a\n\0b\n'   | bin/ltrep -c '%' >> test.act
echo $? 132 >> test.act; echo -en 'a\n\0\nb\n' | bin/ltrep -c '%' >> test.act
echo $? 133 >> test.act; echo -en 'a\n\0b'     | bin/ltrep -c '%' >> test.act
echo $? 134 >> test.act; echo -en '\n\0\0'     | bin/ltrep -cz '%' >> test.act
echo $? 135 >> test.act; echo -en 'a\0\nb\0'   | bin/ltrep -cz '%' >> test.act
echo $? 136 >> test.act; echo -en 'a\0\n\0b\0' | bin/ltrep -cz '%' >> test.act
echo $? 137 >> test.act; echo -en 'a\0\nb'     | bin/ltrep -cz '%' >> test.act
#else   138 flag overrides
echo $? 139 >> test.act; echo -e 'a' | bin/ltrep -px '' >> test.act
echo $? 140 >> test.act; echo -e 'a' | bin/ltrep -xp '' >> test.act
echo $? 141 >> test.act; echo -e 'a' | bin/ltrep -ox '' >> test.act
echo $? 142 >> test.act; echo -e 'a' | bin/ltrep -xo '' >> test.act
echo $? 143 >> test.act; echo -e 'a' | bin/ltrep -op '' >> test.act
echo $? 144 >> test.act; echo -e 'a' | bin/ltrep -po '' >> test.act
echo $? 145 >> test.act; echo -e 'A' | bin/ltrep -is 'a' >> test.act
echo $? 146 >> test.act; echo -e 'A' | bin/ltrep -si 'a' >> test.act
echo $? 147 >> test.act; echo -e 'A' | bin/ltrep -Ss 'a' >> test.act
echo $? 148 >> test.act; echo -e 'A' | bin/ltrep -sS 'a' >> test.act
echo $? 149 >> test.act; echo -e 'a' | bin/ltrep -Si 'A' >> test.act
echo $? 150 >> test.act; echo -e 'a' | bin/ltrep -iS 'A' >> test.act
echo $? 151 >> test.act; echo -e 'a' | bin/ltrep -FE '.' >> test.act
echo $? 152 >> test.act; echo -e 'a' | bin/ltrep -EF '.' >> test.act
echo $? 153 >> test.act; echo -e 'a' | bin/ltrep -Hh 'a' >> test.act
echo $? 154 >> test.act; echo -e 'a' | bin/ltrep -hH 'a' >> test.act
echo $? 155 >> test.act; echo -e 'a' | bin/ltrep -kK 'a' >> test.act
echo $? 156 >> test.act; echo -e 'a' | bin/ltrep -Kk 'a' >> test.act
echo $? 157 >> test.act; echo -e 'a' | bin/ltrep -Tt 'a' >> test.act
echo $? 158 >> test.act; echo -e 'a' | bin/ltrep -tT 'a' >> test.act
echo $? 159 >> test.act; echo -e 'a' | bin/ltrep -nN 'a' >> test.act
echo $? 160 >> test.act; echo -e 'a' | bin/ltrep -Nn 'a' >> test.act
echo $? 161 >> test.act; echo -e 'a' | bin/ltrep -zZ 'a' >> test.act
echo $? 162 >> test.act; echo -e 'a' | bin/ltrep -Zz 'a' >> test.act

diff --text test.exp test.act
# cp test.act test.exp # for updating the test suite
