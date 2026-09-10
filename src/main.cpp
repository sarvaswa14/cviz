#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "lexer.h"
#include "parser.h"

using namespace cviz;

static void print_usage() {
    std::cerr <<
        "usage: cviz <file.c> [options]\n"
        "\n"
        "  --dump-tokens        print the token stream\n"
        "  --dump-ast           print the abstract syntax tree\n"
        "  --emit-trace <path>  write the structured trace (not yet implemented)\n"
        "  -h, --help           show this message\n";
}

static void report(const Source& src, const Diagnostic& d) {
    std::cerr << src.file << ":" << d.span.line << ":" << d.span.col
              << ": " << to_string(d.cls) << " error: " << d.message
              << " [" << d.code << "]\n";

    if (d.span.line >= 1 &&
        d.span.line <= static_cast<int>(src.lines.size())) {
        const std::string& text = src.lines[d.span.line - 1];
        std::cerr << "  " << text << "\n  ";
        for (int i = 1; i < d.span.col; ++i) {
            std::cerr << (i - 1 < static_cast<int>(text.size()) &&
                          text[i - 1] == '\t' ? '\t' : ' ');
        }
        std::cerr << '^';
        for (int i = 1; i < d.span.len; ++i) std::cerr << '~';
        std::cerr << "\n";
    }
}

static void indent(int n) { for (int i = 0; i < n; ++i) std::cout << "  "; }

static void dump_expr(const Expr* e, int d);
static void dump_stmt(const Stmt* s, int d);
static void dump_decl(const Decl* dcl, int d);

static void dump_expr(const Expr* e, int d) {
    if (!e) return;
    indent(d);
    std::cout << e->kind_name();
    std::string lbl = e->label();
    if (!lbl.empty()) std::cout << "  " << lbl;
    std::cout << "   #" << e->id
              << " @" << e->span.line << ":" << e->span.col << "\n";

    dump_expr(e->lhs.get(), d + 1);
    dump_expr(e->rhs.get(), d + 1);
    dump_expr(e->third.get(), d + 1);
    for (const auto& a : e->args) dump_expr(a.get(), d + 1);
}

static void dump_stmt(const Stmt* s, int d) {
    if (!s) return;
    indent(d);
    std::cout << s->kind_name() << "   #" << s->id
              << " @" << s->span.line << ":" << s->span.col << "\n";

    dump_expr(s->expr.get(), d + 1);
    dump_expr(s->init_expr.get(), d + 1);
    dump_expr(s->cond_expr.get(), d + 1);
    dump_expr(s->step_expr.get(), d + 1);
    if (s->decl) dump_decl(s->decl.get(), d + 1);
    dump_stmt(s->body.get(), d + 1);
    dump_stmt(s->else_body.get(), d + 1);
    for (const auto& it : s->items) dump_stmt(it.get(), d + 1);
}

static void dump_decl(const Decl* dcl, int d) {
    if (!dcl) return;
    indent(d);
    std::cout << dcl->kind_name() << "   #" << dcl->id
              << " @" << dcl->span.line << ":" << dcl->span.col << "\n";

    if (dcl->kind == DeclKind::Function) {
        indent(d + 1);
        std::cout << dcl->func_name << " : "
                  << (dcl->func_type ? dcl->func_type->to_string() : "?");
        if (!dcl->param_names.empty()) {
            std::cout << "  params(";
            for (size_t i = 0; i < dcl->param_names.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << dcl->param_names[i];
            }
            std::cout << ")";
        }
        std::cout << "\n";
        dump_stmt(dcl->body.get(), d + 1);
    } else if (dcl->kind == DeclKind::StructDef) {
        indent(d + 1);
        std::cout << "struct " << dcl->tag << "\n";
    } else {
        for (const auto& dr : dcl->declarators) {
            indent(d + 1);
            std::cout << dr.name << " : "
                      << (dr.type ? dr.type->to_string() : "?") << "\n";
            if (dr.init) dump_expr(dr.init.get(), d + 2);
        }
    }
}

int main(int argc, char** argv) {
    std::string input;
    std::string trace_path;
    bool dump_tokens = false;
    bool dump_ast = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { print_usage(); return 0; }
        else if (a == "--dump-tokens") { dump_tokens = true; }
        else if (a == "--dump-ast")    { dump_ast = true; }
        else if (a == "--emit-trace") {
            if (i + 1 >= argc) {
                std::cerr << "cviz: --emit-trace needs a path\n";
                return 2;
            }
            trace_path = argv[++i];
        }
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "cviz: unknown option " << a << "\n";
            return 2;
        }
        else if (input.empty()) { input = a; }
        else {
            std::cerr << "cviz: only one input file is supported\n";
            return 2;
        }
    }

    if (input.empty()) { print_usage(); return 2; }

    Source src;
    try {
        src = Source::from_file(input);
    } catch (const std::exception& e) {
        std::cerr << "cviz: " << e.what() << "\n";
        return 2;
    }

    Lexer lexer(src);
    std::vector<Token> tokens = lexer.tokenise();

    if (dump_tokens) {
        for (const Token& t : tokens) {
            std::printf("%3d:%-3d  %-14s  %s\n",
                        t.span.line, t.span.col, tok_name(t.kind),
                        t.lexeme.c_str());
        }
    }

    for (const Diagnostic& d : lexer.diagnostics()) report(src, d);

    Parser parser(src, tokens);
    TranslationUnit tu = parser.parse();

    if (dump_ast) {
        for (const auto& d : tu.decls) dump_decl(d.get(), 0);
    }

    for (const Diagnostic& d : parser.diagnostics()) report(src, d);

    if (!trace_path.empty()) {
        std::cerr << "cviz: trace emission is not implemented yet\n";
    }

    return (lexer.failed() || parser.failed()) ? 1 : 0;
}