#pragma once
#include <string>
#include <vector>

namespace cviz {

// A location in the source, 1-based, as specified in spec/trace-format.md.
struct Span {
    int line = 0;
    int col  = 0;
    int len  = 0;

    bool valid() const { return line > 0; }
};

enum class DiagClass { Lexical, Syntax, Semantic, Lowering };

const char* to_string(DiagClass c);

// `code` is a stable identifier the test corpus asserts on.
// `message` is for humans and may be reworded freely.
struct Diagnostic {
    DiagClass   cls;
    std::string code;
    std::string message;
    Span        span;
};

// The source file, held as lines so spans can be resolved for display.
struct Source {
    std::string              file;
    std::string              text;
    std::vector<std::string> lines;

    static Source from_file(const std::string& path);
    static Source from_text(std::string text, std::string name = "<input>");
};

}  // namespace cviz
