// ============================================================================
// Diagnostic.hpp  --  v10: a user's mistake is DATA, not an exception
// ============================================================================
// ModelError throws because a PROGRAMMER wired the model wrong: there is one
// mistake, it is a bug, and stopping is the right response.
//
// A malformed expression is different. It is a person typing into a cell, and
// v11's spreadsheet must show EVERY bad cell at once -- not the first one and
// then nothing. An exception can only carry one failure and unwinds past the
// rest, so the parser returns a LIST instead and keeps going.

#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace des {

// A range within the text that was parsed. Offsets are relative to the start of
// the expression string -- which in v11 is the contents of one spreadsheet cell.
// The parser therefore never needs to know what a row or a column is; v11 adds
// that identity around the span.
struct SourceSpan {
    std::size_t offset{0};
    std::size_t length{0};
};

enum class Severity { Error, Warning };

struct Diagnostic {
    Severity    severity{Severity::Error};
    SourceSpan  span;
    std::string message;
};

// True if any diagnostic in the list is an Error (Warnings alone are fine).
bool hasErrors(const std::vector<Diagnostic>& diagnostics);

// "col 7: expected ')'" -- one line per diagnostic, for tests and the CLI.
std::string formatDiagnostics(const std::vector<Diagnostic>& diagnostics);

}  // namespace des
