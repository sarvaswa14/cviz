#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "cfg.h"
#include "interp.h"
#include "ir.h"
#include "irgen.h"
#include "lexer.h"
#include "opt.h"
#include "parser.h"
#include "sema.h"
#include "trace.h"
#include "gen.h"
#include "validate.h"

using namespace cviz;

static void print_usage() {
    std::cerr <<
        "usage: cviz <file.c> [options]\n"
        "\n"
        "  front end\n"
        "    --dump-tokens          print the token stream\n"
        "    --dump-ast             print the abstract syntax tree\n"
        "  intermediate code and optimisation\n"
        "    --dump-ir              print three-address code before optimisation\n"
        "    --dump-cfg             print the basic blocks of each function\n"
        "    --dump-opt             print every rewrite, then the optimised code\n"
        "    --O0                   disable the optimiser\n"
        "    --no-copy              disable copy propagation\n"
        "  execution and validation\n"
        "    --run                  execute the (optimised) program\n"
        "    --validate             check the optimiser against the reference and,\n"
        "                           on a mismatch, locate the rewrite responsible\n"
        "    --diff-gcc             compare the execution engine with gcc\n"
        "    --inject-fault <1-4>   deliberately break one optimisation pass\n"
        "    --rewrite-budget <n>   apply only the first n rewrites\n"
        "    --eval                 run the localisation evaluation on this file\n"
        "    --eval-random <n>      run the evaluation over n generated programs\n"
        "    --gen-seed <n>         seed for generation (default 1)\n"
        "    --generate             print a generated program and exit\n"
        "  output\n"
        "    --emit-trace <path>    write the structured trace as JSON\n"
        "    -h, --help             show this message\n";
}

static void show_span(const Source& src, const Span& s, std::ostream& os) {
    if (s.line < 1 || s.line > static_cast<int>(src.lines.size())) return;
    const std::string& text = src.lines[s.line - 1];
    os << "  " << text << "\n  ";
    for (int i = 1; i < s.col; ++i)
        os << (i - 1 < static_cast<int>(text.size()) && text[i - 1] == '\t' ? '\t' : ' ');
    os << '^';
    for (int i = 1; i < s.len; ++i) os << '~';
    os << "\n";
}

static void report(const Source& src, const Diagnostic& d) {
    std::cerr << src.file << ":" << d.span.line << ":" << d.span.col << ": "
              << to_string(d.cls) << " error: " << d.message << " [" << d.code << "]\n";
    show_span(src, d.span, std::cerr);
}

static std::string q(const std::string& s) { return Trace::quote(s); }

// ------------------------------------------------------------- AST dump

static void indent(int n) { for (int i = 0; i < n; ++i) std::cout << "  "; }
static void dump_expr(const Expr* e, int d);
static void dump_stmt(const Stmt* s, int d);
static void dump_decl(const Decl* dcl, int d);

static void dump_expr(const Expr* e, int d) {
    if (!e) return;
    indent(d);
    std::cout << e->kind_name();
    std::string lbl = e->label();
    if (!lbl.empty()) std::cout << "  " << lbl;
    if (e->type) std::cout << "  : " << e->type->to_string();
    std::cout << "   #" << e->id << " @" << e->span.line << ":" << e->span.col << "\n";
    dump_expr(e->lhs.get(), d + 1);
    dump_expr(e->rhs.get(), d + 1);
    dump_expr(e->third.get(), d + 1);
    for (const auto& a : e->args) dump_expr(a.get(), d + 1);
}

static void dump_stmt(const Stmt* s, int d) {
    if (!s) return;
    indent(d);
    std::cout << s->kind_name() << "   #" << s->id << " @" << s->span.line << ":" << s->span.col << "\n";
    dump_expr(s->expr.get(), d + 1);
    dump_expr(s->init_expr.get(), d + 1);
    dump_expr(s->cond_expr.get(), d + 1);
    dump_expr(s->step_expr.get(), d + 1);
    if (s->decl) dump_decl(s->decl.get(), d + 1);
    dump_stmt(s->body.get(), d + 1);
    dump_stmt(s->else_body.get(), d + 1);
    for (const auto& it : s->items) dump_stmt(it.get(), d + 1);
}

static void dump_decl(const Decl* dcl, int d) {
    if (!dcl) return;
    indent(d);
    std::cout << dcl->kind_name() << "   #" << dcl->id << " @" << dcl->span.line << ":" << dcl->span.col << "\n";
    if (dcl->kind == DeclKind::Function) {
        indent(d + 1);
        std::cout << dcl->func_name << " : " << (dcl->func_type ? dcl->func_type->to_string() : "?");
        if (!dcl->param_names.empty()) {
            std::cout << "  params(";
            for (size_t i = 0; i < dcl->param_names.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << dcl->param_names[i];
            }
            std::cout << ")";
        }
        std::cout << "\n";
        dump_stmt(dcl->body.get(), d + 1);
    } else if (dcl->kind == DeclKind::StructDef) {
        indent(d + 1);
        std::cout << "struct " << dcl->tag << "\n";
    } else {
        for (const auto& dr : dcl->declarators) {
            indent(d + 1);
            std::cout << dr.name << " : " << (dr.type ? dr.type->to_string() : "?") << "\n";
            if (dr.init) dump_expr(dr.init.get(), d + 2);
        }
    }
}

// ------------------------------------------------------ front-end trace

static void trace_expr(Trace& tr, const Expr* e, std::vector<int>& kids) {
    if (!e) return;
    kids.push_back(e->id);
    std::vector<int> mine;
    trace_expr(tr, e->lhs.get(), mine);
    trace_expr(tr, e->rhs.get(), mine);
    trace_expr(tr, e->third.get(), mine);
    for (const auto& a : e->args) trace_expr(tr, a.get(), mine);
    std::ostringstream o;
    o << "\"kind\":\"node\",\"node\":" << q(e->kind_name()) << ",\"label\":" << q(e->label())
      << ",\"children\":[";
    for (size_t i = 0; i < mine.size(); ++i) o << (i ? "," : "") << mine[i];
    o << "]";
    tr.event(e->id, o.str(), e->span);
}

static void trace_decl(Trace& tr, const Decl* d, bool root);

static void trace_stmt(Trace& tr, const Stmt* s) {
    if (!s) return;
    std::vector<int> kids;
    trace_expr(tr, s->expr.get(), kids);
    trace_expr(tr, s->init_expr.get(), kids);
    trace_expr(tr, s->cond_expr.get(), kids);
    trace_expr(tr, s->step_expr.get(), kids);
    if (s->decl)      { trace_decl(tr, s->decl.get(), false); kids.push_back(s->decl->id); }
    if (s->body)      { trace_stmt(tr, s->body.get());        kids.push_back(s->body->id); }
    if (s->else_body) { trace_stmt(tr, s->else_body.get());   kids.push_back(s->else_body->id); }
    for (const auto& it : s->items) { trace_stmt(tr, it.get()); kids.push_back(it->id); }
    std::ostringstream o;
    o << "\"kind\":\"node\",\"node\":" << q(s->kind_name()) << ",\"label\":\"\",\"children\":[";
    for (size_t i = 0; i < kids.size(); ++i) o << (i ? "," : "") << kids[i];
    o << "]";
    tr.event(s->id, o.str(), s->span);
}

static void trace_decl(Trace& tr, const Decl* d, bool root) {
    if (!d) return;
    std::vector<int> kids;
    std::string label;
    if (d->kind == DeclKind::Function) {
        label = d->func_name + " : " + (d->func_type ? d->func_type->to_string() : "?");
        if (d->body) { trace_stmt(tr, d->body.get()); kids.push_back(d->body->id); }
    } else if (d->kind == DeclKind::StructDef) {
        label = "struct " + d->tag;
    } else {
        for (const auto& dr : d->declarators) {
            if (!label.empty()) label += ", ";
            label += dr.name + " : " + (dr.type ? dr.type->to_string() : "?");
            if (dr.init) trace_expr(tr, dr.init.get(), kids);
        }
    }
    std::ostringstream o;
    o << "\"kind\":\"node\",\"node\":" << q(d->kind_name()) << ",\"label\":" << q(label);
    if (root) o << ",\"root\":true";
    o << ",\"children\":[";
    for (size_t i = 0; i < kids.size(); ++i) o << (i ? "," : "") << kids[i];
    o << "]";
    tr.event(d->id, o.str(), d->span);
}

static void trace_sema(Trace& tr, const Sema& sema) {
    for (const SemaEvent& e : sema.events()) {
        std::ostringstream o;
        switch (e.kind) {
            case SemaEvent::Kind::ScopeOpen:
                o << "\"kind\":\"scope_open\",\"scope\":" << e.scope << ",\"of\":" << q(e.of);
                break;
            case SemaEvent::Kind::ScopeClose:
                o << "\"kind\":\"scope_close\",\"scope\":" << e.scope;
                break;
            case SemaEvent::Kind::SymbolDecl:
                o << "\"kind\":\"symbol\",\"name\":" << q(e.name) << ",\"type\":" << q(e.type)
                  << ",\"scope\":" << e.scope;
                break;
            case SemaEvent::Kind::TypeAssign:
                o << "\"kind\":\"type\",\"type\":" << q(e.type)
                  << ",\"from\":{\"phase\":\"parse\",\"id\":" << e.node << "}";
                break;
        }
        tr.event(e.id, o.str(), e.span);
    }
}

// ------------------------------------------------------ Phase 2 trace

static std::string from_parse(NodeId n) {
    return ",\"from\":{\"phase\":\"parse\",\"id\":" + std::to_string(n) + "}";
}

static void trace_code(Trace& tr, const Program& p, const char* stage) {
    for (const auto& f : p.functions) {
        CFG g = build_cfg(f);
        for (size_t i = 0; i < f.code.size(); ++i) {
            const Instr& in = f.code[i];
            if (in.op == Op::Nop) continue;
            std::ostringstream o;
            o << "\"kind\":\"instr\",\"stage\":" << q(stage) << ",\"fn\":" << q(f.name)
              << ",\"text\":" << q(in.text()) << ",\"label\":" << (in.op == Op::Label ? "true" : "false");
            if (g.block_of[i] >= 0) o << ",\"block\":" << q(g.blocks[g.block_of[i]].name);
            if (in.node >= 0) o << from_parse(in.node);
            tr.event(in.id, o.str(), in.span);
        }
    }
}

static void trace_cfg(Trace& tr, const Program& p) {
    int id = 0;
    for (const auto& f : p.functions) {
        CFG g = build_cfg(f);
        for (const Block& b : g.blocks) {
            std::ostringstream o;
            o << "\"kind\":\"block\",\"fn\":" << q(f.name) << ",\"block\":" << q(b.name) << ",\"instrs\":[";
            bool first = true;
            for (int i = b.first; i <= b.last; ++i) {
                if (f.code[i].op == Op::Nop) continue;
                o << (first ? "" : ",") << f.code[i].id;
                first = false;
            }
            o << "],\"succ\":[";
            for (size_t k = 0; k < b.succ.size(); ++k) o << (k ? "," : "") << q(g.blocks[b.succ[k]].name);
            o << "],\"pred\":[";
            for (size_t k = 0; k < b.pred.size(); ++k) o << (k ? "," : "") << q(g.blocks[b.pred[k]].name);
            o << "]";
            tr.event(id++, o.str());
        }
    }
}

static std::string rewrite_json(const Rewrite& r) {
    std::ostringstream o;
    o << "\"pass\":" << q(r.pass) << ",\"fn\":" << q(r.fn) << ",\"reason\":" << q(r.reason)
      << ",\"before\":" << q(r.before) << ",\"after\":" << q(r.after) << ",\"instr\":" << r.instr;
    if (r.node >= 0) o << from_parse(r.node);
    return o.str();
}

static void trace_opt(Trace& tr, const Program& before, const Program& after,
                      const std::vector<Rewrite>& log) {
    int counts[4] = {0, 0, 0, 0};
    for (const auto& r : log) {
        if (r.pass == "const") ++counts[0];
        else if (r.pass == "algebraic") ++counts[1];
        else if (r.pass == "cse") ++counts[2];
        else ++counts[3];
        tr.event(r.id, "\"kind\":\"rewrite\"," + rewrite_json(r), r.span);
    }
    std::ostringstream s;
    s << "\"kind\":\"summary\",\"before\":" << before.live_count() << ",\"after\":" << after.live_count()
      << ",\"rewrites\":" << log.size() << ",\"by_pass\":{\"const\":" << counts[0]
      << ",\"algebraic\":" << counts[1] << ",\"cse\":" << counts[2] << ",\"dce\":" << counts[3] << "}";
    tr.event(0, s.str());
}

static std::string outcome_json(const char* prefix, const Outcome& o) {
    std::ostringstream s;
    s << ",\"" << prefix << "_output\":" << q(o.output) << ",\"" << prefix << "_ret\":" << o.ret
      << ",\"" << prefix << "_error\":" << q(o.error) << ",\"" << prefix << "_steps\":" << o.steps;
    return s.str();
}

static void trace_validation(Trace& tr, const Validation& v) {
    std::ostringstream o;
    o << "\"kind\":\"validation\",\"result\":" << q(v.equal ? "equal" : "divergent")
      << outcome_json("ref", v.reference) << outcome_json("opt", v.optimised)
      << ",\"rewrites\":" << v.rewrites << ",\"runs\":" << v.runs;
    if (v.localised)
        o << ",\"culprit\":{\"id\":" << v.culprit.id << "," << rewrite_json(v.culprit)
          << ",\"span\":" << Trace::span_json(v.culprit.span) << "}";
    tr.event(0, o.str(), v.localised ? v.culprit.span : Span{});
}

// ------------------------------------------------------ reports

static std::string first_line_of(const std::string& s) {
    std::string l = s.substr(0, s.find('\n'));
    return l.size() > 60 ? l.substr(0, 57) + "..." : l;
}

static void print_validation(const Source& src, const Validation& v) {
    if (v.equal) {
        std::cout << "validation: the optimised program matches the unoptimised reference\n"
                  << "  output    : " << v.reference.output.size() << " bytes, return value "
                  << v.reference.ret << "\n"
                  << "  rewrites  : " << v.rewrites << " applied, behaviour unchanged\n";
        return;
    }
    std::cout << "validation: MISMATCH between the reference and the optimised program\n";
    auto show = [](const char* who, const Outcome& o) {
        std::cout << "  " << who << ": return " << o.ret;
        if (!o.error.empty()) std::cout << ", error \"" << o.error << "\"";
        std::cout << ", output \"" << first_line_of(o.output) << "\"\n";
    };
    show("reference", v.reference);
    show("optimised", v.optimised);
    if (!v.localised) {
        std::cout << "  the responsible rewrite could not be isolated\n";
        return;
    }
    const Rewrite& r = v.culprit;
    std::cout << "  bisection : " << v.runs << " interpreter runs over " << v.rewrites << " rewrites\n"
              << "  culprit   : rewrite #" << r.id << " (" << r.pass << ") in function " << r.fn << "\n"
              << "              " << r.before << "   ->   " << r.after << "\n"
              << "  reason    : " << r.reason << "\n"
              << "  source    : line " << r.span.line << ", column " << r.span.col << "\n";
    show_span(src, r.span, std::cout);
}

static void print_rewrites(const std::vector<Rewrite>& log) {
    for (const auto& r : log) {
        std::printf("#%-4d %-9s %-6s %s   ->   %s\n", r.id, r.pass.c_str(), r.fn.c_str(),
                    r.before.c_str(), r.after.c_str());
        std::printf("      %s  (line %d)\n", r.reason.c_str(), r.span.line);
    }
}

static void print_cfg(const Program& p) {
    for (const auto& f : p.functions) {
        CFG g = build_cfg(f);
        std::cout << "func " << f.name << "\n";
        for (const Block& b : g.blocks) {
            std::cout << "  " << b.name << "  ->";
            if (b.succ.empty()) std::cout << " exit";
            for (int s : b.succ) std::cout << " " << g.blocks[s].name;
            std::cout << "\n";
            for (int i = b.first; i <= b.last; ++i)
                if (f.code[i].op != Op::Nop) std::cout << "      " << f.code[i].text() << "\n";
        }
        std::cout << "\n";
    }
}

static void print_eval(const std::string& file, const Program& ir) {
    Program o = ir;
    Optimizer opt(OptConfig{});
    opt.run(o);
    std::cout << "eval file=" << file << " instr_before=" << ir.live_count()
              << " instr_after=" << o.live_count() << " rewrites=" << opt.rewrites().size() << "\n";
    for (const auto& r : evaluate_faults(ir)) {
        std::cout << "fault " << r.fault << " " << fault_name(r.fault)
                  << " triggered=" << (r.triggered ? "yes" : "no");
        if (r.triggered)
            std::cout << " culprit=#" << r.culprit << " pass=" << r.pass
                      << " exact=" << (r.exact ? "yes" : "no") << " line=" << r.line
                      << " runs=" << r.runs << " rewrites=" << r.rewrites
                      << " pass_level=" << r.pass_level;
        std::cout << "\n";
    }
    CorruptionSummary c = evaluate_corruptions(ir);
    std::cout << "corruptions candidates=" << c.candidates << " divergent=" << c.divergent
              << " masked=" << (c.candidates - c.divergent) << " exact=" << c.exact
              << " runs=" << c.total_runs << " rewrites=" << c.rewrites << "\n";
}

// ------------------------------------------------------ main

int main(int argc, char** argv) {
    std::string input, trace_path;
    bool dump_tokens = false, dump_ast = false, dump_ir = false, dump_cfg = false;
    bool dump_opt = false, no_opt = false, run = false, validate = false;
    bool diff_gcc = false, eval = false, generate = false;
    int  eval_random = 0;
    unsigned gen_seed = 1;
    OptConfig oc;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* what) -> std::string {
            if (i + 1 >= argc) { std::cerr << "cviz: " << a << " needs " << what << "\n"; std::exit(2); }
            return argv[++i];
        };
        if (a == "-h" || a == "--help")    { print_usage(); return 0; }
        else if (a == "--dump-tokens")     dump_tokens = true;
        else if (a == "--dump-ast")        dump_ast = true;
        else if (a == "--dump-ir")         dump_ir = true;
        else if (a == "--dump-cfg")        dump_cfg = true;
        else if (a == "--dump-opt")        dump_opt = true;
        else if (a == "--O0")              no_opt = true;
        else if (a == "--no-copy")         oc.skip_copy = true;
        else if (a == "--run")             run = true;
        else if (a == "--validate")        validate = true;
        else if (a == "--diff-gcc")        diff_gcc = true;
        else if (a == "--eval")            eval = true;
        else if (a == "--generate")        generate = true;
        else if (a == "--eval-random")     eval_random = std::atoi(need("a count").c_str());
        else if (a == "--gen-seed")        gen_seed = static_cast<unsigned>(std::atoi(need("a seed").c_str()));
        else if (a == "--emit-trace")      trace_path = need("a path");
        else if (a == "--inject-fault")    oc.fault = std::atoi(need("a fault number").c_str());
        else if (a == "--rewrite-budget")  oc.budget = std::atoll(need("a number").c_str());
        else if (!a.empty() && a[0] == '-') { std::cerr << "cviz: unknown option " << a << "\n"; return 2; }
        else if (input.empty())            input = a;
        else { std::cerr << "cviz: only one input file is supported\n"; return 2; }
    }
    if (generate) { std::cout << generate_program(gen_seed); return 0; }

    if (eval_random > 0) {
        RandomEval e = evaluate_random(eval_random, gen_seed);
        std::cout << "programs generated : " << (e.programs + e.rejected)
                  << " (" << e.programs << " usable, " << e.rejected << " rejected)\n"
                  << "instructions       : " << e.instr_before << " before, " << e.instr_after
                  << " after optimisation\n"
                  << "rewrites           : " << e.rewrites << "\n\n";
        std::cout << "fault                          trials  triggered  exact  pass-correct\n";
        int tt = 0, tg = 0, tx = 0, tp = 0;
        for (int f = 1; f <= FAULT_COUNT; ++f) {
            std::printf("%-30s %6d %10d %6d %13d\n", fault_name(f), e.trials[f],
                        e.triggered[f], e.exact[f], e.pass_ok[f]);
            tt += e.trials[f]; tg += e.triggered[f]; tx += e.exact[f]; tp += e.pass_ok[f];
        }
        std::printf("%-30s %6d %10d %6d %13d\n", "total", tt, tg, tx, tp);
        if (tg) std::printf("mean interpreter runs per localisation: %.2f\n",
                            static_cast<double>(e.fault_runs) / tg);
        if (tg) std::printf("mean rewrites left by pass-level blame: %.2f\n",
                            static_cast<double>(e.fault_pass_level) / tg);
        const CorruptionSummary& c = e.corr;
        std::cout << "\nsingle-rewrite corruptions\n"
                  << "  candidates : " << c.candidates << "\n"
                  << "  divergent  : " << c.divergent << " (" << (c.candidates - c.divergent)
                  << " masked, i.e. the corruption never reached the output)\n"
                  << "  localised  : " << c.exact << " of " << c.divergent << "\n";
        if (c.divergent) {
            std::printf("  mean runs  : %.2f per localisation\n",
                        static_cast<double>(c.total_runs) / c.divergent);
            std::printf("  pass-level : %.2f rewrites left to inspect on average\n",
                        static_cast<double>(c.pass_level_sum) / c.divergent);
        }
        return 0;
    }

    if (input.empty()) { print_usage(); return 2; }

    Source src;
    try {
        src = Source::from_file(input);
    } catch (const std::exception& e) {
        std::cerr << "cviz: " << e.what() << "\n";
        return 2;
    }

    Trace trace(src);

    // ---- lexical analysis
    trace.begin_phase("lex");
    Lexer lexer(src);
    std::vector<Token> tokens = lexer.tokenise();
    for (size_t i = 0; i < tokens.size(); ++i) {
        std::ostringstream o;
        o << "\"kind\":\"token\",\"tclass\":" << q(tok_name(tokens[i].kind)) << ",\"lexeme\":" << q(tokens[i].lexeme);
        trace.event(static_cast<int>(i), o.str(), tokens[i].span);
    }
    for (const auto& d : lexer.diagnostics()) trace.diagnostic(d);
    trace.end_phase(!lexer.failed());
    if (dump_tokens)
        for (const Token& t : tokens)
            std::printf("%3d:%-3d  %-14s  %s\n", t.span.line, t.span.col, tok_name(t.kind), t.lexeme.c_str());
    for (const auto& d : lexer.diagnostics()) report(src, d);

    // ---- syntax analysis
    trace.begin_phase("parse");
    Parser parser(src, tokens);
    TranslationUnit tu = parser.parse();
    for (const auto& d : tu.decls) trace_decl(trace, d.get(), true);
    for (const auto& d : parser.diagnostics()) trace.diagnostic(d);
    trace.end_phase(!parser.failed());
    for (const auto& d : parser.diagnostics()) report(src, d);

    // ---- semantic analysis
    trace.begin_phase("sem");
    Sema sema(src);
    if (!parser.failed()) sema.analyse(tu);
    trace_sema(trace, sema);
    for (const auto& d : sema.diagnostics()) trace.diagnostic(d);
    trace.end_phase(!sema.failed());
    for (const auto& d : sema.diagnostics()) report(src, d);

    if (dump_ast) for (const auto& d : tu.decls) dump_decl(d.get(), 0);

    bool ok = !lexer.failed() && !parser.failed() && !sema.failed();
    int status = ok ? 0 : 1;

    if (ok) {
        // ---- intermediate code
        trace.begin_phase("ir");
        IRGen gen(src);
        Program ir = gen.lower(tu);
        trace_code(trace, ir, "ir");
        for (const auto& d : gen.diagnostics()) trace.diagnostic(d);
        trace.end_phase(!gen.failed());
        for (const auto& d : gen.diagnostics()) report(src, d);

        if (gen.failed()) {
            status = 1;
        } else {
            trace.begin_phase("cfg");
            trace_cfg(trace, ir);
            trace.end_phase(true);

            // ---- optimisation
            Program opt = ir;
            std::vector<Rewrite> log;
            if (!no_opt) {
                Optimizer o(oc);
                o.run(opt);
                log = o.rewrites();
                trace.begin_phase("opt");
                trace_opt(trace, ir, opt, log);
                trace_code(trace, opt, "optimised");
                trace.end_phase(true);
            }

            if (dump_ir)  std::cout << ir.dump();
            if (dump_cfg) print_cfg(ir);
            if (dump_opt) {
                print_rewrites(log);
                std::cout << "\n" << ir.live_count() << " instructions before, "
                          << opt.live_count() << " after, " << log.size() << " rewrites\n\n"
                          << opt.dump();
            }

            // ---- validation
            if (!no_opt && (validate || !trace_path.empty())) {
                Validation v = validate_program(ir, oc);
                trace.begin_phase("verify");
                trace_validation(trace, v);
                trace.end_phase(v.equal);
                if (validate) print_validation(src, v);
                if (!v.equal) status = 4;
            }

            if (run) {
                Outcome out = Interpreter(no_opt ? ir : opt).run();
                std::cout << out.output;
                if (!out.output.empty() && out.output.back() != '\n') std::cout << "\n";
                std::cerr << "[exit value " << out.ret << ", " << out.steps << " steps";
                if (!out.error.empty()) std::cerr << ", runtime error: " << out.error;
                std::cerr << "]\n";
            }

            if (diff_gcc) {
                GccResult g = diff_against_gcc(input, ir);
                if (!g.ran) {
                    std::cout << "diff-gcc: " << g.error << "\n";
                } else {
                    std::cout << "diff-gcc: " << (g.match ? "MATCH" : "MISMATCH")
                              << "  gcc output " << g.gcc.output.size() << " bytes, exit "
                              << g.gcc.ret << "  |  cviz output " << g.ours.output.size()
                              << " bytes, exit " << (g.ours.ret & 0xFF) << "\n";
                    if (!g.match) status = 5;
                }
            }

            if (eval) print_eval(input, ir);
        }
    }

    if (!trace_path.empty() && !trace.write(trace_path)) {
        std::cerr << "cviz: cannot write " << trace_path << "\n";
        return 2;
    }
    return status;
}
