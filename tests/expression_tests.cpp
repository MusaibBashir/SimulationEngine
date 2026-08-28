// ============================================================================
// tests/expression_tests.cpp  --  v10: the expression layer
// ============================================================================
#include "harness.hpp"
#include "des.hpp"

using namespace des;
using des_test::check;
using des_test::checkClose;
using des_test::section;

void runExpressionTests() {
    section("Expression layer");
    check(true, "suite is wired up");

    section("Value");
    {
        Value n = 3.5;
        Value t = std::string("Ball");
        check(isNumber(n) && !isText(n), "a double Value is a number");
        check(isText(t) && !isNumber(t), "a string Value is text");
        checkClose(asNumber(n), 3.5, 1e-12, "asNumber round-trips");
        check(asText(t) == "Ball", "asText round-trips");

        check(truthy(Value(1.0)), "non-zero is true");
        check(!truthy(Value(0.0)), "zero is false");
        check(truthy(Value(-2.0)), "negative non-zero is true");

        // A string in a numeric position is an ERROR, never a coerced 0.
        bool threw = false;
        try { asNumber(t); } catch (const ExpressionError&) { threw = true; }
        check(threw, "asNumber on text throws rather than coercing");

        threw = false;
        try { truthy(t); } catch (const ExpressionError&) { threw = true; }
        check(threw, "truthy on text throws rather than coercing");

        check(formatValue(Value(2.0)) == "2", "whole numbers format without a decimal point");
        check(formatValue(t) == "Ball", "text formats as itself");
    }

    section("Lexer");
    {
        LexResult r = tokenise("1 + 2.5 * x");
        check(!hasErrors(r.diagnostics), "a clean expression lexes without diagnostics");
        check(r.tokens.size() == 6, "five tokens plus End");
        check(r.tokens[0].kind == TokenKind::Number, "first token is a number");
        checkClose(r.tokens[0].number, 1.0, 1e-12, "first number value");
        check(r.tokens[1].kind == TokenKind::Plus, "operator token");
        checkClose(r.tokens[2].number, 2.5, 1e-12, "decimal number");
        check(r.tokens[4].kind == TokenKind::Identifier && r.tokens[4].text == "x", "identifier");
        check(r.tokens[5].kind == TokenKind::End, "End terminates the stream");

        // A DOT IS AN IDENTIFIER CHARACTER. Entity.Type is ONE token, which is
        // what buys Arena's dotted names with no grammar rule for them.
        LexResult dotted = tokenise("Entity.Type");
        check(dotted.tokens.size() == 2, "a dotted name is a single token");
        check(dotted.tokens[0].text == "Entity.Type", "dot is part of the identifier");

        // A dot followed by a DIGIT is still a number, though.
        LexResult leading = tokenise(".5");
        check(leading.tokens[0].kind == TokenKind::Number, "a leading dot starts a number");
        checkClose(leading.tokens[0].number, 0.5, 1e-12, "leading-dot number value");

        // Two-character operators must not lex as two one-character ones.
        LexResult ops = tokenise("a >= b && c != d");
        check(ops.tokens[1].kind == TokenKind::GreaterEqual, ">= is one token");
        check(ops.tokens[3].kind == TokenKind::AndAnd, "&& is one token");
        check(ops.tokens[5].kind == TokenKind::BangEqual, "!= is one token");
        LexResult single = tokenise("a > b");
        check(single.tokens[1].kind == TokenKind::Greater, "a lone > is still Greater");

        LexResult str = tokenise("Entity.Type == \"Ball\"");
        check(str.tokens[2].kind == TokenKind::Text && str.tokens[2].text == "Ball",
              "a quoted string lexes as Text without its quotes");

        // Spans are what v11 points a spreadsheet cursor at, so they are
        // asserted exactly rather than merely being non-zero.
        LexResult sp = tokenise("ab + c");
        check(sp.tokens[0].span.offset == 0 && sp.tokens[0].span.length == 2, "span of 'ab'");
        check(sp.tokens[2].span.offset == 5 && sp.tokens[2].span.length == 1, "span of 'c'");
        check(sp.tokens[3].span.offset == 6, "End sits at the end of the input");

        LexResult bad = tokenise("1 @ 2");
        check(hasErrors(bad.diagnostics), "an unknown character is a diagnostic");
        check(bad.diagnostics[0].span.offset == 2, "the diagnostic points at the '@'");
        check(bad.tokens.size() == 3, "the bad character is skipped, the rest still lexes");

        // One call reports EVERY bad character, not just the first -- v11 must
        // show every problem in a cell at once.
        LexResult many = tokenise("1 @ 2 $ 3");
        check(many.diagnostics.size() >= 2, "one call reports more than one bad character");

        LexResult unterminated = tokenise("\"oops");
        check(hasErrors(unterminated.diagnostics), "an unterminated string is a diagnostic");
        check(unterminated.tokens[0].kind == TokenKind::Text,
              "an unterminated string still produces a token to parse with");

        LexResult assign = tokenise("a = 1");
        check(hasErrors(assign.diagnostics), "a single '=' is an error");
        check(assign.diagnostics[0].message.find("'=='") != std::string::npos,
              "the message suggests '==' rather than saying 'unexpected character'");

        LexResult empty = tokenise("");
        check(empty.tokens.size() == 1 && empty.tokens[0].kind == TokenKind::End,
              "an empty string lexes to just End");
    }
}
