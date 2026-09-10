#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "lexer.h"

using namespace cviz;

static void print_usage() {
    std::cerr <<
        "usage: cviz <file.c> [options]\n"
        "\n"
        "  --dump-tokens        print the token stream\n"
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

int main(int argc, char** argv) {
    std::string input;
    std::string trace_path;
    bool dump_tokens = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { print_usage(); return 0; }
        else if (a == "--dump-tokens")  { dump_tokens = true; }
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

    if (!trace_path.empty()) {
        std::cerr << "cviz: trace emission is not implemented yet\n";
    }

    return lexer.failed() ? 1 : 0;
}