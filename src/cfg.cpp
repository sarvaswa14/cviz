#include "cfg.h"
#include <unordered_map>

namespace cviz {

// A leader is the first instruction, any label, or any instruction that
// follows a jump, branch or return. Removed instructions are skipped.
CFG build_cfg(const Function& f) {
    CFG g;
    int n = static_cast<int>(f.code.size());
    g.block_of.assign(n, -1);

    std::vector<int> leaders;
    bool start = true;
    for (int i = 0; i < n; ++i) {
        const Instr& in = f.code[i];
        if (in.op == Op::Nop) continue;
        if (start || in.op == Op::Label) {
            if (leaders.empty() || leaders.back() != i) leaders.push_back(i);
            start = false;
        }
        if (in.op == Op::Jmp || in.op == Op::Br || in.op == Op::Ret) start = true;
    }

    for (size_t k = 0; k < leaders.size(); ++k) {
        Block b;
        b.name  = "B" + std::to_string(k);
        b.first = leaders[k];
        b.last  = k + 1 < leaders.size() ? leaders[k + 1] - 1 : n - 1;
        for (int i = b.first; i <= b.last; ++i) g.block_of[i] = static_cast<int>(k);
        g.blocks.push_back(b);
    }

    std::unordered_map<std::string, int> label_block;
    for (int i = 0; i < n; ++i)
        if (f.code[i].op == Op::Label) label_block[f.code[i].target] = g.block_of[i];

    auto add_edge = [&](int from, int to) {
        if (to < 0) return;
        for (int s : g.blocks[from].succ) if (s == to) return;
        g.blocks[from].succ.push_back(to);
        g.blocks[to].pred.push_back(from);
    };
    auto target = [&](const std::string& l) {
        auto it = label_block.find(l);
        return it == label_block.end() ? -1 : it->second;
    };

    for (size_t k = 0; k < g.blocks.size(); ++k) {
        int from = static_cast<int>(k);
        const Instr* last = nullptr;
        for (int i = g.blocks[k].last; i >= g.blocks[k].first; --i) {
            if (f.code[i].op != Op::Nop) { last = &f.code[i]; break; }
        }
        bool has_next = k + 1 < g.blocks.size();
        if (!last) { if (has_next) add_edge(from, from + 1); continue; }
        switch (last->op) {
            case Op::Jmp: add_edge(from, target(last->target)); break;
            case Op::Br:
                add_edge(from, target(last->target));
                add_edge(from, target(last->target2));
                break;
            case Op::Ret: break;
            default: if (has_next) add_edge(from, from + 1); break;
        }
    }
    return g;
}

std::vector<bool> reachable(const CFG& g) {
    std::vector<bool> seen(g.blocks.size(), false);
    if (g.blocks.empty()) return seen;
    std::vector<int> stack{0};
    seen[0] = true;
    while (!stack.empty()) {
        int b = stack.back();
        stack.pop_back();
        for (int s : g.blocks[b].succ) {
            if (!seen[s]) { seen[s] = true; stack.push_back(s); }
        }
    }
    return seen;
}

}  // namespace cviz
