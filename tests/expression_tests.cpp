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
}
