setlocal commentstring=(\ %s\ ){}
setlocal comments=sb:(,mb:\|,e:){}

syntax match ltreLiteralChar '[^\\\-.~\[\]<>%{}*+?:|&=!() \x00-\x20\x7f-\xff]'
syntax match ltreMetaEscape '\\[\\\-.~\[\]<>%{}*+?:|&=!() ]'hs=s+1
syntax match ltreSymMetaEsc '\\[\\\-.~\[\]<>%{}*+?:|&=!() ]'he=e-1 contained
syntax match ltreSimpleEscape '\\[bfnrtve]'
syntax match ltreHexEscape '\\x\x\x'
syntax match ltreSymsetCompl '\~\([^\\]\|\\[^x]\|\\x\x\x\)' " moved up for lower priority
syntax match ltreCharRange '\~\?\([^\\]\|\\[^x]\|\\x\x\x\)-\([^\\]\|\\[^x]\|\\x\x\x\)'
syntax match ltreSymsetWild '\~\?\.'
syntax match ltreShorthand '\~\?\\[mMaAkKcCdDgGlLpPqQsSuUhHzZ]'
syntax region ltreSymsetUnion matchgroup=PreProc start='\~\?\[' skip='\\.' end='[\]>%{}*+?:|&=!()]'
      \ keepend extend contains=ltreSymMetaEsc,ltreSimpleEscape,ltreHexEscape,ltreCharRange,
      \ ltreSymsetWild,ltreSymsetCompl,ltreShorthand,ltreSymsetUnion,ltreSymsetInter
syntax region ltreSymsetInter matchgroup=PreProc start='\~\?<' skip='\\.' end='[\]>%{}*+?:|&=!()]'
      \ keepend extend contains=ltreSymMetaEsc,ltreSimpleEscape,ltreHexEscape,ltreCharRange,
      \ ltreSymsetWild,ltreSymsetCompl,ltreShorthand,ltreSymsetUnion,ltreSymsetInter
syntax match ltreWildcard '%'
syntax match ltreDualConcat ':' " moved up for lower priority
syntax match ltreQuantifier ':\?\([*+?]\|{\d*\(,\d*\)\?}\)!\?'
syntax match ltreBooleanOp '[|&=!]'
syntax region ltreComment start='(\s' end='){}' contains=ltreTodo
syntax keyword ltreTodo TODO FIXME XXX NOTE contained

highlight default link ltreLiteralChar Character
highlight default link ltreMetaEscape Character
highlight default link ltreSymMetaEsc Default
highlight default link ltreSimpleEscape SpecialChar
highlight default link ltreHexEscape SpecialChar
highlight default link ltreCharRange Identifier
highlight default link ltreSymsetWild Identifier
highlight default link ltreShorthand Identifier
highlight default link ltreSymsetCompl Identifier
highlight default link ltreSymsetUnion PreProc
highlight default link ltreSymsetInter PreProc
highlight default link ltreWildcard Keyword
highlight default link ltreQuantifier Repeat
highlight default link ltreDualConcat Operator
highlight default link ltreBooleanOp Operator
highlight default link ltreComment Comment
highlight default link ltreTodo Todo
