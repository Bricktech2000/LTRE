# when updating the test suite make sure to <c-v> ... r0gvg<c-a>

rm test.act
true

#else   000 -v, -p, -i, -S
echo $? 001 >> test.act; echo -e 'bb' | $@ -i 'Aa' >> test.act
echo $? 002 >> test.act; echo -e 'aa' | $@ -i 'Aa' >> test.act
echo $? 003 >> test.act; echo -e 'Aa' | $@ -i 'Aa' >> test.act
echo $? 004 >> test.act; echo -e 'AA' | $@ -i 'Aa' >> test.act
echo $? 005 >> test.act; echo -e 'bb' | $@ -v 'Aa' >> test.act
echo $? 006 >> test.act; echo -e 'aa' | $@ -v 'Aa' >> test.act
echo $? 007 >> test.act; echo -e 'Aa' | $@ -v 'Aa' >> test.act
echo $? 008 >> test.act; echo -e 'AA' | $@ -v 'Aa' >> test.act
echo $? 009 >> test.act; echo -e 'bb' | $@ -vi 'Aa' >> test.act
echo $? 010 >> test.act; echo -e 'aa' | $@ -vi 'Aa' >> test.act
echo $? 011 >> test.act; echo -e 'Aa' | $@ -vi 'Aa' >> test.act
echo $? 012 >> test.act; echo -e 'AA' | $@ -vi 'Aa' >> test.act
echo $? 013 >> test.act; echo -e 'bb' | $@ -vp 'a' >> test.act
echo $? 014 >> test.act; echo -e 'aa' | $@ -vp 'a' >> test.act
echo $? 015 >> test.act; echo -e 'Aa' | $@ -vp 'a' >> test.act
echo $? 016 >> test.act; echo -e 'AA' | $@ -vp 'a' >> test.act
echo $? 017 >> test.act; echo -e 'bb' | $@ -vpi 'a' >> test.act
echo $? 018 >> test.act; echo -e 'aa' | $@ -vpi 'a' >> test.act
echo $? 019 >> test.act; echo -e 'Aa' | $@ -vpi 'a' >> test.act
echo $? 020 >> test.act; echo -e 'AA' | $@ -vpi 'a' >> test.act
echo $? 021 >> test.act; echo -e 'bb' | $@ -S 'Aa' >> test.act
echo $? 022 >> test.act; echo -e 'aa' | $@ -S 'Aa' >> test.act
echo $? 023 >> test.act; echo -e 'Aa' | $@ -S 'Aa' >> test.act
echo $? 024 >> test.act; echo -e 'AA' | $@ -S 'Aa' >> test.act
echo $? 025 >> test.act; echo -e 'bb' | $@ -S 'aa' >> test.act
echo $? 026 >> test.act; echo -e 'aa' | $@ -S 'aa' >> test.act
echo $? 027 >> test.act; echo -e 'Aa' | $@ -S 'aa' >> test.act
echo $? 028 >> test.act; echo -e 'AA' | $@ -S 'aa' >> test.act
#else   029 -o
echo $? 030 >> test.act; echo -e 'abaabb' | $@ -o 'b+' >> test.act
echo $? 031 >> test.act; echo -e 'abbba'  | $@ -o 'b+' >> test.act
echo $? 032 >> test.act; echo -e 'aaaa'   | $@ -o 'a+' >> test.act
echo $? 033 >> test.act; echo -e 'bar'    | $@ -ob 'a' >> test.act
echo $? 034 >> test.act; echo -e 'bar'    | $@ -ok 'a' >> test.act
#else   035 -F, [--]
echo $? 036 >> test.act; echo -e '-W'  | $@ '\-W' >> test.act
echo $? 037 >> test.act; echo -e '-W'  | $@ -F -- '-W' >> test.act
echo $? 038 >> test.act; echo -e '-W'  | $@ -Fv -- '-W' >> test.act
echo $? 039 >> test.act; echo -e 'a+'  | $@ -Fi 'a+' >> test.act
echo $? 040 >> test.act; echo -e 'A+'  | $@ -Fi 'a+' >> test.act
echo $? 041 >> test.act; echo -e 'a++' | $@ -Fp 'a+' >> test.act
echo $? 042 >> test.act; echo -e 'A++' | $@ -Fp 'a+' >> test.act
#else   043 -H, -n, -k, -b, -T
echo $? 044 >> test.act; echo -e 'ab\ncd' | $@ '%' >> test.act
echo $? 045 >> test.act; echo -e 'ab\ncd' | $@ -b '%' >> test.act
echo $? 046 >> test.act; echo -e 'ab\ncd' | $@ -k '%' >> test.act
echo $? 047 >> test.act; echo -e 'ab\ncd' | $@ -kb '%' >> test.act
echo $? 048 >> test.act; echo -e 'ab\ncd' | $@ -n '%' >> test.act
echo $? 049 >> test.act; echo -e 'ab\ncd' | $@ -nb '%' >> test.act
echo $? 050 >> test.act; echo -e 'ab\ncd' | $@ -nk '%' >> test.act
echo $? 051 >> test.act; echo -e 'ab\ncd' | $@ -nkb '%' >> test.act
echo $? 052 >> test.act; echo -e 'ab\ncd' | $@ -H '%' >> test.act
echo $? 053 >> test.act; echo -e 'ab\ncd' | $@ -Hb '%' >> test.act
echo $? 054 >> test.act; echo -e 'ab\ncd' | $@ -Hk '%' >> test.act
echo $? 055 >> test.act; echo -e 'ab\ncd' | $@ -Hkb '%' >> test.act
echo $? 056 >> test.act; echo -e 'ab\ncd' | $@ -Hn '%' >> test.act
echo $? 057 >> test.act; echo -e 'ab\ncd' | $@ -Hnb '%' >> test.act
echo $? 058 >> test.act; echo -e 'ab\ncd' | $@ -Hnk '%' >> test.act
echo $? 059 >> test.act; echo -e 'ab\ncd' | $@ -Hnkb '%' >> test.act
echo $? 060 >> test.act; echo -e 'ab\ncd' | $@ -T '%' >> test.act
echo $? 061 >> test.act; echo -e 'ab\ncd' | $@ -HnkbT '%' >> test.act
#else   062 -c, -l, -L
echo $? 063 >> test.act; echo -e 'ab\ncd\nae' | $@ -c '%' >> test.act
echo $? 064 >> test.act; echo -e 'ab\ncd\nae' | $@ -c 'a.' >> test.act
echo $? 065 >> test.act; echo -e 'ab\ncd\nae' | $@ -vc 'a.' >> test.act
echo $? 066 >> test.act; echo -e 'ab\ncd\nae' | $@ -Hc '%' >> test.act
echo $? 067 >> test.act; echo -e 'ab\ncd\nae' | $@ -nc '%' >> test.act
echo $? 068 >> test.act; echo -e 'ab\ncd\nae' | $@ -kc '%' >> test.act
echo $? 069 >> test.act; echo -e 'ab\ncd\nae' | $@ -bc '%' >> test.act
echo $? 070 >> test.act; echo -e 'ab\ncd\nae' | $@ -l '%' >> test.act
echo $? 071 >> test.act; echo -e 'ab\ncd\nae' | $@ -l 'a.' >> test.act
echo $? 072 >> test.act; echo -e 'ab\ncd\nae' | $@ -vl 'a.' >> test.act
echo $? 073 >> test.act; echo -e 'ab\ncd\nae' | $@ -Hl '%' >> test.act
echo $? 074 >> test.act; echo -e 'ab\ncd\nae' | $@ -nl '%' >> test.act
echo $? 075 >> test.act; echo -e 'ab\ncd\nae' | $@ -kl '%' >> test.act
echo $? 076 >> test.act; echo -e 'ab\ncd\nae' | $@ -bl '%' >> test.act
echo $? 077 >> test.act; echo -e 'ab\ncd\nae' | $@ -L '' >> test.act
echo $? 078 >> test.act; echo -e 'ab\ncd\nae' | $@ -L 'a.' >> test.act
echo $? 079 >> test.act; echo -e 'ab\ncd\nae' | $@ -vL 'a.' >> test.act
echo $? 080 >> test.act; echo -e 'ab\ncd\nae' | $@ -HL '' >> test.act
echo $? 081 >> test.act; echo -e 'ab\ncd\nae' | $@ -nL '' >> test.act
echo $? 082 >> test.act; echo -e 'ab\ncd\nae' | $@ -kL '' >> test.act
echo $? 083 >> test.act; echo -e 'ab\ncd\nae' | $@ -bL '' >> test.act
echo $? 084 >> test.act; echo -e 'ab\ncd\nae' | $@ -cl '%' >> test.act
echo $? 085 >> test.act; echo -e 'ab\ncd\nae' | $@ -cl 'a.' >> test.act
echo $? 086 >> test.act; echo -e 'ab\ncd\nae' | $@ -cl '' >> test.act
echo $? 087 >> test.act; echo -e 'ab\ncd\nae' | $@ -cL '%' >> test.act
echo $? 088 >> test.act; echo -e 'ab\ncd\nae' | $@ -cL 'a.' >> test.act
echo $? 089 >> test.act; echo -e 'ab\ncd\nae' | $@ -cL '' >> test.act
echo $? 090 >> test.act; echo -e 'ab\ncd\nae' | $@ -lL '%' >> test.act
echo $? 091 >> test.act; echo -e 'ab\ncd\nae' | $@ -lL 'a.' >> test.act
echo $? 092 >> test.act; echo -e 'ab\ncd\nae' | $@ -lL '' >> test.act
#else   093 [files...], -0
echo $? 094 >> test.act; echo -e 'rm\t' | $@ 'rm\s%' >> test.act
echo $? 095 >> test.act; echo -e 'rm\t' | $@ 'rm\s%' - >> test.act
echo $? 096 >> test.act; echo -e 'rm\t' | $@ 'rm\s%' test.sh >> test.act
echo $? 097 >> test.act; echo -e 'rm\t' | $@ 'rm\s%' - test.sh >> test.act
echo $? 098 >> test.act; echo -e 'rm\t' | $@ 'rm\s%' test.sh - >> test.act
echo $? 099 >> test.act; echo -e 'rm\t' | $@ -c 'rm\s%' test.sh - >> test.act
echo $? 100 >> test.act; echo -e 'rm\t' | $@ -l 'rm\s%' test.sh - >> test.act
echo $? 101 >> test.act; echo -e 'rm\t' | $@ -L 'rm\s%' test.sh - >> test.act
echo $? 102 >> test.act; echo -e 'rm\t' | $@ -Hc 'rm\s%' test.sh - >> test.act
echo $? 103 >> test.act; echo -e 'rm\t' | $@ -Hnk 'rm\s%' test.sh - >> test.act
echo $? 104 >> test.act; echo -e 'rm\t' | $@ -HbT 'rm\s%' test.sh - >> test.act
echo $? 105 >> test.act; echo -e 'rm\t' | $@ -0 'rm\s%' test.sh - >> test.act
echo $? 106 >> test.act; echo -e 'rm\t' | $@ -0c 'rm\s%' test.sh - >> test.act
echo $? 107 >> test.act; echo -e 'rm\t' | $@ -0l 'rm\s%' test.sh - >> test.act
echo $? 108 >> test.act; echo -e 'rm\t' | $@ -0L 'rm\s%' test.sh - >> test.act
echo $? 109 >> test.act; echo -e 'rm\t' | $@ -0Hc 'rm\s%' test.sh - >> test.act
echo $? 110 >> test.act; echo -e 'rm\t' | $@ -0Hnk 'rm\s%' test.sh - >> test.act
echo $? 111 >> test.act; echo -e 'rm\t' | $@ -0HbT 'rm\s%' test.sh - >> test.act
#else   112 soft errors, -q
echo $? 113 >> test.act; echo -e 'a' | $@ 'a' - >> test.act 2> /dev/null
echo $? 114 >> test.act; echo -e 'a' | $@ 'a' err >> test.act 2> /dev/null
echo $? 115 >> test.act; echo -e 'a' | $@ 'a' - err >> test.act 2> /dev/null
echo $? 116 >> test.act; echo -e 'a' | $@ 'a' err - >> test.act 2> /dev/null
echo $? 117 >> test.act; echo -e 'a' | $@ 'b' - >> test.act 2> /dev/null
echo $? 118 >> test.act; echo -e 'a' | $@ 'b' err >> test.act 2> /dev/null
echo $? 119 >> test.act; echo -e 'a' | $@ 'b' - err >> test.act 2> /dev/null
echo $? 120 >> test.act; echo -e 'a' | $@ 'b' err - >> test.act 2> /dev/null
echo $? 121 >> test.act; echo -e 'a' | $@ -q 'a' - >> test.act 2> /dev/null
echo $? 122 >> test.act; echo -e 'a' | $@ -q 'a' err >> test.act 2> /dev/null
echo $? 123 >> test.act; echo -e 'a' | $@ -q 'a' - err >> test.act 2> /dev/null
echo $? 124 >> test.act; echo -e 'a' | $@ -q 'a' err - >> test.act 2> /dev/null
echo $? 125 >> test.act; echo -e 'a' | $@ -q 'b' - >> test.act 2> /dev/null
echo $? 126 >> test.act; echo -e 'a' | $@ -q 'b' err >> test.act 2> /dev/null
echo $? 127 >> test.act; echo -e 'a' | $@ -q 'b' - err >> test.act 2> /dev/null
echo $? 128 >> test.act; echo -e 'a' | $@ -q 'b' err - >> test.act 2> /dev/null
#else   129 NUL bytes, -z, -1
echo $? 130 >> test.act; echo -en ''           | $@ -c '%' >> test.act
echo $? 131 >> test.act; echo -en '\0\n\n'     | $@ -c '%' >> test.act
echo $? 132 >> test.act; echo -en 'a\n\0b\n'   | $@ -c '%' >> test.act
echo $? 133 >> test.act; echo -en 'a\n\0\nb\n' | $@ -c '%' >> test.act
echo $? 134 >> test.act; echo -en 'a\n\0b'     | $@ -c '%' >> test.act
echo $? 135 >> test.act; echo -en ''           | $@ -cz '%' >> test.act
echo $? 136 >> test.act; echo -en '\n\0\0'     | $@ -cz '%' >> test.act
echo $? 137 >> test.act; echo -en 'a\0\nb\0'   | $@ -cz '%' >> test.act
echo $? 138 >> test.act; echo -en 'a\0\n\0b\0' | $@ -cz '%' >> test.act
echo $? 139 >> test.act; echo -en 'a\0\nb'     | $@ -cz '%' >> test.act
echo $? 140 >> test.act; echo -en ''           | $@ -c1 '%' >> test.act
echo $? 141 >> test.act; echo -en '\0\n\n'     | $@ -c1 '%' >> test.act
echo $? 142 >> test.act; echo -en 'a\n\0b\n'   | $@ -c1 '%' >> test.act
echo $? 143 >> test.act; echo -en 'a\n\0\nb\n' | $@ -c1 '%' >> test.act
echo $? 144 >> test.act; echo -en 'a\n\0b'     | $@ -c1 '%' >> test.act
#else   145 flag overrides
echo $? 146 >> test.act; echo -e 'a'  | $@ -px '' >> test.act
echo $? 147 >> test.act; echo -e 'a'  | $@ -xp '' >> test.act
echo $? 148 >> test.act; echo -e 'a'  | $@ -ox '' >> test.act
echo $? 149 >> test.act; echo -e 'a'  | $@ -xo '' >> test.act
echo $? 150 >> test.act; echo -e 'a'  | $@ -op '' >> test.act
echo $? 151 >> test.act; echo -e 'a'  | $@ -po '' >> test.act
echo $? 152 >> test.act; echo -e 'A'  | $@ -is 'a' >> test.act
echo $? 153 >> test.act; echo -e 'A'  | $@ -si 'a' >> test.act
echo $? 154 >> test.act; echo -e 'A'  | $@ -Ss 'a' >> test.act
echo $? 155 >> test.act; echo -e 'A'  | $@ -sS 'a' >> test.act
echo $? 156 >> test.act; echo -e 'a'  | $@ -Si 'A' >> test.act
echo $? 157 >> test.act; echo -e 'a'  | $@ -iS 'A' >> test.act
echo $? 158 >> test.act; echo -e 'a'  | $@ -FE '.' >> test.act
echo $? 159 >> test.act; echo -e 'a'  | $@ -EF '.' >> test.act
echo $? 160 >> test.act; echo -e 'a'  | $@ -Hh 'a' >> test.act
echo $? 161 >> test.act; echo -e 'a'  | $@ -hH 'a' >> test.act
echo $? 162 >> test.act; echo -e 'a'  | $@ -kK 'a' >> test.act
echo $? 163 >> test.act; echo -e 'a'  | $@ -Kk 'a' >> test.act
echo $? 164 >> test.act; echo -e 'a'  | $@ -Tt 'a' >> test.act
echo $? 165 >> test.act; echo -e 'a'  | $@ -tT 'a' >> test.act
echo $? 166 >> test.act; echo -e 'a'  | $@ -nN 'a' >> test.act
echo $? 167 >> test.act; echo -e 'a'  | $@ -Nn 'a' >> test.act
echo $? 168 >> test.act; echo -e '\0' | $@ -zZ '.' >> test.act
echo $? 169 >> test.act; echo -e '\0' | $@ -Zz '.' >> test.act
echo $? 170 >> test.act; echo -e '\0' | $@ -1Z '.' >> test.act
echo $? 171 >> test.act; echo -e '\0' | $@ -Z1 '.' >> test.act
echo $? 172 >> test.act; echo -e '\0' | $@ -1z '.' >> test.act
echo $? 173 >> test.act; echo -e '\0' | $@ -z1 '.' >> test.act

diff --text test.exp test.act
# cp test.act test.exp # for updating the test suite
