#include "interp.h"
#include <cstdio>

namespace cviz {

struct RuntimeError { std::string msg; };

long long& Interpreter::cell(long long addr) {
    if (addr < 0 || addr >= static_cast<long long>(mem_.size()))
        throw RuntimeError{"out-of-bounds memory access at cell " + std::to_string(addr)};
    return mem_[static_cast<size_t>(addr)];
}

long long Interpreter::value(const Operand& o, Frame& fr) {
    switch (o.kind) {
        case Operand::K::Const: return o.c;
        case Operand::K::None:  return 0;
        case Operand::K::Str:   throw RuntimeError{"string literal used as a value"};
        case Operand::K::Var: {
            if (is_global_name(o.name)) {
                auto it = gvars_.find(o.name);
                return it == gvars_.end() ? 0 : it->second;
            }
            auto it = fr.vars.find(o.name);
            return it == fr.vars.end() ? 0 : it->second;   // uninitialised reads as 0
        }
    }
    return 0;
}

void Interpreter::assign(const std::string& name, long long v, Frame& fr) {
    if (is_global_name(name)) gvars_[name] = v;
    else fr.vars[name] = v;
}

size_t Interpreter::target(const Function& f, const std::string& label) {
    auto& m = labels_[&f];
    if (m.empty()) {
        for (size_t i = 0; i < f.code.size(); ++i)
            if (f.code[i].op == Op::Label) m[f.code[i].target] = i;
    }
    auto it = m.find(label);
    if (it == m.end()) throw RuntimeError{"jump to unknown label " + label};
    return it->second;
}

// printf supports the conversions a C test program realistically uses:
// %d %i %u %x %X %c %s %ld %lld %%, with flags and widths.
long long Interpreter::builtin(const std::string& name, const std::vector<Operand>& args, Frame& fr) {
    if (name == "putchar") {
        long long c = args.empty() ? 0 : value(args[0], fr);
        out_ += static_cast<char>(c);
        return c & 0xFF;
    }
    if (args.empty() || !args[0].is_str()) throw RuntimeError{"printf needs a string literal format"};
    const std::string& fmt = args[0].name;
    size_t next = 1, start = out_.size();
    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%') { out_ += fmt[i]; continue; }
        if (i + 1 < fmt.size() && fmt[i + 1] == '%') { out_ += '%'; ++i; continue; }
        std::string spec = "%";
        size_t j = i + 1;
        while (j < fmt.size() && std::string("-+ #0123456789.").find(fmt[j]) != std::string::npos)
            spec += fmt[j++];
        while (j < fmt.size() && (fmt[j] == 'l' || fmt[j] == 'h')) ++j;
        if (j >= fmt.size()) break;
        char conv = fmt[j];
        i = j;
        char buf[128];
        if (conv == 's') {
            if (next >= args.size() || !args[next].is_str()) throw RuntimeError{"%s needs a string literal"};
            std::snprintf(buf, sizeof buf, (spec + "s").c_str(), args[next++].name.c_str());
        } else {
            long long v = next < args.size() ? value(args[next++], fr) : 0;
            switch (conv) {
                case 'd': case 'i': std::snprintf(buf, sizeof buf, (spec + "lld").c_str(), v); break;
                case 'u': std::snprintf(buf, sizeof buf, (spec + "u").c_str(), static_cast<unsigned>(v)); break;
                case 'x': std::snprintf(buf, sizeof buf, (spec + "x").c_str(), static_cast<unsigned>(v)); break;
                case 'X': std::snprintf(buf, sizeof buf, (spec + "X").c_str(), static_cast<unsigned>(v)); break;
                case 'c': std::snprintf(buf, sizeof buf, (spec + "c").c_str(), static_cast<int>(v)); break;
                default:  throw RuntimeError{std::string("unsupported printf conversion %") + conv};
            }
        }
        out_ += buf;
    }
    if (out_.size() > 4000000) throw RuntimeError{"output limit exceeded"};
    return static_cast<long long>(out_.size() - start);
}

long long Interpreter::call(const Function& f, const std::vector<long long>& args) {
    if (++depth_ > 2500) throw RuntimeError{"call depth limit exceeded"};
    Frame fr;
    size_t mark = mem_.size();
    for (const auto& a : f.arrays) {
        fr.arrays[a.first] = static_cast<long long>(mem_.size());
        mem_.resize(mem_.size() + static_cast<size_t>(a.second), 0);
    }
    for (size_t i = 0; i < f.params.size(); ++i)
        fr.vars[f.params[i]] = i < args.size() ? args[i] : 0;

    const auto& code = f.code;
    size_t pc = 0;
    long long result = 0;
    while (pc < code.size()) {
        const Instr& in = code[pc];
        if (++steps_ > limit_) throw RuntimeError{"step limit exceeded"};
        switch (in.op) {
            case Op::Nop:
            case Op::Label:
                ++pc;
                break;
            case Op::Copy:
                assign(in.dst, value(in.a, fr), fr);
                ++pc;
                break;
            case Op::Bin: {
                long long r;
                if (!eval_binop(in.bop, value(in.a, fr), value(in.b, fr), in.w32, r))
                    throw RuntimeError{"arithmetic fault (division by zero)"};
                assign(in.dst, r, fr);
                ++pc;
                break;
            }
            case Op::Un: {
                long long r;
                if (!eval_unop(in.bop, value(in.a, fr), in.w32, r))
                    throw RuntimeError{"invalid unary operation"};
                assign(in.dst, r, fr);
                ++pc;
                break;
            }
            case Op::AddrOf: {
                long long base;
                auto it = fr.arrays.find(in.sym);
                if (it != fr.arrays.end()) base = it->second;
                else {
                    auto g = garrays_.find(in.sym);
                    if (g == garrays_.end()) throw RuntimeError{"unknown array " + in.sym};
                    base = g->second;
                }
                assign(in.dst, base, fr);
                ++pc;
                break;
            }
            case Op::Load: {
                long long v = cell(value(in.a, fr));
                assign(in.dst, v, fr);
                ++pc;
                break;
            }
            case Op::Store: {
                long long v = value(in.b, fr);
                cell(value(in.a, fr)) = v;
                ++pc;
                break;
            }
            case Op::Jmp:
                pc = target(f, in.target);
                break;
            case Op::Br:
                pc = target(f, value(in.a, fr) ? in.target : in.target2);
                break;
            case Op::Call: {
                long long r;
                if (in.callee == "printf" || in.callee == "putchar") {
                    r = builtin(in.callee, in.args, fr);
                } else {
                    const Function* g = p_.find(in.callee);
                    if (!g) throw RuntimeError{"call to undefined function " + in.callee};
                    std::vector<long long> av;
                    for (const auto& a : in.args) av.push_back(value(a, fr));
                    r = call(*g, av);
                }
                if (!in.dst.empty()) assign(in.dst, r, fr);
                ++pc;
                break;
            }
            case Op::Ret:
                result = in.a.is_none() ? 0 : value(in.a, fr);
                pc = code.size();
                break;
        }
    }
    mem_.resize(mark);
    --depth_;
    return result;
}

Outcome Interpreter::run() {
    Outcome o;
    try {
        for (const auto& g : p_.globals) {
            if (g.array) {
                garrays_[g.name] = static_cast<long long>(mem_.size());
                mem_.resize(mem_.size() + static_cast<size_t>(g.cells), 0);
            } else {
                gvars_[g.name] = g.init;
            }
        }
        const Function* m = p_.find("main");
        if (!m) throw RuntimeError{"program has no main function"};
        o.ret = call(*m, {});
    } catch (const RuntimeError& e) {
        o.error = e.msg;
    }
    o.output = out_;
    o.steps = steps_;
    return o;
}

}  // namespace cviz
