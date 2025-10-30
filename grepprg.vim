set grepprg=ltrep\ -Hnk
set grepformat=%f:%l:%c:%m

" keep in mind that when using |:grep| there are three interpreters reading
" over the pattern:
"  1. Vim. to inhibit expansion of |, %, #, and <cword> and friends,
"     backslash-escape them. backslashes don't need escaping. for more
"     information see |cmdline-lines|, |cmdline-special|, ex_cmds.h, and
"     do_one_cmd(), expand_filename() and eval_vars() in ex_docmd.c.
"  2. your shell. in most shells you can backslash-escape metacharacters
"     or use a single-quoted string.
"  3. the grep. metacharacters need escaping to be matched literally.
" for example, to grep for a literal | recursively, use :gr \\\\| **<cr>. Vim
" turns \| into | and gives \\\|, then the shell turns \\ into \ and \| into |
" and gives \|, then the grep turns \| into | and searches for that.
