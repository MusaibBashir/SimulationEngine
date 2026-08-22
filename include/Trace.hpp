// ============================================================================
// Trace.hpp  --  v3 step 8: write down everything that happened
// ============================================================================
// report() gives aggregates. Aggregates tell you the answer is 3.28; they never
// tell you why. A trace is a line per event -- who arrived when, who waited,
// who was served -- and it is what makes a small run checkable against a table
// worked by hand.
//
// It is also the best refactoring tool in the project. Trace a run before a
// change and after it; if the diff is empty, behaviour is identical. That is a
// far stronger statement than "the averages still look about right".

#pragma once

#include <fstream>
#include <string>
#include "Common.hpp"

namespace des {


enum class TraceLevel {
    Off,       // no file is opened, and every trace call returns immediately
    Events,    // one line per event
    Detailed   // events plus queue contents and state after each one
};

class Trace {
private:
    std::ofstream m_out;
    TraceLevel    m_level{TraceLevel::Off};
    bool          m_markdown{true};
    long long     m_lineCount{0};
    bool          m_headerWritten{false};

public:
    Trace() = default;
    ~Trace();   // closes the stream

    // An ofstream cannot be copied, so neither can this. Say so explicitly
    // rather than letting the error surface as a wall of template output.
    Trace(const Trace&) = delete;
    Trace& operator=(const Trace&) = delete;

    // Opens the file and writes the header. markdown=true emits a table that
    // renders in your notes; false emits plain aligned columns.
    bool open(const std::string& path, TraceLevel level, bool markdown = true);
    void close();

    bool isOn() const { return m_level != TraceLevel::Off; }
    TraceLevel level() const { return m_level; }
    long long lineCount() const { return m_lineCount; }

    // One row per event. The early return when Off is what makes tracing free
    // in production runs -- the call site never has to guard.
    void event(SimTime now,
               const std::string& eventName,
               EntityId entity,
               const std::string& station,
               const std::string& detail,
               std::size_t queueLength,
               int busy);

    // Free-form line, for run headers and anything that is not an event.
    void note(const std::string& text);
};

}  // namespace des
