#pragma once
#include <string>
#include <vector>
#include "ir.h"

namespace cviz {

// One individual transformation made by the optimiser. Rewrites are numbered
// in the order they are attempted, and that order is deterministic, which is
// what makes rewrite-level bisection sound (see validate.h).
struct Rewrite {
    int         id = 0;
    std::string pass;          // const, algebraic, cse, dce
    std::string fn;
    std::string reason;        // why the rewrite was justified
    std::string before, after;
    int         instr = -1;
    NodeId      node  = -1;    // provenance, inherited from the instruction
    Span        span;
    int         round = 0;             // optimisation round it was made in
    bool        injected = false;      // ground truth for the evaluation only
    bool        corruptible = false;   // has a constant operand the evaluation can perturb
};

// Deliberate faults, used only to evaluate localisation.
enum Fault {
    FAULT_NONE        = 0,
    FAULT_FOLD_SUB    = 1,   // constant folding computes b - a for a - b
    FAULT_ALG_NEG     = 2,   // algebraic simplification rewrites 0 - x as x
    FAULT_CSE_STALE   = 3,   // CSE keeps an expression after an operand changes
    FAULT_DCE_CALLARG = 4,   // DCE ignores uses that are call arguments
    FAULT_COPY_STALE  = 5,   // copy propagation keeps a copy after its source changes
};
constexpr int FAULT_COUNT = 5;
const char* fault_name(int f);
const char* fault_pass(int f);   // the pass each fault is planted in

struct OptConfig {
    long long budget = -1;   // apply only rewrites 1..budget; -1 means all
    int       fault = FAULT_NONE;
    long long corrupt_at = -1; // off-by-one corruption of this rewrite's constant
    bool      skip_copy = false; // disable copy propagation (see the report, 7.4)
};

class Optimizer {
public:
    explicit Optimizer(OptConfig c) : cfg_(c) {}

    void run(Program& p);

    const std::vector<Rewrite>& rewrites() const { return log_; }
    long long attempts() const { return attempts_; }

private:
    OptConfig cfg_;
    std::vector<Rewrite> log_;
    long long attempts_ = 0;
    int round_ = 0;

    template <class F>
    bool rewrite(Function& f, Instr& in, const char* pass, const std::string& reason,
                 bool injected, F mutate);

    bool const_prop(Function& f);
    bool copy_prop(Function& f);
    bool algebraic(Function& f);
    bool cse(Function& f);
    bool dce(Function& f);
};

}  // namespace cviz
