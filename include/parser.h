#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "ast.h"
#include "source.h"
#include "token.h"

namespace cviz {

class Parser {
public:
    Parser(const Source& src, std::vector<Token> toks)
        : src_(src), toks_(std::move(toks)) {}

    TranslationUnit parse();

    const std::vector<Diagnostic>& diagnostics() const { return diags_; }
    bool failed() const { return !diags_.empty(); }
    int  node_count() const { return next_id_; }

private:
    struct ParseError {};

    const Source&      src_;
    std::vector<Token> toks_;
    size_t             pos_ = 0;
    int                next_id_ = 0;
    std::vector<Diagnostic> diags_;
    std::vector<std::string> pending_params_;

    const Token& peek(int n = 0) const;
    const Token& previous() const;
    bool  check(Tok k) const;
    bool  match(Tok k);
    const Token& advance();
    const Token& expect(Tok k, const std::string& what);

    void  error(const std::string& code, const std::string& msg, Span s);
    [[noreturn]] void fatal(const std::string& code, const std::string& msg, Span s);
    void  sync();

    ExprPtr new_expr(ExprKind k, Span s);
    StmtPtr new_stmt(StmtKind k, Span s);
    DeclPtr new_decl(DeclKind k, Span s);

    bool is_type_start(int n = 0) const;
    bool starts_declarator(size_t idx) const;
    void skip_balanced_parens();

    TypePtr parse_decl_specifiers();
    TypePtr parse_struct_specifier();
    TypePtr parse_declarator(TypePtr base, std::string& name);
    TypePtr parse_direct_declarator(TypePtr base, std::string& name);
    TypePtr parse_suffixes(TypePtr base);
    TypePtr parse_type_name();

    DeclPtr parse_external_decl();
    DeclPtr parse_declaration(TypePtr specs, Span start);

    StmtPtr parse_statement();
    StmtPtr parse_compound();
    StmtPtr parse_if();
    StmtPtr parse_while();
    StmtPtr parse_do_while();
    StmtPtr parse_for();
    StmtPtr parse_switch();

    ExprPtr parse_expression();
    ExprPtr parse_assignment();
    ExprPtr parse_conditional();
    ExprPtr parse_binary(int min_prec);
    ExprPtr parse_cast();
    ExprPtr parse_unary();
    ExprPtr parse_postfix();
    ExprPtr parse_primary();
};

}  // namespace cviz