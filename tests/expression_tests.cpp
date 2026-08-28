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

    section("AST and evaluation");
    {
        // Hand-built trees only -- there is no parser yet, deliberately. A
        // wrong answer here cannot be a parser bug.
        auto lit = [](double v) {
            return std::make_unique<LiteralExpression>(Value(v), SourceSpan{0, 0});
        };
        Entity e(1, 0.0);
        e.setAttribute("defects", 3.0);
        EvalContext ctx(&e, nullptr, nullptr, nullptr);

        BinaryExpression add(BinaryOp::Add, lit(2.0), lit(3.0), SourceSpan{0, 0});
        checkClose(asNumber(add.evaluate(ctx)), 5.0, 1e-12, "2 + 3");

        BinaryExpression gt(BinaryOp::Greater, lit(3.0), lit(2.0), SourceSpan{0, 0});
        checkClose(asNumber(gt.evaluate(ctx)), 1.0, 1e-12, "a true comparison is 1.0");

        BinaryExpression lt(BinaryOp::Less, lit(3.0), lit(2.0), SourceSpan{0, 0});
        checkClose(asNumber(lt.evaluate(ctx)), 0.0, 1e-12, "a false comparison is 0.0");

        UnaryExpression neg(UnaryOp::Negate, lit(4.0), SourceSpan{0, 0});
        checkClose(asNumber(neg.evaluate(ctx)), -4.0, 1e-12, "unary minus");

        UnaryExpression notZero(UnaryOp::Not, lit(0.0), SourceSpan{0, 0});
        checkClose(asNumber(notZero.evaluate(ctx)), 1.0, 1e-12, "!0 is 1");

        NameExpression name("defects", SourceSpan{0, 7});
        checkClose(asNumber(name.evaluate(ctx)), 3.0, 1e-12, "a name reads an entity attribute");

        Entity ball(2, 0.0);
        ball.setType("Ball");
        EvalContext bctx(&ball, nullptr, nullptr, nullptr);
        NameExpression etype("Entity.Type", SourceSpan{0, 11});
        check(asText(etype.evaluate(bctx)) == "Ball", "Entity.Type reads the entity's type");

        BinaryExpression typeEq(BinaryOp::Equal,
            std::make_unique<NameExpression>("Entity.Type", SourceSpan{0, 11}),
            std::make_unique<LiteralExpression>(Value(std::string("Ball")), SourceSpan{0, 0}),
            SourceSpan{0, 0});
        checkClose(asNumber(typeEq.evaluate(bctx)), 1.0, 1e-12, "text equality compares");

        // Text supports equality and nothing else.
        {
            BinaryExpression bad(BinaryOp::Add,
                std::make_unique<LiteralExpression>(Value(std::string("a")), SourceSpan{0, 0}),
                lit(1.0), SourceSpan{0, 0});
            bool threw = false;
            try { bad.evaluate(ctx); } catch (const ExpressionError&) { threw = true; }
            check(threw, "arithmetic on text throws rather than coercing");
        }

        // meanIfKnown: a literal knows its own mean, arithmetic does not.
        check(lit(5.0)->meanIfKnown().has_value(), "a literal has a known mean");
        checkClose(*lit(5.0)->meanIfKnown(), 5.0, 1e-12, "a literal's mean is itself");
        BinaryExpression sum(BinaryOp::Add, lit(1.0), lit(2.0), SourceSpan{0, 0});
        check(!sum.meanIfKnown().has_value(), "arithmetic does not claim a known mean");
        UnaryExpression negKnown(UnaryOp::Negate, lit(3.0), SourceSpan{0, 0});
        checkClose(*negKnown.meanIfKnown(), -3.0, 1e-12, "negation of a known mean is known");
        NameExpression unknownMean("x", SourceSpan{0, 1});
        check(!unknownMean.meanIfKnown().has_value(), "a name has no known mean");

        // A DistributionExpression reports its distribution's mean, and draws
        // identically to the distribution it wraps.
        {
            DistributionExpression de(exponential(0.8));
            check(de.meanIfKnown().has_value(), "a distribution has a known mean");
            checkClose(*de.meanIfKnown(), 0.8, 1e-12, "EXPO(0.8) mean is 0.8");

            RandomStream a(4242u), b(4242u);
            EvalContext dctx(nullptr, nullptr, nullptr, &a);
            auto plain = exponential(0.8);
            checkClose(asNumber(de.evaluate(dctx)), plain->draw(b), 1e-12,
                       "a wrapped distribution draws identically to a bare one");
        }

        // Division by zero is a run-time error, not an inf that spreads.
        BinaryExpression div(BinaryOp::Divide, lit(1.0), lit(0.0), SourceSpan{0, 0});
        bool threw = false;
        try { div.evaluate(ctx); } catch (const ExpressionError&) { threw = true; }
        check(threw, "division by zero throws rather than producing inf");

        // Short-circuit: the right operand must NOT be evaluated when the left
        // decides the answer. Without it `Count > 0 && Total / Count > 5`
        // cannot be written at all.
        {
            BinaryExpression safe(BinaryOp::And,
                std::make_unique<BinaryExpression>(BinaryOp::Greater, lit(0.0), lit(1.0),
                                                   SourceSpan{0, 0}),
                std::make_unique<BinaryExpression>(BinaryOp::Divide, lit(1.0), lit(0.0),
                                                   SourceSpan{0, 0}),
                SourceSpan{0, 0});
            bool blewUp = false;
            double got = -1.0;
            try { got = asNumber(safe.evaluate(ctx)); }
            catch (const ExpressionError&) { blewUp = true; }
            check(!blewUp, "&& does not evaluate its right operand when the left is false");
            checkClose(got, 0.0, 1e-12, "&& short-circuits to 0");
        }
        {
            BinaryExpression safeOr(BinaryOp::Or, lit(1.0),
                std::make_unique<BinaryExpression>(BinaryOp::Divide, lit(1.0), lit(0.0),
                                                   SourceSpan{0, 0}),
                SourceSpan{0, 0});
            bool blewUp = false;
            try { safeOr.evaluate(ctx); } catch (const ExpressionError&) { blewUp = true; }
            check(!blewUp, "|| does not evaluate its right operand when the left is true");
        }

        // Reading an attribute with no entity throws rather than reading 0.
        {
            EvalContext none(nullptr, nullptr, nullptr, nullptr);
            bool t = false;
            try { name.evaluate(none); } catch (const ExpressionError& ) { t = true; }
            check(t, "an attribute read with no entity throws");
        }

        // Validation, before any run.
        ValidationContext vc;
        vc.field = FieldContext::NoEntity;
        vc.attributeNames = {"defects"};
        std::vector<Diagnostic> diags;
        name.validate(vc, diags);
        check(hasErrors(diags), "attribute reference with no entity is a validation error");
        check(diags[0].span.offset == 0 && diags[0].span.length == 7,
              "the diagnostic spans the name");

        std::vector<Diagnostic> ok;
        vc.field = FieldContext::HasEntity;
        name.validate(vc, ok);
        check(!hasErrors(ok), "attribute reference with an entity validates");

        std::vector<Diagnostic> tnow;
        NameExpression clock("TNOW", SourceSpan{0, 4});
        clock.validate(vc, tnow);
        check(!hasErrors(tnow), "TNOW validates in any context");

        NameExpression unknown("nosuch", SourceSpan{0, 6});
        std::vector<Diagnostic> u;
        unknown.validate(vc, u);
        check(hasErrors(u), "an unresolvable name is a validation error");

        // validate() descends, and collects EVERY problem rather than the first.
        {
            BinaryExpression twoBad(BinaryOp::Add,
                std::make_unique<NameExpression>("nope1", SourceSpan{0, 5}),
                std::make_unique<NameExpression>("nope2", SourceSpan{8, 5}),
                SourceSpan{0, 0});
            std::vector<Diagnostic> both;
            twoBad.validate(vc, both);
            check(both.size() == 2, "validate collects every bad name, not just the first");
        }

        // clone() is deep -- v11 copies expressions when a cell is duplicated.
        auto copy = add.clone();
        checkClose(asNumber(copy->evaluate(ctx)), 5.0, 1e-12, "clone evaluates identically");
        check(add.describe() == copy->describe(), "clone describes identically");

        // A LambdaExpression makes the existing std::function API one code path.
        {
            LambdaExpression pred([](const Entity& en){ return en.attribute("defects") > 2.0; });
            checkClose(asNumber(pred.evaluate(ctx)), 1.0, 1e-12, "a C++ predicate evaluates");
            auto pc = pred.clone();
            checkClose(asNumber(pc->evaluate(ctx)), 1.0, 1e-12, "a cloned predicate evaluates");
        }
    }
}
