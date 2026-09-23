#pragma once
#include <string>
#include <vector>
#include "interp.h"
#include "ir.h"
#include "opt.h"

namespace cviz {

// Provenance-guided rewrite bisection.
//
// The unoptimised program is the reference. If the optimised program
// behaves differently, the optimiser is re-run with a rewrite budget and the
// smallest budget that reproduces the difference is found by binary search.
// Because rewrites are attempted in a fixed order, a budget of N applies
// exactly the first N rewrites of the full run, so the rewrite numbered with
// that smallest budget is the first one after which behaviour diverges.
// Its recorded provenance then leads back to the source line.
struct Validation {
    std::vector<Rewrite> log;      // every rewrite of the unrestricted run
    Outcome   reference;
    Outcome   optimised;
    bool      equal = true;
    bool      localised = false;
    Rewrite   culprit;
    int       runs = 0;            // interpreter runs spent bisecting
    long long rewrites = 0;        // rewrites in the full optimisation
    int       pass_level = 0;      // rewrites a pass-level blame would leave to inspect
};

Validation validate_program(const Program& unopt, const OptConfig& cfg);

// Differential testing of the execution engine against gcc.
struct GccResult {
    bool        ran = false;
    std::string error;
    Outcome     gcc;
    Outcome     ours;
    bool        match = false;
};
GccResult diff_against_gcc(const std::string& c_file, const Program& unopt);

// Evaluation of localisation accuracy (see the Phase 2 report, section 7).
struct FaultResult {
    int         fault = 0;
    bool        triggered = false;   // behaviour changed
    bool        exact = false;       // culprit is a rewrite the fault produced
    std::string pass;
    int         culprit = 0;
    int         line = 0;
    int         runs = 0;
    long long   rewrites = 0;
    int         pass_level = 0;
};
std::vector<FaultResult> evaluate_faults(const Program& unopt);

struct CorruptionSummary {
    int    candidates = 0;   // rewrites whose result is a constant
    int    divergent = 0;    // corruptions that changed observable behaviour
    int    exact = 0;        // divergent ones localised to the corrupted rewrite
    long long total_runs = 0;
    long long rewrites = 0;
    long long pass_level_sum = 0;   // summed over divergent trials
};
CorruptionSummary evaluate_corruptions(const Program& unopt, int limit = 400);

// Aggregate results over randomly generated programs.
struct RandomEval {
    int programs = 0, rejected = 0;
    long long instr_before = 0, instr_after = 0, rewrites = 0;
    int trials[FAULT_COUNT + 1] = {0};
    int triggered[FAULT_COUNT + 1] = {0};
    int exact[FAULT_COUNT + 1] = {0};
    int pass_ok[FAULT_COUNT + 1] = {0};
    long long fault_runs = 0;
    long long fault_pass_level = 0;
    CorruptionSummary corr;
};
RandomEval evaluate_random(int count, unsigned seed, int corruption_limit = 60);

}  // namespace cviz
