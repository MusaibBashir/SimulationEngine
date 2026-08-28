// ============================================================================
// tests/expression_tests.cpp  --  v10: the expression layer
// ============================================================================
#include <fstream>
#include <iterator>
#include <string>
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

    section("Parser");
    {
        Entity e(1, 0.0);
        e.setAttribute("defects", 3.0);
        RandomStream rng(1234u);
        EvalContext ctx(&e, nullptr, nullptr, &rng);

        auto value = [&](const char* s) {
            ParseResult r = parseExpression(s);
            check(!hasErrors(r.diagnostics), std::string("parses cleanly: ") + s);
            return asNumber(r.expr->evaluate(ctx));
        };

        checkClose(value("1 + 2 * 3"),      7.0, 1e-12, "* binds tighter than +");
        checkClose(value("(1 + 2) * 3"),    9.0, 1e-12, "parentheses override");
        checkClose(value("2 ^ 3 ^ 2"),    512.0, 1e-12, "^ is RIGHT-associative: 2^(3^2)");
        checkClose(value("10 - 3 - 2"),     5.0, 1e-12, "- is LEFT-associative");
        checkClose(value("100 / 10 / 2"),   5.0, 1e-12, "/ is LEFT-associative");
        checkClose(value("-2 ^ 2"),        -4.0, 1e-12, "unary minus binds looser than ^");
        checkClose(value("1 + 2 > 2"),      1.0, 1e-12, "arithmetic binds tighter than comparison");
        checkClose(value("1 > 2 || 3 > 2"), 1.0, 1e-12, "|| is loosest");
        checkClose(value("0 && 1 || 1"),    1.0, 1e-12, "&& binds tighter than ||");
        checkClose(value("!0"),             1.0, 1e-12, "logical not");
        checkClose(value("7 % 3"),          1.0, 1e-12, "modulo");
        checkClose(value("defects > 2"),    1.0, 1e-12, "a name in a comparison");
        checkClose(value("2 <= 2 != 0"),    1.0, 1e-12, "comparisons chain left-associatively");

        checkClose(value("MIN(3, 1, 2)"),   1.0, 1e-12, "MIN is variadic");
        checkClose(value("MAX(3, 1, 2)"),   3.0, 1e-12, "MAX is variadic");
        checkClose(value("ABS(-4)"),        4.0, 1e-12, "ABS");
        checkClose(value("ROUND(2.6)"),     3.0, 1e-12, "ROUND");
        checkClose(value("TRUNC(2.6)"),     2.0, 1e-12, "TRUNC");
        checkClose(value("SQRT(9)"),        3.0, 1e-12, "SQRT");
        checkClose(value("MOD(7, 3)"),      1.0, 1e-12, "MOD");
        checkClose(value("LN(EXP(1))"),     1.0, 1e-9,  "LN and EXP are inverses");
        checkClose(value("MAX(1, MIN(5, 3))"), 3.0, 1e-12, "calls nest");

        // A parsed distribution must draw IDENTICALLY to a constructed one --
        // they are the same objects underneath.
        {
            RandomStream a(4242u), b(4242u);
            EvalContext actx(nullptr, nullptr, nullptr, &a);
            ParseResult r = parseExpression("EXPO(0.8)");
            check(!hasErrors(r.diagnostics), "EXPO(0.8) parses");
            auto plain = exponential(0.8);
            checkClose(asNumber(r.expr->evaluate(actx)), plain->draw(b), 1e-12,
                       "a parsed EXPO draws identically to exponential(0.8)");
            checkClose(*r.expr->meanIfKnown(), 0.8, 1e-12, "constant args keep a knowable mean");
        }
        {
            ParseResult r = parseExpression("TRIA(1, 2, 3)");
            check(!hasErrors(r.diagnostics), "TRIA parses");
            checkClose(*r.expr->meanIfKnown(), 2.0, 1e-12, "TRIA(1,2,3) mean is 2");
        }
        {
            // Arena's DISC takes CUMULATIVE probabilities; Discrete takes
            // individual ones. Mean 1*0.3 + 2*0.5 + 3*0.2 = 1.9.
            ParseResult r = parseExpression("DISC(0.3, 1, 0.8, 2, 1.0, 3)");
            check(!hasErrors(r.diagnostics), "DISC parses");
            checkClose(*r.expr->meanIfKnown(), 1.9, 1e-9,
                       "DISC reads its probabilities as cumulative, as Arena does");
        }
        {
            // A computed argument cannot have a mean known in advance.
            ParseResult r = parseExpression("EXPO(defects)");
            check(!hasErrors(r.diagnostics), "EXPO with a computed argument parses");
            check(!r.expr->meanIfKnown().has_value(),
                  "a computed distribution argument has no knowable mean");
            check(asNumber(r.expr->evaluate(ctx)) > 0.0, "and it still draws");
        }

        // Error POSITIONS, character-exact: v11 puts a cursor on this offset.
        {
            ParseResult r = parseExpression("EXPO(0.8");
            check(hasErrors(r.diagnostics), "unclosed paren is an error");
            check(r.diagnostics[0].message.find("expected ')'") != std::string::npos,
                  "the message names what was expected");
            check(r.diagnostics[0].span.offset == 8, "diagnostic points at the end of input");
        }
        {
            ParseResult r = parseExpression("1 + ");
            check(hasErrors(r.diagnostics), "a dangling operator is an error");
            check(r.diagnostics[0].span.offset == 4, "diagnostic at the missing operand");
        }
        {
            ParseResult r = parseExpression("EXPOO(1)");
            check(hasErrors(r.diagnostics), "an unknown function is an error");
            check(r.diagnostics[0].message.find("EXPOO") != std::string::npos,
                  "the message names the function");
        }
        {
            ParseResult r = parseExpression("EXPO(1, 2)");
            check(hasErrors(r.diagnostics), "wrong arity is an error");
            check(r.diagnostics[0].message.find("1 argument") != std::string::npos,
                  "the message says how many arguments were expected");
        }
        {
            ParseResult r = parseExpression("DISC(0.5, 1, 1.0)");
            check(hasErrors(r.diagnostics), "DISC with an odd argument count is an error");
        }
        {
            ParseResult r = parseExpression("NQ(1 + 1)");
            check(hasErrors(r.diagnostics), "NQ of a computed value is an error");
            check(r.diagnostics[0].message.find("NAME of a block") != std::string::npos,
                  "the message says NQ wants a block name");
        }
        {
            ParseResult r = parseExpression("1 2");
            check(hasErrors(r.diagnostics), "trailing junk is an error");
        }

        // One parse reports MANY problems -- v11 shows every bad cell at once.
        {
            ParseResult r = parseExpression("1 @ 2 $ 3");
            check(r.diagnostics.size() >= 2, "one parse reports more than one problem");
        }

        // NQ captures the block NAME rather than resolving it as a variable.
        {
            ParseResult r = parseExpression("NQ(Teller)");
            check(!hasErrors(r.diagnostics), "NQ(Teller) parses");
            check(r.expr->describe() == "NQ(Teller)", "NQ keeps the block name verbatim");
            std::vector<Diagnostic> d;
            ValidationContext vc;
            r.expr->validate(vc, d);
            check(!hasErrors(d), "the block name is not validated as a variable");
        }
        {
            ParseResult r = parseExpression("NQ(\"Teller\")");
            check(!hasErrors(r.diagnostics), "a quoted block name is accepted too");
            check(r.expr->describe() == "NQ(Teller)", "and means the same thing");
        }

        {
            Entity ball(2, 0.0);
            ball.setType("Ball");
            EvalContext bctx(&ball, nullptr, nullptr, nullptr);
            ParseResult r = parseExpression("Entity.Type == \"Ball\"");
            check(!hasErrors(r.diagnostics), "a string comparison parses");
            checkClose(asNumber(r.expr->evaluate(bctx)), 1.0, 1e-12, "entity type compares equal");
        }

        // expr() throws: on the C++ API a malformed expression IS a programmer
        // error, and there is no cell to point at.
        {
            bool threw = false;
            try { expr("1 +"); } catch (const ModelError&) { threw = true; }
            check(threw, "expr() throws on a malformed expression");
            check(expr("2 + 2") != nullptr, "expr() returns a usable tree");
        }

        // clone() survives a round trip through the parser's node kinds.
        {
            ParseResult r = parseExpression("MIN(defects, 10) + 1");
            auto copy = r.expr->clone();
            checkClose(asNumber(copy->evaluate(ctx)), 4.0, 1e-12, "a cloned call tree evaluates");
        }
    }

    section("Variables");
    {
        VariableStore v;
        v.declare("WIPCount", 0.0);
        check(v.has("WIPCount"), "a declared variable is present");
        check(!v.has("nosuch"), "an undeclared variable is absent");
        checkClose(v.get("WIPCount"), 0.0, 1e-12, "initial value");

        v.set("WIPCount", 5.0, 0.0);
        checkClose(v.get("WIPCount"), 5.0, 1e-12, "set round-trips");

        // Configuration survives reset; run state does not.
        v.reset();
        checkClose(v.get("WIPCount"), 0.0, 1e-12, "reset restores the initial value");
        check(v.has("WIPCount"), "reset keeps the declaration");
        check(v.count() == 1, "names() lists the declarations");
        check(v.names()[0] == "WIPCount", "declaration order is kept");

        bool threw = false;
        try { v.declare("WIPCount", 1.0); } catch (const ModelError&) { threw = true; }
        check(threw, "a duplicate declaration throws");

        // ONE NAMESPACE: a variable may not shadow an attribute. The
        // alternative is a resolution order, and that means one of the two
        // reads silently wrong.
        VariableStore w;
        w.noteAttributeNames({"priority"});
        threw = false;
        try { w.declare("priority", 0.0); } catch (const ModelError&) { threw = true; }
        check(threw, "a variable colliding with an attribute is refused");
        w.declare("shift", 1.0);
        check(w.has("shift"), "a non-colliding name is still fine");

        threw = false;
        try { w.set("nosuch", 1.0, 0.0); } catch (const ModelError&) { threw = true; }
        check(threw, "setting an undeclared variable throws rather than auto-declaring");

        threw = false;
        try { w.get("nosuch"); } catch (const ModelError&) { threw = true; }
        check(threw, "reading an undeclared variable throws");

        // Time-persistent average, hand-computed: 0 over [0,2), 10 over [2,4)
        // gives a time average of 5 over [0,4).
        VariableStore t;
        t.declare("Level", 0.0);
        t.resetStatistics(0.0);
        t.set("Level", 10.0, 2.0);
        t.updateIntegrals(4.0);
        checkClose(t.timeAverage("Level"), 5.0, 1e-9, "time-persistent average, hand-computed");

        // Warm-up: the measurement restarts, the VALUE does not.
        t.resetStatistics(4.0);
        checkClose(t.get("Level"), 10.0, 1e-12, "resetStatistics leaves the value alone");
        t.updateIntegrals(6.0);
        checkClose(t.timeAverage("Level"), 10.0, 1e-9,
                   "the average measures only since the warm-up");

        // A variable that never changes still averages to its value.
        VariableStore c;
        c.declare("Fixed", 3.0);
        c.resetStatistics(0.0);
        c.updateIntegrals(10.0);
        checkClose(c.timeAverage("Fixed"), 3.0, 1e-9, "a constant variable averages to itself");

        // Before any time has passed, report the current value rather than
        // dividing by zero.
        VariableStore z;
        z.declare("Z", 7.0);
        checkClose(z.timeAverage("Z"), 7.0, 1e-12, "zero measured time reports the value");

        // A NameExpression now resolves against a real store.
        VariableStore live;
        live.declare("Rate", 2.5);
        EvalContext lctx(nullptr, &live, nullptr, nullptr);
        NameExpression rate("Rate", SourceSpan{0, 4});
        checkClose(asNumber(rate.evaluate(lctx)), 2.5, 1e-12, "a name resolves to a variable");

        ValidationContext vc;
        vc.variables = &live;
        vc.field = FieldContext::NoEntity;
        std::vector<Diagnostic> d;
        rate.validate(vc, d);
        check(!hasErrors(d), "a declared variable validates even with no entity");
    }

    section("Variables and model state in a running model");
    {
        SimulationSystem sim(12345u);
        sim.model().variable("Served", 0.0)
                   .arrivals(constant(1.0))
                   .station("Teller", 1, FIFO, constant(0.5))
                   .entryAt("Teller");
        sim.stopAt(10.0);
        sim.initialise();

        checkClose(sim.model().variables().get("Served"), 0.0, 1e-12,
                   "a variable starts at its initial value");
        sim.run();

        const IModelState& state = sim;
        checkClose(state.now(), sim.clock().now(), 1e-12, "IModelState::now is the clock");
        check(state.queueLength("Teller") >= 0.0, "queueLength answers for a real block");
        checkClose(state.resourceCapacity("Teller"), 1.0, 1e-12, "resourceCapacity answers");
        check(state.numberInSystem() >= 0.0, "numberInSystem answers");

        // An unknown block name is an ERROR, not a silent zero. A typo in NQ()
        // must not read as "the queue is empty".
        bool threw = false;
        try { state.queueLength("Tellr"); } catch (const ModelError&) { threw = true; }
        check(threw, "queueLength on an unknown block throws rather than reading 0");
        threw = false;
        try { state.resourceBusy("nope"); } catch (const ModelError&) { threw = true; }
        check(threw, "resourceBusy on an unknown name throws");

        // A second run starts clean.
        sim.initialise();
        checkClose(sim.model().variables().get("Served"), 0.0, 1e-12,
                   "initialise() resets variables");

        // A variable colliding with a declared attribute is refused.
        {
            SimulationSystem s2(1u);
            s2.model().attribute("priority", uniform(0.0, 1.0));
            bool collided = false;
            try { s2.model().variable("priority", 0.0); }
            catch (const ModelError&) { collided = true; }
            check(collided, "Model::variable refuses to shadow an arrival attribute");
        }

        // A shared resource is addressable by its own name, a private one by
        // the block's -- there is no other name for it.
        {
            SimulationSystem s3(2u);
            s3.model().resource("Nurse", 2)
                      .arrivals(constant(5.0))
                      .stationUsing("Triage", "Nurse", FIFO, constant(1.0))
                      .entryAt("Triage");
            s3.stopAt(20.0).initialise();
            s3.run();
            const IModelState& st = s3;
            checkClose(st.resourceCapacity("Nurse"), 2.0, 1e-12, "a shared resource by name");
            checkClose(st.resourceCapacity("Triage"), 2.0, 1e-12, "or by the block using it");
        }

        // The variable integral is closed in the same step as every other one,
        // so a time average taken over a whole run is right. Value 0 for the
        // first half of a 10-unit run and 4 for the second averages to 2.
        {
            SimulationSystem s4(3u);
            s4.model().variable("Level", 0.0)
                      .arrivals(constant(1.0))
                      .station("W", 1, FIFO, constant(0.1))
                      .entryAt("W");
            s4.stopAt(10.0).initialise();
            s4.model().variables().set("Level", 4.0, 0.0);
            s4.run();
            checkClose(s4.variableAverage("Level"), 4.0, 1e-9,
                       "a variable held all run averages to its value");
        }
    }

    section("Decide by a text condition");
    {
        // Text and lambda must agree EXACTLY -- same seed, same routing.
        auto countExpress = [](bool useText) {
            SimulationSystem sim(4242u);
            Model& m = sim.model();
            m.attribute("priority", uniform(0.0, 4.0))
             .arrivals(exponential(1.0));
            if (useText) m.decideWhen("Sort", "priority > 2");
            else         m.decideByCondition("Sort",
                            [](const Entity& e){ return e.attribute("priority") > 2; });
            m.station("Express", 1, FIFO, exponential(0.3))
             .station("Normal",  1, FIFO, exponential(0.3))
             .routeTrue("Sort", "Express")
             .route("Sort", "Normal")
             .entryAt("Sort");
            sim.stopAt(200.0).initialise();
            sim.run();
            return sim.model().nodeAs<DecideNode>("Sort").tookTrue();
        };
        const long long viaText = countExpress(true);
        check(viaText == countExpress(false),
              "a text condition routes IDENTICALLY to the equivalent lambda");
        check(viaText > 0, "the condition actually fired");

        // A malformed condition is a programmer error on the C++ API.
        {
            SimulationSystem sim(1u);
            bool threw = false;
            try { sim.model().decideWhen("Bad", "priority >"); }
            catch (const ModelError&) { threw = true; }
            check(threw, "a malformed condition text throws on the C++ API");
        }

        // A condition reading live model state -- impossible with a lambda,
        // which cannot see the queue.
        {
            SimulationSystem sim(7u);
            sim.model().arrivals(constant(1.0))
                       .decideWhen("Full", "NQ(Busy) > 2")
                       .station("Busy", 1, FIFO, constant(2.0))
                       .station("Overflow", 1, FIFO, constant(0.1))
                       .routeTrue("Full", "Overflow")
                       .route("Full", "Busy")
                       .entryAt("Full");
            sim.model().allowOverload();
            sim.stopAt(50.0).initialise();
            sim.run();
            check(sim.model().nodeAs<DecideNode>("Full").tookTrue() > 0,
                  "NQ() in a condition sees the queue actually filling");
        }

        // A condition reading a variable.
        {
            SimulationSystem sim(9u);
            sim.model().variable("Gate", 0.0)
                       .arrivals(constant(1.0))
                       .decideWhen("Check", "Gate > 0")
                       .station("Open", 1, FIFO, constant(0.1))
                       .station("Shut", 1, FIFO, constant(0.1))
                       .routeTrue("Check", "Open")
                       .route("Check", "Shut")
                       .entryAt("Check");
            sim.stopAt(20.0).initialise();
            sim.model().variables().set("Gate", 1.0, 0.0);
            sim.run();
            check(sim.model().nodeAs<DecideNode>("Check").tookTrue() > 0,
                  "a variable read in a condition routes on its value");
            check(sim.model().nodeAs<DecideNode>("Check").tookFalse() == 0,
                  "and nothing takes the other branch while it stays set");
        }

        // Mixing chance and condition branches is still refused.
        {
            SimulationSystem sim(1u);
            sim.model().decideNWayByChance("Mix").station("A", 1, FIFO, constant(1.0));
            bool threw = false;
            try { sim.model().branchWhen("Mix", "1 > 0", "A"); }
            catch (const ModelError&) { threw = true; }
            check(threw, "a condition branch on a by-chance Decide is still refused");
        }
    }

    section("Assign");
    {
        // Counted = Counted + 1 -- an expression with a global on BOTH sides.
        // This is the thing attributes structurally cannot express.
        {
            SimulationSystem sim(99u);
            sim.model().variable("Counted", 0.0)
                       .arrivals(constant(1.0))
                       .assignVariable("Count", "Counted", "Counted + 1")
                       .station("Work", 1, FIFO, constant(0.5))
                       .route("Count", "Work")
                       .entryAt("Count");
            sim.stopAt(10.5).initialise();
            sim.run();
            checkClose(sim.model().variables().get("Counted"), 11.0, 1e-12,
                       "the variable counted every arrival");
        }

        // An attribute assigned from an expression over another attribute.
        {
            SimulationSystem s2(5u);
            s2.model().attribute("base", constant(2.0))
                      .arrivals(constant(1.0))
                      .assignTo("Double", "doubled", "base * 3")
                      .recordAttribute("Seen", "doubled")
                      .route("Double", "Seen")
                      .entryAt("Double");
            s2.stopAt(5.0).initialise();
            s2.run();
            checkClose(s2.model().nodeAs<RecordNode>("Seen").average(), 6.0, 1e-9,
                       "the assigned attribute is base * 3");
        }

        // One Assign block, several fields, mixed targets.
        {
            SimulationSystem s3(6u);
            s3.model().variable("Total", 0.0)
                      .arrivals(constant(1.0))
                      .assignTo("Stamp", "size", "3")
                      .assignVariable("Stamp", "Total", "Total + size")
                      .station("W", 1, FIFO, constant(0.1))
                      .route("Stamp", "W")
                      .entryAt("Stamp");
            s3.stopAt(4.5).initialise();
            s3.run();
            // Assert the RELATIONSHIP, not a predicted arrival count: every
            // entity through the block adds exactly the size just stamped on
            // it, which only holds if the fields run in order.
            const long long stamped = s3.model().nodeAs<AssignNode>("Stamp").count();
            check(stamped > 0, "the Assign block saw entities");
            checkClose(s3.model().variables().get("Total"), 3.0 * static_cast<double>(stamped),
                       1e-12,
                       "fields run in order, so the variable sees the attribute just set");
        }

        // Assigning the entity's TYPE, which per-type reporting keys on.
        {
            SimulationSystem s4(7u);
            s4.model().arrivals(constant(1.0))
                      .assignEntityType("Rename", "\"Widget\"")
                      .station("W", 1, FIFO, constant(0.1))
                      .route("Rename", "W")
                      .entryAt("Rename");
            s4.stopAt(5.0).initialise();
            s4.run();
            check(s4.byType().count("Widget") == 1,
                  "entities are reported under their reassigned type");
        }

        // A reserved engine attribute is still refused.
        {
            SimulationSystem s5(8u);
            bool threw = false;
            try { s5.model().assignTo("Bad", "waitTime", "1"); }
            catch (const ModelError&) { threw = true; }
            check(threw, "a reserved attribute name is still refused");
        }

        // Text into a numeric variable is refused rather than stored as zero.
        {
            SimulationSystem s6(1u);
            s6.model().variable("V", 0.0)
                      .arrivals(constant(1.0))
                      .assignVariable("A", "V", "\"text\"")
                      .station("W", 1, FIFO, constant(0.1))
                      .route("A", "W")
                      .entryAt("A");
            s6.stopAt(5.0).initialise();
            bool threw = false;
            try { s6.run(); } catch (const ModelError&) { threw = true; }
            check(threw, "text assigned to a numeric variable throws rather than storing 0");
        }
    }

    section("Durations as expressions");
    {
        // A parsed EXPO must give the IDENTICAL run to a constructed one.
        auto wait = [](bool useText) {
            SimulationSystem sim(31337u);
            Model& m = sim.model();
            if (useText) m.arrivals("EXPO(1.0)").station("T", 1, FIFO, "EXPO(0.8)");
            else         m.arrivals(exponential(1.0)).station("T", 1, FIFO, exponential(0.8));
            m.entryAt("T");
            sim.stopAt(500.0).initialise();
            sim.run();
            return sim.model().station("T")->stats().averageWaitingTime();
        };
        checkClose(wait(true), wait(false), 1e-12,
                   "a parsed EXPO drives the run IDENTICALLY to a constructed one");

        // A service time that depends on the entity -- impossible before v10.
        {
            SimulationSystem sim(11u);
            sim.model().attribute("size", constant(3.0))
                       .arrivals(constant(5.0))
                       .station("Cut", 1, FIFO, "size * 0.5")
                       .entryAt("Cut");
            sim.stopAt(50.0).initialise();
            sim.run();
            // Arrivals every 5, service 1.5, capacity 1 -- nothing ever
            // queues, so time at the block IS the service time.
            checkClose(sim.model().station("Cut")->stats().averageTimeInSystem(), 1.5, 1e-9,
                       "service time computed from an entity attribute");
        }

        // A Delay written as text.
        {
            SimulationSystem sim(12u);
            sim.model().arrivals(constant(2.0))
                       .delay("Move", "1.5")
                       .station("W", 1, FIFO, constant(0.1))
                       .route("Move", "W")
                       .entryAt("Move");
            sim.stopAt(20.0).initialise();
            sim.run();
            checkClose(sim.model().nodeAs<DelayNode>("Move").stats().averageTimeInSystem(),
                       1.5, 1e-9, "a Delay written as text holds for the stated time");
        }

        // An interarrival field has NO entity: an attribute there must be
        // caught before the run, not throw halfway through it.
        {
            SimulationSystem sim(1u);
            bool threw = false;
            try {
                sim.model().attribute("size", constant(1.0))
                           .arrivals("size * 2")
                           .station("X", 1, FIFO, constant(1.0))
                           .entryAt("X");
                sim.stopAt(10.0).initialise();
            } catch (const ModelError&) { threw = true; }
            check(threw, "an attribute in an interarrival field is refused at initialise()");
        }

        // An unknown name anywhere is refused before the run.
        {
            SimulationSystem sim(1u);
            bool threw = false;
            try {
                sim.model().arrivals(constant(1.0))
                           .station("X", 1, FIFO, "nosuchname * 2")
                           .entryAt("X");
                sim.stopAt(10.0).initialise();
            } catch (const ModelError&) { threw = true; }
            check(threw, "an unknown name in a service field is refused before the run");
        }

        // v9 open item 5: two blocks' draws are independent, because they are
        // separate AST nodes rather than one shared code path.
        {
            // Capacity 50 so nothing queues, and a fixed entity count so the
            // SAME number of service draws happen either way. Time at the block
            // is then exactly the service time, and the service stream is
            // untouched by what the Delay did.
            auto serviceAt = [](double delayDuration) {
                SimulationSystem sim(777u);
                sim.useSeparateStreams();
                sim.model().arrivals("EXPO(1.0)")
                           .delay("Move", constant(delayDuration))
                           .station("T", 50, FIFO, "EXPO(0.8)")
                           .route("Move", "T")
                           .entryAt("Move");
                sim.stopAfter(100).initialise();
                sim.run();
                return sim.model().station("T")->stats().averageTimeInSystem();
            };
            checkClose(serviceAt(1.0), serviceAt(2.0), 1e-12,
                       "changing a Delay's duration does not shift the service stream");
        }
    }

    section("Stability with unknowable means");
    {
        // A constant-argument distribution keeps a knowable mean, so the
        // overload refusal fires exactly as it did in v9.
        {
            SimulationSystem sim(1u);
            bool threw = false;
            try {
                sim.model().arrivals("EXPO(1.0)")
                           .station("Slow", 1, FIFO, "EXPO(2.0)")
                           .entryAt("Slow");
                sim.stopAt(10.0).initialise();
            } catch (const ModelError&) { threw = true; }
            check(threw, "rho >= 1 is still refused when the mean is knowable");
        }

        // A computed service time has NO knowable mean. The engine must say so
        // -- not pass silently, and not refuse a model it cannot judge.
        {
            SimulationSystem sim(2u);
            sim.model().variable("Rate", 2.0)
                       .arrivals("EXPO(1.0)")
                       .station("Var", 1, FIFO, "EXPO(Rate)")
                       .entryAt("Var");
            sim.stopAt(10.0).initialise();          // must NOT throw
            const Model::StabilityReport r = sim.model().stability();
            check(!r.checked, "a computed mean makes the check inconclusive");
            check(r.unverifiable.size() == 1 && r.unverifiable[0] == "Var",
                  "the report names the block it could not verify");
        }

        // A fully knowable model still reports as checked.
        {
            SimulationSystem sim(3u);
            sim.model().arrivals("EXPO(2.0)")
                       .station("Fast", 1, FIFO, "EXPO(0.5)")
                       .entryAt("Fast");
            sim.stopAt(10.0).initialise();
            const Model::StabilityReport r = sim.model().stability();
            check(r.checked, "a knowable model reports as checked");
            check(r.unverifiable.empty(), "and names nothing as unverifiable");
            checkClose(r.maxUtilisation, 0.25, 1e-9, "and reports the offered load");
        }

        check(expr("5")->meanIfKnown().has_value(), "a bare number has a known mean");
        checkClose(*expr("EXPO(0.8)")->meanIfKnown(), 0.8, 1e-12, "EXPO(0.8) mean is 0.8");
        check(!expr("NQ(X) * 2")->meanIfKnown().has_value(),
              "model state has no known mean");
    }

    section("The text path and the code path trace identically");
    {
        // This project's own argument, applied to itself: a refactor that
        // changes behaviour shows up as a DIFF, not as a slightly-off average.
        // If the text-built model and the code-built model agree EVENT FOR
        // EVENT across a model using every converted field, the text path is
        // correct. A near-miss average would not be evidence.
        // An Assign is deliberately NOT in this model. A distribution DRAWS
        // and an expression COMPUTES, so the two spellings consume different
        // amounts of randomness and could never trace alike -- that is a real
        // difference, not a defect, and Task 9 tests Assign on its own terms.
        auto buildBothWays = [](bool useText, const char* tracePath) {
            SimulationSystem sim(20260828u);
            Model& m = sim.model();
            m.attribute("priority", uniform(0.0, 4.0));
            if (useText) {
                m.arrivals("EXPO(1.0)")
                 .station("Teller", 2, FIFO, "TRIA(0.5, 1.0, 2.5)")
                 .decideWhen("Recheck", "priority > 3")
                 .delay("Walk", "0.25")
                 .station("Extra", 1, FIFO, "EXPO(0.4)");
            } else {
                m.arrivals(exponential(1.0))
                 .station("Teller", 2, FIFO, triangular(0.5, 1.0, 2.5))
                 .decideByCondition("Recheck",
                     [](const Entity& e){ return e.attribute("priority") > 3.0; })
                 .delay("Walk", constant(0.25))
                 .station("Extra", 1, FIFO, exponential(0.4));
            }
            m.route("Teller", "Recheck")
             .routeTrue("Recheck", "Walk")
             .route("Walk", "Extra")
             .entryAt("Teller");
            sim.enableTrace(tracePath, TraceLevel::Events);
            sim.stopAt(150.0).initialise();
            sim.run();
        };
        buildBothWays(true,  "trace_text.md");
        buildBothWays(false, "trace_code.md");

        std::ifstream ta("trace_text.md", std::ios::binary);
        std::ifstream tb("trace_code.md", std::ios::binary);
        check(ta.good() && tb.good(), "both traces were written");
        const std::string textTrace((std::istreambuf_iterator<char>(ta)),
                                     std::istreambuf_iterator<char>());
        const std::string codeTrace((std::istreambuf_iterator<char>(tb)),
                                     std::istreambuf_iterator<char>());
        // Compare the EVENT TABLE, not the whole file. The header prints each
        // field's description, and "EXPO(1)" against "Exponential(mean=1)" is
        // the two spellings reporting themselves honestly -- a real difference
        // between what was typed, not a difference in what was simulated. The
        // events are where the claim lives.
        auto eventsOf = [](const std::string& trace) {
            const std::size_t at = trace.find("| t | event |");
            return at == std::string::npos ? std::string() : trace.substr(at);
        };
        const std::string textEvents = eventsOf(textTrace);
        const std::string codeEvents = eventsOf(codeTrace);

        check(!textEvents.empty(), "the trace contains an event table");
        check(textEvents.size() > 5000, "the event table is substantial, not a stub");
        check(textEvents == codeEvents,
              "the text-built model traces IDENTICALLY to the code-built one, event for event");
    }
}
