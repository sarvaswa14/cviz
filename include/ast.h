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

// Unique within the parse phase; used for trace provenance.
using NodeId = int;

struct Type;
using TypePtr = std::shared_ptr<Type>;

enum class TypeKind { Void, Char, Short, Int, Long, Float, Double, Bool,
                      Pointer, Array, Function, Struct };

// Types are recursive: pointer-to, array-of and function-returning wrap a
// base type. Shared because many nodes refer to the same type object.
struct Type {
    TypeKind kind = TypeKind::Int;
    bool     is_unsigned = false;
    bool     variadic = false;       // Function: accepts extra arguments

    TypePtr  base;
    long long array_len = -1;
    std::vector<TypePtr> params;
    std::string tag;

    static TypePtr make(TypeKind k, bool uns = false);
    static TypePtr pointer_to(TypePtr b);
    static TypePtr array_of(TypePtr b, long long n);
    static TypePtr function(TypePtr ret, std::vector<TypePtr> ps);

    std::string to_string() const;
};

enum class ExprKind {
    IntLit, CharLit, StringLit, FloatLit, Ident,
    Unary, Binary, Assign, Conditional, Call, Index, Member,
    Cast, SizeofExpr, SizeofType, PostIncDec, PreIncDec, Comma,
};

struct Expr {
    ExprKind kind;
    NodeId   id   = 0;
    Span     span;
    TypePtr  type;              // filled by semantic analysis

    long long   int_value = 0;
    std::string text;

    Tok      op = Tok::Error;
    bool     arrow = false;

    ExprPtr  lhs;
    ExprPtr  rhs;
    ExprPtr  third;

    std::vector<ExprPtr> args;
    TypePtr  cast_type;

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

    ExprPtr  expr;
    ExprPtr  init_expr;
    ExprPtr  cond_expr;
    ExprPtr  step_expr;

    StmtPtr  body;
    StmtPtr  else_body;

    std::vector<StmtPtr> items;
    DeclPtr  decl;

    const char* kind_name() const;
};

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

    std::vector<Declarator> declarators;

    std::string              func_name;
    TypePtr                  func_type;
    std::vector<std::string> param_names;
    StmtPtr                  body;

    std::string tag;
    std::vector<Declarator> members;

    const char* kind_name() const;
};

struct TranslationUnit {
    std::vector<DeclPtr> decls;
};

}  // namespace cviz
