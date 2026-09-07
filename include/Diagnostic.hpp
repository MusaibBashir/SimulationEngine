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
#include <optional>
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

// v11: which cell a diagnostic came from, when it came from one. Additive, so
// every v10 caller compiles unchanged -- the expression layer keeps filling in
// only the span, and the compiler wraps cell identity around it. That is the
// join v10 was built toward: the parser still never learns what a table is.
struct CellRef {
    std::string moduleType;
    std::size_t row{0};        // 0-based POSITION, as a person counts rows
    std::string column;
};

struct Diagnostic {
    Severity    severity{Severity::Error};
    SourceSpan  span;
    std::string message;
    std::optional<CellRef> cell;   // v11: set only when it came from a cell

    Diagnostic() = default;

    // A CONSTRUCTOR, not aggregate initialisation, and the reason is the whole
    // point of adding `cell` this way: fifteen v10 sites write
    // Diagnostic{severity, span, message}, and under -Wextra an aggregate would
    // warn at every one of them about the member they do not set. A defaulted
    // parameter keeps those sites correct AND quiet, which is what "additive"
    // has to mean if the warning gate is to stay clean.
    Diagnostic(Severity s, SourceSpan sp, std::string m,
               std::optional<CellRef> c = std::nullopt)
        : severity(s), span(sp), message(std::move(m)), cell(std::move(c)) {}
};

// True if any diagnostic in the list is an Error (Warnings alone are fine).
bool hasErrors(const std::vector<Diagnostic>& diagnostics);

// "col 7: expected ')'" -- one line per diagnostic, for tests and the CLI.
std::string formatDiagnostics(const std::vector<Diagnostic>& diagnostics);

}  // namespace des
