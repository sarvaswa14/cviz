#include "ast.h"

namespace cviz {

TypePtr Type::make(TypeKind k, bool uns) {
    auto t = std::make_shared<Type>();
    t->kind = k;
    t->is_unsigned = uns;
    return t;
}

TypePtr Type::pointer_to(TypePtr b) {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Pointer;
    t->base = std::move(b);
    return t;
}

TypePtr Type::array_of(TypePtr b, long long n) {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Array;
    t->base = std::move(b);
    t->array_len = n;
    return t;
}

TypePtr Type::function(TypePtr ret, std::vector<TypePtr> ps) {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Function;
    t->base = std::move(ret);
    t->params = std::move(ps);
    return t;
}

std::string Type::to_string() const {
    std::string s;
    if (is_unsigned) s += "unsigned ";
    switch (kind) {
        case TypeKind::Void:   return s + "void";
        case TypeKind::Char:   return s + "char";
        case TypeKind::Short:  return s + "short";
        case TypeKind::Int:    return s + "int";
        case TypeKind::Long:   return s + "long";
        case TypeKind::Float:  return "float";
        case TypeKind::Double: return "double";
        case TypeKind::Bool:   return "_Bool";
        case TypeKind::Pointer: {
            // A pointer to an array or function needs parentheses, as in C.
            if (base && (base->kind == TypeKind::Array ||
                         base->kind == TypeKind::Function)) {
                std::string inner = base->to_string();
                size_t brk = inner.find_first_of("[(");
                if (brk != std::string::npos) {
                    return inner.substr(0, brk) + "(*)" + inner.substr(brk);
                }
            }
            return (base ? base->to_string() : "?") + "*";
        }
        case TypeKind::Array: {
            // Dimensions are collected outermost first so they read in
            // declaration order.
            std::string dims;
            const Type* t = this;
            while (t && t->kind == TypeKind::Array) {
                dims += "[";
                if (t->array_len >= 0) dims += std::to_string(t->array_len);
                dims += "]";
                t = t->base.get();
            }
            return (t ? t->to_string() : "?") + dims;
        }
        case TypeKind::Function: {
            std::string r = base ? base->to_string() : "?";
            r += "(";
            for (size_t i = 0; i < params.size(); ++i) {
                if (i) r += ", ";
                r += params[i] ? params[i]->to_string() : "?";
            }
            if (variadic) r += params.empty() ? "..." : ", ...";
            return r + ")";
        }
        case TypeKind::Struct:
            return "struct " + (tag.empty() ? std::string("<anon>") : tag);
    }
    return "?";
}

const char* Expr::kind_name() const {
    switch (kind) {
        case ExprKind::IntLit:      return "IntLit";
        case ExprKind::CharLit:     return "CharLit";
        case ExprKind::StringLit:   return "StringLit";
        case ExprKind::FloatLit:    return "FloatLit";
        case ExprKind::Ident:       return "Ident";
        case ExprKind::Unary:       return "Unary";
        case ExprKind::Binary:      return "Binary";
        case ExprKind::Assign:      return "Assign";
        case ExprKind::Conditional: return "Conditional";
        case ExprKind::Call:        return "Call";
        case ExprKind::Index:       return "Index";
        case ExprKind::Member:      return "Member";
        case ExprKind::Cast:        return "Cast";
        case ExprKind::SizeofExpr:  return "SizeofExpr";
        case ExprKind::SizeofType:  return "SizeofType";
        case ExprKind::PostIncDec:  return "PostIncDec";
        case ExprKind::PreIncDec:   return "PreIncDec";
        case ExprKind::Comma:       return "Comma";
    }
    return "Expr";
}

std::string Expr::label() const {
    switch (kind) {
        case ExprKind::IntLit:      return std::to_string(int_value);
        case ExprKind::CharLit:     return text;
        case ExprKind::StringLit:   return text;
        case ExprKind::FloatLit:    return text;
        case ExprKind::Ident:       return text;
        case ExprKind::Member:      return (arrow ? "->" : ".") + text;
        case ExprKind::Cast:        return cast_type ? cast_type->to_string() : "cast";
        case ExprKind::SizeofType:  return cast_type ? cast_type->to_string() : "sizeof";
        case ExprKind::Index:       return "[]";
        case ExprKind::Call:        return "()";
        case ExprKind::Conditional: return "?:";
        default: break;
    }
    if (op != Tok::Error) {
        switch (op) {
            case Tok::Plus: return "+";   case Tok::Minus: return "-";
            case Tok::Star: return "*";   case Tok::Slash: return "/";
            case Tok::Percent: return "%";
            case Tok::Lt: return "<";     case Tok::Gt: return ">";
            case Tok::Le: return "<=";    case Tok::Ge: return ">=";
            case Tok::Eq: return "==";    case Tok::Ne: return "!=";
            case Tok::AndAnd: return "&&"; case Tok::OrOr: return "||";
            case Tok::Amp: return "&";    case Tok::Pipe: return "|";
            case Tok::Caret: return "^";  case Tok::Tilde: return "~";
            case Tok::Bang: return "!";
            case Tok::LShift: return "<<"; case Tok::RShift: return ">>";
            case Tok::Assign: return "=";
            case Tok::PlusAssign: return "+=";
            case Tok::MinusAssign: return "-=";
            case Tok::StarAssign: return "*=";
            case Tok::SlashAssign: return "/=";
            case Tok::PercentAssign: return "%=";
            case Tok::LShiftAssign: return "<<=";
            case Tok::RShiftAssign: return ">>=";
            case Tok::AmpAssign: return "&=";
            case Tok::CaretAssign: return "^=";
            case Tok::PipeAssign: return "|=";
            case Tok::PlusPlus: return "++";
            case Tok::MinusMinus: return "--";
            default: break;
        }
    }
    return "";
}

const char* Stmt::kind_name() const {
    switch (kind) {
        case StmtKind::Compound: return "Compound";
        case StmtKind::ExprStmt: return "ExprStmt";
        case StmtKind::If:       return "If";
        case StmtKind::While:    return "While";
        case StmtKind::DoWhile:  return "DoWhile";
        case StmtKind::For:      return "For";
        case StmtKind::Switch:   return "Switch";
        case StmtKind::Case:     return "Case";
        case StmtKind::Default:  return "Default";
        case StmtKind::Break:    return "Break";
        case StmtKind::Continue: return "Continue";
        case StmtKind::Return:   return "Return";
        case StmtKind::DeclStmt: return "DeclStmt";
        case StmtKind::Empty:    return "Empty";
    }
    return "Stmt";
}

const char* Decl::kind_name() const {
    switch (kind) {
        case DeclKind::Variable:  return "VarDecl";
        case DeclKind::Function:  return "FuncDecl";
        case DeclKind::StructDef: return "StructDef";
    }
    return "Decl";
}

}  // namespace cviz
