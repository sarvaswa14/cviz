# Grammar of the core subset

EBNF. `{ x }` means zero or more, `[ x ]` means optional, `|` alternates.
Terminals are token classes from spec/tokens.md, written in upper case or
as literal punctuation in quotes.

Baseline C99. This grammar covers the **core** subset only. Stretch
constructs are listed in section 8; they are lexed and parsed far enough
to be rejected with a diagnostic, not silently accepted.

## 1. Translation unit

    translation_unit := { external_decl } EOF_TOKEN

    external_decl    := function_def | declaration

    function_def     := decl_specifiers declarator compound_stmt

    declaration      := decl_specifiers [ init_declarator_list ] ';'

A function definition and a declaration share a prefix, so the parser
parses `decl_specifiers declarator` first and then decides: `{` means a
function definition, anything else means a declaration.

## 2. Declaration specifiers

    decl_specifiers  := type_specifier { type_specifier }

    type_specifier   := KW_VOID | KW_CHAR | KW_SHORT | KW_INT | KW_LONG
                      | KW_SIGNED | KW_UNSIGNED
                      | struct_specifier

    struct_specifier := KW_STRUCT [ IDENT ] '{' { struct_decl } '}'
                      | KW_STRUCT IDENT

    struct_decl      := decl_specifiers declarator_list ';'

    declarator_list  := declarator { ',' declarator }

Multiple type specifiers combine, so `unsigned long int` is one type. The
legal combinations are checked in semantic analysis, not here — the
grammar accepts any sequence and the analyser rejects `int void`.

## 3. Declarators

This is the difficult part of C syntax. The declarator is read
outside-in, and the type it denotes is assembled inside-out.

    init_declarator_list := init_declarator { ',' init_declarator }

    init_declarator      := declarator [ '=' initialiser ]

    declarator           := { '*' } direct_declarator

    direct_declarator    := IDENT
                          | '(' declarator ')'
                          | direct_declarator '[' [ INT_LIT ] ']'
                          | direct_declarator '(' [ param_list ] ')'

    param_list           := param_decl { ',' param_decl }
                          | KW_VOID

    param_decl           := decl_specifiers declarator
                          | decl_specifiers abstract_declarator

    abstract_declarator  := { '*' } [ '(' abstract_declarator ')' ]
                            { '[' [ INT_LIT ] ']' }

    initialiser          := assignment_expr
                          | '{' initialiser { ',' initialiser } [ ',' ] '}'

`direct_declarator` is left-recursive as written. The parser implements
it as a loop: parse the base (IDENT or a parenthesised declarator), then
repeatedly consume `[` or `(` suffixes, applying each to the type built
so far.

**Worked example.** `int *arr[10]`

    declarator        = '*' direct_declarator
    direct_declarator = arr [ 10 ]

Suffixes bind tighter than the leading `*`, so the array applies first:
`arr` is an array of 10, of pointer to int. Whereas `int (*arr)[10]`
parenthesises the `*`, so the pointer applies first: `arr` is a pointer
to an array of 10 int.

## 4. Statements

    statement       := compound_stmt
                     | expression_stmt
                     | selection_stmt
                     | iteration_stmt
                     | jump_stmt

    compound_stmt   := '{' { declaration | statement } '}'

    expression_stmt := [ expression ] ';'

    selection_stmt  := KW_IF '(' expression ')' statement
                       [ KW_ELSE statement ]
                     | KW_SWITCH '(' expression ')' statement

    labeled_stmt    := KW_CASE constant_expr ':' statement
                     | KW_DEFAULT ':' statement

    iteration_stmt  := KW_WHILE '(' expression ')' statement
                     | KW_DO statement KW_WHILE '(' expression ')' ';'
                     | KW_FOR '(' [ expression ] ';' [ expression ] ';'
                                  [ expression ] ')' statement

    jump_stmt       := KW_CONTINUE ';'
                     | KW_BREAK ';'
                     | KW_RETURN [ expression ] ';'

`labeled_stmt` appears only inside the statement of a `switch`; the
parser accepts it wherever a statement is expected and the semantic
analyser rejects a `case` outside a `switch`.

The dangling `else` is resolved by binding to the nearest unmatched `if`,
which recursive descent gives for free: on seeing `else`, consume it
immediately rather than returning.

C99 permits declarations inside a compound statement to be interleaved
with statements, which is why `compound_stmt` alternates rather than
requiring all declarations first.

## 5. Expressions

Sixteen precedence levels, lowest first. The parser implements levels 3
through 14 by precedence climbing over a table of binary operators, and
the rest as explicit functions.

     1  expression        := assignment_expr { ',' assignment_expr }
     2  assignment_expr   := conditional_expr
                           | unary_expr assign_op assignment_expr
     3  conditional_expr  := logical_or_expr
                             [ '?' expression ':' conditional_expr ]
     4  logical_or_expr   := logical_and_expr { '||' logical_and_expr }
     5  logical_and_expr  := or_expr          { '&&' or_expr }
     6  or_expr           := xor_expr         { '|'  xor_expr }
     7  xor_expr          := and_expr         { '^'  and_expr }
     8  and_expr          := equality_expr    { '&'  equality_expr }
     9  equality_expr     := relational_expr  { ( '==' | '!=' ) relational_expr }
    10  relational_expr   := shift_expr       { ( '<' | '>' | '<=' | '>=' ) shift_expr }
    11  shift_expr        := additive_expr    { ( '<<' | '>>' ) additive_expr }
    12  additive_expr     := mult_expr        { ( '+' | '-' ) mult_expr }
    13  mult_expr         := cast_expr        { ( '*' | '/' | '%' ) cast_expr }
    14  cast_expr         := unary_expr | '(' type_name ')' cast_expr
    15  unary_expr        := postfix_expr
                           | ( '++' | '--' ) unary_expr
                           | unary_op cast_expr
                           | KW_SIZEOF unary_expr
                           | KW_SIZEOF '(' type_name ')'
    16  postfix_expr      := primary_expr { postfix_suffix }

    postfix_suffix    := '[' expression ']'
                       | '(' [ arg_list ] ')'
                       | '.' IDENT
                       | '->' IDENT
                       | '++' | '--'

    primary_expr      := IDENT | INT_LIT | CHAR_LIT | STRING_LIT
                       | '(' expression ')'

    arg_list          := assignment_expr { ',' assignment_expr }

    unary_op          := '&' | '*' | '+' | '-' | '~' | '!'

    assign_op         := '=' | '*=' | '/=' | '%=' | '+=' | '-='
                       | '<<=' | '>>=' | '&=' | '^=' | '|='

    type_name         := decl_specifiers [ abstract_declarator ]

    constant_expr     := conditional_expr

Assignment is right-associative and its left side must be a unary
expression, which is what makes `a + b = c` a syntax error rather than a
semantic one. Whether the left side is a *modifiable lvalue* is a
semantic question, checked later.

`cast_expr` and `'(' expression ')'` both begin with `(`, so the parser
looks at the token after `(`: a type specifier keyword means a cast,
anything else means a parenthesised expression.

## 6. Preprocessing directives

Handled before parsing, on the token stream.

    directive := '#' KW_DEFINE IDENT { token } NEWLINE
               | '#' KW_IFDEF IDENT NEWLINE
               | '#' KW_IFNDEF IDENT NEWLINE
               | '#' KW_ELSE NEWLINE
               | '#' KW_ENDIF NEWLINE
               | '#' KW_INCLUDE '"' path '"' NEWLINE

Object-like macros only in core; function-like macros are stretch.
`#include` resolves relative to the including file's directory.

## 7. Error recovery

On a syntax error the parser reports it, then discards tokens until it
reaches a synchronisation point: a `;` at the current brace depth, a `}`
that closes the current block, or a token that can begin a declaration at
file scope. Parsing then resumes. This is what allows several independent
errors to be reported from one run.

## 8. Stretch constructs

Parsed only as far as needed to reject them clearly:

    float, double, _Bool          type specifiers
    union, enum, typedef          declarations
    goto, labels                  jump statements
    function-pointer declarators  e.g. int (*f)(int)
    variadic parameters           '...'
    function-like macros

Each produces a `not-in-subset` diagnostic naming the construct, so the
message is specific rather than a generic parse failure.