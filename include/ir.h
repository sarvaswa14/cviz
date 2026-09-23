#pragma once
#include <string>
#include <vector>
#include "ast.h"
#include "source.h"
#include "token.h"

namespace cviz {

// Three-address code. Naming convention, which keeps the three kinds of
// storage from ever colliding:
//   %N       compiler temporaries
//   name.N   locals and parameters (the suffix makes shadowed names unique)
//   name     globals (C identifiers cannot contain '.' or '%')
struct Operand {
    enum class K { None, Const, Var, Str };
    K kind = K::None;
    long long c = 0;
    std::string name;   // Var: storage name. Str: decoded string contents.

    static Operand none()                       { return Operand{}; }
    static Operand constant(long long v)        { Operand o; o.kind = K::Const; o.c = v; return o; }
    static Operand var(const std::string& n)    { Operand o; o.kind = K::Var; o.name = n; return o; }
    static Operand str(const std::string& s)    { Operand o; o.kind = K::Str; o.name = s; return o; }

    bool is_none()  const { return kind == K::None; }
    bool is_const() const { return kind == K::Const; }
    bool is_var()   const { return kind == K::Var; }
    bool is_str()   const { return kind == K::Str; }
    bool operator==(const Operand& o) const { return kind == o.kind && c == o.c && name == o.name; }

    std::string text() const;
};

enum class Op {
    Copy,    // dst = a
    Bin,     // dst = a bop b
    Un,      // dst = bop a
    AddrOf,  // dst = &sym            base address of an array
    Load,    // dst = *a
    Store,   // *a = b
    Label,   // target:
    Jmp,     // goto target
    Br,      // if a goto target else target2
    Call,    // dst = call callee(args)
    Ret,     // ret a
    Nop,     // removed by an optimisation
};

struct Instr {
    int         id = 0;          // unique across the program
    Op          op = Op::Nop;
    std::string dst;
    Operand     a, b;
    Tok         bop = Tok::Error;
    bool        w32 = true;      // result wraps to 32 bits (int arithmetic)
    std::string sym;             // AddrOf: the array
    std::string target, target2; // Label name, jump targets
    std::string callee;
    std::vector<Operand> args;

    NodeId node = -1;            // provenance: the AST node that produced it
    Span   span;

    bool defines() const;
    std::string text() const;
};

struct Global {
    std::string name;
    bool        array = false;
    long long   cells = 1;
    long long   init  = 0;
    Span        span;
};

struct Function {
    std::string name;
    std::vector<std::string> params;
    std::vector<Instr> code;
    std::vector<std::pair<std::string, long long>> arrays;  // local arrays and their size in cells
    NodeId node = -1;
    Span   span;

    size_t live_count() const;
};

struct Program {
    std::vector<Global>   globals;
    std::vector<Function> functions;

    const Function* find(const std::string& name) const;
    size_t live_count() const;
    std::string dump() const;
};

bool is_global_name(const std::string& n);
const char* op_symbol(Tok t);

// One definition of arithmetic, shared by the constant folder and the
// interpreter so the two can never disagree about what an operation means.
// Returns false for division by zero and signed overflow in division.
bool eval_binop(Tok op, long long a, long long b, bool w32, long long& out);
bool eval_unop(Tok op, long long a, bool w32, long long& out);

}  // namespace cviz
