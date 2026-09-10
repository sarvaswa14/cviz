#include "trace.h"
#include <fstream>
#include <sstream>
#include <cstdio>
namespace cviz {

Trace::Trace(const Source& src) : src_(src) {}

std::string Trace::escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string Trace::span_json(const Span& s) {
    std::ostringstream o;
    o << "{\"line\":" << s.line << ",\"col\":" << s.col
      << ",\"len\":" << s.len << "}";
    return o.str();
}

void Trace::begin_phase(const std::string& name) {
    Phase p;
    p.name = name;
    phases_.push_back(std::move(p));
}

void Trace::end_phase(bool ok) {
    if (!phases_.empty()) phases_.back().ok = ok;
}

void Trace::event(int id, const std::string& fields, const Span& s) {
    if (phases_.empty()) return;
    std::ostringstream o;
    o << "{\"id\":" << id << "," << fields;
    // An artefact with no source origin omits the span entirely.
    if (s.valid()) o << ",\"span\":" << span_json(s);
    o << "}";
    phases_.back().events.push_back(o.str());
}

void Trace::event(int id, const std::string& fields) {
    event(id, fields, Span{});
}

void Trace::diagnostic(const Diagnostic& d) {
    if (phases_.empty()) return;
    std::ostringstream o;
    o << "{\"class\":\"" << to_string(d.cls) << "\""
      << ",\"code\":\"" << escape(d.code) << "\""
      << ",\"message\":\"" << escape(d.message) << "\""
      << ",\"span\":" << span_json(d.span) << "}";
    phases_.back().diags.push_back(o.str());
}

bool Trace::write(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out << "{\n  \"version\": 1,\n";
    out << "  \"source\": {\n";
    out << "    \"file\": \"" << escape(src_.file) << "\",\n";
    out << "    \"lines\": [";
    for (size_t i = 0; i < src_.lines.size(); ++i) {
        if (i) out << ", ";
        out << "\"" << escape(src_.lines[i]) << "\"";
    }
    out << "]\n  },\n";

    out << "  \"phases\": [\n";
    for (size_t p = 0; p < phases_.size(); ++p) {
        const Phase& ph = phases_[p];
        out << "    {\n";
        out << "      \"phase\": \"" << ph.name << "\",\n";
        out << "      \"status\": \"" << (ph.ok ? "ok" : "error") << "\",\n";

        out << "      \"events\": [\n";
        for (size_t i = 0; i < ph.events.size(); ++i) {
            out << "        " << ph.events[i];
            if (i + 1 < ph.events.size()) out << ",";
            out << "\n";
        }
        out << "      ],\n";

        out << "      \"diagnostics\": [\n";
        for (size_t i = 0; i < ph.diags.size(); ++i) {
            out << "        " << ph.diags[i];
            if (i + 1 < ph.diags.size()) out << ",";
            out << "\n";
        }
        out << "      ]\n";

        out << "    }";
        if (p + 1 < phases_.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return true;
}

}  // namespace cviz