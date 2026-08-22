// ============================================================================
// Trace.cpp
// ============================================================================

#include "Trace.hpp"
#include <iomanip>
#include <sstream>

namespace des {


Trace::~Trace() { close(); }

bool Trace::open(const std::string& path, TraceLevel level, bool markdown) {
    close();
    m_level    = level;
    m_markdown = markdown;
    m_lineCount = 0;
    if (m_level == TraceLevel::Off) return true;   // Off opens no file at all

    m_out.open(path);
    if (!m_out) {
        // Failing to open the trace must NOT take the simulation down -- the
        // trace is diagnostics, not the product. Degrade to Off and say so.
        m_level = TraceLevel::Off;
        return false;
    }

    m_headerWritten = false;   // written lazily on the first event, so that a
                               // note() before then lands ABOVE the table
                               // instead of splitting it in half
    if (m_markdown) m_out << "# Simulation trace\n";
    return true;
}

void Trace::close() {
    if (m_out.is_open()) {
        if (m_markdown) m_out << "\n*" << m_lineCount << " events traced.*\n";
        m_out.close();
    }
    m_level = TraceLevel::Off;
}

void Trace::event(SimTime now,
                  const std::string& eventName,
                  EntityId entity,
                  const std::string& station,
                  const std::string& detail,
                  std::size_t queueLength,
                  int busy) {
    if (m_level == TraceLevel::Off) return;   // the whole cost when tracing is off

    if (!m_headerWritten) {
        if (m_markdown) {
            m_out << "\n| t | event | entity | station | detail | queue | busy |\n";
            m_out << "|---:|---|---:|---|---|---:|---:|\n";
        } else {
            m_out << "        t  event       entity  station     detail\n";
            m_out << "---------------------------------------------------------------\n";
        }
        m_headerWritten = true;
    }
    ++m_lineCount;
    m_out << std::fixed << std::setprecision(4);
    if (m_markdown) {
        m_out << "| " << now
              << " | " << eventName
              << " | " << entity
              << " | " << station
              << " | " << detail
              << " | " << queueLength
              << " | " << busy
              << " |\n";
    } else {
        m_out << std::setw(9) << now << "  "
              << std::setw(10) << std::left << eventName << std::right
              << std::setw(6) << entity << "  "
              << std::setw(10) << std::left << station << std::right
              << "  " << detail
              << "  [q=" << queueLength << " busy=" << busy << "]\n";
    }
}

void Trace::note(const std::string& text) {
    if (m_level == TraceLevel::Off) return;
    if (m_markdown) m_out << "\n" << text << "\n\n";
    else            m_out << text << "\n";
}

}  // namespace des
