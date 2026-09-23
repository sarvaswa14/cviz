#include "ir.h"
#include <climits>
#include <cstdint>
#include <sstream>

namespace cviz {

bool is_global_name(const std::string& n) {
    return !n.empty() && n[0] != '%' && n.find('.') == std::string::npos;
}

const char* op_symbol(Tok t) {
    switch (t) {
        case Tok::Plus: return "+";     case Tok::Minus: return "-";
        case Tok::Star: return "*";     case Tok::Slash: return "/";
        case Tok::Percent: return "%";
        case Tok::Lt: return "<";       case Tok::Gt: return ">";
        case Tok::Le: return "<=";      case Tok::Ge: return ">=";
        case Tok::Eq: return "==";      case Tok::Ne: return "!=";
        case Tok::Amp: return "&";      case Tok::Pipe: return "|";
        case Tok::Caret: return "^";    case Tok::Tilde: return "~";
        case Tok::Bang: return "!";
        case Tok::LShift: return "<<";  case Tok::RShift: return ">>";
        default: return "?";
    }
}

static std::string escape_str(const std::string& s) {
    std::string o = "\"";
    for (char c : s) {
        switch (c) {
            case '\n': o += "\\n"; break;
            case '\t': o += "\\t"; break;
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            default:   o += c;
        }
    }
    return o + "\"";
}

std::string Operand::text() const {
    switch (kind) {
        case K::None:  return "";
        case K::Const: return std::to_string(c);
        case K::Var:   return name;
        case K::Str:   return escape_str(name);
    }
    return "";
}

bool Instr::defines() const {
    switch (op) {
        case Op::Copy: case Op::Bin: case Op::Un:
        case Op::AddrOf: case Op::Load: case Op::Call:
            return !dst.empty();
        default:
            return false;
    }
}

std::string Instr::text() const {
    switch (op) {
        case Op::Copy:   return dst + " = " + a.text();
        case Op::Bin:    return dst + " = " + a.text() + " " + op_symbol(bop) + " " + b.text();
        case Op::Un:     return dst + " = " + op_symbol(bop) + a.text();
        case Op::AddrOf: return dst + " = &" + sym;
        case Op::Load:   return dst + " = *" + a.text();
        case Op::Store:  return "*" + a.text() + " = " + b.text();
        case Op::Label:  return target + ":";
        case Op::Jmp:    return "goto " + target;
        case Op::Br:     return "if " + a.text() + " goto " + target + " else " + target2;
        case Op::Call: {
            std::string s = dst.empty() ? "" : dst + " = ";
            s += "call " + callee + "(";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) s += ", ";
                s += args[i].text();
            }
            return s + ")";
        }
        case Op::Ret: return a.is_none() ? "ret" : "ret " + a.text();
        case Op::Nop: return "nop";
    }
    return "?";
}

size_t Function::live_count() const {
    size_t n = 0;
    for (const auto& in : code)
        if (in.op != Op::Nop && in.op != Op::Label) ++n;
    return n;
}

const Function* Program::find(const std::string& name) const {
    for (const auto& f : functions)
        if (f.name == name) return &f;
    return nullptr;
}

size_t Program::live_count() const {
    size_t n = 0;
    for (const auto& f : functions) n += f.live_count();
    return n;
}

std::string Program::dump() const {
    std::ostringstream o;
    for (const auto& g : globals) {
        if (g.array) o << "global " << g.name << "[" << g.cells << "]\n";
        else         o << "global " << g.name << " = " << g.init << "\n";
    }
    if (!globals.empty()) o << "\n";
    for (const auto& f : functions) {
        o << "func " << f.name << "(";
        for (size_t i = 0; i < f.params.size(); ++i) {
            if (i) o << ", ";
            o << f.params[i];
        }
        o << ")\n";
        for (const auto& a : f.arrays) o << "  array " << a.first << "[" << a.second << "]\n";
        for (const auto& in : f.code) {
            if (in.op == Op::Nop) continue;
            if (in.op == Op::Label) o << in.text() << "\n";
            else                    o << "  " << in.text() << "\n";
        }
        o << "\n";
    }
    return o.str();
}

static long long wrap32(long long v) {
    return static_cast<long long>(static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(v))));
}

bool eval_binop(Tok op, long long a, long long b, bool w32, long long& out) {
    auto fit = [&](long long v) { return w32 ? wrap32(v) : v; };
    uint64_t ua = static_cast<uint64_t>(a), ub = static_cast<uint64_t>(b);
    switch (op) {
        case Tok::Plus:  out = fit(static_cast<long long>(ua + ub)); return true;
        case Tok::Minus: out = fit(static_cast<long long>(ua - ub)); return true;
        case Tok::Star:  out = fit(static_cast<long long>(ua * ub)); return true;
        case Tok::Slash:
        case Tok::Percent: {
            if (b == 0) return false;
            if (w32 ? (a == INT32_MIN && b == -1) : (a == LLONG_MIN && b == -1)) return false;
            out = fit(op == Tok::Slash ? a / b : a % b);   // C truncates toward zero
            return true;
        }
        case Tok::Amp:   out = fit(a & b); return true;
        case Tok::Pipe:  out = fit(a | b); return true;
        case Tok::Caret: out = fit(a ^ b); return true;
        case Tok::LShift:
            out = w32 ? wrap32(static_cast<long long>(static_cast<uint32_t>(a) << (b & 31)))
                      : static_cast<long long>(ua << (b & 63));
            return true;
        case Tok::RShift:
            out = w32 ? static_cast<long long>(static_cast<int32_t>(a) >> (b & 31))
                      : a >> (b & 63);
            return true;
        case Tok::Lt: out = a <  b; return true;
        case Tok::Gt: out = a >  b; return true;
        case Tok::Le: out = a <= b; return true;
        case Tok::Ge: out = a >= b; return true;
        case Tok::Eq: out = a == b; return true;
        case Tok::Ne: out = a != b; return true;
        default: return false;
    }
}

bool eval_unop(Tok op, long long a, bool w32, long long& out) {
    auto fit = [&](long long v) { return w32 ? wrap32(v) : v; };
    switch (op) {
        case Tok::Minus: out = fit(static_cast<long long>(0 - static_cast<uint64_t>(a))); return true;
        case Tok::Tilde: out = fit(~a); return true;
        case Tok::Bang:  out = !a; return true;
        case Tok::Plus:  out = a; return true;
        default: return false;
    }
}

}  // namespace cviz
