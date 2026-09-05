#include "Parser.hpp"
#include <utility>
#include "Functions.hpp"
#include "Lexer.hpp"
#include "ModelError.hpp"

namespace des {
namespace {

class Parser {
private:
    std::vector<Token>       m_tokens;
    std::size_t              m_at{0};
    std::vector<Diagnostic>* m_diagnostics;

    const Token& peek() const  { return m_tokens[m_at]; }
    bool at(TokenKind k) const { return peek().kind == k; }

    Token advance() {
        Token t = m_tokens[m_at];
        if (m_at + 1 < m_tokens.size()) ++m_at;
        return t;
    }

    bool match(TokenKind k) {
        if (!at(k)) return false;
        advance();
        return true;
    }

    void error(const std::string& message) { error(message, peek().span); }

    void error(const std::string& message, SourceSpan span) {
        // One diagnostic per position: a recovery loop can otherwise emit the
        // same complaint repeatedly and bury the first cause.
        if (!m_diagnostics->empty() && m_diagnostics->back().span.offset == span.offset) return;
        m_diagnostics->push_back(Diagnostic{Severity::Error, span, message});
    }

    // A stand-in so the tree stays well-formed and parsing can continue. Never
    // evaluated: hasErrors() gates that.
    ExpressionPtr recover() {
        return std::make_unique<LiteralExpression>(Value(0.0), peek().span);
    }

    ExpressionPtr parseOr() {
        ExpressionPtr left = parseAnd();
        while (at(TokenKind::OrOr)) {
            const SourceSpan span = advance().span;
            left = std::make_unique<BinaryExpression>(BinaryOp::Or, std::move(left),
                                                      parseAnd(), span);
        }
        return left;
    }

    ExpressionPtr parseAnd() {
        ExpressionPtr left = parseComparison();
        while (at(TokenKind::AndAnd)) {
            const SourceSpan span = advance().span;
            left = std::make_unique<BinaryExpression>(BinaryOp::And, std::move(left),
                                                      parseComparison(), span);
        }
        return left;
    }

    ExpressionPtr parseComparison() {
        ExpressionPtr left = parseAdditive();
        for (;;) {
            BinaryOp op;
            switch (peek().kind) {
                case TokenKind::Less:         op = BinaryOp::Less;         break;
                case TokenKind::LessEqual:    op = BinaryOp::LessEqual;    break;
                case TokenKind::Greater:      op = BinaryOp::Greater;      break;
                case TokenKind::GreaterEqual: op = BinaryOp::GreaterEqual; break;
                case TokenKind::EqualEqual:   op = BinaryOp::Equal;        break;
                case TokenKind::BangEqual:    op = BinaryOp::NotEqual;     break;
                default: return left;
            }
            const SourceSpan span = advance().span;
            left = std::make_unique<BinaryExpression>(op, std::move(left), parseAdditive(), span);
        }
    }

    ExpressionPtr parseAdditive() {
        ExpressionPtr left = parseMultiplicative();
        for (;;) {
            BinaryOp op;
            if      (at(TokenKind::Plus))  op = BinaryOp::Add;
            else if (at(TokenKind::Minus)) op = BinaryOp::Subtract;
            else return left;
            const SourceSpan span = advance().span;
            left = std::make_unique<BinaryExpression>(op, std::move(left),
                                                      parseMultiplicative(), span);
        }
    }

    ExpressionPtr parseMultiplicative() {
        ExpressionPtr left = parseUnary();
        for (;;) {
            BinaryOp op;
            if      (at(TokenKind::Star))    op = BinaryOp::Multiply;
            else if (at(TokenKind::Slash))   op = BinaryOp::Divide;
            else if (at(TokenKind::Percent)) op = BinaryOp::Modulo;
            else return left;
            const SourceSpan span = advance().span;
            left = std::make_unique<BinaryExpression>(op, std::move(left), parseUnary(), span);
        }
    }

    // Unary binds LOOSER than ^, so -2^2 is -(2^2) = -4, as in every maths
    // convention and in Arena. Getting this backwards is the classic
    // precedence-climb bug.
    ExpressionPtr parseUnary() {
        if (at(TokenKind::Minus) || at(TokenKind::Bang)) {
            const bool negate = at(TokenKind::Minus);
            const SourceSpan span = advance().span;
            ExpressionPtr operand = parseUnary();
            // FOLD -literal into a literal. The lexer only starts a number on a
            // digit, so -1 would otherwise be a UnaryExpression, and every
            // "are all my arguments constant?" test would say no -- silently
            // turning UNIF(-1, 3) into an expression with no knowable mean.
            if (negate) {
                if (const auto* lit = dynamic_cast<const LiteralExpression*>(operand.get()))
                    if (isNumber(lit->value()))
                        return std::make_unique<LiteralExpression>(
                            Value(-asNumber(lit->value())), span);
            }
            return std::make_unique<UnaryExpression>(
                negate ? UnaryOp::Negate : UnaryOp::Not, std::move(operand), span);
        }
        return parsePower();
    }

    ExpressionPtr parsePower() {
        ExpressionPtr base = parsePrimary();
        if (at(TokenKind::Caret)) {
            const SourceSpan span = advance().span;
            // Right-associative: recurse into parseUnary, not parsePower.
            return std::make_unique<BinaryExpression>(BinaryOp::Power, std::move(base),
                                                      parseUnary(), span);
        }
        return base;
    }

    ExpressionPtr parsePrimary() {
        if (at(TokenKind::Number)) {
            const Token t = advance();
            return std::make_unique<LiteralExpression>(Value(t.number), t.span);
        }
        if (at(TokenKind::Text)) {
            const Token t = advance();
            return std::make_unique<LiteralExpression>(Value(t.text), t.span);
        }
        if (at(TokenKind::LParen)) {
            advance();
            ExpressionPtr inner = parseOr();
            if (!match(TokenKind::RParen)) error("expected ')'");
            return inner;
        }
        if (at(TokenKind::Identifier)) {
            const Token name = advance();
            if (at(TokenKind::LParen)) return parseCall(name);
            return std::make_unique<NameExpression>(name.text, name.span);
        }

        error(std::string("expected a value, got ") + describe(peek().kind));
        if (!at(TokenKind::End)) advance();   // skip it so we make progress
        return recover();
    }

    ExpressionPtr parseCall(const Token& name) {
        advance();                                     // consume '('
        std::vector<ExpressionPtr> args;
        std::vector<SourceSpan>    argSpans;
        if (!at(TokenKind::RParen)) {
            do {
                argSpans.push_back(peek().span);
                args.push_back(parseOr());
            } while (match(TokenKind::Comma));
        }
        if (!match(TokenKind::RParen)) error("expected ')'");
        return buildCall(name.text, std::move(args), argSpans, name.span, *m_diagnostics);
    }

public:
    Parser(std::vector<Token> tokens, std::vector<Diagnostic>* diagnostics)
        : m_tokens(std::move(tokens)), m_diagnostics(diagnostics) {}

    ExpressionPtr run() {
        ExpressionPtr e = parseOr();
        if (!at(TokenKind::End))
            error(std::string("unexpected ") + describe(peek().kind) +
                  " after the end of the expression");
        return e;
    }
};

}  // namespace

ParseResult parseExpression(const std::string& source) {
    ParseResult result;
    LexResult lexed = tokenise(source);
    result.diagnostics = std::move(lexed.diagnostics);
    Parser parser(std::move(lexed.tokens), &result.diagnostics);
    result.expr = parser.run();
    return result;
}

ExpressionPtr expr(const std::string& source) {
    ParseResult r = parseExpression(source);
    if (hasErrors(r.diagnostics))
        throw ModelError("cannot parse expression \"" + source + "\":\n" +
                         formatDiagnostics(r.diagnostics));
    return std::move(r.expr);
}

}  // namespace des
