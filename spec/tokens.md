# Token specification

Baseline: ISO C99. This file defines what the lexer recognises. Whether a
construct is *supported* is a separate question, settled by the parser and
semantic analyser — see the note on reserved keywords below.

## 1. Character handling

Source is read as bytes. A line ends at `\n`; a `\r\n` pair is treated as
one line ending and the `\r` is not part of any token. Every token records
a span (1-based line and column, length in characters on that line).

## 2. Whitespace and comments

Whitespace (space, tab, vertical tab, form feed, newline) separates tokens
and is otherwise discarded.

    /* ... */    block comment, does not nest, may span lines
    // ...       line comment, ends at the newline

An unterminated block comment is a lexical error (`unterminated-comment`)
reported at the position where the comment opened.

## 3. Identifiers

    identifier := ( letter | '_' ) ( letter | digit | '_' )*

Case sensitive. No length limit. An identifier that matches a keyword is
lexed as that keyword, not as an identifier.

## 4. Keywords

All 37 C99 keywords are reserved and lexed as keywords:

    auto      break     case      char      const     continue
    default   do        double    else      enum      extern
    float     for       goto      if        inline    int
    long      register  restrict  return    short     signed
    sizeof    static    struct    switch    typedef   union
    unsigned  void      volatile  while     _Bool     _Complex
    _Imaginary

**Why all of them.** The core subset uses far fewer, but reserving the
full set means `float x;` lexes as KW_FLOAT followed by an identifier and
reaches the parser, which can then reject it with a clear diagnostic. If
`float` were left unreserved it would lex as an identifier and produce a
confusing syntax error instead. This is what implements the proposal's
commitment that unsupported constructs produce a diagnostic rather than
being partially accepted.

Keyword token classes are `KW_` followed by the keyword in upper case:
`KW_INT`, `KW_WHILE`, `KW_BOOL` for `_Bool`, and so on.

## 5. Integer constants

    decimal     := [1-9] digit*
    octal       := '0' [0-7]*
    hexadecimal := ( '0x' | '0X' ) hexdigit+

Optional suffix, any case, in any order: `u`/`U` and up to two `l`/`L`.
The token class is `INT_LIT`. The token carries the numeric value and the
suffix as written.

A hexadecimal prefix with no digits after it is `malformed-integer`.
A digit sequence beginning `0` that contains `8` or `9` is
`invalid-octal-digit`.

## 6. Character constants

    char_const := "'" c_char "'"

`c_char` is any character other than `'`, `\` or newline, or an escape
sequence. Token class `CHAR_LIT`, value is the integer code.

## 7. String literals

    string_lit := '"' s_char* '"'

`s_char` is any character other than `"`, `\` or newline, or an escape
sequence. Token class `STRING_LIT`. Adjacent string literals are *not*
concatenated by the lexer; that is the parser's job if implemented.

An unterminated character or string literal at end of line is
`unterminated-literal`.

## 8. Escape sequences

Valid in character and string literals:

    \'  \"  \?  \\  \a  \b  \f  \n  \r  \t  \v
    \ooo        one to three octal digits
    \xhh...     one or more hex digits

Any other character after a backslash is `unknown-escape`.

## 9. Floating constants

Recognised by the lexer, class `FLOAT_LIT`, so that the parser can reject
them cleanly while `float` and `double` remain stretch items.

    digit+ '.' digit* exponent?  |  '.' digit+ exponent?  |  digit+ exponent
    exponent := ( 'e' | 'E' ) ( '+' | '-' )? digit+

Optional suffix `f`, `F`, `l`, `L`.

## 10. Punctuators

Longest match wins, so `>>=` is one token, not `>>` then `=`.

Three characters:

    <<=  >>=  ...

Two characters:

    ->  ++  --  <<  >>  <=  >=  ==  !=  &&  ||
    +=  -=  *=  /=  %=  &=  ^=  |=

One character:

    [  ]  (  )  {  }  .  &  *  +  -  ~  !  /  %
    <  >  ^  |  ?  :  ;  =  ,  #

Token class names:

    LBRACKET RBRACKET LPAREN RPAREN LBRACE RBRACE
    DOT ARROW PLUSPLUS MINUSMINUS AMP STAR PLUS MINUS TILDE BANG
    SLASH PERCENT LSHIFT RSHIFT LT GT LE GE EQ NE CARET PIPE
    ANDAND OROR QUESTION COLON SEMI ELLIPSIS
    ASSIGN STAR_ASSIGN SLASH_ASSIGN PERCENT_ASSIGN PLUS_ASSIGN
    MINUS_ASSIGN LSHIFT_ASSIGN RSHIFT_ASSIGN AMP_ASSIGN
    CARET_ASSIGN PIPE_ASSIGN COMMA HASH

## 11. End of input

The lexer emits a final `EOF_TOKEN` with a span at the position one past
the last character. The parser relies on this rather than checking for
an empty token list.

## 12. Lexical error classes

    unterminated-comment
    unterminated-literal
    malformed-integer
    invalid-octal-digit
    unknown-escape
    stray-character

`stray-character` covers any byte that cannot begin a token, such as `@`
or `` ` ``. The lexer reports it, skips the character, and continues, so
that a single stray byte does not stop tokenisation.