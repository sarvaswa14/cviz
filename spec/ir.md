# Intermediate representation

Three-address code over an unbounded supply of virtual registers. The
representation is machine independent: it knows nothing about physical
registers, stack frames or instruction encodings, which is what allows the
optimisation passes to be written and tested before a code generator exists.

## 1. Storage

Three kinds of name, which can never collide because a C identifier may
contain neither `%` nor `.`:

    %N        compiler temporary
    name.N    local variable or parameter (the suffix distinguishes shadowed names)
    name      global variable

Scalars live in named storage and have no address. Arrays live in memory and
are reached through `&`, loads and stores. Memory is word addressed: every
scalar element occupies one cell and a pointer value is a cell index. This is
why `&x` on a scalar is rejected during lowering.

## 2. Instructions

    dst = a                     copy
    dst = a op b                binary operation
    dst = op a                  unary operation
    dst = &array                base address of an array
    dst = *a                    load through an address
    *a = b                      store through an address
    L:                          label
    goto L                      unconditional jump
    if a goto L1 else L2        conditional jump
    dst = call f(a, b, ...)     call, dst omitted for void
    ret a                       return, operand omitted for void
    nop                         an instruction removed by an optimisation

Every instruction carries the identifier of the syntax tree node it was
lowered from and the source span of that node. This is the provenance link
that the trace, the visualiser and rewrite bisection all depend on.

Binary and unary instructions carry a width flag. When set, the result wraps
to 32 bits, which is how `int` arithmetic is modelled; `long` and address
arithmetic are computed at 64 bits.

## 3. Lowering rules

| Construct | Lowering |
|---|---|
| `if`, `while`, `do`, `for` | labels and conditional jumps |
| `switch` | a chain of equality tests, then the body in source order, so fall-through needs no special case |
| `&&`, `||` | branches that skip the right operand when the result is already decided |
| `a[i]` | `base + i * cells(element)`, then a load |
| `p + i` | `p + i * cells(pointee)` |
| `p - q` | `(p - q) / cells(pointee)` |
| `x++` used as a value | the old value is copied to a temporary before the update |
| array used in an expression | `&array`, the decay to a pointer |
| `sizeof` | a constant in C byte sizes, independent of the cell model |

## 4. Basic blocks

A block is a maximal run of instructions that control enters only at the
first and leaves only at the last. Leaders are the first instruction, every
label, and every instruction following a jump, branch or return. Successors
come from the last instruction of the block: a jump has one, a branch two, a
return none, anything else falls through.

## 5. Not yet lowered

Floating point, structures and unions, the address of a scalar, calls through
expressions, and array initialiser lists. Each is reported as a lowering
diagnostic rather than silently mistranslated.
