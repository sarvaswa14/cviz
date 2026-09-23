#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "ast.h"
#include "source.h"

namespace cviz {

struct Symbol {
    std::string name;
    TypePtr     type;
    int         scope_level = 0;
    bool        is_function = false;
    bool        defined     = false;
    Span        span;
};

// A stack of scopes: file scope at level 0, then function and block scopes.
// Lookup walks outwards, so an inner declaration shadows an outer one.
class SymbolTable {
public:
    void push(const char* kind);
    void pop();

    // Returns nullptr if the innermost scope already has this name.
    Symbol* declare(const Symbol& s);

    Symbol* lookup(const std::string& name);
    Symbol* lookup_current(const std::string& name);

    int level() const { return static_cast<int>(scopes_.size()) - 1; }

    struct Scope {
        std::string kind;
        std::unordered_map<std::string, Symbol> syms;
        std::vector<std::string> order;
    };
    const std::vector<Scope>& scopes() const { return scopes_; }

private:
    std::vector<Scope> scopes_;
};

// Events recorded for the trace, in the order they occurred.
struct SemaEvent {
    enum class Kind { ScopeOpen, ScopeClose, SymbolDecl, TypeAssign };
    Kind        kind;
    int         id = 0;
    int         scope = 0;
    std::string name;
    std::string type;
    std::string of;
    NodeId      node = -1;
    Span        span;
};

class Sema {
public:
    explicit Sema(const Source& src) : src_(src) {}

    void analyse(TranslationUnit& tu);

    const std::vector<Diagnostic>& diagnostics() const { return diags_; }
    const std::vector<SemaEvent>&  events() const { return events_; }
    bool failed() const { return !diags_.empty(); }

private:
    const Source& src_;
    SymbolTable   table_;
    std::vector<Diagnostic> diags_;
    std::vector<SemaEvent>  events_;
    int  event_id_ = 0;
    int  scope_counter_ = 0;
    TypePtr current_return_;
    int  loop_depth_ = 0;
    int  switch_depth_ = 0;

    void error(const std::string& code, const std::string& msg, Span s);

    void declare_builtins();
    void open_scope(const char* kind);
    void close_scope();
    void record_symbol(const Symbol& s);
    void record_type(const Expr* e);

    void visit_decl(Decl& d);
    void visit_stmt(Stmt& s);
    TypePtr visit_expr(Expr& e);

    static bool is_integer(const TypePtr& t);
    static bool is_arithmetic(const TypePtr& t);
    static bool is_scalar(const TypePtr& t);
    static bool is_lvalue(const Expr& e);
    static int  rank(const TypePtr& t);
    static TypePtr decay(const TypePtr& t);
    static TypePtr usual_conversions(const TypePtr& a, const TypePtr& b);
    static bool compatible(const TypePtr& a, const TypePtr& b);
};

}  // namespace cviz
