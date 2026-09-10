#pragma once
#include <string>
#include <vector>
#include "source.h"

namespace cviz {

// Writes the structured trace defined in spec/trace-format.md. Phases push
// events through this; the visualiser is the only consumer.
class Trace {
public:
    explicit Trace(const Source& src);

    void begin_phase(const std::string& name);
    void end_phase(bool ok);

    // `fields` is pre-rendered JSON for the event body, without the
    // enclosing braces, e.g. "\"kind\":\"token\",\"lexeme\":\"sum\"".
    void event(int id, const std::string& fields, const Span& s);
    void event(int id, const std::string& fields);

    void diagnostic(const Diagnostic& d);

    bool write(const std::string& path) const;

    static std::string escape(const std::string& s);
    static std::string span_json(const Span& s);

private:
    struct Phase {
        std::string name;
        bool ok = true;
        std::vector<std::string> events;
        std::vector<std::string> diags;
    };

    const Source& src_;
    std::vector<Phase> phases_;
};

}  // namespace cviz