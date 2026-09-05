// ============================================================================
// Lexer.hpp  --  v10: text into tokens, each carrying where it came from
// ============================================================================
// Every token carries a SourceSpan, and that is the point of writing a lexer at
// all rather than pattern-matching strings. v11 puts a cursor in a spreadsheet
// cell on the exact character that is wrong; it can only do that if position
// survives from here all the way to the diagnostic.
//
// ONE DELIBERATE ODDITY: a dot is an IDENTIFIER character, not an operator. So
// `Entity.Type` is a single token and there is no member-access node in the
// grammar. That buys Arena's dotted names for free. The cost is that a stray
// dot lands inside an identifier and is reported as an unknown NAME rather than
// as a syntax error -- which is the better of the two messages, because dotted
// names are far more common than mistyped operators.
//
// IT NEVER THROWS. An unrecognised character produces a diagnostic and is
// SKIPPED, so one call reports every bad character rather than dying on the
// first. That is the same reason the parser recovers: v11's spreadsheet has to
// show every bad cell at once, and an exception can only carry one failure.

#pragma once
#include <string>
#include <vector>
#include "Diagnostic.hpp"

namespace des {

enum class TokenKind {
    Number, Text, Identifier,
    Plus, Minus, Star, Slash, Percent, Caret, Bang,
    Less, LessEqual, Greater, GreaterEqual, EqualEqual, BangEqual,
    AndAnd, OrOr,
    LParen, RParen, Comma,
    End,
    Invalid
};

struct Token {
    TokenKind   kind{TokenKind::Invalid};
    SourceSpan  span;
    double      number{0.0};   // Number only
    std::string text;          // Identifier and Text only
};

struct LexResult {
    std::vector<Token>      tokens;        // always ends with an End token
    std::vector<Diagnostic> diagnostics;
};

LexResult tokenise(const std::string& source);

// "')'", "a number" -- for building messages that name what was expected.
const char* describe(TokenKind kind);

}  // namespace des
