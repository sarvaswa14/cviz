#include "source.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cviz {

const char* to_string(DiagClass c) {
    switch (c) {
        case DiagClass::Lexical:  return "lexical";
        case DiagClass::Syntax:   return "syntax";
        case DiagClass::Semantic: return "semantic";
    }
    return "unknown";
}

static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\r') continue;         
        if (c == '\n') { out.push_back(cur); cur.clear(); }
        else           { cur.push_back(c); }
    }
    out.push_back(cur);                 
    return out;
}

Source Source::from_text(std::string text, std::string name) {
    Source s;
    s.file  = std::move(name);
    s.text  = std::move(text);
    s.lines = split_lines(s.text);
    return s;
}

Source Source::from_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return from_text(ss.str(), path);
}

}  