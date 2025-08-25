#include "ltre.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct test {
  char *pattern;
  char *input;
  bool matches;
  bool errors;
  bool partial;
  bool ignorecase;
  bool complement;
  bool reverse;
  bool quick;
};

void test(struct test args) {
#define test(...) test((struct test){__VA_ARGS__})
  static struct test memo = {0};
  static struct dstate *dfa = NULL, *ldfa = NULL;

  if (memo.pattern && strcmp(memo.pattern, args.pattern) == 0 &&
      memcmp(&memo.errors, &args.errors, sizeof(bool[6])) == 0)
    goto check_matches;

  // puts(args.pattern); // to test ltre.vim

  char *error = NULL, *loc = args.pattern;
  struct regex *regex = ltre_parse(&loc, &error);

  if (!!error != args.errors)
    printf("test failed: /%s/ parse\n", args.pattern);
  // if (error)
  //   printf("note: /%s/ %s near '%.16s'\n", args.pattern, error, loc);

  if (error)
    return;
  if (args.errors) {
    regex_decref(regex);
    return;
  }

  if (args.partial)
    regex = regex_concat(REGEXES(regex_univ(), regex, regex_univ()));
  if (args.ignorecase)
    regex = regex_ignorecase(regex, false);
  if (args.complement)
    regex = regex_compl(regex);
  if (args.reverse)
    regex = regex_reverse(regex);

  // regex -> pattern -> regex
  char *pattern = ltre_stringify(regex);
  regex = ltre_parse(&pattern, NULL), free(pattern);
  if (regex == NULL)
    abort(); // invariant broken

  // regex -> dfa
  struct dstate *clone;
  dfa_free(dfa), dfa = ltre_compile(regex_incref(regex));

  // dfa -> image -> dfa
  size_t write_size, read_size;
  uint8_t *image = dfa_serialize(dfa, &write_size);
  clone = dfa, dfa = dfa_deserialize(image, &read_size), free(image);

  if (write_size != read_size)
    abort(); // invariant broken
  if (!dfa_equivalent(dfa, clone))
    abort(); // invariant broken
  dfa_free(clone);

  if (!args.quick) {
    // dfa -> regex -> pattern -> regex -> dfa
    struct regex *regex = ltre_decompile(dfa);
    char *pattern = ltre_stringify(regex);
    regex = ltre_parse(&pattern, NULL), free(pattern);
    clone = dfa, dfa = ltre_compile(regex);

    if (!dfa_equivalent(dfa, clone))
      abort(); // invariant broken
    dfa_free(clone);
  }

  dfa_free(ldfa), ldfa = dstate_alloc(regex);

  memo = args;
check_matches:
  if (ltre_matches(dfa, (uint8_t *)args.input) != args.matches ||
      ltre_matches_lazy(&ldfa, (uint8_t *)args.input) != args.matches)
    printf("test failed: /%s/ against '%s'\n", args.pattern, args.input);
}

int main(void) {
  // catastrophic backtracking
  test("a**c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", false);
  test("(x+x+)+y", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", false);

  // powerset construction state blowout
  test("%0%|%1%|%2%|%3%|%4%|%5%", "", false);
  test("%0%|%1%|%2%|%3%|%4%|%5%", "123", true);

  // determinization state blowout
  test("[01]*1[01]{8}", "11011100011100", true, .quick = true);
  test("[01]*1[01]{8}", "01010010010010", false, .quick = true);

  // potential edge cases
  test("abba", "abba", true);
  test("ab|abba", "abba", true);
  test("[ab]+", "abba", true);
  test("[ab]+", "abc", false);
  test(".", "abba", false);
  test(".*", "abba", true);
  test("(a|b+){3}", "abbba", true);
  test("(a|b+){3}", "abbab", false);
  test("\\x61\\+", "a+", true);
  test("a*b+bc", "abbbbc", true);
  test("zzz|b+c", "abbbbc", false);
  test("zzz|ab+c", "abbbbc", true);
  test("a+b|c", "abbbbc", false);
  test("ab+|c", "abbbbc", false);
  test("", "", true);
  test("[]", "", false);
  test("[]*", "", true);
  test("[]+", "", false);
  test("[]?", "", true);
  test("()", "", true);
  test("()*", "", true);
  test("()+", "", true);
  test("()?", "", true);
  test("(())", "", true);
  test("()()", "", true);
  test("()", "a", false);
  test("a()", "a", true);
  test("()a", "a", true);
  test("", "\n", false);
  test("\\n", "\n", true);
  test(".", "\n", true);
  test("\\\\n", "\n", false);
  test("(|n)(\\n)", "\n", true);
  test("\\r?\\n", "\n", true);
  test("\\r?\\n", "\r\n", true);
  test("(a*)*", "a", true);
  test("(a+)+", "aa", true);
  test("(a?)?", "", true);
  test("a+", "aa", true);
  test("a?", "aa", false);
  test("(a+)?", "aa", true);
  test("(ba+)?", "baa", true);
  test("(ab+)?", "b", false);
  test("(a+b)?", "a", false);
  test("(a+a+)+", "a", false);
  test("a+", "", false);
  test("(a+|)+", "aa", true);
  test("(a+|)+", "", true);
  test("(a|b)?", "", true);
  test("(a|b)?", "a", true);
  test("(a|b)?", "b", true);
  test("x*|", "xx", true);
  test("x*|", "", true);
  test("x+|", "xx", true);
  test("x+|", "", true);
  test("x?|", "x", true);
  test("x?|", "", true);
  test("x*y*", "yx", false);
  test("x+y+", "yx", false);
  test("x?y?", "yx", false);
  test("x+y*", "xyx", false);
  test("x*y+", "yxy", false);
  test("x*|y*", "xy", false);
  test("x+|y+", "xy", false);
  test("x?|y?", "xy", false);
  test("x+|y*", "xy", false);
  test("x*|y+", "xy", false);
  test("a{1,2}", "", false);
  test("a{1,2}", "a", true);
  test("a{1,2}", "aa", true);
  test("a{1,2}", "aaa", false);
  test("a{0,}", "", true);
  test("a{0,}", "a", true);
  test("a{0,}", "aa", true);
  test("a{0,}", "aaa", true);
  test("a{1,}", "", false);
  test("a{1,}", "a", true);
  test("a{1,}", "aa", true);
  test("a{1,}", "aaa", true);
  test("a{3,}", "aa", false);
  test("a{3,}", "aaa", true);
  test("a{3,}", "aaaa", true);
  test("a{3,}", "aaaaa", true);
  test("a{0,2}", "", true);
  test("a{0,2}", "a", true);
  test("a{0,2}", "aa", true);
  test("a{0,2}", "aaa", false);
  test("a{2}", "a", false);
  test("a{2}", "aa", true);
  test("a{2}", "aaa", false);
  test("a{0}", "", true);
  test("a{0}", "a", false);

  // partial, ignorecase, complement, reverse
  test("", "", true, .partial = true);
  test("", "abc", true, .partial = true);
  test("b", "abc", true, .partial = true);
  test("ba", "abc", false, .partial = true);
  test("abc", "abc", true, .partial = true);
  test("[]", "", false, .partial = true);
  test("", "", true, .ignorecase = true);
  test("abCdEF", "aBCdEf", true, .ignorecase = true);
  test("ab", "abc", false, .ignorecase = true);
  test("!ab", "ab", true, .ignorecase = true);
  test("a", "", true, .complement = true);
  test("a", "aa", true, .complement = true);
  test("a", "a", false, .complement = true);
  test("ab*", "ac", true, .complement = true);
  test("ab*", "abb", false, .complement = true);
  test("", "", true, .reverse = true);
  test("abc", "abc", false, .reverse = true);
  test("abc", "cba", true, .reverse = true);
  test("a*b", "ba", true, .reverse = true);
  test("a*b", "ab", false, .reverse = true);

  // shorthands
  // we partition \x00-\xff into subranges for which all "ctype.h" functions
  // plus `isascii` have a constant truth value. the test strings consist of
  // a pair of representatives for each such subrange, namely the maximal and
  // minimal elements; when they are the same, like with ' ', one is omitted
  test("<>*", "\1\x08\t\n\r\x0e\x1f !/09:@AFGZ[`afgz{~\x7f\x80\xff", true);
  test("%[]%", "\1\x08\t\n\r\x0e\x1f !/09:@AFGZ[`afgz{~\x7f\x80\xff", false);
  test("\\m*", "09AFGZafgz", true);
  test("%\\m%", "\1\x08\t\n\r\x0e\x1f !/:@[`{~\x7f\x80\xff", false);
  test("\\a*", "AFGZafgz", true);
  test("%\\a%", "\1\x08\t\n\r\x0e\x1f !/09:@[`{~\x7f\x80\xff", false);
  test("\\k*", "\t ", true);
  test("%\\k%", "\1\x08\n\r\x0e\x1f!/09:@AFGZ[`afgz{~\x7f\x80\xff", false);
  test("\\c*", "\1\x08\n\r\x0e\x1f\x7f", true);
  test("%\\c%", " !/09:@AFGZ[`afgz{~\x80\xff", false);
  test("\\d*", "09", true);
  test("%\\d%", "\1\x08\t\n\r\x0e\x1f !/:@AFGZ[`afgz{~\x7f\x80\xff", false);
  test("\\g*", "!/09:@AFGZ[`afgz{~", true);
  test("%\\g%", "\1\x08\t\n\r\x0e\x1f \x7f\x80\xff", false);
  test("\\l*", "afgz", true);
  test("%\\l%", "\1\x08\t\n\r\x0e\x1f !/09:@AFGZ[`{~\x7f\x80\xff", false);
  test("\\p*", " !/09:@AFGZ[`afgz{~", true);
  test("%\\p%", "\1\x08\t\n\r\x0e\x1f\x7f\x80\xff", false);
  test("\\q*", "!/:@[`{~", true);
  test("%\\q%", "\1\x08\t\n\r\x0e\x1f 09AFGZafgz\x7f\x80\xff", false);
  test("\\s*", "\t\n\r ", true);
  test("%\\s%", "\1\x08\x0e\x1f!/09:@AFGZ[`afgz{~\x7f\x80\xff", false);
  test("\\u*", "AFGZ", true);
  test("%\\u%", "\1\x08\t\n\r\x0e\x1f !/09:@[`afgz{~\x7f\x80\xff", false);
  test("\\h*", "09AFaf", true);
  test("%\\h%", "\1\x08\t\n\r\x0e\x1f !/:@GZ[`gz{~\x7f\x80\xff", false);
  test("\\z*", "\1\x08\t\n\r\x0e\x1f !/09:@AFGZ[`afgz{~\x7f", true);
  test("%\\z%", "\x80\xff", false);

  // parse errors
  test("abc]", .errors = true);
  test("[abc", .errors = true);
  test("abc)", .errors = true);
  test("(abc", .errors = true);
  test("+a", .errors = true);
  test("a|*", .errors = true);
  test("\\", .errors = true);
  test("\\x0", .errors = true);
  test("\\yyy", .errors = true);
  test("[a\\x]", .errors = true);
  test("\a", .errors = true);
  test("\b", .errors = true);
  test("~~a", .errors = true);
#define NAT_OVF "9999999999999999999999999999999999999999"
  test("a{" NAT_OVF "}", .errors = true);
  test("a{" NAT_OVF ",}", .errors = true);
  test("a{," NAT_OVF "}", .errors = true);
  test("a{" NAT_OVF "," NAT_OVF "}", .errors = true);

  // nonstandard features
  test("~a", "z", true);
  test("~a", "a", false);
  test("~\\n", "\r", true);
  test("~\\n", "\n", false);
  test("~.", "\n", false);
  test("~.", "a", false);
  test("~a-z*", "1A!2$B", true);
  test("~a-z*", "1aA", false);
  test("a-z*", "abc", true);
  test("~[\\d~\\m]+", "abcABC", true);
  test("~[\\d~\\m]+", "abc123", false);
  test("~[\\d\\M]+", "abcABC", true);
  test("~[\\d~\\M]+", "abc123", false);
  test("[[abc]]+", "abc", true);
  test("[a[bc]]+", "abc", true);
  test("[a[b]c]+", "abc", true);
  test("[a][b][c]", "abc", true);
  test("~[~a~b]", "a", false);
  test("~[~a~b]", "b", false);
  test("~[~a~b]", "", false);
  test("<ab>", "a", false);
  test("<ab>", "b", false);
  test("<ab>", "", false);
  test("\\~", "~", true);
  test("~\\~", "~", false);
  test("~[~\\~]", "~", true);
  test("~[_~[a_b_c]]+", "abc", true);
  test("~[_~[a_b_c]]+", "a_c", false);
  test("<[a_b_c]~_>+", "abc", true);
  test("<[a_b_c]~_>+", "a_c", false);
  test("~[~0-74]+", "0123567", true);
  test("~[~0-74]+", "89", false);
  test("~[~0-74]+", "4", false);
  test("<0-7~4>+", "0123567", true);
  test("<0-7~4>+", "89", false);
  test("<0-7~4>+", "4", false);
  test("[]", " ", false);
  test("~[]", " ", true);
  test("<>", " ", true);
  test("~<>", " ", false);
  test("9-0*", "abc", true);
  test("9-0*", "18", false);
  test("9-0*", "09", true);
  test("9-0*", "/:", true);
  test("b-a*", "ab", true);
  test("a-b*", "ab", true);
  test("a-a*", "ab", false);
  test("a-a*", "aa", true);
  test("\\.-4+", "./01234", true);
  test("5-\\?+", "56789:;<=>?", true);
  test("\\(-\\++", "()*+", true);
  test("\\t-\\r+", "\t\n\v\f\r", true);
  test("a{,2}", "", true);
  test("a{,2}", "a", true);
  test("a{,2}", "aa", true);
  test("a{,2}", "aaa", false);
  test("a{}", "", true);
  test("a{}", "a", false);
  test("a{,}", "", true);
  test("a{,}", "a", true);
  test("a{2}+", "", false);
  test("a{2}+", "a", false);
  test("a{2}+", "aa", true);
  test("a{2}+", "aaa", false);
  test("a{2}+", "aaaa", true);
  test("!", "", false);
  test("!", "a", true);
  test("!", "aa", true);
  test("!0*", "", false);
  test("!0*", "0", false);
  test("!0*", "00", false);
  test("!0*", "001", true);
  test("ab&cd", "", false);
  test("ab&cd", "ab", false);
  test("ab&cd", "cd", false);
  test("...&%a%", "ab", false);
  test("...&%a%", "bc", false);
  test("...&%a%", "abc", true);
  test("...&%a%", "bcd", false);
  test("...=%a%", "ab", false);
  test("...=%a%", "bc", true);
  test("...=%a%", "abc", true);
  test("...=%a%", "bcd", false);
  test("a&b|c", "a", false);
  test("a&b|c", "b", false);
  test("a&b|c", "c", false);
  test("a|b&c", "a", true);
  test("a|b&c", "b", false);
  test("a|b&c", "c", false);
  test("\\m+&!\\d+", "", false);
  test("\\m+&!\\d+", "abc", true);
  test("\\m+&!\\d+", "abc123", true);
  test("\\m+&!\\d+", "1a2b3c", true);
  test("\\m+&!\\d+", "123", false);
  test("0x(!\\h+)", "0yz", false);
  test("0x(!\\h+)", "0x12", false);
  test("0x(!\\h+)", "0x", true);
  test("0x(!\\h+)", "0xy", true);
  test("0x(!\\h+)", "0xyz", true);
  test("0x(%\\H%|)", "0yz", false);
  test("0x(%\\H%|)", "0x12", false);
  test("0x(%\\H%|)", "0x", true);
  test("0x(%\\H%|)", "0xy", true);
  test("0x(%\\H%|)", "0xyz", true);
  test("a!b", "", false);
  test("a!b", "a", true);
  test("a!b", "b", false);
  test("a!b", "aa", false);
  test("\\m{3}+!\\s+", "", false);
  test("\\m{3}+!\\s+", "foo", true);
  test("\\m{3}+!\\s+", "foo bar", true);
  test("\\m{3}+!\\s+", "foobar", false);
  test("\\m{3}+!\\s+", "foo\nbar\rbaz", true);
  test("\\m{3}+!\\s+", "john", false);
  test("\\m{3}+!\\s+", "john doe", false);
  test("(0|1-90-9*)*!\\s+", "", true);
  test("(0|1-90-9*)*!\\s+", "0", true);
  test("(0|1-90-9*)*!\\s+", "0 ", false);
  test("(0|1-90-9*)*!\\s+", "123", true);
  test("(0|1-90-9*)*!\\s+", "1\t2 3", true);
  test("(0|1-90-9*)*!\\s+", "012", false);
  test("(0|1-90-9*)*!\\s+", "0\r 1\n2", true);
  test(":a", "", true);
  test(":a", "a", false);
  test(":a", "ab", false);
  test("a:", "", true);
  test("a:", "a", false);
  test("a:", "ab", false);
  test(":", "", true);
  test(":", "a", true);
  test(":", "ab", false);
  test(":(!/):0*", "/2", true);
  test(":(!/):0*", "1/0", true);
  test(":(!/):0*", "1/2", false);
  test(":(!/):0*", "1/", true);
  test(":(!/):0*", "2/1/0", false);
  test(":(!/):0*", "1/00", true);
  test(":(!/):0*", "0/2", false);
  test(":(!/):0*", "a/b", false);
  test(":(!/):0*", "a-b", true);
  test("0?:{3}", "00", true);
  test("0?:{3}", "10", true);
  test("0?:{3}", "12", true);
  test("0?:{3}", "000", true);
  test("0?:{3}", "120", true);
  test("0?:{3}", "003", true);
  test("0?:{3}", "123", false);
  test("0?:{3}", "0000", true);
  test("0?:{3}", "0001", true);
  test("0?:{3}", "0021", false);
  test("0?:{3}", "00000", true);
  test("0?:{3}", "03000", true);
  test("0?:{3}", "20000", false);
  test("0?:{3}", "000000", false);
  test("0?:{3}", "001000", false);
  test("(\\l*\\d*):?", "", false);
  test("(\\l*\\d*):?", "b", true);
  test("(\\l*\\d*):?", "2", true);
  test("(\\l*\\d*):?", "c3", true);
  test("(\\l*\\d*):?", "4d", false);
  test("(\\l*\\d*):?", "ee56", true);
  test("(\\l*\\d*):?", "f6g", false);
  test("(%\\M%|\\d%)?:*!\\.", "", false);
  test("(%\\M%|\\d%)?:*!\\.", "a", false);
  test("(%\\M%|\\d%)?:*!\\.", "a2", false);
  test("(%\\M%|\\d%)?:*!\\.", "2a", true);
  test("(%\\M%|\\d%)?:*!\\.", "2", true);
  test("(%\\M%|\\d%)?:*!\\.", "a.b", false);
  test("(%\\M%|\\d%)?:*!\\.", "a.b.", true);
  test("(%\\M%|\\d%)?:*!\\.", "a..b", true);
  test("(%\\M%|\\d%)?:*!\\.", "a0b.c1.d", false);
  test("(%\\M%|\\d%)?:*!\\.", "a0c.d1.2", true);
  test("b(!a*)", "", false);
  test("b(!a*)", "b", false);
  test("b(!a*)", "ba", false);
  test("b(!a*)", "bbaa", true);
  test("a*(!)", "", false);
  test("a*(!)", "a", true);
  test("a*(!)", "bc", true);
  test("(!)*", "", true);
  test("(!)*", "a", true);
  test("(!)*", "ab", true);
  test("(!)+", "", false);
  test("(!)+", "a", true);
  test("(!)+", "ab", true);
  test("(!)?", "", true);
  test("(!)?", "a", true);
  test("(!)?", "ab", true);
  test("a**", "a", true);
  test("a*+", "a", true);
  test("a*?", "a", true);
  test("a+*", "a", true);
  test("a++", "a", true);
  test("a+?", "a", true);
  test("a?*", "a", true);
  test("a?+", "a", true);
  test("a??", "a", true);
  test("a*{}", "a", false);
  test("a+{}", "a", false);
  test("a?{}", "a", false);
  test("a{}*", "a", false);
  test("a{}+", "a", false);
  test("a{}?", "a", false);
  test("a{}{}", "a", false);
  test("abc>", .errors = true);
  test("<abc", .errors = true);
  test("[a?b]", .errors = true);
  test("[a-]", .errors = true);
  test("[--]", .errors = true);
  test("[-]", .errors = true);
  test("-", .errors = true);
  test("--", .errors = true);
  test("---", .errors = true);
  test("a-", .errors = true);
  test(".-a", .errors = true);
  test("a-.", .errors = true);
  test("a-\\", .errors = true);
  test("a{2,1}", .errors = true);
  test("a{1 2}", .errors = true);
  test("a{1, 2}", .errors = true);
  test("a{a}", .errors = true);
  test("!!a", .errors = true);
  test("a!!b", .errors = true);

  // realistic regexes
#define HEX_RGB "#\\h{3}{1,2}"
  test(HEX_RGB, "000", false);
  test(HEX_RGB, "#0aA", true);
  test(HEX_RGB, "#00ff", false);
  test(HEX_RGB, "#abcdef", true);
  test(HEX_RGB, "#abcdeff", false);
  // ISO/IEC 9899:TC3, $6.4.9 'Comments'
#define BLOCK_COMMENT "/\\*(!%\\*/%)\\*/"
#define LINE_COMMENT "//~\\n*\\n"
#define COMMENT BLOCK_COMMENT "|" LINE_COMMENT
  test(COMMENT, "// */\n", true);
  test(COMMENT, "// //\n", true);
  test(COMMENT, "/* */", true);
  test(COMMENT, "/*/", false);
  test(COMMENT, "/*/*/", true);
  test(COMMENT, "/**/*/", false);
  test(COMMENT, "/*/**/*/", false);
  test(COMMENT, "/*//*/", true);
  test(COMMENT, "/**/\n", false);
  test(COMMENT, "//**/\n", true);
  test(COMMENT, "///*\n*/", false);
  test(COMMENT, "//\n\n", false);
  // ISO/IEC 9899:TC3, $7.19.6.1 'The fprintf function'.
  // see also gcc-14/gcc/c-family/c-format.cc:713 'print_char_table'
  // and gcc-14/gcc/c-family/c-format.h:25 'enum format_lengths'
#define FIELD_WIDTH "(\\*|1-90-9*)?"
#define PRECISION "(\\.(\\*|1-90-9*)?)?"
#define DI "[\\-\\+\\ 0]*" FIELD_WIDTH PRECISION "(hh|ll|[hljzt])?[di]"
#define U "[\\-0]*" FIELD_WIDTH PRECISION "(hh|ll|[hljzt])?u"
#define OX "[\\-#0]*" FIELD_WIDTH PRECISION "(hh|ll|[hljzt])?[oxX]"
#define FEGA "[\\-\\+\\ #0]*" FIELD_WIDTH PRECISION "[lL]?[fFeEgGaA]"
#define C "\\-*" FIELD_WIDTH "l?c"
#define S "\\-*" FIELD_WIDTH PRECISION "l?s"
#define P "\\-*" FIELD_WIDTH "p"
#define N FIELD_WIDTH "(hh|ll|[hljzt])?n"
#define CONV_SPEC                                                              \
  "\\%(" DI "|" U "|" OX "|" FEGA "|" C "|" S "|" P "|" N "|\\%)"
  test(CONV_SPEC, "%", false);
  test(CONV_SPEC, "%*", false);
  test(CONV_SPEC, "%%", true);
  test(CONV_SPEC, "%5%", false);
  test(CONV_SPEC, "%p", true);
  test(CONV_SPEC, "%*p", true);
  test(CONV_SPEC, "% *p", false);
  test(CONV_SPEC, "%5p", true);
  test(CONV_SPEC, "d", false);
  test(CONV_SPEC, "%d", true);
  test(CONV_SPEC, "%*d", true);
  test(CONV_SPEC, "%**d", false);
  test(CONV_SPEC, "%.16s", true);
  test(CONV_SPEC, "% 5.3f", true);
  test(CONV_SPEC, "%*32.4g", false);
  test(CONV_SPEC, "%-#65.4g", true);
  test(CONV_SPEC, "%03c", false);
  test(CONV_SPEC, "%06i", true);
  test(CONV_SPEC, "%lu", true);
  test(CONV_SPEC, "%hhu", true);
  test(CONV_SPEC, "%Lu", false);
  test(CONV_SPEC, "%-*p", true);
  test(CONV_SPEC, "%-.*p", false);
  test(CONV_SPEC, "%id", false);
  test(CONV_SPEC, "%%d", false);
  test(CONV_SPEC, "i%d", false);
  test(CONV_SPEC, "%c%s", false);
  test(CONV_SPEC, "%0n", false);
  test(CONV_SPEC, "% u", false);
  test(CONV_SPEC, "%+c", false);
  test(CONV_SPEC, "%0-++ 0i", true);
  test(CONV_SPEC, "%30c", true);
  test(CONV_SPEC, "%03c", false);
#define PRINTF_FMT "(~\\%|" CONV_SPEC ")*"
  test(PRINTF_FMT, "%", false);
  test(PRINTF_FMT, "%*", false);
  test(PRINTF_FMT, "%%", true);
  test(PRINTF_FMT, "%5%", false);
  test(PRINTF_FMT, "%id", true);
  test(PRINTF_FMT, "%%d", true);
  test(PRINTF_FMT, "i%d", true);
  test(PRINTF_FMT, "%c%s", true);
  test(PRINTF_FMT, "%u + %d", true);
  test(PRINTF_FMT, "%d:", true);
  // ISO/IEC 9899:TC3, $6.4.1 'Keywords', $6.4.2 'Identifiers' and $6.4.3
  // 'Universal character names'. does not uphold $6.4.3 paragraph 2
#define KEYWORD                                                                \
  "(auto|break|case|char|const|continue|default|do|double|else|enum|extern|"   \
  "float|for|goto|if|inline|int|long|register|restrict|return|short|signed|"   \
  "sizeof|static|struct|switch|typedef|union|unsigned|void|volatile|while|"    \
  "_Bool|_Complex|_Imaginary)"
#define IDENTIFIER "(_|\\m|\\\\u\\h{4}|\\\\U\\h{8})+&!\\d%&!" KEYWORD
  test(IDENTIFIER, "", false);
  test(IDENTIFIER, "_", true);
  test(IDENTIFIER, "_foo", true);
  test(IDENTIFIER, "_Bool", false);
  test(IDENTIFIER, "a1", true);
  test(IDENTIFIER, "5b", false);
  test(IDENTIFIER, "if", false);
  test(IDENTIFIER, "ifa", true);
  test(IDENTIFIER, "bif", true);
  test(IDENTIFIER, "if2", true);
  test(IDENTIFIER, "1if", false);
  test(IDENTIFIER, "\\u12", false);
  test(IDENTIFIER, "\\u1A2b", true);
  test(IDENTIFIER, "\\u1234", true);
  test(IDENTIFIER, "\\u123x", false);
  test(IDENTIFIER, "\\u1234x", true);
  test(IDENTIFIER, "\\U12345678", true);
  test(IDENTIFIER, "\\U1234567y", false);
  test(IDENTIFIER, "\\U12345678y", true);
  // RFC 8259, $7 'Strings'. the RFC requires that UTF-8 be used for open
  // exchange of JSON; this regex lets through any byte outside 7-bit ASCII,
  // and so really accepts a superset of UTF-8-encoded JSON strings. also, the
  // RFC points out that the grammar would allow string values that are invalid
  // Unicode, but doesn't elaborate, so I guess we can let those through too?
#define JSON_STR "\"(~[\\x00-\\x1f\"\\\\]|\\\\[\"\\\\/bfnrt]|\\\\u\\h{4})*\""
  test(JSON_STR, "foo", false);
  test(JSON_STR, "\"foo", false);
  test(JSON_STR, "foo \"bar\"", false);
  test(JSON_STR, "\"foo\\\"", false);
  test(JSON_STR, "\"\\\"", false);
  test(JSON_STR, "\"\"\"", false);
  test(JSON_STR, "\"\"", true);
  test(JSON_STR, "\"foo\"", true);
  test(JSON_STR, "\"foo\\\"\"", true);
  test(JSON_STR, "\"foo\\\\\"", true);
  test(JSON_STR, "\"\\nbar\"", true);
  test(JSON_STR, "\"\nbar\"", false);
  test(JSON_STR, "\"\\abar\"", false);
  test(JSON_STR, "\"foo\\v\"", false);
  test(JSON_STR, "\"\\u1A2b\"", true);
  test(JSON_STR, "\"\\uDEAD\"", true);
  test(JSON_STR, "\"\\uF00\"", false);
  test(JSON_STR, "\"\\uF00BAR\"", true);
  test(JSON_STR, "\"foo\\/\"", true);
  test(JSON_STR, "\"\xcf\x84\"", true);
  test(JSON_STR, "\"\x80\"", true);
  test(JSON_STR, "\"\x88x/\"", true);
  // RFC 8259, $6 'Numbers'
#define JSON_NUM "\\-?(0|1-90-9*)(\\.\\d+)?([eE][\\+\\-]?\\d+)?"
  test(JSON_NUM, "e", false);
  test(JSON_NUM, "1", true);
  test(JSON_NUM, "10", true);
  test(JSON_NUM, "01", false);
  test(JSON_NUM, "-5", true);
  test(JSON_NUM, "+5", false);
  test(JSON_NUM, ".3", false);
  test(JSON_NUM, "2.", false);
  test(JSON_NUM, "2.3", true);
  test(JSON_NUM, "1e", false);
  test(JSON_NUM, "1e0", true);
  test(JSON_NUM, "1E+0", true);
  test(JSON_NUM, "1e-0", true);
  test(JSON_NUM, "1E10", true);
  test(JSON_NUM, "1e+00", true);
  // RFC 8259, $3 'Values'
#define JSON_BOOL "true|false"
#define JSON_NULL "null"
#define JSON_PRIM JSON_STR "|" JSON_NUM "|" JSON_BOOL "|" JSON_NULL
  test(JSON_PRIM, "nul", false);
  test(JSON_PRIM, "null", true);
  test(JSON_PRIM, "nulll", false);
  test(JSON_PRIM, "true", true);
  test(JSON_PRIM, "false", true);
  test(JSON_PRIM, "{}", false);
  test(JSON_PRIM, "[]", false);
  test(JSON_PRIM, "1,", false);
  test(JSON_PRIM, "-5.6e2", true);
  test(JSON_PRIM, "\"1a\\n\"", true);
  test(JSON_PRIM, "\"1a\\n\" ", false);
  // RFC 8259, $5 'Arrays'
#define JSON_ARR(JSON_VAL) WS("\\[") JSON_VAL "*!" WS(",") WS("\\]")
  // RFC 8259, $4 'Objects'
#define JSON_OBJ(JSON_VAL)                                                     \
  WS("\\{") "(" JSON_STR WS("\\:") JSON_VAL ")*!" WS(",") WS("\\}")
  // RFC 8259, $2 'JSON Grammar'. JSON texts are not a regular language because
  // structured types are self-referencial. these macros are written in such a
  // way that `#define JSON_TEXT WS(FIX(JSON_VAL))` would recognize all JSON
  // texts, but we don't have a fixed-point combinator for C macros, so instead
  // we manually unroll it a few times around the base case `"(" JSON_PRIM ")"`
#define JSON_STRUC(JSON_VAL) JSON_ARR(JSON_VAL) "|" JSON_OBJ(JSON_VAL)
#define JSON_VAL(JSON_VAL) "(" JSON_PRIM "|" JSON_STRUC(JSON_VAL) ")"
#define JSON_TEXT WS(JSON_VAL(JSON_VAL("(" JSON_PRIM ")")))
#define WS(FACTOR) "[\\ \\t\\n\\r]*{2}!" FACTOR
  // test cases taken from JSONW's readme
  test(JSON_TEXT, "null", true);
  test(JSON_TEXT, " 123\t", true);
  test(JSON_TEXT, "[1, 2, 3]", true);
  test(JSON_TEXT, "true false", false);
  test(JSON_TEXT, "[1, 2, 3,]", false);
  test(JSON_TEXT, "{ num: 123 }", false);
  test(JSON_TEXT, "[\"foo\", { \"bar\": 123 }, [true]]", true);
  test(JSON_TEXT, "[\"foo\", { \"bar\": 123 }, [true], baz]", false);
  test(JSON_TEXT, "{ \"null\": null, \"true\": true, \"\\\"str\\\"\": \"str\"}",
       true);
  test(JSON_TEXT, "{ \"a\": 0.1, \"b\": 0.2, \"a\": 0.3 }", true);
  test(JSON_TEXT, "[\"abc\", \"\\u0000\", \"ab\\u0000c\"]", true);
  test(JSON_TEXT,
       "{ \"size\": { \"width\": 800, \"height\": 600, \"depth\": 4 } }", true);
  test(JSON_TEXT, "{ \"size\": { \"width\": \"800\", \"height\": 600 } }",
       true);
  test(JSON_TEXT, "{ \"size\": [] }", true);
  test(JSON_TEXT, "[\"foo\", \"bar\"]", true);
  test(JSON_TEXT, "{ \"name\": \"John\", \"birth\": 1978 }", true);
  test(JSON_TEXT, "{ \"birth\": 2010 }", true);
  test(JSON_TEXT, "{ \"name\": \"too long!\" }", true);
  test(JSON_TEXT, "{ \"birth\": 1.2 }", true);
  test(JSON_TEXT, "{ \"age\": 5 }", true);
  // RFC 3629, $3 'UTF-8 definition'. derived from the plain English definition
#define BYTE_PAT                                                               \
  "(\\z|\\xc0-\\xdf\\x80-\\xbf{1}|\\xe0-\\xef\\x80-\\xbf{2}|"                  \
  "\\xf0-\\xf7\\x80-\\xbf{3})"
#define OVERLONG "(\\xc0-\\xc1.|\\xe0\\x80-\\x9f.|\\xf0\\x80-\\x8f..)"
#define SURROGATE "\\xed\\xa0-\\xbf."
#define TOO_BIG "(\\xf4\\x90-\\xff\\x80-\\xbf{2}|\\xf5-\\xff\\x80-\\xbf{3})"
#define UTF8_CHAR_1 "(" BYTE_PAT "&!" OVERLONG "&!" SURROGATE "&!" TOO_BIG ")"
  // RFC 3629, $4 'Syntax of UTF-8 Byte Sequences'. direct transcription of ABNF
#define UTF8_1 "\\x00-\\x7f"
#define UTF8_2 "\\xc2-\\xdf\\x80-\\xbf{1}"
#define UTF8_3                                                                 \
  "\\xe0\\xa0-\\xbf\\x80-\\xbf{1}|\\xe1-\\xec\\x80-\\xbf{2}|"                  \
  "\\xed\\x80-\\x9f\\x80-\\xbf{1}|\\xee-\\xef\\x80-\\xbf{2}"
#define UTF8_4                                                                 \
  "\\xf0\\x90-\\xbf\\x80-\\xbf{2}|\\xf1-\\xf3\\x80-\\xbf{3}|"                  \
  "\\xf4\\x80-\\x8f\\x80-\\xbf{2}"
#define UTF8_CHAR_2 "(" UTF8_1 "|" UTF8_2 "|" UTF8_3 "|" UTF8_4 ")"
  // generated by `ltre_decompile`, with manual adjustments
#define UTF8_CHAR_3                                                            \
  "(\\z|(\\xc2-\\xdf|\\xe0\\xa0-\\xbf|\\xed\\x80-\\x9f|"                       \
  "(<\\xe1-\\xef~\\xed>|\\xf0\\x90-\\xbf|\\xf4\\x80-\\x8f|"                    \
  "\\xf1-\\xf3\\x80-\\xbf)\\x80-\\xbf)\\x80-\\xbf)"
  // all three regular expressions above should accept the same language
#define UTF8_CHAR_ALL UTF8_CHAR_1 "&" UTF8_CHAR_2 "&" UTF8_CHAR_3
#define UTF8_CHARS_ALL UTF8_CHAR_1 "*&" UTF8_CHAR_2 "*&" UTF8_CHAR_3 "*"
#define UTF8_CHAR_SOME UTF8_CHAR_1 "|" UTF8_CHAR_2 "|" UTF8_CHAR_3
#define UTF8_CHARS_SOME UTF8_CHAR_1 "*|" UTF8_CHAR_2 "*|" UTF8_CHAR_3 "*"
  test(UTF8_CHAR_SOME, "ab", false);
  test(UTF8_CHAR_SOME, "\x80x", false);
  test(UTF8_CHAR_SOME, "\x80", false);
  test(UTF8_CHAR_SOME, "\xbf", false);
  test(UTF8_CHAR_SOME, "\xc0", false);
  test(UTF8_CHAR_SOME, "\xc1", false);
  test(UTF8_CHAR_SOME, "\xff", false);
  test(UTF8_CHAR_SOME, "\xed\xa1\x8c", false); // RFC ex. (surrogate)
  test(UTF8_CHAR_SOME, "\xed\xbe\xb4", false); // RFC ex. (surrogate)
  test(UTF8_CHAR_SOME, "\xed\xa0\x80", false); // d800, first surrogate
  test(UTF8_CHAR_SOME, "\xc0\x80", false);     // RFC ex. (overlong nul)
  test(UTF8_CHAR_ALL, "\x7f", true);
  test(UTF8_CHAR_ALL, "\xf0\x9e\x84\x93", true);
  test(UTF8_CHAR_ALL, "\x2f", true);               // solidus
  test(UTF8_CHAR_SOME, "\xc0\xaf", false);         // overlong solidus
  test(UTF8_CHAR_SOME, "\xe0\x80\xaf", false);     // overlong solidus
  test(UTF8_CHAR_SOME, "\xf0\x80\x80\xaf", false); // overlong solidus
  test(UTF8_CHAR_SOME, "\xf7\xbf\xbf\xbf", false); // 1fffff, too big
  test(UTF8_CHARS_ALL, "\x41\xe2\x89\xa2\xce\x91\x2e", true);         // RFC ex.
  test(UTF8_CHARS_ALL, "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4", true); // RFC ex.
  test(UTF8_CHARS_ALL, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", true); // RFC ex.
  test(UTF8_CHARS_ALL, "\xef\xbb\xbf\xf0\xa3\x8e\xb4", true);         // RFC ex.
  test(UTF8_CHARS_ALL, "abcABC123<=>", true);
  test(UTF8_CHARS_ALL, "\xc2\x80", true);
  test(UTF8_CHARS_SOME, "\xc2\x7f", false);     // bad tail
  test(UTF8_CHARS_SOME, "\xe2\x28\xa1", false); // bad tail
  test(UTF8_CHARS_SOME, "\x80x/", false);
  // IPv4 addresses in dot-decimal notation
#define IPV4 "(250-5|(20-4|1\\d|1-9?)\\d){4}!\\."
  test(IPV4, "0.0.0.0", true);
  test(IPV4, "1.1.1.1", true);
  test(IPV4, "10.0.0.4", true);
  test(IPV4, "127.0.0.1", true);
  test(IPV4, "192.168.0.1", true);
  test(IPV4, "55.148.8.11", true);
  test(IPV4, "255.255.255.255", true);
  test(IPV4, "1..1.1", false);
  test(IPV4, "0.0.0.0.", false);
  test(IPV4, ".0.0.0.0", false);
  test(IPV4, "1.1.01.1", false);
  test(IPV4, "10.0.0.256", false);
  test(IPV4, "12.224.29.25.149", false);
  // minimal DFA over the alphabet '0-1' for the regular expression /1[10]*0/
#define FA_PATH(TRANS) "\\u+!0-1&[]:(!\\u0-1\\u|" TRANS "):[]"
#define ACCEPT FA_PATH("A1B|A0D|D[01]D|[BC]1B|[BC]0C") "&A%%[C]"
#define REJECT FA_PATH("A1B|A0D|D[01]D|[BC]1B|[BC]0C") "&A%~[C]"
  test(ACCEPT, "A0D", false);
  test(ACCEPT, "A0C", false);
  test(ACCEPT, "A5B", false);
  test(ACCEPT, "A1B", false);
  test(ACCEPT, "A1B0C", true);
  test(ACCEPT, "A1B1B0C0C", true);
  test(ACCEPT, "B1B0C", false);
  test(REJECT, "A0D", true);
  test(REJECT, "A0C", false);
  test(REJECT, "A5B", false);
  test(REJECT, "A1B", true);
  test(REJECT, "A1B0C", false);
  test(REJECT, "A1B1B0C0C", false);
  test(REJECT, "B1B0C", false);
  // year/month and month/year dates that can be parsed unambiguously, mainly
  // to showcase an exclusive-or operation on regular expressions
#define YEAR "(\\d{4}|[05-9]\\d)"
#define MONTH "(0?1-9|10-2)"
#define UNAMBIGUOUS YEAR "/" MONTH "=!" MONTH "/" YEAR
  test(UNAMBIGUOUS, "3/98", true);
  test(UNAMBIGUOUS, "05/98", true);
  test(UNAMBIGUOUS, "10/98", true);
  test(UNAMBIGUOUS, "98/12", true);
  test(UNAMBIGUOUS, "98/13", false);
  test(UNAMBIGUOUS, "98/17", false);
  test(UNAMBIGUOUS, "07/55", true);
  test(UNAMBIGUOUS, "07/14", false);
  test(UNAMBIGUOUS, "07/1914", true);
  test(UNAMBIGUOUS, "07/2014", true);
  test(UNAMBIGUOUS, "3/2", false);
  test(UNAMBIGUOUS, "3/02", true);
  test(UNAMBIGUOUS, "03/2", true);
  test(UNAMBIGUOUS, "03/02", false);
  test(UNAMBIGUOUS, "03/2002", true);
  test(UNAMBIGUOUS, "2003/02", true);
  test(UNAMBIGUOUS, "11/12", false);
  test(UNAMBIGUOUS, "2011/12", true);
  test(UNAMBIGUOUS, "11/2012", true);
  // constructed by state removal on the minimal 3-state DFA over the alphabet
  // '0-9'. test cases are random numbers 1..1e12, logarithmically distributed
#define DIV_BY_3                                                               \
  "([0369]|[147][0369]*[258]|([258]|[147][0369]*[147])"                        \
  "([0369]|[258][0369]*[147])*([147]|[258][0369]*[258]))*"
  test(DIV_BY_3, "", true);
  test(DIV_BY_3, "3", true);
  test(DIV_BY_3, "4818", true);
  test(DIV_BY_3, "756", true);
  test(DIV_BY_3, "146", false);
  test(DIV_BY_3, "446127512", false);
  test(DIV_BY_3, "24641410726", false);
  test(DIV_BY_3, "6012627460", false);
  test(DIV_BY_3, "91564250", false);
  test(DIV_BY_3, "2308562", false);
  test(DIV_BY_3, "76", false);
  test(DIV_BY_3, "2222530", false);
  test(DIV_BY_3, "18", true);
  test(DIV_BY_3, "10361335", false);
  test(DIV_BY_3, "1374", true);
  test(DIV_BY_3, "70", false);
  test(DIV_BY_3, "26054309489", false);
  test(DIV_BY_3, "124859573097", true);
#define PWD_REQS "\\p{8,}&%\\l%&%\\u%&%\\d%&%\\q%"
  test(PWD_REQS, "pa$$W0rd", true);
  test(PWD_REQS, "Password1!", true);
  test(PWD_REQS, "Password1", false);
  test(PWD_REQS, "password1!", false);
  test(PWD_REQS, "PASSWORD1!", false);
  test(PWD_REQS, "Password!", false);
  test(PWD_REQS, "Pass1!", false);
  test(PWD_REQS, "Password\t1!", false);
}
