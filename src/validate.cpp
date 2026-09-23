#include "validate.h"
#include "gen.h"
#include "irgen.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include <cstdio>
#include <cstdlib>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace cviz {

static Outcome run_with(const Program& unopt, OptConfig cfg, std::vector<Rewrite>* log) {
    Program p = unopt;
    Optimizer o(cfg);
    o.run(p);
    if (log) *log = o.rewrites();
    return Interpreter(p).run();
}

Validation validate_program(const Program& unopt, const OptConfig& base) {
    Validation v;
    v.reference = Interpreter(unopt).run();

    std::vector<Rewrite> log;
    OptConfig full = base;
    full.budget = -1;
    v.optimised = run_with(unopt, full, &log);
    v.log = log;
    v.rewrites = static_cast<long long>(log.size());
    v.equal = v.reference.same_as(v.optimised);
    if (v.equal) return v;

    // Invariant: budget lo behaves like the reference, budget hi does not.
    // Budget 0 applies no rewrites, so it is the reference by construction.
    long long lo = 0, hi = v.rewrites;
    while (hi - lo > 1) {
        long long mid = lo + (hi - lo) / 2;
        OptConfig c = base;
        c.budget = mid;
        Outcome o = run_with(unopt, c, nullptr);
        ++v.runs;
        if (o.same_as(v.reference)) lo = mid;
        else hi = mid;
    }
    for (const auto& r : log) {
        if (r.id == hi) { v.culprit = r; v.localised = true; break; }
    }
    // What a pass-level blame (one pass running over one function, as in a
    // pass-granularity bisection) would leave the developer to inspect.
    if (v.localised) {
        for (const auto& r : log)
            if (r.pass == v.culprit.pass && r.fn == v.culprit.fn && r.round == v.culprit.round)
                ++v.pass_level;
    }
    return v;
}

GccResult diff_against_gcc(const std::string& c_file, const Program& unopt) {
    GccResult g;
    g.ours = Interpreter(unopt).run();
#ifdef _WIN32
    const std::string exe = "cviz_reference.exe", run = "cviz_reference.exe";
#else
    const std::string exe = "./cviz_reference", run = "./cviz_reference";
#endif
    std::string compile = "gcc -std=c99 -w -include stdio.h -o " + exe + " \"" + c_file + "\"";
    if (std::system(compile.c_str()) != 0) {
        g.error = "gcc failed to compile the program";
        return g;
    }
#ifdef _WIN32
    FILE* pipe = _popen(run.c_str(), "r");
#else
    FILE* pipe = popen(run.c_str(), "r");
#endif
    if (!pipe) {
        g.error = "could not run the gcc-compiled program";
        return g;
    }
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, pipe)) > 0) g.gcc.output.append(buf, n);
#ifdef _WIN32
    int status = _pclose(pipe);
    g.gcc.ret = status;
#else
    int status = pclose(pipe);
    g.gcc.ret = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    std::remove(exe.c_str());
    g.ran = true;
    // An exit status carries only the low 8 bits of main's return value.
    g.match = g.gcc.output == g.ours.output && g.ours.error.empty() &&
              (g.ours.ret & 0xFF) == (g.gcc.ret & 0xFF);
    return g;
}

std::vector<FaultResult> evaluate_faults(const Program& unopt) {
    std::vector<FaultResult> out;
    for (int f = FAULT_FOLD_SUB; f <= FAULT_COPY_STALE; ++f) {
        OptConfig c;
        c.fault = f;
        Validation v = validate_program(unopt, c);
        FaultResult r;
        r.fault = f;
        r.triggered = !v.equal;
        r.rewrites = v.rewrites;
        r.runs = v.runs;
        if (v.localised) {
            r.exact   = v.culprit.injected;
            r.pass    = v.culprit.pass;
            r.culprit = v.culprit.id;
            r.line    = v.culprit.span.line;
            r.pass_level = v.pass_level;
        }
        out.push_back(r);
    }
    return out;
}

// For every rewrite that produces a constant, corrupt that one rewrite by
// one and ask whether bisection points back at exactly that rewrite.
CorruptionSummary evaluate_corruptions(const Program& unopt, int limit) {
    CorruptionSummary s;
    std::vector<Rewrite> log;
    run_with(unopt, OptConfig{}, &log);
    s.rewrites = static_cast<long long>(log.size());
    for (const auto& r : log) {
        if (!r.corruptible) continue;
        if (s.candidates >= limit) break;
        ++s.candidates;
        OptConfig c;
        c.corrupt_at = r.id;
        Validation v = validate_program(unopt, c);
        if (v.equal) continue;               // the corruption was masked
        ++s.divergent;
        s.total_runs += v.runs;
        s.pass_level_sum += v.pass_level;
        if (v.localised && v.culprit.id == r.id) ++s.exact;
    }
    return s;
}

// ---------------------------------------------------------------- random

RandomEval evaluate_random(int count, unsigned seed, int corruption_limit) {
    RandomEval e;
    for (int i = 0; i < count; ++i) {
        Source src = Source::from_text(generate_program(seed + static_cast<unsigned>(i)),
                                       "generated-" + std::to_string(seed + static_cast<unsigned>(i)) + ".c");
        Lexer lex(src);
        auto toks = lex.tokenise();
        Parser parser(src, toks);
        TranslationUnit tu = parser.parse();
        Sema sema(src);
        if (!parser.failed()) sema.analyse(tu);
        IRGen gen(src);
        Program ir = gen.lower(tu);
        if (lex.failed() || parser.failed() || sema.failed() || gen.failed()) { ++e.rejected; continue; }

        Outcome base = Interpreter(ir).run();
        if (!base.error.empty()) { ++e.rejected; continue; }

        ++e.programs;
        e.instr_before += static_cast<long long>(ir.live_count());
        Program opt = ir;
        Optimizer o{OptConfig{}};
        o.run(opt);
        e.instr_after += static_cast<long long>(opt.live_count());
        e.rewrites += static_cast<long long>(o.rewrites().size());

        for (const auto& r : evaluate_faults(ir)) {
            ++e.trials[r.fault];
            if (!r.triggered) continue;
            ++e.triggered[r.fault];
            if (r.exact) ++e.exact[r.fault];
            if (r.pass == fault_pass(r.fault)) ++e.pass_ok[r.fault];
            e.fault_runs += r.runs;
            e.fault_pass_level += r.pass_level;
        }
        CorruptionSummary c = evaluate_corruptions(ir, corruption_limit);
        e.corr.candidates += c.candidates;
        e.corr.divergent  += c.divergent;
        e.corr.exact      += c.exact;
        e.corr.total_runs += c.total_runs;
        e.corr.pass_level_sum += c.pass_level_sum;
        e.corr.rewrites   += c.rewrites;
    }
    return e;
}

}  // namespace cviz
