#include "irgen.h"

namespace cviz {

void IRGen::error(const std::string& code, const std::string& msg, Span s) {
    diags_.push_back(Diagnostic{DiagClass::Lowering, code, msg, s});
}

// The returned reference is only valid until the next emit.
Instr& IRGen::emit(Op op) {
    fn_->code.emplace_back();
    Instr& in = fn_->code.back();
    in.op   = op;
    in.id   = next_id_++;
    in.node = cur_node_;
    in.span = cur_span_;
    return in;
}

std::string IRGen::new_temp()  { return "%" + std::to_string(++temp_); }
std::string IRGen::new_label() { return "L" + std::to_string(++label_); }

void IRGen::place_label(const std::string& l) { emit(Op::Label).target = l; }
void IRGen::jump(const std::string& l)        { emit(Op::Jmp).target = l; }

void IRGen::branch(Operand c, const std::string& t, const std::string& f) {
    Instr& br = emit(Op::Br);
    br.a = c;
    br.target = t;
    br.target2 = f;
}

void IRGen::push_scope() { scopes_.emplace_back(); }
void IRGen::pop_scope()  { scopes_.pop_back(); }

IRGen::VarInfo* IRGen::find(const std::string& name) {
    for (size_t i = scopes_.size(); i-- > 0;) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) return &it->second;
    }
    return nullptr;
}

// Locals get a numeric suffix so that shadowed names stay distinct in the IR.
std::string IRGen::declare_local(const std::string& name, TypePtr t) {
    std::string u = name + "." + std::to_string(++local_);
    scopes_.back()[name] = VarInfo{u, t, false};
    if (is_array(t)) fn_->arrays.push_back({u, cells(t)});
    return u;
}

bool IRGen::is_array(const TypePtr& t) { return t && t->kind == TypeKind::Array; }

bool IRGen::is_ptrlike(const TypePtr& t) {
    return t && (t->kind == TypeKind::Pointer || t->kind == TypeKind::Array);
}

TypePtr IRGen::pointee(const TypePtr& t) { return t ? t->base : nullptr; }

bool IRGen::narrow(const TypePtr& t) {
    if (!t) return true;
    switch (t->kind) {
        case TypeKind::Int: case TypeKind::Short: case TypeKind::Char: case TypeKind::Bool:
            return true;
        default:
            return false;
    }
}

// The IR memory model is word-addressed: every scalar occupies one cell, so
// an array's size in cells is the product of its dimensions.
long long IRGen::cells(const TypePtr& t) {
    if (!t) return 1;
    if (t->kind == TypeKind::Array) {
        long long n = t->array_len < 0 ? 1 : t->array_len;
        return n * cells(t->base);
    }
    return 1;
}

// sizeof reports C sizes (as on x86-64), independent of the cell model.
long long IRGen::byte_size(const TypePtr& t) {
    if (!t) return 4;
    switch (t->kind) {
        case TypeKind::Char: case TypeKind::Bool: case TypeKind::Void: return 1;
        case TypeKind::Short:  return 2;
        case TypeKind::Int:    case TypeKind::Float: return 4;
        case TypeKind::Long:   case TypeKind::Double: case TypeKind::Pointer: return 8;
        case TypeKind::Array:  return (t->array_len < 0 ? 0 : t->array_len) * byte_size(t->base);
        default:               return 1;
    }
}

std::string IRGen::decode_string(const std::string& lex) {
    std::string out;
    size_t i = 1, end = lex.size() >= 2 ? lex.size() - 1 : lex.size();
    while (i < end) {
        char c = lex[i++];
        if (c != '\\' || i >= end) { out += c; continue; }
        char e = lex[i++];
        switch (e) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case '0': out += '\0'; break;
            case 'a': out += '\a'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'v': out += '\v'; break;
            default:  out += e;    break;
        }
    }
    return out;
}

bool IRGen::const_eval(const Expr& e, long long& out) {
    switch (e.kind) {
        case ExprKind::IntLit: case ExprKind::CharLit:
            out = e.int_value;
            return true;
        case ExprKind::Unary: {
            long long v;
            if (!e.lhs || !const_eval(*e.lhs, v)) return false;
            return eval_unop(e.op, v, true, out);
        }
        case ExprKind::Binary: {
            long long a, b;
            if (!e.lhs || !e.rhs || !const_eval(*e.lhs, a) || !const_eval(*e.rhs, b)) return false;
            if (e.op == Tok::AndAnd) { out = a && b; return true; }
            if (e.op == Tok::OrOr)   { out = a || b; return true; }
            return eval_binop(e.op, a, b, true, out);
        }
        case ExprKind::Cast:
            return e.lhs && const_eval(*e.lhs, out);
        case ExprKind::SizeofType:
            out = byte_size(e.cast_type);
            return true;
        case ExprKind::SizeofExpr:
            out = byte_size(e.lhs ? e.lhs->type : nullptr);
            return true;
        default:
            return false;
    }
}

Program IRGen::lower(TranslationUnit& tu) {
    scopes_.clear();
    scopes_.emplace_back();

    // Register every function first, so calls to functions defined later in
    // the file resolve.
    for (auto& d : tu.decls) {
        if (d->kind == DeclKind::Function) funcs_[d->func_name] = d->func_type;
        if (d->kind == DeclKind::Variable)
            for (auto& dr : d->declarators)
                if (dr.type && dr.type->kind == TypeKind::Function) funcs_[dr.name] = dr.type;
    }

    for (auto& d : tu.decls) {
        if (d->kind == DeclKind::Function && d->body) lower_function(*d);
        else if (d->kind == DeclKind::Variable)       lower_global(*d);
    }
    return prog_;
}

void IRGen::lower_global(Decl& d) {
    for (auto& dr : d.declarators) {
        if (dr.name.empty()) continue;
        if (dr.type && dr.type->kind == TypeKind::Function) continue;
        if (dr.type && dr.type->kind == TypeKind::Struct) {
            error("ir-unsupported", "struct variables are not yet lowered to IR", dr.span);
            continue;
        }
        Global g;
        g.name = dr.name;
        g.span = dr.span;
        if (is_array(dr.type)) {
            g.array = true;
            g.cells = cells(dr.type);
        } else if (dr.init) {
            long long v = 0;
            if (!const_eval(*dr.init, v))
                error("non-constant-initialiser",
                      "global '" + dr.name + "' must be initialised with a constant", dr.init->span);
            g.init = v;
        }
        prog_.globals.push_back(g);
        scopes_[0][dr.name] = VarInfo{dr.name, dr.type, true};
    }
}

void IRGen::lower_function(Decl& d) {
    Function f;
    f.name = d.func_name;
    f.node = d.id;
    f.span = d.span;
    fn_ = &f;
    temp_ = local_ = label_ = 0;

    Prov p(this, d.id, d.span);
    push_scope();

    const auto& ps = d.func_type->params;
    for (size_t i = 0; i < ps.size(); ++i) {
        if (ps[i] && ps[i]->kind == TypeKind::Void) continue;
        TypePtr pt = ps[i];
        if (is_array(pt)) pt = Type::pointer_to(pt->base);
        std::string pn = i < d.param_names.size() ? d.param_names[i] : "";
        if (pn.empty()) pn = "_arg" + std::to_string(i);
        f.params.push_back(declare_local(pn, pt));
    }

    for (auto& item : d.body->items) lower_stmt(*item);

    // Falling off the end of a function returns; main returns 0 (C99 5.1.2.2.3).
    bool ends_in_ret = false;
    for (auto it = f.code.rbegin(); it != f.code.rend(); ++it) {
        if (it->op == Op::Nop) continue;
        ends_in_ret = it->op == Op::Ret;
        break;
    }
    if (!ends_in_ret) {
        Instr& r = emit(Op::Ret);
        if (f.name == "main") r.a = Operand::constant(0);
    }

    pop_scope();
    check_strings(f);
    prog_.functions.push_back(std::move(f));
    fn_ = nullptr;
}

// String literals exist only as printf arguments in the IR memory model.
void IRGen::check_strings(const Function& f) {
    for (const auto& in : f.code) {
        bool bad = in.a.is_str() || in.b.is_str();
        if (in.op == Op::Call && in.callee != "printf") {
            for (const auto& a : in.args) if (a.is_str()) bad = true;
        }
        if (bad) error("ir-unsupported",
                       "string literals are only supported as printf arguments", in.span);
    }
}

void IRGen::lower_local_decl(Decl& d) {
    for (auto& dr : d.declarators) {
        if (dr.name.empty()) continue;
        if (dr.type && dr.type->kind == TypeKind::Function) {
            funcs_[dr.name] = dr.type;
            continue;
        }
        if (dr.type && dr.type->kind == TypeKind::Struct) {
            error("ir-unsupported", "struct variables are not yet lowered to IR", dr.span);
            continue;
        }
        std::string un = declare_local(dr.name, dr.type);
        if (!dr.init) continue;
        if (is_array(dr.type)) {
            error("ir-unsupported", "array initialisers are not yet lowered to IR", dr.span);
            continue;
        }
        Prov p(this, dr.init->id, dr.init->span);
        Operand v = lower_expr(*dr.init);
        Instr& c = emit(Op::Copy);
        c.dst = un;
        c.a = v;
    }
}

void IRGen::collect_cases(Stmt& s, std::vector<Stmt*>& out) {
    if (s.kind == StmtKind::Switch) return;   // nested switches own their cases
    if (s.kind == StmtKind::Case || s.kind == StmtKind::Default) out.push_back(&s);
    if (s.body) collect_cases(*s.body, out);
    if (s.else_body) collect_cases(*s.else_body, out);
    for (auto& it : s.items) collect_cases(*it, out);
}

void IRGen::lower_stmt(Stmt& s) {
    Prov p(this, s.id, s.span);
    switch (s.kind) {
        case StmtKind::Compound:
            push_scope();
            for (auto& it : s.items) lower_stmt(*it);
            pop_scope();
            break;

        case StmtKind::DeclStmt:
            if (s.decl) lower_local_decl(*s.decl);
            break;

        case StmtKind::ExprStmt:
            if (s.expr) lower_expr(*s.expr);
            break;

        case StmtKind::If: {
            Operand c = lower_expr(*s.expr);
            std::string lt = new_label(), lf = new_label();
            branch(c, lt, lf);
            place_label(lt);
            lower_stmt(*s.body);
            if (s.else_body) {
                std::string le = new_label();
                jump(le);
                place_label(lf);
                lower_stmt(*s.else_body);
                place_label(le);
            } else {
                place_label(lf);
            }
            break;
        }

        case StmtKind::While: {
            std::string lc = new_label(), lb = new_label(), le = new_label();
            place_label(lc);
            Operand c = lower_expr(*s.expr);
            branch(c, lb, le);
            place_label(lb);
            break_to_.push_back(le);
            continue_to_.push_back(lc);
            lower_stmt(*s.body);
            break_to_.pop_back();
            continue_to_.pop_back();
            jump(lc);
            place_label(le);
            break;
        }

        case StmtKind::DoWhile: {
            std::string lb = new_label(), lc = new_label(), le = new_label();
            place_label(lb);
            break_to_.push_back(le);
            continue_to_.push_back(lc);
            lower_stmt(*s.body);
            break_to_.pop_back();
            continue_to_.pop_back();
            place_label(lc);
            Operand c = lower_expr(*s.expr);
            branch(c, lb, le);
            place_label(le);
            break;
        }

        case StmtKind::For: {
            push_scope();
            if (s.init_expr) lower_expr(*s.init_expr);
            std::string lc = new_label(), lb = new_label(), ls = new_label(), le = new_label();
            place_label(lc);
            if (s.cond_expr) {
                Operand c = lower_expr(*s.cond_expr);
                branch(c, lb, le);
            }
            place_label(lb);
            break_to_.push_back(le);
            continue_to_.push_back(ls);
            lower_stmt(*s.body);
            break_to_.pop_back();
            continue_to_.pop_back();
            place_label(ls);
            if (s.step_expr) lower_expr(*s.step_expr);
            jump(lc);
            place_label(le);
            pop_scope();
            break;
        }

        // A switch becomes a chain of comparisons followed by the body laid
        // out in source order, which is what gives fall-through for free.
        case StmtKind::Switch: {
            Operand v = lower_expr(*s.expr);
            std::string sv = new_temp();
            { Instr& c = emit(Op::Copy); c.dst = sv; c.a = v; }

            std::vector<Stmt*> cases;
            if (s.body) collect_cases(*s.body, cases);
            std::unordered_map<const Stmt*, std::string> labels;
            std::string le = new_label(), ldef;

            for (Stmt* cs : cases) {
                std::string l = new_label();
                labels[cs] = l;
                if (cs->kind == StmtKind::Default) { ldef = l; continue; }
                long long k = 0;
                if (!cs->expr || !const_eval(*cs->expr, k)) {
                    error("non-constant-case", "case label is not a constant expression", cs->span);
                    continue;
                }
                Operand eq = bin(Tok::Eq, Operand::var(sv), Operand::constant(k), true);
                std::string nx = new_label();
                branch(eq, l, nx);
                place_label(nx);
            }
            jump(ldef.empty() ? le : ldef);

            case_labels_.push_back(labels);
            break_to_.push_back(le);
            if (s.body) lower_stmt(*s.body);
            break_to_.pop_back();
            case_labels_.pop_back();
            place_label(le);
            break;
        }

        case StmtKind::Case:
        case StmtKind::Default: {
            if (!case_labels_.empty()) {
                auto it = case_labels_.back().find(&s);
                if (it != case_labels_.back().end()) place_label(it->second);
            }
            if (s.body) lower_stmt(*s.body);
            break;
        }

        case StmtKind::Break:
            if (break_to_.empty()) error("ir-internal", "break with no target", s.span);
            else jump(break_to_.back());
            break;

        case StmtKind::Continue:
            if (continue_to_.empty()) error("ir-internal", "continue with no target", s.span);
            else jump(continue_to_.back());
            break;

        case StmtKind::Return: {
            Operand v;
            if (s.expr) v = lower_expr(*s.expr);
            emit(Op::Ret).a = v;
            break;
        }

        case StmtKind::Empty:
            break;
    }
}

Operand IRGen::bin(Tok op, Operand a, Operand b, bool w32) {
    std::string t = new_temp();
    Instr& in = emit(Op::Bin);
    in.dst = t;
    in.a = a;
    in.b = b;
    in.bop = op;
    in.w32 = w32;
    return Operand::var(t);
}

Operand IRGen::lower_expr(Expr& e) {
    Prov p(this, e.id, e.span);
    switch (e.kind) {
        case ExprKind::IntLit:
        case ExprKind::CharLit:
            return Operand::constant(e.int_value);

        case ExprKind::StringLit:
            return Operand::str(decode_string(e.text));

        case ExprKind::FloatLit:
            error("ir-unsupported", "floating-point values are not lowered to IR", e.span);
            return Operand::constant(0);

        case ExprKind::Ident: {
            VarInfo* v = find(e.text);
            if (!v) {
                error("ir-unsupported",
                      funcs_.count(e.text) ? "functions can only be used as call targets"
                                           : "no storage for '" + e.text + "'", e.span);
                return Operand::constant(0);
            }
            if (is_array(v->type)) {        // an array decays to its address
                std::string t = new_temp();
                Instr& a = emit(Op::AddrOf);
                a.dst = t;
                a.sym = v->uname;
                return Operand::var(t);
            }
            return Operand::var(v->uname);
        }

        case ExprKind::Unary: {
            if (e.op == Tok::Amp) return address_of(*e.lhs);
            if (e.op == Tok::Star) {
                Operand addr = lower_expr(*e.lhs);
                if (is_array(e.type)) return addr;
                std::string t = new_temp();
                Instr& l = emit(Op::Load);
                l.dst = t;
                l.a = addr;
                return Operand::var(t);
            }
            Operand a = lower_expr(*e.lhs);
            if (e.op == Tok::Plus) return a;
            std::string t = new_temp();
            Instr& u = emit(Op::Un);
            u.dst = t;
            u.a = a;
            u.bop = e.op;
            u.w32 = narrow(e.type);
            return Operand::var(t);
        }

        case ExprKind::Binary: {
            if (e.op == Tok::AndAnd || e.op == Tok::OrOr) return short_circuit(e);
            TypePtr lt = e.lhs->type, rt = e.rhs->type;
            bool lp = is_ptrlike(lt), rp = is_ptrlike(rt);
            Operand a = lower_expr(*e.lhs);
            Operand b = lower_expr(*e.rhs);

            // Pointer arithmetic is scaled by the size of the pointee in cells.
            if ((e.op == Tok::Plus || e.op == Tok::Minus) && (lp || rp)) {
                if (lp && rp) {
                    Operand d = bin(Tok::Minus, a, b, false);
                    long long k = cells(pointee(lt));
                    return k == 1 ? d : bin(Tok::Slash, d, Operand::constant(k), false);
                }
                Operand ptr = lp ? a : b, idx = lp ? b : a;
                long long k = cells(pointee(lp ? lt : rt));
                Operand scaled = k == 1 ? idx : bin(Tok::Star, idx, Operand::constant(k), false);
                return bin(e.op, ptr, scaled, false);
            }
            return bin(e.op, a, b, narrow(e.type));
        }

        case ExprKind::Assign:
            return lower_assign(e);

        case ExprKind::Conditional: {
            std::string t = new_temp();
            Operand c = lower_expr(*e.lhs);
            std::string lt = new_label(), lf = new_label(), le = new_label();
            branch(c, lt, lf);
            place_label(lt);
            Operand x = lower_expr(*e.rhs);
            { Instr& m = emit(Op::Copy); m.dst = t; m.a = x; }
            jump(le);
            place_label(lf);
            Operand y = lower_expr(*e.third);
            { Instr& m = emit(Op::Copy); m.dst = t; m.a = y; }
            place_label(le);
            return Operand::var(t);
        }

        case ExprKind::Call:
            return lower_call(e);

        case ExprKind::Index: {
            Operand addr = element_address(e);
            if (is_array(e.type)) return addr;   // a row of a 2-D array
            std::string t = new_temp();
            Instr& l = emit(Op::Load);
            l.dst = t;
            l.a = addr;
            return Operand::var(t);
        }

        case ExprKind::Member:
            error("ir-unsupported", "struct member access is not yet lowered to IR", e.span);
            return Operand::constant(0);

        case ExprKind::Cast:
            return lower_expr(*e.lhs);

        case ExprKind::SizeofExpr:
            return Operand::constant(byte_size(e.lhs ? e.lhs->type : nullptr));

        case ExprKind::SizeofType:
            return Operand::constant(byte_size(e.cast_type));

        case ExprKind::PreIncDec:
        case ExprKind::PostIncDec:
            return lower_incdec(e);

        case ExprKind::Comma:
            lower_expr(*e.lhs);
            return lower_expr(*e.rhs);
    }
    return Operand::constant(0);
}

Operand IRGen::short_circuit(Expr& e) {
    bool is_and = e.op == Tok::AndAnd;
    std::string t = new_temp();
    Operand a = lower_expr(*e.lhs);
    std::string lr = new_label(), ls = new_label(), le = new_label();
    if (is_and) branch(a, lr, ls);
    else        branch(a, ls, lr);
    place_label(lr);
    Operand b = lower_expr(*e.rhs);
    {
        Instr& ne = emit(Op::Bin);
        ne.dst = t;
        ne.a = b;
        ne.b = Operand::constant(0);
        ne.bop = Tok::Ne;
    }
    jump(le);
    place_label(ls);
    { Instr& c = emit(Op::Copy); c.dst = t; c.a = Operand::constant(is_and ? 0 : 1); }
    place_label(le);
    return Operand::var(t);
}

Operand IRGen::address_of(Expr& x) {
    Prov p(this, x.id, x.span);
    if (x.kind == ExprKind::Ident) {
        VarInfo* v = find(x.text);
        if (v && is_array(v->type)) {
            std::string t = new_temp();
            Instr& a = emit(Op::AddrOf);
            a.dst = t;
            a.sym = v->uname;
            return Operand::var(t);
        }
        error("ir-unsupported",
              "taking the address of scalar '" + x.text + "' is not supported by the IR memory model",
              x.span);
        return Operand::constant(0);
    }
    if (x.kind == ExprKind::Index) return element_address(x);
    if (x.kind == ExprKind::Unary && x.op == Tok::Star) return lower_expr(*x.lhs);
    error("ir-unsupported", "cannot take the address of this expression", x.span);
    return Operand::constant(0);
}

Operand IRGen::element_address(Expr& e) {
    Prov p(this, e.id, e.span);
    Operand base = lower_expr(*e.lhs);
    Operand idx  = lower_expr(*e.rhs);
    long long k = cells(e.type);
    Operand off = k == 1 ? idx : bin(Tok::Star, idx, Operand::constant(k), false);
    return bin(Tok::Plus, base, off, false);
}

IRGen::LValue IRGen::lower_lvalue(Expr& e) {
    Prov p(this, e.id, e.span);
    LValue lv;
    lv.type = e.type;
    if (e.kind == ExprKind::Ident) {
        VarInfo* v = find(e.text);
        if (v && !is_array(v->type)) {
            lv.is_var = true;
            lv.name = v->uname;
            return lv;
        }
    } else if (e.kind == ExprKind::Index) {
        lv.addr = element_address(e);
        return lv;
    } else if (e.kind == ExprKind::Unary && e.op == Tok::Star) {
        lv.addr = lower_expr(*e.lhs);
        return lv;
    }
    error("ir-unsupported", "this expression cannot be assigned in the IR model", e.span);
    lv.is_var = true;
    lv.name = "%invalid";
    return lv;
}

Operand IRGen::load(const LValue& lv) {
    if (lv.is_var) return Operand::var(lv.name);
    std::string t = new_temp();
    Instr& l = emit(Op::Load);
    l.dst = t;
    l.a = lv.addr;
    return Operand::var(t);
}

void IRGen::store(const LValue& lv, Operand v) {
    if (lv.is_var) {
        Instr& c = emit(Op::Copy);
        c.dst = lv.name;
        c.a = v;
    } else {
        Instr& s = emit(Op::Store);
        s.a = lv.addr;
        s.b = v;
    }
}

static Tok compound_base(Tok t) {
    switch (t) {
        case Tok::PlusAssign:    return Tok::Plus;
        case Tok::MinusAssign:   return Tok::Minus;
        case Tok::StarAssign:    return Tok::Star;
        case Tok::SlashAssign:   return Tok::Slash;
        case Tok::PercentAssign: return Tok::Percent;
        case Tok::AmpAssign:     return Tok::Amp;
        case Tok::PipeAssign:    return Tok::Pipe;
        case Tok::CaretAssign:   return Tok::Caret;
        case Tok::LShiftAssign:  return Tok::LShift;
        case Tok::RShiftAssign:  return Tok::RShift;
        default:                 return Tok::Error;
    }
}

Operand IRGen::lower_assign(Expr& e) {
    LValue lv = lower_lvalue(*e.lhs);
    if (e.op == Tok::Assign) {
        Operand v = lower_expr(*e.rhs);
        store(lv, v);
        return v;
    }
    Tok op = compound_base(e.op);
    Operand cur = load(lv);
    Operand r = lower_expr(*e.rhs);
    Operand res;
    if ((op == Tok::Plus || op == Tok::Minus) && is_ptrlike(lv.type)) {
        long long k = cells(pointee(lv.type));
        Operand scaled = k == 1 ? r : bin(Tok::Star, r, Operand::constant(k), false);
        res = bin(op, cur, scaled, false);
    } else {
        res = bin(op, cur, r, narrow(lv.type));
    }
    store(lv, res);
    return res;
}

Operand IRGen::lower_incdec(Expr& e) {
    LValue lv = lower_lvalue(*e.lhs);
    Operand cur = load(lv);
    // A postfix result must be captured before the variable is updated.
    if (e.kind == ExprKind::PostIncDec && lv.is_var) {
        std::string t = new_temp();
        Instr& c = emit(Op::Copy);
        c.dst = t;
        c.a = cur;
        cur = Operand::var(t);
    }
    bool ptr = is_ptrlike(lv.type);
    long long k = ptr ? cells(pointee(lv.type)) : 1;
    Operand nv = bin(e.op == Tok::PlusPlus ? Tok::Plus : Tok::Minus, cur, Operand::constant(k),
                     !ptr && narrow(lv.type));
    store(lv, nv);
    return e.kind == ExprKind::PreIncDec ? nv : cur;
}

Operand IRGen::lower_call(Expr& e) {
    if (!e.lhs || e.lhs->kind != ExprKind::Ident) {
        error("ir-unsupported", "calls through expressions are not supported", e.span);
        return Operand::constant(0);
    }
    std::string callee = e.lhs->text;
    std::vector<Operand> args;
    for (auto& a : e.args) args.push_back(lower_expr(*a));

    bool builtin = callee == "printf" || callee == "putchar";
    bool has_value = builtin;
    if (!builtin) {
        auto it = funcs_.find(callee);
        if (it == funcs_.end()) {
            error("ir-unknown", "call to undefined function '" + callee + "'", e.span);
        } else {
            has_value = it->second && it->second->base && it->second->base->kind != TypeKind::Void;
        }
    }

    std::string t = has_value ? new_temp() : "";
    Instr& c = emit(Op::Call);
    c.dst = t;
    c.callee = callee;
    c.args = args;
    return has_value ? Operand::var(t) : Operand::constant(0);
}

}  // namespace cviz
