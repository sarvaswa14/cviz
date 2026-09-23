# cviz — an inspectable C compiler

A compiler for a defined subset of ISO C (C99 baseline), written in C++ with
no parser generator and no external libraries. Every phase emits a structured
JSON trace, and a browser visualiser reads it, so the whole translation can be
inspected rather than taken on trust.

BCSE307P Compiler Design Laboratory — Sarvaswa Bhanti, 24BCE0333

## What works today

| Phase | State |
|---|---|
| Lexical analysis | complete, with recovery |
| Syntax analysis | complete, including C declarators |
| Semantic analysis | scoped symbol table, full type checking |
| Intermediate code | three-address code over virtual registers |
| Control-flow graph | basic blocks, successors, reachability |
| Optimisation | five passes, every rewrite individually recorded |
| Execution engine | runs the IR directly; agrees with gcc on the test corpus |
| Code generation | Phase 3 |

## Provenance-guided rewrite bisection

The optimiser records every individual rewrite it makes. The execution engine
runs the unoptimised program as a reference. If the optimised program behaves
differently, cviz binary searches a rewrite budget to find the single rewrite
after which behaviour first changes, then follows that rewrite's recorded
provenance back to the source line that produced it.

    cviz prog.c --validate --inject-fault 5

    validation: MISMATCH between the reference and the optimised program
      bisection : 5 interpreter runs over 29 rewrites
      culprit   : rewrite #5 (copy) in function mix
      reason    : x.5 holds a copy of b.2 at this point
      source    : line 14, column 11

## Build

    make                 # or, on Windows, b.bat

## Use

    cviz file.c --dump-tokens      token stream
    cviz file.c --dump-ast         typed syntax tree
    cviz file.c --dump-ir          three-address code
    cviz file.c --dump-cfg         basic blocks
    cviz file.c --dump-opt         every rewrite, then the optimised code
    cviz file.c --run              execute it
    cviz file.c --validate         check the optimiser, and locate any fault
    cviz file.c --diff-gcc         compare the execution engine with gcc
    cviz file.c --emit-trace t.json
    cviz --eval-random 200         the localisation evaluation
    cviz --generate --gen-seed 7   print a generated test program

Then open `viz/index.html` and load the trace file.

## Reproducing the evaluation

    sh tools/evaluate.sh           # or tools\evaluate.bat on Windows

## Layout

    include/ src/   compiler sources
    spec/           language, IR, optimisation and trace specifications
    tests/          valid and invalid corpora
    viz/            the trace visualiser, one file, no build step
    tools/          evaluation scripts
    docs/           development log
