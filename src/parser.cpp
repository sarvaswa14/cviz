#include "parser.h"
#include <cstdlib>

namespace cviz {

const Token& Parser::peek(int n) const {
    size_t i = pos_ + static_cast<size_t>(n);
    return i < toks_.size() ? toks_[i] : toks_.back();
}

const Token& Parser::previous() const {
    return toks_[pos_ > 0 ? pos_ - 1 : 0];
}

bool Parser::check(Tok k) const { return peek().kind == k; }

const Token& Parser::advance() {
    if (pos_ < toks_.size() - 1) ++pos_;
    return toks_[pos_ - 1];
}

bool Parser::match(Tok k) {
    if (!check(k)) return false;
    advance();
    return true;
}

const Token& Parser::expect(Tok k, const std::string& what) {
    if (check(k)) return advance();
    fatal("expected-token", "expected " + what + " but found '" +
          (peek().kind == Tok::EndOfFile ? "end of file" : peek().lexeme) + "'",
          peek().span);
}

void Parser::error(const std::string& code, const std::string& msg, Span s) {
    diags_.push_back(Diagnostic{DiagClass::Syntax, code, msg, s});
}

void Parser::fatal(const std::string& code, const std::string& msg, Span s) {
    error(code, msg, s);
    throw ParseError{};
}

// Panic-mode recovery, as specified in spec/grammar.md section 7.
void Parser::sync() {
    while (!check(Tok::EndOfFile)) {
        if (previous().kind == Tok::Semi) return;
        switch (peek().kind) {
            case Tok::RBrace: advance(); return;
            case Tok::Semi:   advance(); return;
            case Tok::KwInt: case Tok::KwChar: case Tok::KwVoid:
            case Tok::KwShort: case Tok::KwLong: case Tok::KwSigned:
            case Tok::KwUnsigned: case Tok::KwStruct:
            case Tok::KwIf: case Tok::KwWhile: case Tok::KwFor:
            case Tok::KwReturn: case Tok::KwSwitch:
                return;
            default: advance();
        }
    }
}

ExprPtr Parser::new_expr(ExprKind k, Span s) {
    auto e = std::make_unique<Expr>();
    e->kind = k;
    e->id   = next_id_++;
    e->span = s;
    return e;
}

StmtPtr Parser::new_stmt(StmtKind k, Span s) {
    auto st = std::make_unique<Stmt>();
    st->kind = k;
    st->id   = next_id_++;
    st->span = s;
    return st;
}

DeclPtr Parser::new_decl(DeclKind k, Span s) {
    auto d = std::make_unique<Decl>();
    d->kind = k;
    d->id   = next_id_++;
    d->span = s;
    return d;
}

bool Parser::is_type_start(int n) const {
    switch (peek(n).kind) {
        case Tok::KwVoid: case Tok::KwChar: case Tok::KwShort:
        case Tok::KwInt: case Tok::KwLong: case Tok::KwSigned:
        case Tok::KwUnsigned: case Tok::KwStruct:
        case Tok::KwFloat: case Tok::KwDouble: case Tok::KwBool:
        case Tok::KwUnion: case Tok::KwEnum: case Tok::KwConst:
            return true;
        default: return false;
    }
}

bool Parser::starts_declarator(size_t idx) const {
    if (idx >= toks_.size()) return false;
    switch (toks_[idx].kind) {
        case Tok::Star: case Tok::Ident: case Tok::LParen: return true;
        default: return false;
    }
}

void Parser::skip_balanced_parens() {
    int depth = 0;
    do {
        if (check(Tok::LParen)) ++depth;
        else if (check(Tok::RParen)) --depth;
        else if (check(Tok::EndOfFile)) return;
        advance();
    } while (depth > 0);
}

TypePtr Parser::parse_decl_specifiers() {
    bool uns = false, sign = false;
    int longs = 0;
    bool has_void = false, has_char = false, has_short = false, has_int = false;
    TypePtr stretch;
    Span first = peek().span;

    for (;;) {
        switch (peek().kind) {
            case Tok::KwConst:    advance(); continue;
            case Tok::KwUnsigned: uns = true;  advance(); continue;
            case Tok::KwSigned:   sign = true; advance(); continue;
            case Tok::KwVoid:     has_void = true;  advance(); continue;
            case Tok::KwChar:     has_char = true;  advance(); continue;
            case Tok::KwShort:    has_short = true; advance(); continue;
            case Tok::KwInt:      has_int = true;   advance(); continue;
            case Tok::KwLong:     ++longs;          advance(); continue;
            case Tok::KwStruct:   return parse_struct_specifier();

            case Tok::KwFloat: case Tok::KwDouble: case Tok::KwBool:
                error("not-in-subset",
                      "type '" + peek().lexeme + "' is not in the supported subset",
                      peek().span);
                stretch = Type::make(peek().kind == Tok::KwFloat ? TypeKind::Float
                                     : peek().kind == Tok::KwDouble ? TypeKind::Double
                                                                    : TypeKind::Bool);
                advance();
                continue;

            case Tok::KwUnion: case Tok::KwEnum: case Tok::KwTypedef:
                fatal("not-in-subset",
                      "'" + peek().lexeme + "' is not in the supported subset",
                      peek().span);

            default: break;
        }
        break;
    }

    if (stretch) return stretch;
    (void)sign;

    if (has_void)  return Type::make(TypeKind::Void);
    if (has_char)  return Type::make(TypeKind::Char, uns);
    if (has_short) return Type::make(TypeKind::Short, uns);
    if (longs > 0) return Type::make(TypeKind::Long, uns);
    if (has_int || uns || sign) return Type::make(TypeKind::Int, uns);

    fatal("expected-type", "expected a type specifier", first);
}

TypePtr Parser::parse_struct_specifier() {
    Span s = peek().span;
    expect(Tok::KwStruct, "'struct'");

    std::string tag;
    if (check(Tok::Ident)) tag = advance().lexeme;

    auto t = Type::make(TypeKind::Struct);
    t->tag = tag;

    if (match(Tok::LBrace)) {
        while (!check(Tok::RBrace) && !check(Tok::EndOfFile)) {
            TypePtr member_base = parse_decl_specifiers();
            do {
                std::string name;
                TypePtr mt = parse_declarator(member_base, name);
                (void)mt;
            } while (match(Tok::Comma));
            expect(Tok::Semi, "';' after struct member");
        }
        expect(Tok::RBrace, "'}' to close struct");
    } else if (tag.empty()) {
        fatal("expected-token", "expected a tag or '{' after 'struct'", s);
    }
    return t;
}

TypePtr Parser::parse_declarator(TypePtr base, std::string& name) {
    while (match(Tok::Star)) {
        while (match(Tok::KwConst)) {}
        base = Type::pointer_to(base);
    }
    return parse_direct_declarator(std::move(base), name);
}

// The declarator is read outside-in while the type is built inside-out.
// A parenthesised declarator is handled by parsing the suffixes that
// follow the group first, then re-reading the group against that type.
TypePtr Parser::parse_direct_declarator(TypePtr base, std::string& name) {
    if (check(Tok::LParen) && starts_declarator(pos_ + 1)) {
        size_t inner = pos_ + 1;
        skip_balanced_parens();
        TypePtr outer = parse_suffixes(std::move(base));
        size_t after = pos_;

        pos_ = inner;
        TypePtr result = parse_declarator(std::move(outer), name);
        expect(Tok::RParen, "')' to close declarator");
        pos_ = after;
        return result;
    }

    if (check(Tok::Ident)) name = advance().lexeme;
    return parse_suffixes(std::move(base));
}

// Suffixes are collected left to right and applied right to left, so that
// int a[2][3] is an array of 2 arrays of 3, not the reverse.
TypePtr Parser::parse_suffixes(TypePtr base) {
    struct Suffix {
        bool is_array;
        long long len;
        std::vector<TypePtr> params;
        std::vector<std::string> names;
    };
    std::vector<Suffix> sufs;

    for (;;) {
        if (match(Tok::LBracket)) {
            long long n = -1;
            if (check(Tok::IntLit)) n = advance().int_value;
            expect(Tok::RBracket, "']'");
            sufs.push_back(Suffix{true, n, {}, {}});
        } else if (match(Tok::LParen)) {
            Suffix f{false, 0, {}, {}};
            if (!check(Tok::RParen)) {
                if (check(Tok::KwVoid) && peek(1).kind == Tok::RParen) {
                    advance();
                } else {
                    do {
                        if (check(Tok::Ellipsis)) {
                            error("not-in-subset",
                                  "variadic parameters are not in the supported subset",
                                  peek().span);
                            advance();
                            break;
                        }
                        TypePtr pb = parse_decl_specifiers();
                        std::string pname;
                        TypePtr pt = parse_declarator(pb, pname);
                        f.params.push_back(pt);
                        f.names.push_back(pname);
                    } while (match(Tok::Comma));
                }
            }
            expect(Tok::RParen, "')' after parameter list");
            sufs.push_back(std::move(f));
        } else {
            break;
        }
    }

    for (size_t i = sufs.size(); i-- > 0;) {
        if (sufs[i].is_array) {
            base = Type::array_of(std::move(base), sufs[i].len);
        } else {
            pending_params_ = sufs[i].names;
            base = Type::function(std::move(base), sufs[i].params);
        }
    }
    return base;
}

TypePtr Parser::parse_type_name() {
    TypePtr base = parse_decl_specifiers();
    std::string ignored;
    return parse_declarator(std::move(base), ignored);
}

TranslationUnit Parser::parse() {
    TranslationUnit tu;
    while (!check(Tok::EndOfFile)) {
        try {
            DeclPtr d = parse_external_decl();
            if (d) tu.decls.push_back(std::move(d));
        } catch (const ParseError&) {
            sync();
        }
    }
    return tu;
}

DeclPtr Parser::parse_external_decl() {
    Span start = peek().span;
    TypePtr specs = parse_decl_specifiers();

    if (match(Tok::Semi)) {
        auto d = new_decl(DeclKind::StructDef, start);
        d->tag = specs->tag;
        return d;
    }

    std::string name;
    pending_params_.clear();
    TypePtr t = parse_declarator(specs, name);
    std::vector<std::string> params = pending_params_;

    if (t->kind == TypeKind::Function && check(Tok::LBrace)) {
        auto d = new_decl(DeclKind::Function, start);
        d->func_name   = name;
        d->func_type   = t;
        d->param_names = params;
        d->body        = parse_compound();
        return d;
    }

    auto d = new_decl(DeclKind::Variable, start);
    for (;;) {
        Declarator dr;
        dr.name = name;
        dr.type = t;
        dr.span = start;
        if (match(Tok::Assign)) dr.init = parse_assignment();
        d->declarators.push_back(std::move(dr));

        if (!match(Tok::Comma)) break;
        name.clear();
        t = parse_declarator(specs, name);
    }
    expect(Tok::Semi, "';' after declaration");
    return d;
}

DeclPtr Parser::parse_declaration(TypePtr specs, Span start) {
    auto d = new_decl(DeclKind::Variable, start);
    do {
        std::string name;
        TypePtr t = parse_declarator(specs, name);
        Declarator dr;
        dr.name = name;
        dr.type = t;
        dr.span = start;
        if (match(Tok::Assign)) dr.init = parse_assignment();
        d->declarators.push_back(std::move(dr));
    } while (match(Tok::Comma));
    expect(Tok::Semi, "';' after declaration");
    return d;
}

StmtPtr Parser::parse_compound() {
    Span s = peek().span;
    expect(Tok::LBrace, "'{'");
    auto st = new_stmt(StmtKind::Compound, s);

    while (!check(Tok::RBrace) && !check(Tok::EndOfFile)) {
        try {
            if (is_type_start()) {
                Span ds = peek().span;
                TypePtr specs = parse_decl_specifiers();
                auto ds_stmt = new_stmt(StmtKind::DeclStmt, ds);
                ds_stmt->decl = parse_declaration(specs, ds);
                st->items.push_back(std::move(ds_stmt));
            } else {
                st->items.push_back(parse_statement());
            }
        } catch (const ParseError&) {
            sync();
        }
    }
    expect(Tok::RBrace, "'}' to close block");
    return st;
}

StmtPtr Parser::parse_statement() {
    Span s = peek().span;
    switch (peek().kind) {
        case Tok::LBrace:  return parse_compound();
        case Tok::KwIf:    return parse_if();
        case Tok::KwWhile: return parse_while();
        case Tok::KwDo:    return parse_do_while();
        case Tok::KwFor:   return parse_for();
        case Tok::KwSwitch:return parse_switch();

        case Tok::KwGoto:
            fatal("not-in-subset",
                  "'goto' is not in the supported subset", s);

        case Tok::KwCase: {
            advance();
            auto st = new_stmt(StmtKind::Case, s);
            st->expr = parse_conditional();
            expect(Tok::Colon, "':' after case label");
            st->body = parse_statement();
            return st;
        }
        case Tok::KwDefault: {
            advance();
            auto st = new_stmt(StmtKind::Default, s);
            expect(Tok::Colon, "':' after default label");
            st->body = parse_statement();
            return st;
        }
        case Tok::KwBreak: {
            advance();
            expect(Tok::Semi, "';' after break");
            return new_stmt(StmtKind::Break, s);
        }
        case Tok::KwContinue: {
            advance();
            expect(Tok::Semi, "';' after continue");
            return new_stmt(StmtKind::Continue, s);
        }
        case Tok::KwReturn: {
            advance();
            auto st = new_stmt(StmtKind::Return, s);
            if (!check(Tok::Semi)) st->expr = parse_expression();
            expect(Tok::Semi, "';' after return");
            return st;
        }
        case Tok::Semi: {
            advance();
            return new_stmt(StmtKind::Empty, s);
        }
        default: {
            auto st = new_stmt(StmtKind::ExprStmt, s);
            st->expr = parse_expression();
            expect(Tok::Semi, "';' after expression");
            return st;
        }
    }
}

// On seeing 'else', bind it immediately, which attaches it to the nearest
// unmatched 'if' (spec/grammar.md section 4).
StmtPtr Parser::parse_if() {
    Span s = peek().span;
    advance();
    auto st = new_stmt(StmtKind::If, s);
    expect(Tok::LParen, "'(' after if");
    st->expr = parse_expression();
    expect(Tok::RParen, "')' after condition");
    st->body = parse_statement();
    if (match(Tok::KwElse)) st->else_body = parse_statement();
    return st;
}

StmtPtr Parser::parse_while() {
    Span s = peek().span;
    advance();
    auto st = new_stmt(StmtKind::While, s);
    expect(Tok::LParen, "'(' after while");
    st->expr = parse_expression();
    expect(Tok::RParen, "')' after condition");
    st->body = parse_statement();
    return st;
}

StmtPtr Parser::parse_do_while() {
    Span s = peek().span;
    advance();
    auto st = new_stmt(StmtKind::DoWhile, s);
    st->body = parse_statement();
    expect(Tok::KwWhile, "'while' after do body");
    expect(Tok::LParen, "'(' after while");
    st->expr = parse_expression();
    expect(Tok::RParen, "')' after condition");
    expect(Tok::Semi, "';' after do-while");
    return st;
}

StmtPtr Parser::parse_for() {
    Span s = peek().span;
    advance();
    auto st = new_stmt(StmtKind::For, s);
    expect(Tok::LParen, "'(' after for");
    if (!check(Tok::Semi)) st->init_expr = parse_expression();
    expect(Tok::Semi, "';' after for initialiser");
    if (!check(Tok::Semi)) st->cond_expr = parse_expression();
    expect(Tok::Semi, "';' after for condition");
    if (!check(Tok::RParen)) st->step_expr = parse_expression();
    expect(Tok::RParen, "')' after for clauses");
    st->body = parse_statement();
    return st;
}

StmtPtr Parser::parse_switch() {
    Span s = peek().span;
    advance();
    auto st = new_stmt(StmtKind::Switch, s);
    expect(Tok::LParen, "'(' after switch");
    st->expr = parse_expression();
    expect(Tok::RParen, "')' after switch expression");
    st->body = parse_statement();
    return st;
}

static int binary_prec(Tok k) {
    switch (k) {
        case Tok::OrOr:    return 1;
        case Tok::AndAnd:  return 2;
        case Tok::Pipe:    return 3;
        case Tok::Caret:   return 4;
        case Tok::Amp:     return 5;
        case Tok::Eq: case Tok::Ne: return 6;
        case Tok::Lt: case Tok::Gt: case Tok::Le: case Tok::Ge: return 7;
        case Tok::LShift: case Tok::RShift: return 8;
        case Tok::Plus: case Tok::Minus: return 9;
        case Tok::Star: case Tok::Slash: case Tok::Percent: return 10;
        default: return -1;
    }
}

static bool is_assign_op(Tok k) {
    switch (k) {
        case Tok::Assign: case Tok::PlusAssign: case Tok::MinusAssign:
        case Tok::StarAssign: case Tok::SlashAssign: case Tok::PercentAssign:
        case Tok::LShiftAssign: case Tok::RShiftAssign:
        case Tok::AmpAssign: case Tok::CaretAssign: case Tok::PipeAssign:
            return true;
        default: return false;
    }
}

ExprPtr Parser::parse_expression() {
    ExprPtr e = parse_assignment();
    while (check(Tok::Comma)) {
        Span s = peek().span;
        advance();
        auto c = new_expr(ExprKind::Comma, s);
        c->op  = Tok::Comma;
        c->lhs = std::move(e);
        c->rhs = parse_assignment();
        e = std::move(c);
    }
    return e;
}

ExprPtr Parser::parse_assignment() {
    ExprPtr lhs = parse_conditional();
    if (is_assign_op(peek().kind)) {
        Span s = peek().span;
        Tok op = advance().kind;
        auto a = new_expr(ExprKind::Assign, s);
        a->op  = op;
        a->lhs = std::move(lhs);
        a->rhs = parse_assignment();
        return a;
    }
    return lhs;
}

ExprPtr Parser::parse_conditional() {
    ExprPtr cond = parse_binary(1);
    if (check(Tok::Question)) {
        Span s = peek().span;
        advance();
        auto c = new_expr(ExprKind::Conditional, s);
        c->lhs = std::move(cond);
        c->rhs = parse_expression();
        expect(Tok::Colon, "':' in conditional expression");
        c->third = parse_conditional();
        return c;
    }
    return cond;
}

// Precedence climbing over the table above, which replaces one function
// per level (spec/grammar.md section 5).
ExprPtr Parser::parse_binary(int min_prec) {
    ExprPtr lhs = parse_cast();
    for (;;) {
        int prec = binary_prec(peek().kind);
        if (prec < min_prec) return lhs;
        Span s = peek().span;
        Tok op = advance().kind;
        ExprPtr rhs = parse_binary(prec + 1);
        auto b = new_expr(ExprKind::Binary, s);
        b->op  = op;
        b->lhs = std::move(lhs);
        b->rhs = std::move(rhs);
        lhs = std::move(b);
    }
}

// '(' begins either a cast or a parenthesised expression; the token after
// it decides which.
ExprPtr Parser::parse_cast() {
    if (check(Tok::LParen) && is_type_start(1)) {
        Span s = peek().span;
        advance();
        TypePtr t = parse_type_name();
        expect(Tok::RParen, "')' after cast type");
        auto c = new_expr(ExprKind::Cast, s);
        c->cast_type = t;
        c->lhs = parse_cast();
        return c;
    }
    return parse_unary();
}

ExprPtr Parser::parse_unary() {
    Span s = peek().span;

    if (check(Tok::PlusPlus) || check(Tok::MinusMinus)) {
        Tok op = advance().kind;
        auto e = new_expr(ExprKind::PreIncDec, s);
        e->op  = op;
        e->lhs = parse_unary();
        return e;
    }

    switch (peek().kind) {
        case Tok::Amp: case Tok::Star: case Tok::Plus:
        case Tok::Minus: case Tok::Tilde: case Tok::Bang: {
            Tok op = advance().kind;
            auto e = new_expr(ExprKind::Unary, s);
            e->op  = op;
            e->lhs = parse_cast();
            return e;
        }
        case Tok::KwSizeof: {
            advance();
            if (check(Tok::LParen) && is_type_start(1)) {
                advance();
                TypePtr t = parse_type_name();
                expect(Tok::RParen, "')' after sizeof type");
                auto e = new_expr(ExprKind::SizeofType, s);
                e->cast_type = t;
                return e;
            }
            auto e = new_expr(ExprKind::SizeofExpr, s);
            e->lhs = parse_unary();
            return e;
        }
        default: return parse_postfix();
    }
}

ExprPtr Parser::parse_postfix() {
    ExprPtr e = parse_primary();
    for (;;) {
        Span s = peek().span;
        if (match(Tok::LBracket)) {
            auto n = new_expr(ExprKind::Index, s);
            n->lhs = std::move(e);
            n->rhs = parse_expression();
            expect(Tok::RBracket, "']' after subscript");
            e = std::move(n);
        } else if (match(Tok::LParen)) {
            auto n = new_expr(ExprKind::Call, s);
            n->lhs = std::move(e);
            if (!check(Tok::RParen)) {
                do { n->args.push_back(parse_assignment()); }
                while (match(Tok::Comma));
            }
            expect(Tok::RParen, "')' after arguments");
            e = std::move(n);
        } else if (check(Tok::Dot) || check(Tok::Arrow)) {
            bool arrow = check(Tok::Arrow);
            advance();
            auto n = new_expr(ExprKind::Member, s);
            n->arrow = arrow;
            n->lhs   = std::move(e);
            n->text  = expect(Tok::Ident, "a member name").lexeme;
            e = std::move(n);
        } else if (check(Tok::PlusPlus) || check(Tok::MinusMinus)) {
            Tok op = advance().kind;
            auto n = new_expr(ExprKind::PostIncDec, s);
            n->op  = op;
            n->lhs = std::move(e);
            e = std::move(n);
        } else {
            return e;
        }
    }
}

ExprPtr Parser::parse_primary() {
    Span s = peek().span;
    switch (peek().kind) {
        case Tok::IntLit: {
            const Token& t = advance();
            auto e = new_expr(ExprKind::IntLit, s);
            e->int_value = t.int_value;
            return e;
        }
        case Tok::CharLit: {
            const Token& t = advance();
            auto e = new_expr(ExprKind::CharLit, s);
            e->int_value = t.int_value;
            e->text = t.lexeme;
            return e;
        }
        case Tok::StringLit: {
            const Token& t = advance();
            auto e = new_expr(ExprKind::StringLit, s);
            e->text = t.lexeme;
            return e;
        }
        case Tok::FloatLit: {
            const Token& t = advance();
            error("not-in-subset",
                  "floating-point constants are not in the supported subset", s);
            auto e = new_expr(ExprKind::FloatLit, s);
            e->text = t.lexeme;
            return e;
        }
        case Tok::Ident: {
            const Token& t = advance();
            auto e = new_expr(ExprKind::Ident, s);
            e->text = t.lexeme;
            return e;
        }
        case Tok::LParen: {
            advance();
            ExprPtr e = parse_expression();
            expect(Tok::RParen, "')'");
            return e;
        }
        default:
            fatal("expected-expression",
                  "expected an expression but found '" +
                  (peek().kind == Tok::EndOfFile ? std::string("end of file")
                                                 : peek().lexeme) + "'", s);
    }
}

}  // namespace cviz