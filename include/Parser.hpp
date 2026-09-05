// ============================================================================
// Parser.hpp  --  v10: tokens into a tree, reporting everything wrong
// ============================================================================
// Recursive descent with a precedence climb. Two things about it are not
// ordinary.
//
// IT DOES NOT THROW. A malformed expression is a person typing in a cell, and
// v11's spreadsheet must show every bad cell at once. So the parser recovers:
// on an error it records a diagnostic, skips something it can resume past, and
// keeps going.
//
// IT RESOLVES NOTHING. An identifier becomes a NameExpression holding a string;
// whether that is an attribute, a variable or TNOW is decided later. The parser
// has no name tables, so v11 adding a Variable data module cannot make it
// stale.

#pragma once
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "Expression.hpp"

namespace des {

struct ParseResult {
    ExpressionPtr           expr;      // never null; a placeholder on error
    std::vector<Diagnostic> diagnostics;
};

ParseResult parseExpression(const std::string& source);

// For C++ model code, where a bad expression is a PROGRAMMER error and there is
// no cell to point at. Throws ModelError listing every diagnostic.
ExpressionPtr expr(const std::string& source);

}  // namespace des
