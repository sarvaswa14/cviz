#include "sema.h"

namespace cviz {

void SymbolTable::push(const char* kind) {
    Scope s;
    s.kind = kind;
    scopes_.push_back(std::move(s));
}

void SymbolTable::pop() {
    if (!scopes_.empty()) scopes_.pop_back();
}

Symbol* SymbolTable::declare(const Symbol& s) {
    if (scopes_.empty()) push("file");
    Scope& cur = scopes_.back();
    if (cur.syms.count(s.name)) return nullptr;
    cur.order.push_back(s.name);
    auto& slot = cur.syms[s.name];
    slot = s;
    slot.scope_level = level();
    return &slot;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (size_t i = scopes_.size(); i-- > 0;) {
        auto it = scopes_[i].syms.find(name);
        if (it != scopes_[i].syms.end()) return &it->second;
    }
    return nullptr;
}

Symbol* SymbolTable::lookup_current(const std::string& name) {
    if (scopes_.empty()) return nullptr;
    auto it = scopes_.back().syms.find(name);
    return it == scopes_.back().syms.end() ? nullptr : &it->second;
}

void Sema::error(const std::string& code, const std::string& msg, Span s) {
    diags_.push_back(Diagnostic{DiagClass::Semantic, code, msg, s});
}

void Sema::open_scope(const char* kind) {
    table_.push(kind);
    SemaEvent e;
    e.kind  = SemaEvent::Kind::ScopeOpen;
    e.id    = event_id_++;
    e.scope = ++scope_counter_;
    e.of    = kind;
    events_.push_back(e);
}

void Sema::close_scope() {
    SemaEvent e;
    e.kind  = SemaEvent::Kind::ScopeClose;
    e.id    = event_id_++;
    e.scope = scope_counter_;
    events_.push_back(e);
    table_.pop();
}

void Sema::record_symbol(const Symbol& s) {
    SemaEvent e;
    e.kind  = SemaEvent::Kind::SymbolDecl;
    e.id    = event_id_++;
    e.scope = scope_counter_;
    e.name  = s.name;
    e.type  = s.type ? s.type->to_string() : "?";
    e.span  = s.span;
    events_.push_back(e);
}

void Sema::record_type(const Expr* e) {
    if (!e || !e->type) return;
    SemaEvent ev;
    ev.kind = SemaEvent::Kind::TypeAssign;
    ev.id   = event_id_++;
    ev.type = e->type->to_string();
    ev.node = e->id;
    ev.span = e->span;
    events_.push_back(ev);
}

// printf and putchar are provided by the execution engine rather than by a
// header, since the preprocessor does not yet handle system includes. They
// are declared in file scope but not recorded in the trace.
void Sema::declare_builtins() {
    auto charp = Type::pointer_to(Type::make(TypeKind::Char));
    auto pf = Type::function(Type::make(TypeKind::Int), {charp});
    pf->variadic = true;
    Symbol s1; s1.name = "printf"; s1.type = pf; s1.is_function = true; s1.defined = true;
    table_.declare(s1);

    auto pc = Type::function(Type::make(TypeKind::Int), {Type::make(TypeKind::Int)});
    Symbol s2; s2.name = "putchar"; s2.type = pc; s2.is_function = true; s2.defined = true;
    table_.declare(s2);
}

bool Sema::is_integer(const TypePtr& t) {
    if (!t) return false;
    switch (t->kind) {
        case TypeKind::Char: case TypeKind::Short:
        case TypeKind::Int:  case TypeKind::Long:
        case TypeKind::Bool:
            return true;
        default: return false;
    }
}

bool Sema::is_arithmetic(const TypePtr& t) {
    if (!t) return false;
    if (is_integer(t)) return true;
    return t->kind == TypeKind::Float || t->kind == TypeKind::Double;
}

bool Sema::is_scalar(const TypePtr& t) {
    if (!t) return false;
    return is_arithmetic(t) || t->kind == TypeKind::Pointer;
}

int Sema::rank(const TypePtr& t) {
    if (!t) return 0;
    switch (t->kind) {
        case TypeKind::Bool:  return 1;
        case TypeKind::Char:  return 2;
        case TypeKind::Short: return 3;
        case TypeKind::Int:   return 4;
        case TypeKind::Long:  return 5;
        default: return 0;
    }
}

// An array used in an expression becomes a pointer to its first element,
// and a function becomes a pointer to function (C99 6.3.2.1).
TypePtr Sema::decay(const TypePtr& t) {
    if (!t) return t;
    if (t->kind == TypeKind::Array)    return Type::pointer_to(t->base);
    if (t->kind == TypeKind::Function) return Type::pointer_to(t);
    return t;
}

// Integer promotion followed by the usual arithmetic conversions.
TypePtr Sema::usual_conversions(const TypePtr& a, const TypePtr& b) {
    if (!is_arithmetic(a) || !is_arithmetic(b)) return a ? a : b;
    if (a->kind == TypeKind::Double || b->kind == TypeKind::Double)
        return Type::make(TypeKind::Double);
    if (a->kind == TypeKind::Float || b->kind == TypeKind::Float)
        return Type::make(TypeKind::Float);

    TypePtr pa = rank(a) < 4 ? Type::make(TypeKind::Int) : a;
    TypePtr pb = rank(b) < 4 ? Type::make(TypeKind::Int) : b;

    if (rank(pa) > rank(pb)) return pa;
    if (rank(pb) > rank(pa)) return pb;
    return pa->is_unsigned ? pa : pb;
}

bool Sema::compatible(const TypePtr& a, const TypePtr& b) {
    if (!a || !b) return true;
    if (a->kind == TypeKind::Pointer && b->kind == TypeKind::Pointer) {
        if (!a->base || !b->base) return true;
        if (a->base->kind == TypeKind::Void || b->base->kind == TypeKind::Void)
            return true;
        return a->base->kind == b->base->kind &&
               a->base->is_unsigned == b->base->is_unsigned;
    }
    if (is_arithmetic(a) && is_arithmetic(b)) return true;
    if (a->kind == TypeKind::Struct && b->kind == TypeKind::Struct)
        return a->tag == b->tag;
    return a->kind == b->kind;
}

bool Sema::is_lvalue(const Expr& e) {
    switch (e.kind) {
        case ExprKind::Ident:
        case ExprKind::Index:
        case ExprKind::Member:
            return true;
        case ExprKind::Unary:
            return e.op == Tok::Star;
        default:
            return false;
    }
}

void Sema::analyse(TranslationUnit& tu) {
    open_scope("file");
    declare_builtins();
    for (auto& d : tu.decls) visit_decl(*d);
    close_scope();
}

void Sema::visit_decl(Decl& d) {
    if (d.kind == DeclKind::StructDef) return;

    if (d.kind == DeclKind::Function) {
        Symbol fn;
        fn.name        = d.func_name;
        fn.type        = d.func_type;
        fn.is_function = true;
        fn.defined     = d.body != nullptr;
        fn.span        = d.span;

        Symbol* prev = table_.lookup_current(d.func_name);
        if (prev && prev->defined && fn.defined) {
            error("duplicate-declaration",
                  "function '" + d.func_name + "' is defined more than once", d.span);
        } else if (!prev) {
            table_.declare(fn);
            record_symbol(fn);
        } else {
            prev->defined = prev->defined || fn.defined;
        }

        if (!d.body) return;

        current_return_ = d.func_type ? d.func_type->base : nullptr;
        open_scope("function");

        const auto& ps = d.func_type ? d.func_type->params : std::vector<TypePtr>{};
        for (size_t i = 0; i < ps.size(); ++i) {
            if (ps[i] && ps[i]->kind == TypeKind::Void) continue;
            std::string pname = i < d.param_names.size() ? d.param_names[i] : "";
            if (pname.empty()) continue;
            Symbol p;
            p.name = pname;
            p.type = decay(ps[i]);
            p.span = d.span;
            if (!table_.declare(p)) {
                error("duplicate-declaration",
                      "parameter '" + pname + "' is declared more than once", d.span);
            } else {
                record_symbol(p);
            }
        }

        // The body shares the function scope, so a local may not shadow a
        // parameter of the same function.
        for (auto& item : d.body->items) visit_stmt(*item);

        close_scope();
        current_return_ = nullptr;
        return;
    }

    for (auto& dr : d.declarators) {
        if (dr.name.empty()) continue;

        if (dr.type && dr.type->kind == TypeKind::Void) {
            error("invalid-type", "variable '" + dr.name + "' declared with type void", dr.span);
        }

        Symbol s;
        s.name = dr.name;
        s.type = dr.type;
        s.span = dr.span;
        s.is_function = dr.type && dr.type->kind == TypeKind::Function;

        if (!table_.declare(s)) {
            error("duplicate-declaration",
                  "'" + dr.name + "' is already declared in this scope", dr.span);
        } else {
            record_symbol(s);
        }

        if (dr.init) {
            TypePtr it = visit_expr(*dr.init);
            TypePtr target = decay(dr.type);
            if (it && target && !compatible(target, decay(it))) {
                error("incompatible-initialiser",
                      "cannot initialise '" + target->to_string() +
                      "' from '" + it->to_string() + "'", dr.init->span);
            }
        }
    }
}

void Sema::visit_stmt(Stmt& s) {
    switch (s.kind) {
        case StmtKind::Compound:
            open_scope("block");
            for (auto& it : s.items) visit_stmt(*it);
            close_scope();
            break;

        case StmtKind::DeclStmt:
            if (s.decl) visit_decl(*s.decl);
            break;

        case StmtKind::ExprStmt:
            if (s.expr) visit_expr(*s.expr);
            break;

        case StmtKind::If: {
            if (s.expr) {
                TypePtr t = decay(visit_expr(*s.expr));
                if (t && !is_scalar(t)) {
                    error("invalid-condition",
                          "condition has non-scalar type '" + t->to_string() + "'", s.expr->span);
                }
            }
            if (s.body) visit_stmt(*s.body);
            if (s.else_body) visit_stmt(*s.else_body);
            break;
        }

        case StmtKind::While:
        case StmtKind::DoWhile: {
            if (s.expr) {
                TypePtr t = decay(visit_expr(*s.expr));
                if (t && !is_scalar(t)) {
                    error("invalid-condition",
                          "condition has non-scalar type '" + t->to_string() + "'", s.expr->span);
                }
            }
            ++loop_depth_;
            if (s.body) visit_stmt(*s.body);
            --loop_depth_;
            break;
        }

        case StmtKind::For: {
            open_scope("block");
            if (s.init_expr) visit_expr(*s.init_expr);
            if (s.cond_expr) {
                TypePtr t = decay(visit_expr(*s.cond_expr));
                if (t && !is_scalar(t)) {
                    error("invalid-condition",
                          "condition has non-scalar type '" + t->to_string() + "'", s.cond_expr->span);
                }
            }
            if (s.step_expr) visit_expr(*s.step_expr);
            ++loop_depth_;
            if (s.body) visit_stmt(*s.body);
            --loop_depth_;
            close_scope();
            break;
        }

        case StmtKind::Switch: {
            if (s.expr) {
                TypePtr t = decay(visit_expr(*s.expr));
                if (t && !is_integer(t)) {
                    error("invalid-switch",
                          "switch expression has non-integer type '" + t->to_string() + "'", s.expr->span);
                }
            }
            ++switch_depth_;
            if (s.body) visit_stmt(*s.body);
            --switch_depth_;
            break;
        }

        case StmtKind::Case:
            if (switch_depth_ == 0) error("misplaced-label", "'case' label outside a switch statement", s.span);
            if (s.expr) visit_expr(*s.expr);
            if (s.body) visit_stmt(*s.body);
            break;

        case StmtKind::Default:
            if (switch_depth_ == 0) error("misplaced-label", "'default' label outside a switch statement", s.span);
            if (s.body) visit_stmt(*s.body);
            break;

        case StmtKind::Break:
            if (loop_depth_ == 0 && switch_depth_ == 0)
                error("misplaced-jump", "'break' outside a loop or switch statement", s.span);
            break;

        case StmtKind::Continue:
            if (loop_depth_ == 0) error("misplaced-jump", "'continue' outside a loop", s.span);
            break;

        case StmtKind::Return: {
            bool want_void = current_return_ && current_return_->kind == TypeKind::Void;
            if (s.expr) {
                TypePtr t = decay(visit_expr(*s.expr));
                if (want_void) {
                    error("return-mismatch", "returning a value from a function returning void", s.expr->span);
                } else if (t && current_return_ && !compatible(current_return_, t)) {
                    error("return-mismatch",
                          "cannot return '" + t->to_string() +
                          "' from a function returning '" + current_return_->to_string() + "'", s.expr->span);
                }
            } else if (current_return_ && !want_void) {
                error("return-mismatch",
                      "return with no value in a function returning '" +
                      current_return_->to_string() + "'", s.span);
            }
            break;
        }

        case StmtKind::Empty:
            break;
    }
}

TypePtr Sema::visit_expr(Expr& e) {
    switch (e.kind) {
        case ExprKind::IntLit:
        case ExprKind::CharLit:
            e.type = Type::make(TypeKind::Int);
            break;

        case ExprKind::FloatLit:
            e.type = Type::make(TypeKind::Double);
            break;

        case ExprKind::StringLit:
            e.type = Type::pointer_to(Type::make(TypeKind::Char));
            break;

        case ExprKind::Ident: {
            Symbol* s = table_.lookup(e.text);
            if (!s) {
                error("undeclared-identifier", "use of undeclared identifier '" + e.text + "'", e.span);
                e.type = Type::make(TypeKind::Int);
            } else {
                e.type = s->type;
            }
            break;
        }

        case ExprKind::Unary: {
            TypePtr t = e.lhs ? visit_expr(*e.lhs) : nullptr;
            switch (e.op) {
                case Tok::Amp:
                    if (e.lhs && !is_lvalue(*e.lhs))
                        error("invalid-operand", "cannot take the address of a non-lvalue", e.span);
                    e.type = Type::pointer_to(t);
                    break;
                case Tok::Star: {
                    TypePtr d = decay(t);
                    if (!d || d->kind != TypeKind::Pointer) {
                        error("invalid-operand",
                              "cannot dereference a value of type '" + (d ? d->to_string() : "?") + "'", e.span);
                        e.type = Type::make(TypeKind::Int);
                    } else {
                        e.type = d->base;
                    }
                    break;
                }
                case Tok::Bang:
                    if (t && !is_scalar(decay(t)))
                        error("invalid-operand", "operand of '!' has non-scalar type", e.span);
                    e.type = Type::make(TypeKind::Int);
                    break;
                case Tok::Tilde:
                    if (t && !is_integer(t))
                        error("invalid-operand", "operand of '~' must have integer type", e.span);
                    e.type = rank(t) < 4 ? Type::make(TypeKind::Int) : t;
                    break;
                default:
                    if (t && !is_arithmetic(t))
                        error("invalid-operand", "operand must have arithmetic type", e.span);
                    e.type = rank(t) < 4 ? Type::make(TypeKind::Int) : t;
                    break;
            }
            break;
        }

        case ExprKind::Binary: {
            TypePtr a = decay(e.lhs ? visit_expr(*e.lhs) : nullptr);
            TypePtr b = decay(e.rhs ? visit_expr(*e.rhs) : nullptr);

            switch (e.op) {
                case Tok::Plus: case Tok::Minus: {
                    bool pa = a && a->kind == TypeKind::Pointer;
                    bool pb = b && b->kind == TypeKind::Pointer;
                    if (pa && pb) {
                        if (e.op == Tok::Minus) {
                            e.type = Type::make(TypeKind::Long);
                        } else {
                            error("invalid-operands", "cannot add two pointers", e.span);
                            e.type = a;
                        }
                    } else if (pa || pb) {
                        TypePtr p = pa ? a : b;
                        TypePtr i = pa ? b : a;
                        if (i && !is_integer(i))
                            error("invalid-operands", "pointer arithmetic requires an integer operand", e.span);
                        e.type = p;
                    } else {
                        if ((a && !is_arithmetic(a)) || (b && !is_arithmetic(b)))
                            error("invalid-operands", "operands must have arithmetic type", e.span);
                        e.type = usual_conversions(a, b);
                    }
                    break;
                }
                case Tok::Star: case Tok::Slash:
                    if ((a && !is_arithmetic(a)) || (b && !is_arithmetic(b)))
                        error("invalid-operands", "operands must have arithmetic type", e.span);
                    e.type = usual_conversions(a, b);
                    break;

                case Tok::Percent: case Tok::Amp: case Tok::Pipe:
                case Tok::Caret: case Tok::LShift: case Tok::RShift:
                    if ((a && !is_integer(a)) || (b && !is_integer(b)))
                        error("invalid-operands", "operands must have integer type", e.span);
                    e.type = usual_conversions(a, b);
                    break;

                case Tok::Lt: case Tok::Gt: case Tok::Le: case Tok::Ge:
                case Tok::Eq: case Tok::Ne:
                    if (a && b && !compatible(a, b))
                        error("invalid-operands",
                              "cannot compare '" + a->to_string() + "' with '" + b->to_string() + "'", e.span);
                    e.type = Type::make(TypeKind::Int);
                    break;

                case Tok::AndAnd: case Tok::OrOr:
                    if ((a && !is_scalar(a)) || (b && !is_scalar(b)))
                        error("invalid-operands", "operands must have scalar type", e.span);
                    e.type = Type::make(TypeKind::Int);
                    break;

                default:
                    e.type = usual_conversions(a, b);
                    break;
            }
            break;
        }

        case ExprKind::Assign: {
            TypePtr lt = e.lhs ? visit_expr(*e.lhs) : nullptr;
            TypePtr rt = decay(e.rhs ? visit_expr(*e.rhs) : nullptr);

            if (e.lhs && !is_lvalue(*e.lhs)) {
                error("invalid-lvalue", "left operand of assignment is not a modifiable lvalue", e.span);
            } else if (lt && lt->kind == TypeKind::Array) {
                error("invalid-lvalue", "cannot assign to an array", e.span);
            } else if (lt && rt && !compatible(decay(lt), rt)) {
                error("incompatible-assignment",
                      "cannot assign '" + rt->to_string() + "' to '" + lt->to_string() + "'", e.span);
            }
            e.type = lt;
            break;
        }

        case ExprKind::Conditional: {
            if (e.lhs) {
                TypePtr c = decay(visit_expr(*e.lhs));
                if (c && !is_scalar(c))
                    error("invalid-condition", "condition has non-scalar type", e.lhs->span);
            }
            TypePtr a = decay(e.rhs ? visit_expr(*e.rhs) : nullptr);
            TypePtr b = decay(e.third ? visit_expr(*e.third) : nullptr);
            if (a && b && !compatible(a, b))
                error("incompatible-branches", "branches of the conditional have incompatible types", e.span);
            e.type = is_arithmetic(a) && is_arithmetic(b) ? usual_conversions(a, b) : (a ? a : b);
            break;
        }

        case ExprKind::Call: {
            TypePtr ct = e.lhs ? visit_expr(*e.lhs) : nullptr;
            std::vector<TypePtr> ats;
            for (auto& a : e.args) ats.push_back(decay(visit_expr(*a)));

            TypePtr ft = ct;
            if (ft && ft->kind == TypeKind::Pointer && ft->base && ft->base->kind == TypeKind::Function)
                ft = ft->base;
            if (!ft || ft->kind != TypeKind::Function) {
                if (e.lhs && e.lhs->kind == ExprKind::Ident && table_.lookup(e.lhs->text))
                    error("not-callable", "'" + e.lhs->text + "' is not a function", e.span);
                e.type = Type::make(TypeKind::Int);
                break;
            }

            std::vector<TypePtr> ps;
            for (const auto& p : ft->params) {
                if (p && p->kind == TypeKind::Void) continue;
                ps.push_back(p);
            }

            bool count_ok = ft->variadic ? ats.size() >= ps.size() : ats.size() == ps.size();
            if (!count_ok) {
                error("argument-count",
                      "function expects " + std::string(ft->variadic ? "at least " : "") +
                      std::to_string(ps.size()) + " argument" + (ps.size() == 1 ? "" : "s") +
                      " but " + std::to_string(ats.size()) + " given", e.span);
            } else {
                for (size_t i = 0; i < ps.size(); ++i) {
                    if (ps[i] && ats[i] && !compatible(decay(ps[i]), ats[i])) {
                        error("argument-type",
                              "argument " + std::to_string(i + 1) + " has type '" + ats[i]->to_string() +
                              "' but '" + ps[i]->to_string() + "' is expected", e.args[i]->span);
                    }
                }
            }
            e.type = ft->base;
            break;
        }

        // a[i] is defined as *(a + i): the base must decay to a pointer and
        // the subscript must be an integer.
        case ExprKind::Index: {
            TypePtr b = decay(e.lhs ? visit_expr(*e.lhs) : nullptr);
            TypePtr i = decay(e.rhs ? visit_expr(*e.rhs) : nullptr);
            if (!b || b->kind != TypeKind::Pointer) {
                error("invalid-subscript", "subscripted value is not an array or pointer", e.span);
                e.type = Type::make(TypeKind::Int);
            } else {
                if (i && !is_integer(i))
                    error("invalid-subscript", "array subscript must have integer type", e.span);
                e.type = b->base;
            }
            break;
        }

        case ExprKind::Member: {
            TypePtr b = e.lhs ? visit_expr(*e.lhs) : nullptr;
            if (e.arrow) {
                TypePtr d = decay(b);
                if (!d || d->kind != TypeKind::Pointer) {
                    error("invalid-member", "'->' applied to a value that is not a pointer", e.span);
                    e.type = Type::make(TypeKind::Int);
                    break;
                }
                b = d->base;
            }
            if (!b || b->kind != TypeKind::Struct)
                error("invalid-member", "member access on a value that is not a struct", e.span);
            e.type = Type::make(TypeKind::Int);
            break;
        }

        case ExprKind::Cast:
            if (e.lhs) visit_expr(*e.lhs);
            e.type = e.cast_type;
            break;

        case ExprKind::SizeofExpr:
            if (e.lhs) visit_expr(*e.lhs);
            e.type = Type::make(TypeKind::Long, true);
            break;

        case ExprKind::SizeofType:
            e.type = Type::make(TypeKind::Long, true);
            break;

        case ExprKind::PreIncDec:
        case ExprKind::PostIncDec: {
            TypePtr t = e.lhs ? visit_expr(*e.lhs) : nullptr;
            if (e.lhs && !is_lvalue(*e.lhs))
                error("invalid-lvalue", "operand of increment or decrement is not a modifiable lvalue", e.span);
            TypePtr d = decay(t);
            if (d && !is_scalar(d))
                error("invalid-operand", "operand of increment or decrement must be scalar", e.span);
            e.type = t;
            break;
        }

        case ExprKind::Comma:
            if (e.lhs) visit_expr(*e.lhs);
            e.type = e.rhs ? visit_expr(*e.rhs) : nullptr;
            break;
    }

    record_type(&e);
    return e.type;
}

}  // namespace cviz
