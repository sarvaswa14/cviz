#include "gen.h"
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace cviz {

namespace {

class Gen {
public:
    explicit Gen(unsigned seed) : rng_(seed) {}
    std::string build();

private:
    std::mt19937 rng_;
    std::ostringstream o_;
    std::vector<std::string> scalars_;   // readable and assignable
    std::vector<std::string> readonly_;  // loop counters: readable only
    std::vector<std::string> arrays_;
    int  funcs_ = 0;
    int  cur_ = 0;
    std::vector<int> arity_;
    int  loop_depth_ = 0;
    int  calls_left_ = 0;

    int  below(int n) { return n <= 0 ? 0 : static_cast<int>(rng_() % static_cast<unsigned>(n)); }
    bool chance(int pct) { return below(100) < pct; }
    std::string pick(const std::vector<std::string>& v) { return v[below(static_cast<int>(v.size()))]; }
    std::string ind(int n) { return std::string(static_cast<size_t>(4 * n), ' '); }

    std::string leaf();
    std::string expr(int depth);
    void stmt(int depth, int tab);
    void block(int n, int depth, int tab);
    void function(int index, int arity, bool is_main);
};

std::string Gen::leaf() {
    int r = below(100);
    std::vector<std::string> all = scalars_;
    all.insert(all.end(), readonly_.begin(), readonly_.end());
    if (r < 45 && !all.empty()) return pick(all);
    if (r < 60 && !arrays_.empty()) {
        std::string idx = all.empty() ? std::to_string(below(16)) : pick(all);
        return pick(arrays_) + "[(" + idx + ") & 15]";
    }
    if (r < 70) return std::to_string(below(2));          // 0 and 1 feed algebraic identities
    return std::to_string(below(100));
}

std::string Gen::expr(int depth) {
    if (depth <= 0 || chance(25)) return leaf();
    static const char* arith[] = {" + ", " - ", " & ", " | ", " ^ "};
    static const char* cmp[]   = {" < ", " > ", " <= ", " >= ", " == ", " != "};
    switch (below(10)) {
        case 0: return "(" + expr(depth - 1) + arith[below(5)] + expr(depth - 1) + ")";
        case 1: return "(" + leaf() + " * " + leaf() + ")";          // products stay small
        case 2: return "(" + expr(depth - 1) + (chance(50) ? " / " : " % ") +
                       std::to_string(1 + below(13)) + ")";          // non-zero constant divisor
        case 3: return "(" + expr(depth - 1) + cmp[below(6)] + expr(depth - 1) + ")";
        case 4: {
            const char* u[] = {"-", "~", "!"};
            return "(" + std::string(u[below(3)]) + "(" + expr(depth - 1) + "))";
        }
        case 5: return chance(50)
                    ? "((((" + expr(depth - 1) + ") & 1023) << " + std::to_string(below(4)) + "))"
                    : "((" + expr(depth - 1) + ") >> " + std::to_string(below(4)) + ")";
        case 6: {                                                    // algebraic identities
            std::string e = expr(depth - 1);
            switch (below(4)) {
                case 0: return "((" + e + ") * 1)";
                case 1: return "((" + e + ") + 0)";
                case 2: return "(0 - (" + e + "))";
                default: { std::string l = leaf(); return "((" + l + ") - (" + l + "))"; }
            }
        }
        case 7: return "(" + std::to_string(below(50)) + (chance(50) ? " + " : " * ") +
                       std::to_string(below(50)) + ")";              // constant subexpression
        case 8: {                                                    // deliberate redundancy
            std::string a = leaf(), b = leaf();
            return "((" + a + " + " + b + ") + (" + a + " + " + b + "))";
        }
        default: return "((" + expr(depth - 1) + ") ? (" + expr(depth - 1) + ") : (" +
                        expr(depth - 1) + "))";
    }
}

void Gen::stmt(int depth, int tab) {
    int r = below(100);
    if (r < 30 && !scalars_.empty()) {
        o_ << ind(tab) << pick(scalars_) << " = (" << expr(depth) << ") % 997;\n";
    } else if (r < 40 && !arrays_.empty()) {
        o_ << ind(tab) << pick(arrays_) << "[(" << leaf() << ") & 15] = (" << expr(depth) << ") % 997;\n";
    } else if (r < 50 && !scalars_.empty()) {
        // a store that is immediately overwritten, so the first is dead
        std::string v = pick(scalars_);
        o_ << ind(tab) << v << " = (" << expr(depth) << ") % 997;\n";
        o_ << ind(tab) << v << " = (" << expr(depth) << ") % 997;\n";
    } else if (r < 62 && loop_depth_ < 2) {
        std::string i = "i" + std::to_string(loop_depth_);
        int k = 1 + below(5);
        o_ << ind(tab) << "for (" << i << " = 0; " << i << " < " << k << "; " << i << "++) {\n";
        ++loop_depth_;
        block(1 + below(2), depth, tab + 1);
        --loop_depth_;
        o_ << ind(tab) << "}\n";
    } else if (r < 74) {
        bool constant_cond = chance(40);
        std::string c = constant_cond
            ? "(" + std::to_string(below(40)) + " < " + std::to_string(below(40)) + ")"
            : "(" + expr(depth) + ")";
        o_ << ind(tab) << "if " << c << " {\n";
        block(1 + below(2), depth, tab + 1);
        o_ << ind(tab) << "} else {\n";
        block(1 + below(2), depth, tab + 1);
        o_ << ind(tab) << "}\n";
    } else if (r < 84 && cur_ > 0 && calls_left_ > 0 && loop_depth_ == 0 && !scalars_.empty()) {
        int callee = below(cur_);
        --calls_left_;
        o_ << ind(tab) << pick(scalars_) << " = f" << callee << "(";
        for (int i = 0; i < arity_[static_cast<size_t>(callee)]; ++i)
            o_ << (i ? ", " : "") << "(" << expr(1) << ") % 97";
        o_ << ") % 997;\n";
    } else if (r < 92 && !scalars_.empty()) {
        o_ << ind(tab) << "g" << below(2) << " = (g" << below(2) << " + (" << expr(depth)
           << ")) % 997;\n";
    } else if (!scalars_.empty()) {
        o_ << ind(tab) << "printf(\"%d\\n\", " << pick(scalars_) << ");\n";
    } else {
        o_ << ind(tab) << ";\n";
    }
}

void Gen::block(int n, int depth, int tab) {
    for (int i = 0; i < n; ++i) stmt(depth, tab);
}

void Gen::function(int index, int arity, bool is_main) {
    cur_ = index;
    calls_left_ = 2;
    scalars_.clear();
    readonly_.clear();
    arrays_.clear();

    if (is_main) o_ << "int main(void) {\n";
    else {
        o_ << "int f" << index << "(";
        for (int i = 0; i < arity; ++i) o_ << (i ? ", " : "") << "int p" << i;
        if (arity == 0) o_ << "void";
        o_ << ") {\n";
    }
    for (int i = 0; i < arity; ++i) scalars_.push_back("p" + std::to_string(i));

    int nv = 3 + below(3);
    o_ << "    int";
    for (int i = 0; i < nv; ++i) {
        o_ << (i ? ", " : " ") << "v" << i << " = " << below(100);
        scalars_.push_back("v" + std::to_string(i));
    }
    o_ << ";\n";
    o_ << "    int i0 = 0, i1 = 0;\n";
    readonly_.push_back("i0");
    readonly_.push_back("i1");

    bool local_array = chance(60);
    if (local_array) {
        o_ << "    int la[16];\n";
        o_ << "    for (i0 = 0; i0 < 16; i0++) la[i0] = (i0 * " << (1 + below(7)) << ") % 97;\n";
        arrays_.push_back("la");
    }
    arrays_.push_back("ga");

    block(3 + below(4), 2 + below(2), 1);

    if (is_main) {
        o_ << "    printf(\"";
        for (size_t i = 0; i < scalars_.size(); ++i) o_ << "%d ";
        o_ << "%d %d\\n\"";
        for (const auto& v : scalars_) o_ << ", " << v;
        o_ << ", g0, g1);\n";
        o_ << "    printf(\"%d %d %d\\n\", ga[0], ga[7], ga[15]);\n";
        o_ << "    return 0;\n}\n";
    } else {
        o_ << "    return (" << expr(2) << ") % 997;\n}\n";
    }
}

std::string Gen::build() {
    o_ << "/* generated program, deterministic and terminating by construction */\n";
    o_ << "int g0 = " << below(50) << ";\n";
    o_ << "int g1 = " << below(50) << ";\n";
    o_ << "int ga[16];\n\n";

    funcs_ = 1 + below(3);
    for (int i = 0; i < funcs_; ++i) arity_.push_back(below(3));
    for (int i = 0; i < funcs_; ++i) {
        function(i, arity_[static_cast<size_t>(i)], false);
        o_ << "\n";
    }
    arity_.push_back(0);
    cur_ = funcs_;
    function(funcs_, 0, true);
    return o_.str();
}

}  // namespace

std::string generate_program(unsigned seed) {
    return Gen(seed).build();
}

}  // namespace cviz
