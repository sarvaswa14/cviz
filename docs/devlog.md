## 2026-09-10 (contd.)
Implemented the AST, the recursive-descent parser including declarator
handling, trace emission for the lexical and parse phases, and the
visualiser. Verified declarator construction on seven forms; int *ptrs[5]
and int (*aptr)[5] produce distinct types as required. Trace loads in the
visualiser and source spans resolve for both tokens and tree nodes.

## 2026-09-10 (contd.)
Implemented semantic analysis: scope stack with shadowing, symbol
declaration and lookup, and a type checker covering integer promotion,
the usual arithmetic conversions, array-to-pointer decay, pointer
arithmetic, call signatures, lvalue requirements and return-type
agreement. Verified against a corpus of seven semantic error classes.
Semantic events are emitted to the trace as scope open/close, symbol
declarations and per-node type assignments.