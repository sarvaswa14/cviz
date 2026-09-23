# Development log

## Phase 1

Repository scaffold, trace format specification, token and grammar
specifications for the core subset. Implemented source handling, token
classes, and the hand-written lexer with error recovery. Verified on a valid
program and on a corpus of four lexical error classes; all four reported with
correct spans and the scan continuing past each.

Implemented the AST, the recursive-descent parser including declarator
handling, trace emission for the lexical and parse phases, and the
visualiser. Verified declarator construction on seven forms; `int *ptrs[5]`
and `int (*aptr)[5]` produce distinct types as required.

Implemented semantic analysis: scope stack with shadowing, symbol
declaration and lookup, and a type checker covering integer promotion, the
usual arithmetic conversions, array-to-pointer decay, pointer arithmetic,
call signatures, lvalue requirements and return-type agreement. Verified
against a corpus of seven semantic error classes.

## Phase 2

Designed the IR (spec/ir.md) before writing the lowering pass, on the same
principle as the trace format in Phase 1: a representation whose shape is
fixed first is one the later phases cannot quietly bend.

Implemented lowering from the typed tree to three-address code, carrying the
syntax tree node and source span onto every instruction. Added the
control-flow graph, then five optimisation passes, each rewrite individually
recorded with the reason it was justified.

Implemented the execution engine. Checked it against gcc on the hand-written
corpus (14 of 14 agree on output and exit status) before using it as an
oracle for anything else, since an oracle that is wrong is worse than none.

Implemented rewrite bisection. Added deliberate faults and a random program
generator so that localisation accuracy could be measured rather than
asserted: 200 generated programs, 1348 trials in which behaviour actually
changed, all 1348 localised to the exact rewrite responsible.

Two findings worth recording. Constant propagation runs before the redundancy
passes and consumes most opportunities, so a hand-written test for the CSE
fault has to take its values from parameters. And copy propagation rewrites
operands before CSE sees them, which masks the CSE fault entirely unless copy
propagation is disabled; `--no-copy` exists for that reason.
