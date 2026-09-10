#pragma once
#include <vector>
#include "source.h"
#include "token.h"

namespace cviz {

class Lexer {
public:
    explicit Lexer(const Source& src) : src_(src) {}

    std::vector<Token> tokenise();

    const std::vector<Diagnostic>& diagnostics() const { return diags_; }
    bool failed() const { return !diags_.empty(); }

private:
    const Source& src_;
    size_t pos_  = 0;    
    int    line_ = 1;
    int    col_  = 1;
    std::vector<Diagnostic> diags_;

    bool at_end() const  { return pos_ >= src_.text.size(); }
    char peek(int n = 0) const;
    char advance();
    bool match(char expected);

    void skip_trivia();         
    Token scan_token();
    Token scan_ident_or_keyword();
    Token scan_number();
    Token scan_char_literal();
    Token scan_string_literal();
    Token scan_punctuator();

    int scan_escape();

    Span span_from(int start_line, int start_col, int length) const;
    void error(const std::string& code, const std::string& message, Span s);
};

}  