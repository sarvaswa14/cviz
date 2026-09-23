#pragma once
#include <string>
#include <vector>
#include "ir.h"

namespace cviz {

// A basic block is a maximal straight-line run of instructions: control
// enters only at the first and leaves only at the last.
struct Block {
    std::string name;
    int first = 0, last = -1;       // inclusive indices into Function::code
    std::vector<int> succ, pred;
};

struct CFG {
    std::vector<Block> blocks;
    std::vector<int>   block_of;    // instruction index -> block index
};

CFG build_cfg(const Function& f);
std::vector<bool> reachable(const CFG& g);

}  // namespace cviz
