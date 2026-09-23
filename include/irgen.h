#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "ast.h"
#include "ir.h"
#include "source.h"

namespace cviz {

// Lowers the typed syntax tree to three-address code. Every instruction it
// emits records the AST node and source span that produced it, which is the
// provenance link the rest of Phase 2 depends on.
class IRGen {
public:
    explicit IRGen(const Source& src) : src_(src) {}

    Program lower(TranslationUnit& tu);

    const std::vector<Diagnostic>& diagnostics() const { return diags_; }
    bool failed() const { return !diags_.empty(); }

private:
    struct VarInfo { std::string uname; TypePtr type; bool global = false; };
    struct LValue  { bool is_var = false; std::string name; Operand addr; TypePtr type; };

    // Sets the provenance for everything emitted while it is alive, and
    // restores the enclosing node's provenance when it goes out of scope.
    struct Prov {
        IRGen* g; NodeId saved_node; Span saved_span;
        Prov(IRGen* gen, NodeId id, Span sp)
            : g(gen), saved_node(gen->cur_node_), saved_span(gen->cur_span_) {
            g->cur_node_ = id; g->cur_span_ = sp;
        }
        ~Prov() { g->cur_node_ = saved_node; g->cur_span_ = saved_span; }
    };

    const Source& src_;
    std::vector<Diagnostic> diags_;
    Program prog_;
    Function* fn_ = nullptr;

    std::vector<std::unordered_map<std::string, VarInfo>> scopes_;
    std::unordered_map<std::string, TypePtr> funcs_;
    std::vector<std::string> break_to_, continue_to_;
    std::vector<std::unordered_map<const Stmt*, std::string>> case_labels_;

    int temp_ = 0, local_ = 0, label_ = 0, next_id_ = 1;
    NodeId cur_node_ = -1;
    Span   cur_span_;

    void error(const std::string& code, const std::string& msg, Span s);
    Instr& emit(Op op);
    std::string new_temp();
    std::string new_label();
    void place_label(const std::string& l);
    void jump(const std::string& l);
    void branch(Operand c, const std::string& t, const std::string& f);

    void push_scope();
    void pop_scope();
    VarInfo* find(const std::string& name);
    std::string declare_local(const std::string& name, TypePtr t);

    static long long cells(const TypePtr& t);
    static long long byte_size(const TypePtr& t);
    static bool is_array(const TypePtr& t);
    static bool is_ptrlike(const TypePtr& t);
    static TypePtr pointee(const TypePtr& t);
    static bool narrow(const TypePtr& t);
    static std::string decode_string(const std::string& lexeme);
    bool const_eval(const Expr& e, long long& out);

    void lower_function(Decl& d);
    void lower_global(Decl& d);
    void lower_local_decl(Decl& d);
    void lower_stmt(Stmt& s);
    void collect_cases(Stmt& s, std::vector<Stmt*>& out);

    Operand lower_expr(Expr& e);
    Operand lower_assign(Expr& e);
    Operand lower_incdec(Expr& e);
    Operand lower_call(Expr& e);
    Operand short_circuit(Expr& e);
    Operand address_of(Expr& e);
    Operand element_address(Expr& e);
    LValue  lower_lvalue(Expr& e);
    Operand load(const LValue& lv);
    void    store(const LValue& lv, Operand v);
    Operand bin(Tok op, Operand a, Operand b, bool w32);
    void    check_strings(const Function& f);
};

}  // namespace cviz
