#pragma once
#include <string>
#include <vector>

namespace cviz {

struct Span {
    int line = 0;
    int col  = 0;
    int len  = 0;

    bool valid() const { return line > 0; }
};

enum class DiagClass { Lexical, Syntax, Semantic };

const char* to_string(DiagClass c);

struct Diagnostic {
    DiagClass   cls;
    std::string code;
    std::string message;
    Span        span;
};

struct Source {
    std::string              file;
    std::string              text;
    std::vector<std::string> lines;

    static Source from_file(const std::string& path);
    static Source from_text(std::string text, std::string name = "<input>");
};

}  