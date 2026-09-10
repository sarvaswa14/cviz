## 2026-09-10 (contd.)
Implemented the AST, the recursive-descent parser including declarator
handling, trace emission for the lexical and parse phases, and the
visualiser. Verified declarator construction on seven forms; int *ptrs[5]
and int (*aptr)[5] produce distinct types as required. Trace loads in the
visualiser and source spans resolve for both tokens and tree nodes.