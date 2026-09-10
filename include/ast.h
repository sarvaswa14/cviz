#pragma once
#include <memory>
#include <string>
#include <vector>
#include "source.h"
#include "token.h"

namespace cviz {

struct Expr;
struct Stmt;
struct Decl;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using DeclPtr = std::unique_ptr<Decl>;

// Every node carries an id, unique within the parse phase. The trace uses
// it for provenance: an IR instruction names the node id it came from.
// See spec/trace-format.md, "Provenance".
using NodeId = int;

// The type a declarator denotes, built inside-out as the declarator is
// read outside-in (spec/grammar.md section 3). Kept structural rather
// than as a field on the AST, because semantic analysis and code
// generation both consult it independently.

struct Type;
using TypePtr = std::shared_ptr<Type>;

enum class TypeKind { Void, Char, Short, Int, Long, Float, Double, Bool,
                      Pointer, Array, Function, Struct };

struct Type {
    TypeKind kind = TypeKind::Int;
    bool     is_unsigned = false;

    TypePtr  base;                  // Pointer: pointee. Array/Function: element/return.
    long long array_len = -1;       // Array: -1 when unsized, as in int a[].
    std::vector<TypePtr> params;    // Function: parameter types.
    std::string tag;                // Struct: the tag name, may be empty.

    static TypePtr make(TypeKind k, bool uns = false);
    static TypePtr pointer_to(TypePtr b);
    static TypePtr array_of(TypePtr b, long long n);
    static TypePtr function(TypePtr ret, std::vector<TypePtr> ps);

    std::string to_string() const;
};


enum class ExprKind {
    IntLit, CharLit, StringLit, FloatLit, Ident,
    Unary,        // op applied to operand
    Binary,       // lhs op rhs
    Assign,       // lhs op= rhs, op == Tok::Assign for plain assignment
    Conditional,  // cond ? then_expr : else_expr
    Call,         // callee(args)
    Index,        // base[index]
    Member,       // base.name  or  base->name
    Cast,         // (type)operand
    SizeofExpr,   // sizeof operand
    SizeofType,   // sizeof(type)
    PostIncDec,   // operand++ or operand--
    PreIncDec,    // ++operand or --operand
    Comma,        // lhs, rhs
};

struct Expr {
    ExprKind kind;
    NodeId   id   = 0;
    Span     span;

    // Filled by semantic analysis, not by the parser.
    TypePtr  type;

    // literals and identifiers
    long long   int_value = 0;
    std::string text;              // identifier name, string literal body

    // operators
    Tok      op = Tok::Error;      // Unary, Binary, Assign, Pre/PostIncDec
    bool     arrow = false;        // Member: true for ->, false for .

    ExprPtr  lhs;                  // Binary/Assign lhs, Unary operand,
                                   // Call callee, Index base, Member base,
                                   // Cast operand, sizeof operand
    ExprPtr  rhs;                  // Binary/Assign rhs, Index subscript
    ExprPtr  third;                // Conditional else branch

    std::vector<ExprPtr> args;     // Call arguments
    TypePtr  cast_type;            // Cast target, SizeofType operand

    // A short label for the trace, e.g. "+" for a Binary, the name for an
    // Ident, the literal text for a constant.
    std::string label() const;
    const char* kind_name() const;
};


enum class StmtKind {
    Compound, ExprStmt, If, While, DoWhile, For, Switch,
    Case, Default, Break, Continue, Return, DeclStmt, Empty,
};

struct Stmt {
    StmtKind kind;
    NodeId   id = 0;
    Span     span;

    ExprPtr  expr;                 // ExprStmt, If/While/Switch condition,
                                   // Return value, Case constant
    ExprPtr  init_expr;            // For: initialiser
    ExprPtr  cond_expr;            // For: condition
    ExprPtr  step_expr;            // For: step

    StmtPtr  body;                 // If then-branch, loop body, Case body
    StmtPtr  else_body;            // If else-branch

    std::vector<StmtPtr> items;    // Compound: declarations and statements
    DeclPtr  decl;                 // DeclStmt

    const char* kind_name() const;
};


// One declarator from a declaration: a name, the type it denotes, and an
// optional initialiser. `int *a, b[4];` produces two of these.
struct Declarator {
    std::string name;
    TypePtr     type;
    ExprPtr     init;
    Span        span;
};

enum class DeclKind { Variable, Function, StructDef };

struct Decl {
    DeclKind kind = DeclKind::Variable;
    NodeId   id = 0;
    Span     span;

    std::vector<Declarator> declarators;   // Variable

    // Function definition
    std::string              func_name;
    TypePtr                  func_type;
    std::vector<std::string> param_names;
    StmtPtr                  body;

    // StructDef
    std::string tag;
    std::vector<Declarator> members;

    const char* kind_name() const;
};


struct TranslationUnit {
    std::vector<DeclPtr> decls;
};

}  // namespace cviz