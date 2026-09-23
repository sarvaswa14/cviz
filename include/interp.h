#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "ir.h"

namespace cviz {

// Everything observable about one run of a program. Two runs are
// equivalent when all three observable fields agree.
struct Outcome {
    std::string output;
    long long   ret = 0;
    std::string error;
    long long   steps = 0;

    bool same_as(const Outcome& o) const {
        return output == o.output && ret == o.ret && error == o.error;
    }
};

// Executes IR directly. Memory is word-addressed: each array element is one
// cell and a pointer is a cell index. Arithmetic comes from eval_binop, the
// same definition the constant folder uses.
class Interpreter {
public:
    explicit Interpreter(const Program& p, long long step_limit = 20000000)
        : p_(p), limit_(step_limit) {}

    Outcome run();

private:
    struct Frame {
        std::unordered_map<std::string, long long> vars;
        std::unordered_map<std::string, long long> arrays;
    };

    const Program& p_;
    long long limit_;
    long long steps_ = 0;
    int depth_ = 0;
    std::vector<long long> mem_;
    std::unordered_map<std::string, long long> gvars_, garrays_;
    std::unordered_map<const Function*, std::unordered_map<std::string, size_t>> labels_;
    std::string out_;

    long long call(const Function& f, const std::vector<long long>& args);
    long long builtin(const std::string& name, const std::vector<Operand>& args, Frame& fr);
    long long value(const Operand& o, Frame& fr);
    void assign(const std::string& name, long long v, Frame& fr);
    long long& cell(long long addr);
    size_t target(const Function& f, const std::string& label);
};

}  // namespace cviz
