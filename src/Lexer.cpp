#include "Lexer.hpp"
#include <cctype>
#include <cstdlib>

namespace des {
namespace {

bool isIdentifierStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

// The dot lives HERE and nowhere else in the grammar. See the header.
bool isIdentifierPart(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
}

}  // namespace

const char* describe(TokenKind kind) {
    switch (kind) {
        case TokenKind::Number:       return "a number";
        case TokenKind::Text:         return "a quoted string";
        case TokenKind::Identifier:   return "a name";
        case TokenKind::Plus:         return "'+'";
        case TokenKind::Minus:        return "'-'";
        case TokenKind::Star:         return "'*'";
        case TokenKind::Slash:        return "'/'";
        case TokenKind::Percent:      return "'%'";
        case TokenKind::Caret:        return "'^'";
        case TokenKind::Bang:         return "'!'";
        case TokenKind::Less:         return "'<'";
        case TokenKind::LessEqual:    return "'<='";
        case TokenKind::Greater:      return "'>'";
        case TokenKind::GreaterEqual: return "'>='";
        case TokenKind::EqualEqual:   return "'=='";
        case TokenKind::BangEqual:    return "'!='";
        case TokenKind::AndAnd:       return "'&&'";
        case TokenKind::OrOr:         return "'||'";
        case TokenKind::LParen:       return "'('";
        case TokenKind::RParen:       return "')'";
        case TokenKind::Comma:        return "','";
        case TokenKind::End:          return "the end of the expression";
        case TokenKind::Invalid:      return "an unrecognised character";
    }
    return "something unexpected";
}

LexResult tokenise(const std::string& source) {
    LexResult result;
    std::size_t i = 0;
    const std::size_t n = source.size();

    auto push = [&result](TokenKind kind, std::size_t start, std::size_t length) {
        Token t;
        t.kind = kind;
        t.span = SourceSpan{start, length};
        result.tokens.push_back(std::move(t));
    };

    while (i < n) {
        const char c = source[i];

        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }

        // --- numbers -------------------------------------------------------
        // A leading dot is only a number when a digit follows it. Otherwise it
        // belongs to an identifier, which is why this test looks ahead.
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(source[i + 1])))) {
            const std::size_t start = i;
            const char* begin = source.c_str() + i;
            char* end = nullptr;
            const double value = std::strtod(begin, &end);
            i += static_cast<std::size_t>(end - begin);
            Token t;
            t.kind = TokenKind::Number;
            t.span = SourceSpan{start, i - start};
            t.number = value;
            result.tokens.push_back(std::move(t));
            continue;
        }

        // --- identifiers (dots included) -----------------------------------
        if (isIdentifierStart(c)) {
            const std::size_t start = i;
            while (i < n && isIdentifierPart(source[i])) ++i;
            Token t;
            t.kind = TokenKind::Identifier;
            t.span = SourceSpan{start, i - start};
            t.text = source.substr(start, i - start);
            result.tokens.push_back(std::move(t));
            continue;
        }

        // --- quoted text ---------------------------------------------------
        if (c == '"') {
            const std::size_t start = i;
            ++i;
            std::string text;
            bool closed = false;
            while (i < n) {
                if (source[i] == '"') { closed = true; ++i; break; }
                text.push_back(source[i]);
                ++i;
            }
            if (!closed) {
                result.diagnostics.push_back(
                    Diagnostic{Severity::Error, SourceSpan{start, i - start},
                               "unterminated string: no closing '\"'"});
            }
            // The token is produced either way. A half-typed string is exactly
            // what a spreadsheet cell holds mid-edit, and returning nothing
            // would make the parser report a second, misleading error on top.
            Token t;
            t.kind = TokenKind::Text;
            t.span = SourceSpan{start, i - start};
            t.text = std::move(text);
            result.tokens.push_back(std::move(t));
            continue;
        }

        // --- two-character operators, checked BEFORE the one-character ones -
        // Order matters: '>' would otherwise swallow the '>' of ">=".
        if (i + 1 < n) {
            const char d = source[i + 1];
            TokenKind two = TokenKind::Invalid;
            if      (c == '<' && d == '=') two = TokenKind::LessEqual;
            else if (c == '>' && d == '=') two = TokenKind::GreaterEqual;
            else if (c == '=' && d == '=') two = TokenKind::EqualEqual;
            else if (c == '!' && d == '=') two = TokenKind::BangEqual;
            else if (c == '&' && d == '&') two = TokenKind::AndAnd;
            else if (c == '|' && d == '|') two = TokenKind::OrOr;
            if (two != TokenKind::Invalid) { push(two, i, 2); i += 2; continue; }
        }

        // --- one-character operators ---------------------------------------
        TokenKind one = TokenKind::Invalid;
        switch (c) {
            case '+': one = TokenKind::Plus;    break;
            case '-': one = TokenKind::Minus;   break;
            case '*': one = TokenKind::Star;    break;
            case '/': one = TokenKind::Slash;   break;
            case '%': one = TokenKind::Percent; break;
            case '^': one = TokenKind::Caret;   break;
            case '!': one = TokenKind::Bang;    break;
            case '<': one = TokenKind::Less;    break;
            case '>': one = TokenKind::Greater; break;
            case '(': one = TokenKind::LParen;  break;
            case ')': one = TokenKind::RParen;  break;
            case ',': one = TokenKind::Comma;   break;
            default: break;
        }
        if (one != TokenKind::Invalid) { push(one, i, 1); ++i; continue; }

        // A lone '=' gets its own message. It is the commonest mistake anyone
        // makes writing a condition, and "unexpected character" would say
        // nothing about what to do instead.
        if (c == '=') {
            result.diagnostics.push_back(
                Diagnostic{Severity::Error, SourceSpan{i, 1},
                           "use '==' to compare; a single '=' is not an operator here"});
            ++i;
            continue;
        }

        // Unknown: report and SKIP, so one call reports every bad character.
        result.diagnostics.push_back(
            Diagnostic{Severity::Error, SourceSpan{i, 1},
                       std::string("unexpected character '") + c + "'"});
        ++i;
    }

    push(TokenKind::End, n, 0);
    return result;
}

}  // namespace des
