#include "lexer.h"
#include <cctype>
#include <cstdlib>

namespace cviz {

char Lexer::peek(int n) const {
    size_t i = pos_ + static_cast<size_t>(n);
    return i < src_.text.size() ? src_.text[i] : '\0';
}

char Lexer::advance() {
    char c = src_.text[pos_++];
    if (c == '\n') { ++line_; col_ = 1; }
    else if (c != '\r') { ++col_; }
    return c;
}

bool Lexer::match(char expected) {
    if (at_end() || src_.text[pos_] != expected) return false;
    advance();
    return true;
}

Span Lexer::span_from(int start_line, int start_col, int length) const {
    Span s;
    s.line = start_line;
    s.col  = start_col;
    s.len  = length;
    return s;
}

void Lexer::error(const std::string& code, const std::string& message, Span s) {
    diags_.push_back(Diagnostic{DiagClass::Lexical, code, message, s});
}

void Lexer::skip_trivia() {
    for (;;) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '\v' || c == '\f') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            while (!at_end() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            int ol = line_, oc = col_;
            advance(); advance();
            bool closed = false;
            while (!at_end()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance(); advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                error("unterminated-comment", "block comment is never closed",
                      span_from(ol, oc, 2));
            }
        } else {
            return;
        }
    }
}

Token Lexer::scan_ident_or_keyword() {
    int sl = line_, sc = col_;
    size_t start = pos_;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) ||
                         peek() == '_')) {
        advance();
    }
    Token t;
    t.lexeme = src_.text.substr(start, pos_ - start);
    t.kind   = keyword_or_ident(t.lexeme);
    t.span   = span_from(sl, sc, static_cast<int>(t.lexeme.size()));
    return t;
}

int Lexer::scan_escape() {
    int sl = line_, sc = col_;
    char c = advance();
    switch (c) {
        case '\'': return '\'';
        case '"':  return '"';
        case '?':  return '?';
        case '\\': return '\\';
        case 'a':  return 7;
        case 'b':  return 8;
        case 'f':  return 12;
        case 'n':  return 10;
        case 'r':  return 13;
        case 't':  return 9;
        case 'v':  return 11;
        default: break;
    }
    if (c >= '0' && c <= '7') {
        int v = c - '0';
        for (int i = 0; i < 2 && peek() >= '0' && peek() <= '7'; ++i) {
            v = v * 8 + (advance() - '0');
        }
        return v;
    }
    if (c == 'x') {
        if (!std::isxdigit(static_cast<unsigned char>(peek()))) {
            error("unknown-escape", "\\x with no hexadecimal digits",
                  span_from(sl, sc, 2));
            return 'x';
        }
        int v = 0;
        while (std::isxdigit(static_cast<unsigned char>(peek()))) {
            char h = advance();
            int d = std::isdigit(static_cast<unsigned char>(h))
                        ? h - '0' : (std::tolower(h) - 'a' + 10);
            v = v * 16 + d;
        }
        return v;
    }
    error("unknown-escape", std::string("unrecognised escape sequence \\") + c,
          span_from(sl, sc, 2));
    return static_cast<unsigned char>(c);
}

Token Lexer::scan_number() {
    int sl = line_, sc = col_;
    size_t start = pos_;
    bool is_float = false;
    long long value = 0;

    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        advance(); advance();
        if (!std::isxdigit(static_cast<unsigned char>(peek()))) {
            error("malformed-integer", "hexadecimal prefix with no digits",
                  span_from(sl, sc, 2));
        }
        while (std::isxdigit(static_cast<unsigned char>(peek()))) {
            char h = advance();
            int d = std::isdigit(static_cast<unsigned char>(h))
                        ? h - '0' : (std::tolower(h) - 'a' + 10);
            value = value * 16 + d;
        }
    } else {
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
            is_float = true;
            advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        } else if (peek() == '.') {
            is_float = true;
            advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            char n1 = peek(1), n2 = peek(2);
            if (std::isdigit(static_cast<unsigned char>(n1)) ||
                ((n1 == '+' || n1 == '-') &&
                 std::isdigit(static_cast<unsigned char>(n2)))) {
                is_float = true;
                advance();
                if (peek() == '+' || peek() == '-') advance();
                while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
            }
        }
        std::string digits = src_.text.substr(start, pos_ - start);
        if (!is_float) {
            if (digits.size() > 1 && digits[0] == '0') {
                for (char d : digits) {
                    if (d == '8' || d == '9') {
                        error("invalid-octal-digit",
                              "digit " + std::string(1, d) +
                                  " is not valid in an octal constant",
                              span_from(sl, sc, static_cast<int>(digits.size())));
                        break;
                    }
                }
                value = std::strtoll(digits.c_str(), nullptr, 8);
            } else {
                value = std::strtoll(digits.c_str(), nullptr, 10);
            }
        }
    }

    size_t suffix_start = pos_;
    while (!at_end() && std::isalpha(static_cast<unsigned char>(peek()))) advance();

    Token t;
    t.lexeme    = src_.text.substr(start, pos_ - start);
    t.suffix    = src_.text.substr(suffix_start, pos_ - suffix_start);
    t.kind      = is_float ? Tok::FloatLit : Tok::IntLit;
    t.int_value = value;
    t.span      = span_from(sl, sc, static_cast<int>(t.lexeme.size()));
    return t;
}

Token Lexer::scan_char_literal() {
    int sl = line_, sc = col_;
    size_t start = pos_;
    advance();
    int value = 0;
    bool closed = false;
    if (!at_end() && peek() != '\n') {
        char c = peek();
        if (c == '\\') { advance(); value = scan_escape(); }
        else           { value = static_cast<unsigned char>(advance()); }
        if (peek() == '\'') { advance(); closed = true; }
    }
    Token t;
    t.lexeme    = src_.text.substr(start, pos_ - start);
    t.kind      = closed ? Tok::CharLit : Tok::Error;
    t.int_value = value;
    t.span      = span_from(sl, sc, static_cast<int>(t.lexeme.size()));
    if (!closed) {
        error("unterminated-literal",
              "character constant is not closed before end of line", t.span);
    }
    return t;
}

Token Lexer::scan_string_literal() {
    int sl = line_, sc = col_;
    size_t start = pos_;
    advance();
    bool closed = false;
    while (!at_end() && peek() != '\n') {
        if (peek() == '"') { advance(); closed = true; break; }
        if (peek() == '\\') { advance(); scan_escape(); }
        else                { advance(); }
    }
    Token t;
    t.lexeme = src_.text.substr(start, pos_ - start);
    t.kind   = closed ? Tok::StringLit : Tok::Error;
    t.span   = span_from(sl, sc, static_cast<int>(t.lexeme.size()));
    if (!closed) {
        error("unterminated-literal",
              "string literal is not closed before end of line", t.span);
    }
    return t;
}

// Longest match first: three characters, then two, then one.
Token Lexer::scan_punctuator() {
    int sl = line_, sc = col_;
    size_t start = pos_;
    char c = advance();
    Tok k = Tok::Error;

    switch (c) {
        case '<':
            if (peek() == '<' && peek(1) == '=') { advance(); advance(); k = Tok::LShiftAssign; }
            else if (match('<')) k = Tok::LShift;
            else if (match('=')) k = Tok::Le;
            else                 k = Tok::Lt;
            break;
        case '>':
            if (peek() == '>' && peek(1) == '=') { advance(); advance(); k = Tok::RShiftAssign; }
            else if (match('>')) k = Tok::RShift;
            else if (match('=')) k = Tok::Ge;
            else                 k = Tok::Gt;
            break;
        case '.':
            if (peek() == '.' && peek(1) == '.') { advance(); advance(); k = Tok::Ellipsis; }
            else k = Tok::Dot;
            break;
        case '-':
            if (match('>'))      k = Tok::Arrow;
            else if (match('-')) k = Tok::MinusMinus;
            else if (match('=')) k = Tok::MinusAssign;
            else                 k = Tok::Minus;
            break;
        case '+':
            if (match('+'))      k = Tok::PlusPlus;
            else if (match('=')) k = Tok::PlusAssign;
            else                 k = Tok::Plus;
            break;
        case '&':
            if (match('&'))      k = Tok::AndAnd;
            else if (match('=')) k = Tok::AmpAssign;
            else                 k = Tok::Amp;
            break;
        case '|':
            if (match('|'))      k = Tok::OrOr;
            else if (match('=')) k = Tok::PipeAssign;
            else                 k = Tok::Pipe;
            break;
        case '*': k = match('=') ? Tok::StarAssign    : Tok::Star;    break;
        case '/': k = match('=') ? Tok::SlashAssign   : Tok::Slash;   break;
        case '%': k = match('=') ? Tok::PercentAssign : Tok::Percent; break;
        case '^': k = match('=') ? Tok::CaretAssign   : Tok::Caret;   break;
        case '=': k = match('=') ? Tok::Eq            : Tok::Assign;  break;
        case '!': k = match('=') ? Tok::Ne            : Tok::Bang;    break;
        case '[': k = Tok::LBracket; break;
        case ']': k = Tok::RBracket; break;
        case '(': k = Tok::LParen;   break;
        case ')': k = Tok::RParen;   break;
        case '{': k = Tok::LBrace;   break;
        case '}': k = Tok::RBrace;   break;
        case '~': k = Tok::Tilde;    break;
        case '?': k = Tok::Question; break;
        case ':': k = Tok::Colon;    break;
        case ';': k = Tok::Semi;     break;
        case ',': k = Tok::Comma;    break;
        case '#': k = Tok::Hash;     break;
        default:  k = Tok::Error;    break;
    }

    Token t;
    t.kind   = k;
    t.lexeme = src_.text.substr(start, pos_ - start);
    t.span   = span_from(sl, sc, static_cast<int>(t.lexeme.size()));
    if (k == Tok::Error) {
        error("stray-character", "character '" + t.lexeme + "' cannot begin a token", t.span);
    }
    return t;
}

Token Lexer::scan_token() {
    char c = peek();
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') return scan_ident_or_keyword();
    if (std::isdigit(static_cast<unsigned char>(c))) return scan_number();
    if (c == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) return scan_number();
    if (c == '\'') return scan_char_literal();
    if (c == '"')  return scan_string_literal();
    return scan_punctuator();
}

std::vector<Token> Lexer::tokenise() {
    std::vector<Token> out;
    for (;;) {
        skip_trivia();
        if (at_end()) break;
        Token t = scan_token();
        // A token that could not be formed is reported and dropped, so one
        // bad byte does not stop tokenisation.
        if (t.kind != Tok::Error) out.push_back(t);
    }
    Token eof;
    eof.kind = Tok::EndOfFile;
    eof.span = span_from(line_, col_, 0);
    out.push_back(eof);
    return out;
}

}  // namespace cviz
