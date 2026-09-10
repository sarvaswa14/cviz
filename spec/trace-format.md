# Trace format (version 1)

The compiler writes one JSON object per run. The visualiser reads it and
holds no other access to the compiler.

## Top level

    {
      "version": 1,
      "source": { "file": "...", "lines": ["int main(void) {", "..."] },
      "phases": [ <phase>, ... ]
    }

`lines` is the source split by line, 0-indexed in the array, so line N of
the file is `lines[N-1]`.

## Span

Every artefact that comes from source text carries a span:

    "span": { "line": 3, "col": 9, "len": 3 }

`line` and `col` are 1-based. `len` is in characters on that line.
Artefacts with no source origin (a compiler-generated temporary, say)
omit `span` entirely rather than using a null or a zero span.

## Phase record

    {
      "phase": "lex",
      "status": "ok" | "error",
      "events": [ <event>, ... ],
      "diagnostics": [ <diagnostic>, ... ]
    }

Phase names, in pipeline order:
`preprocess`, `lex`, `parse`, `sem`, `ir`, `cfg`, `opt`, `codegen`.

A phase that did not run (because an earlier one failed) is omitted.

## Provenance

Every event has an integer `id` unique within its phase. An event that was
produced from an artefact of an earlier phase names it:

    "from": { "phase": "parse", "id": 12 }

This is what lets the visualiser relate an instruction back to the tree
node that produced it, and that node back to the source span. Provenance
is recorded by the phase that creates the artefact; it is never inferred
by the visualiser.

## Events by phase

### preprocess
    { "id": 0, "kind": "define",  "name": "MAX", "body": "100", "span": {...} }
    { "id": 1, "kind": "expand",  "name": "MAX", "span": {...} }
    { "id": 2, "kind": "include", "path": "util.h", "span": {...} }
    { "id": 3, "kind": "skip",    "reason": "ifdef", "span": {...} }

### lex
    { "id": 0, "kind": "token", "tclass": "IDENT", "lexeme": "sum",
      "span": {...} }

`tclass` is the token class name as given in spec/tokens.md.

### parse
    { "id": 12, "kind": "node", "node": "Binary", "label": "+",
      "children": [10, 11], "span": {...} }

Children are parse-phase ids. The root node of each function is marked
`"root": true`.

### sem
    { "id": 0, "kind": "scope_open",  "scope": 2, "of": "block" }
    { "id": 1, "kind": "symbol", "name": "i", "type": "int", "scope": 2,
      "span": {...} }
    { "id": 2, "kind": "type", "type": "int",
      "from": { "phase": "parse", "id": 12 } }
    { "id": 3, "kind": "scope_close", "scope": 2 }

### ir
    { "id": 5, "kind": "instr", "text": "t1 = i < n", "block": "B1",
      "from": { "phase": "parse", "id": 12 } }

### cfg
    { "id": 1, "kind": "block", "block": "B1", "instrs": [4, 5],
      "succ": ["B2", "B3"] }

### opt
    { "id": 0, "kind": "change", "pass": "constfold",
      "removed": [7, 8], "added": ["x = 34"],
      "reason": "operands known at compile time" }

### codegen
    { "id": 9, "kind": "interval", "vreg": "t1", "start": 4, "end": 9,
      "reg": "%eax" }
    { "id": 10, "kind": "asm", "text": "addl %esi, %eax",
      "from": { "phase": "ir", "id": 5 } }

## Diagnostic

    {
      "class": "lexical" | "syntax" | "semantic",
      "code": "undeclared-identifier",
      "message": "use of undeclared identifier 'sum'",
      "span": {...}
    }

`code` is a stable identifier. The negative test corpus asserts on `code`
and `span`, never on `message`, so wording can change without breaking
tests.