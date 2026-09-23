#include "opt.h"
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include "cfg.h"

namespace cviz {

const char* fault_name(int f) {
    switch (f) {
        case FAULT_FOLD_SUB:    return "const-fold-subtract-swap";
        case FAULT_ALG_NEG:     return "algebraic-drop-negation";
        case FAULT_CSE_STALE:   return "cse-stale-reuse";
        case FAULT_DCE_CALLARG: return "dce-ignore-call-arguments";
        case FAULT_COPY_STALE:  return "copy-stale-source";
        default:                return "none";
    }
}

const char* fault_pass(int f) {
    switch (f) {
        case FAULT_FOLD_SUB:    return "const";
        case FAULT_ALG_NEG:     return "algebraic";
        case FAULT_CSE_STALE:   return "cse";
        case FAULT_DCE_CALLARG: return "dce";
        case FAULT_COPY_STALE:  return "copy";
        default:                return "none";
    }
}

// Every transformation goes through here. The attempt counter increases
// whether or not the rewrite is applied, so with a budget of N exactly the
// first N rewrites of the unrestricted run are applied, and nothing else.
template <class F>
bool Optimizer::rewrite(Function& f, Instr& in, const char* pass, const std::string& reason,
                        bool injected, F mutate) {
    ++attempts_;
    if (cfg_.budget >= 0 && attempts_ > cfg_.budget) return false;

    Rewrite r;
    r.id       = static_cast<int>(attempts_);
    r.pass     = pass;
    r.fn       = f.name;
    r.reason   = reason;
    r.before   = in.text();
    r.instr    = in.id;
    r.node     = in.node;
    r.span     = in.span;
    r.injected = injected;
    r.round    = round_;

    mutate(in);

    // The evaluation perturbs one constant produced by one rewrite, to ask
    // whether bisection then points back at exactly that rewrite.
    Operand* k = nullptr;
    if (in.op != Op::Nop) {
        if (in.a.is_const()) k = &in.a;
        else if (in.b.is_const()) k = &in.b;
        else for (auto& a : in.args) if (a.is_const()) { k = &a; break; }
    }
    r.corruptible = k != nullptr;
    if (k && cfg_.corrupt_at == r.id) {
        k->c += 1;
        r.injected = true;
    }
    r.after = in.op == Op::Nop ? "(removed)" : in.text();
    log_.push_back(r);
    return true;
}

static void collect_uses(const Instr& in, std::vector<std::string>& out, bool call_args) {
    auto add = [&](const Operand& o) { if (o.is_var()) out.push_back(o.name); };
    switch (in.op) {
        case Op::Copy: case Op::Un: case Op::Load: case Op::Br: case Op::Ret:
            add(in.a); break;
        case Op::Bin: case Op::Store:
            add(in.a); add(in.b); break;
        case Op::Call:
            if (call_args) for (const auto& a : in.args) add(a);
            break;
        default: break;
    }
}

// Constant propagation and folding, local to each basic block. Scalars in
// the IR have no address, so only an assignment or (for globals) a call can
// change them, and both are tracked here.
bool Optimizer::const_prop(Function& f) {
    bool changed = false;
    CFG g = build_cfg(f);
    for (const Block& b : g.blocks) {
        std::unordered_map<std::string, long long> known;
        for (int i = b.first; i <= b.last; ++i) {
            Instr& in = f.code[i];
            if (in.op == Op::Nop || in.op == Op::Label) continue;

            auto propagate = [&](Operand& o) {
                if (!o.is_var()) return;
                auto it = known.find(o.name);
                if (it == known.end()) return;
                std::string name = o.name;
                long long v = it->second;
                Operand* slot = &o;
                if (rewrite(f, in, "const",
                            name + " is known to hold " + std::to_string(v) + " at this point",
                            false, [&](Instr&) { *slot = Operand::constant(v); }))
                    changed = true;
            };
            propagate(in.a);
            propagate(in.b);
            for (auto& a : in.args) propagate(a);

            if (in.op == Op::Bin && in.a.is_const() && in.b.is_const()) {
                long long x = in.a.c, y = in.b.c, r = 0;
                bool swap = cfg_.fault == FAULT_FOLD_SUB && in.bop == Tok::Minus;
                bool ok = swap ? eval_binop(Tok::Minus, y, x, in.w32, r)
                               : eval_binop(in.bop, x, y, in.w32, r);
                if (ok) {
                    std::string why = std::to_string(x) + " " + op_symbol(in.bop) + " " +
                                      std::to_string(y) + " evaluates to " + std::to_string(r);
                    if (rewrite(f, in, "const", why, swap && x != y, [&](Instr& I) {
                            I.op = Op::Copy; I.a = Operand::constant(r); I.b = Operand();
                        }))
                        changed = true;
                }
            } else if (in.op == Op::Un && in.a.is_const()) {
                long long r = 0;
                if (eval_unop(in.bop, in.a.c, in.w32, r)) {
                    std::string why = std::string(op_symbol(in.bop)) + std::to_string(in.a.c) +
                                      " evaluates to " + std::to_string(r);
                    if (rewrite(f, in, "const", why, false, [&](Instr& I) {
                            I.op = Op::Copy; I.a = Operand::constant(r);
                        }))
                        changed = true;
                }
            } else if (in.op == Op::Br && in.a.is_const()) {
                std::string dest = in.a.c ? in.target : in.target2;
                std::string why = std::string("the condition is always ") +
                                  (in.a.c ? "true" : "false") + ", so control always reaches " + dest;
                if (rewrite(f, in, "const", why, false, [&](Instr& I) {
                        I.op = Op::Jmp; I.target = dest; I.target2.clear(); I.a = Operand();
                    }))
                    changed = true;
            }

            if (in.op == Op::Call) {
                for (auto it = known.begin(); it != known.end();)
                    it = is_global_name(it->first) ? known.erase(it) : std::next(it);
            }
            if (in.defines()) {
                if (in.op == Op::Copy && in.a.is_const()) known[in.dst] = in.a.c;
                else known.erase(in.dst);
            }
        }
    }
    return changed;
}

// Local copy propagation: after d = s, later uses of d in the same block
// read s directly, until either d or s is reassigned. This exposes further
// constant folding, CSE and dead copies.
bool Optimizer::copy_prop(Function& f) {
    bool changed = false;
    CFG g = build_cfg(f);
    for (const Block& b : g.blocks) {
        std::unordered_map<std::string, std::string> copy;
        std::unordered_map<std::string, bool> stale;
        for (int i = b.first; i <= b.last; ++i) {
            Instr& in = f.code[i];
            if (in.op == Op::Nop || in.op == Op::Label) continue;

            auto propagate = [&](Operand& o) {
                if (!o.is_var()) return;
                auto it = copy.find(o.name);
                if (it == copy.end()) return;
                std::string from = o.name, to = it->second;
                Operand* slot = &o;
                if (rewrite(f, in, "copy", from + " holds a copy of " + to + " at this point",
                            stale[from], [&](Instr&) { *slot = Operand::var(to); }))
                    changed = true;
            };
            propagate(in.a);
            propagate(in.b);
            for (auto& a : in.args) propagate(a);

            if (in.op == Op::Call) {
                for (auto it = copy.begin(); it != copy.end();) {
                    if (is_global_name(it->first) || is_global_name(it->second)) {
                        stale.erase(it->first);
                        it = copy.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            if (in.defines()) {
                const std::string d = in.dst;
                for (auto it = copy.begin(); it != copy.end();) {
                    if (it->first == d) { stale.erase(it->first); it = copy.erase(it); continue; }
                    if (it->second == d) {
                        if (cfg_.fault == FAULT_COPY_STALE) { stale[it->first] = true; ++it; }
                        else { stale.erase(it->first); it = copy.erase(it); }
                        continue;
                    }
                    ++it;
                }
            }
            if (in.op == Op::Copy && in.a.is_var() && in.a.name != in.dst) {
                copy[in.dst] = in.a.name;
                stale.erase(in.dst);
            }
        }
    }
    return changed;
}

bool Optimizer::algebraic(Function& f) {
    bool changed = false;
    for (Instr& in : f.code) {
        if (in.op != Op::Bin) continue;
        bool ac = in.a.is_const(), bc = in.b.is_const();
        if (ac && bc) continue;
        std::string A = in.a.text(), B = in.b.text();

        auto to = [&](Operand v, const std::string& why, bool inj) {
            if (rewrite(f, in, "algebraic", why, inj, [&](Instr& I) {
                    I.op = Op::Copy; I.a = v; I.b = Operand();
                }))
                changed = true;
        };

        switch (in.bop) {
            case Tok::Plus:
                if (ac && in.a.c == 0)      to(in.b, "0 + " + B + " is " + B, false);
                else if (bc && in.b.c == 0) to(in.a, A + " + 0 is " + A, false);
                break;
            case Tok::Minus:
                if (bc && in.b.c == 0)
                    to(in.a, A + " - 0 is " + A, false);
                else if (in.a.is_var() && in.b.is_var() && in.a.name == in.b.name)
                    to(Operand::constant(0), A + " - " + A + " is 0", false);
                else if (cfg_.fault == FAULT_ALG_NEG && ac && in.a.c == 0)
                    to(in.b, "0 - " + B + " is " + B, true);
                break;
            case Tok::Star:
                if (ac && in.a.c == 1)      to(in.b, "1 * " + B + " is " + B, false);
                else if (bc && in.b.c == 1) to(in.a, A + " * 1 is " + A, false);
                else if ((ac && in.a.c == 0) || (bc && in.b.c == 0))
                    to(Operand::constant(0), "multiplying by 0 gives 0", false);
                break;
            case Tok::Slash:
                if (bc && in.b.c == 1) to(in.a, A + " / 1 is " + A, false);
                break;
            case Tok::Amp:
                if ((ac && in.a.c == 0) || (bc && in.b.c == 0))
                    to(Operand::constant(0), "a bitwise and with 0 gives 0", false);
                break;
            case Tok::Pipe:
            case Tok::Caret:
                if (ac && in.a.c == 0)      to(in.b, "0 " + std::string(op_symbol(in.bop)) + " " + B + " is " + B, false);
                else if (bc && in.b.c == 0) to(in.a, A + " " + op_symbol(in.bop) + " 0 is " + A, false);
                break;
            case Tok::LShift:
            case Tok::RShift:
                if (bc && in.b.c == 0) to(in.a, "shifting by 0 leaves " + A + " unchanged", false);
                break;
            default:
                break;
        }
    }
    return changed;
}

static bool commutative(Tok t) {
    switch (t) {
        case Tok::Plus: case Tok::Star: case Tok::Amp: case Tok::Pipe:
        case Tok::Caret: case Tok::Eq: case Tok::Ne:
            return true;
        default:
            return false;
    }
}

static std::string cse_key(const Instr& in) {
    std::string x = in.a.text(), y = in.b.text();
    if (commutative(in.bop) && y < x) std::swap(x, y);
    return std::string(op_symbol(in.bop)) + (in.w32 ? "|w|" : "|l|") + x + "|" + y;
}

// Local common-subexpression elimination. An expression stays available
// until one of its operands, or the variable holding it, is reassigned.
bool Optimizer::cse(Function& f) {
    bool changed = false;
    CFG g = build_cfg(f);
    struct Avail { std::string holder; Operand a, b; bool stale = false; };

    for (const Block& blk : g.blocks) {
        std::map<std::string, Avail> avail;
        for (int i = blk.first; i <= blk.last; ++i) {
            Instr& in = f.code[i];
            if (in.op == Op::Nop || in.op == Op::Label) continue;

            if (in.op == Op::Bin) {
                auto it = avail.find(cse_key(in));
                if (it != avail.end() && it->second.holder != in.dst) {
                    std::string h = it->second.holder;
                    std::string expr = in.a.text() + " " + op_symbol(in.bop) + " " + in.b.text();
                    if (rewrite(f, in, "cse", "the value of " + expr + " is already held in " + h,
                                it->second.stale, [&](Instr& I) {
                                    I.op = Op::Copy; I.a = Operand::var(h); I.b = Operand();
                                }))
                        changed = true;
                }
            }

            if (in.defines()) {
                const std::string d = in.dst;
                for (auto it = avail.begin(); it != avail.end();) {
                    const Avail& av = it->second;
                    bool uses_d = (av.a.is_var() && av.a.name == d) || (av.b.is_var() && av.b.name == d);
                    if (av.holder == d) { it = avail.erase(it); continue; }
                    if (uses_d) {
                        if (cfg_.fault == FAULT_CSE_STALE) { it->second.stale = true; ++it; }
                        else it = avail.erase(it);
                        continue;
                    }
                    ++it;
                }
            }
            if (in.op == Op::Call) {
                for (auto it = avail.begin(); it != avail.end();) {
                    const Avail& av = it->second;
                    bool g1 = is_global_name(av.holder) ||
                              (av.a.is_var() && is_global_name(av.a.name)) ||
                              (av.b.is_var() && is_global_name(av.b.name));
                    it = g1 ? avail.erase(it) : std::next(it);
                }
            }
            if (in.op == Op::Bin && !in.dst.empty()) {
                bool self = (in.a.is_var() && in.a.name == in.dst) ||
                            (in.b.is_var() && in.b.name == in.dst);
                std::string key = cse_key(in);
                if (!self && !avail.count(key)) avail[key] = Avail{in.dst, in.a, in.b, false};
            }
        }
    }
    return changed;
}

static bool has_side_effect(const Instr& in) {
    switch (in.op) {
        case Op::Call: case Op::Store: case Op::Load:
            return true;                         // a load may fault
        case Op::Bin:
            if (in.bop == Tok::Slash || in.bop == Tok::Percent)
                return !(in.b.is_const() && in.b.c != 0 && in.b.c != -1);
            return false;
        default:
            return false;
    }
}

static bool used_as_call_arg(const Function& f, const std::string& v) {
    for (const auto& in : f.code) {
        if (in.op != Op::Call) continue;
        for (const auto& a : in.args) if (a.is_var() && a.name == v) return true;
    }
    return false;
}

// Dead-code elimination: first unreachable blocks, then assignments whose
// result is never read, using liveness computed over the whole CFG.
bool Optimizer::dce(Function& f) {
    bool changed = false;
    CFG g = build_cfg(f);
    bool call_args = cfg_.fault != FAULT_DCE_CALLARG;

    std::vector<bool> reach = reachable(g);
    bool removed_block = false;
    for (size_t k = 0; k < g.blocks.size(); ++k) {
        if (reach[k]) continue;
        for (int i = g.blocks[k].first; i <= g.blocks[k].last; ++i) {
            Instr& in = f.code[i];
            if (in.op == Op::Nop) continue;
            if (rewrite(f, in, "dce", "block " + g.blocks[k].name + " can never be reached", false,
                        [](Instr& I) { I.op = Op::Nop; I.dst.clear(); })) {
                changed = true;
                removed_block = true;
            }
        }
    }
    if (removed_block) return changed;   // the CFG changed; liveness runs next round

    size_t nb = g.blocks.size();
    std::vector<std::set<std::string>> use(nb), def(nb), in_live(nb), out_live(nb);
    for (size_t k = 0; k < nb; ++k) {
        for (int i = g.blocks[k].first; i <= g.blocks[k].last; ++i) {
            const Instr& in = f.code[i];
            std::vector<std::string> u;
            collect_uses(in, u, call_args);
            for (auto& x : u) if (!def[k].count(x)) use[k].insert(x);
            if (in.defines()) def[k].insert(in.dst);
        }
    }
    for (bool again = true; again;) {
        again = false;
        for (size_t k = nb; k-- > 0;) {
            std::set<std::string> out;
            for (int s : g.blocks[k].succ) out.insert(in_live[s].begin(), in_live[s].end());
            std::set<std::string> in = use[k];
            for (auto& x : out) if (!def[k].count(x)) in.insert(x);
            if (in != in_live[k] || out != out_live[k]) {
                in_live[k] = std::move(in);
                out_live[k] = std::move(out);
                again = true;
            }
        }
    }

    for (size_t k = 0; k < nb; ++k) {
        std::set<std::string> live = out_live[k];
        for (int i = g.blocks[k].last; i >= g.blocks[k].first; --i) {
            Instr& in = f.code[i];
            if (in.op == Op::Nop || in.op == Op::Label) continue;
            if (in.defines() && !has_side_effect(in) && !is_global_name(in.dst) && !live.count(in.dst)) {
                std::string d = in.dst;
                bool inj = !call_args && used_as_call_arg(f, d);
                if (rewrite(f, in, "dce", "the value assigned to " + d + " is never used afterwards",
                            inj, [](Instr& I) { I.op = Op::Nop; I.dst.clear(); })) {
                    changed = true;
                    continue;
                }
            }
            if (in.defines()) live.erase(in.dst);
            std::vector<std::string> u;
            collect_uses(in, u, call_args);
            live.insert(u.begin(), u.end());
        }
    }
    return changed;
}

void Optimizer::run(Program& p) {
    for (round_ = 1; round_ <= 8; ++round_) {
        bool any = false;
        for (auto& f : p.functions) {
            any |= const_prop(f);
            if (!cfg_.skip_copy) any |= copy_prop(f);
            any |= algebraic(f);
            any |= cse(f);
            any |= dce(f);
        }
        if (!any) break;
    }
}

}  // namespace cviz
