# Trace format (version 2)

Version 2 adds the phases introduced in Phase 2. The top level, spans,
provenance and diagnostics are unchanged from version 1.

Phase names, in pipeline order:
`preprocess`, `lex`, `parse`, `sem`, `ir`, `cfg`, `opt`, `verify`, `codegen`.

## ir

    { "id": 11, "kind": "instr", "stage": "ir", "fn": "main",
      "text": "%3 = a.1 + b.2", "label": false, "block": "B0",
      "from": { "phase": "parse", "id": 38 },
      "span": { "line": 6, "col": 9, "len": 1 } }

## cfg

    { "id": 0, "kind": "block", "fn": "main", "block": "B1",
      "instrs": [12, 13, 14], "succ": ["B2", "B3"], "pred": ["B0"] }

## opt

    { "id": 5, "kind": "rewrite", "pass": "copy", "fn": "mix",
      "reason": "x.5 holds a copy of b.2 at this point",
      "before": "...", "after": "...", "instr": 11,
      "from": { "phase": "parse", "id": 38 }, "span": {...} }

    { "id": 0, "kind": "summary", "before": 41, "after": 28, "rewrites": 37,
      "by_pass": { "const": 21, "copy": 4, "algebraic": 2, "cse": 1, "dce": 9 } }

Instructions after optimisation are emitted in the same phase with
`"stage": "optimised"`.

## verify

    { "id": 0, "kind": "validation", "result": "divergent",
      "ref_output": "...", "ref_ret": 93, "ref_error": "", "ref_steps": 812,
      "opt_output": "...", "opt_ret": 102, "opt_error": "", "opt_steps": 806,
      "rewrites": 29, "runs": 5,
      "culprit": { "id": 5, "pass": "copy", ..., "span": {...} } }

`result` is `equal` when the optimised program matches the reference. When it
is `divergent`, `culprit` carries the single rewrite that bisection blamed,
with the provenance needed to reach the source line.
