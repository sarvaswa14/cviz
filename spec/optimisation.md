# Optimisation and rewrite bisection

## 1. Passes

Five passes run in a fixed order until a round makes no change, to a maximum
of eight rounds.

| Pass | Scope | What it does |
|---|---|---|
| `const` | basic block | propagates known constants into operands, folds constant operations, turns a branch on a constant into a jump |
| `copy` | basic block | after `d = s`, later reads of `d` read `s` instead |
| `algebraic` | instruction | `x + 0`, `x * 1`, `x - x`, `x & 0`, `x << 0` and similar identities |
| `cse` | basic block | reuses an expression already computed into a variable |
| `dce` | function | removes unreachable blocks, then assignments whose result is never read, using liveness over the control-flow graph |

Common-subexpression elimination is local to a block. A global formulation
would need reaching-definition analysis across the whole graph and is not
claimed. Calls invalidate everything known about globals; scalars have no
address, so nothing else can change them behind the optimiser's back.

## 2. Rewrites

Every individual transformation is a **rewrite**, recorded with

* a sequence number, assigned in the order rewrites are attempted,
* the pass, the function and the round,
* the instruction before and after,
* the reason the transformation was justified,
* the provenance inherited from the instruction, and so the source span.

## 3. The rewrite budget

A budget of *N* applies the first *N* rewrites of the unrestricted run and no
others. The attempt counter advances whether or not a rewrite is applied, and
the passes are deterministic, so for every *N* the first *N* rewrites are the
same ones the unrestricted run made. This prefix property is what makes
binary search over the budget meaningful.

## 4. Bisection

The unoptimised program is the reference. If the optimised program behaves
differently, budgets are binary searched between 0, which reproduces the
reference by construction, and the total, which is known to diverge. The
smallest budget that still diverges identifies the rewrite after which
behaviour first changes. Its recorded provenance gives the source line.

Cost is about log2(R) executions of the program rather than R.

## 5. Deliberate faults

Used only to evaluate localisation; they are never enabled in a normal build.

| # | Fault | Pass |
|---|---|---|
| 1 | folds `a - b` as `b - a` | const |
| 2 | rewrites `0 - x` as `x` | algebraic |
| 3 | keeps an expression available after an operand changes | cse |
| 4 | ignores uses that are call arguments | dce |
| 5 | keeps a copy after its source changes | copy |

A separate mode perturbs the constant produced by one chosen rewrite by one,
which gives a large population of single-rewrite faults with exact ground
truth.
