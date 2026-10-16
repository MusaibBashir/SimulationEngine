#include "Json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace des {

JsonValue JsonValue::boolean(bool b) { JsonValue v; v.m_type = Type::Bool;   v.m_bool = b;              return v; }
JsonValue JsonValue::number(double d){ JsonValue v; v.m_type = Type::Number; v.m_number = d;            return v; }
JsonValue JsonValue::string(std::string s) { JsonValue v; v.m_type = Type::String; v.m_string = std::move(s); return v; }
JsonValue JsonValue::array()  { JsonValue v; v.m_type = Type::Array;  return v; }
JsonValue JsonValue::object() { JsonValue v; v.m_type = Type::Object; return v; }

bool JsonValue::has(const std::string& key) const {
    for (const auto& m : m_members) if (m.first == key) return true;
    return false;
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    static const JsonValue none;
    for (const auto& m : m_members) if (m.first == key) return m.second;
    return none;
}

void JsonValue::set(const std::string& key, JsonValue v) {
    for (auto& m : m_members)
        if (m.first == key) { m.second = std::move(v); return; }
    m_members.emplace_back(key, std::move(v));
}

namespace {

class Reader {
public:
    explicit Reader(const std::string& t) : m_t(t) {}

    JsonParse run() {
        JsonParse out;
        skip();
        if (!value(out.value, 0)) { out.error = m_error; return out; }
        skip();
        if (m_at != m_t.size()) { fail("unexpected text after the value"); out.error = m_error; return out; }
        out.ok = true;
        return out;
    }

private:
    const std::string& m_t;
    std::size_t m_at{0};
    std::string m_error;

    // Depth-limited: a reply is untrusted input, and a few thousand '[' would
    // otherwise take the stack with it.
    static constexpr int kMaxDepth = 64;

    bool fail(const std::string& what) {
        if (m_error.empty()) {
            std::size_t line = 1;
            for (std::size_t i = 0; i < m_at && i < m_t.size(); ++i) if (m_t[i] == '\n') ++line;
            m_error = "line " + std::to_string(line) + ": " + what;
        }
        return false;
    }
    void skip() {
        while (m_at < m_t.size() && (m_t[m_at] == ' ' || m_t[m_at] == '\t' ||
                                     m_t[m_at] == '\n' || m_t[m_at] == '\r')) ++m_at;
    }
    bool literal(const char* word) {
        std::size_t i = 0;
        for (; word[i] != '\0'; ++i)
            if (m_at + i >= m_t.size() || m_t[m_at + i] != word[i]) return false;
        m_at += i;
        return true;
    }

    bool value(JsonValue& out, int depth) {
        if (depth > kMaxDepth) return fail("nested too deeply");
        if (m_at >= m_t.size()) return fail("expected a value");
        const char c = m_t[m_at];
        if (c == '{') return object(out, depth);
        if (c == '[') return array(out, depth);
        if (c == '"') { std::string s; if (!string(s)) return false; out = JsonValue::string(std::move(s)); return true; }
        if (literal("true"))  { out = JsonValue::boolean(true);  return true; }
        if (literal("false")) { out = JsonValue::boolean(false); return true; }
        if (literal("null"))  { out = JsonValue(); return true; }
        if (c == '-' || (c >= '0' && c <= '9')) return number(out);
        return fail(std::string("unexpected '") + c + "'");
    }

    bool number(JsonValue& out) {
        const char* begin = m_t.c_str() + m_at;
        char* end = nullptr;
        const double d = std::strtod(begin, &end);
        if (end == begin || !std::isfinite(d)) return fail("bad number");
        m_at += static_cast<std::size_t>(end - begin);
        out = JsonValue::number(d);
        return true;
    }

    static void appendUtf8(std::string& s, unsigned cp) {
        if (cp < 0x80) s += static_cast<char>(cp);
        else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
               s += static_cast<char>(0x80 | (cp & 0x3F)); }
    }

    bool string(std::string& out) {
        ++m_at;   // the opening quote
        while (m_at < m_t.size()) {
            const char c = m_t[m_at++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (m_at >= m_t.size()) break;
            const char e = m_t[m_at++];
            switch (e) {
                case '"': out += '"'; break;   case '\\': out += '\\'; break;
                case '/': out += '/'; break;   case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;  case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;  case 't': out += '\t'; break;
                case 'u': {
                    if (m_at + 4 > m_t.size()) return fail("bad \\u escape");
                    unsigned cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char h = m_t[m_at++];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                        else return fail("bad \\u escape");
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }

    bool array(JsonValue& out, int depth) {
        ++m_at;
        out = JsonValue::array();
        skip();
        if (m_at < m_t.size() && m_t[m_at] == ']') { ++m_at; return true; }
        for (;;) {
            JsonValue item;
            skip();
            if (!value(item, depth + 1)) return false;
            out.push(std::move(item));
            skip();
            if (m_at < m_t.size() && m_t[m_at] == ',') { ++m_at; continue; }
            if (m_at < m_t.size() && m_t[m_at] == ']') { ++m_at; return true; }
            return fail("expected ',' or ']'");
        }
    }

    bool object(JsonValue& out, int depth) {
        ++m_at;
        out = JsonValue::object();
        skip();
        if (m_at < m_t.size() && m_t[m_at] == '}') { ++m_at; return true; }
        for (;;) {
            skip();
            if (m_at >= m_t.size() || m_t[m_at] != '"') return fail("expected a key");
            std::string key;
            if (!string(key)) return false;
            skip();
            if (m_at >= m_t.size() || m_t[m_at] != ':') return fail("expected ':'");
            ++m_at;
            skip();
            JsonValue v;
            if (!value(v, depth + 1)) return false;
            out.set(key, std::move(v));
            skip();
            if (m_at < m_t.size() && m_t[m_at] == ',') { ++m_at; continue; }
            if (m_at < m_t.size() && m_t[m_at] == '}') { ++m_at; return true; }
            return fail("expected ',' or '}'");
        }
    }
};

void writeString(const std::string& s, std::string& out) {
    out += '"';
    for (const char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    out += '"';
}

void write(const JsonValue& v, std::string& out, int indent, int depth) {
    auto newline = [&](int d) {
        if (indent <= 0) return;
        out += '\n';
        out.append(static_cast<std::size_t>(indent * d), ' ');
    };
    switch (v.type()) {
        case JsonValue::Type::Null:   out += "null"; break;
        case JsonValue::Type::Bool:   out += v.asBool() ? "true" : "false"; break;
        case JsonValue::Type::Number: {
            const double d = v.asNumber();
            char buf[32];
            if (d == std::floor(d) && std::fabs(d) < 1e15) {
                std::snprintf(buf, sizeof buf, "%.0f", d);
            } else {
                // The shortest precision that reads back as the same double.
                for (int p = 6; p <= 17; ++p) {
                    std::snprintf(buf, sizeof buf, "%.*g", p, d);
                    if (std::strtod(buf, nullptr) == d) break;
                }
            }
            out += buf;
            break;
        }
        case JsonValue::Type::String: writeString(v.asString(), out); break;
        case JsonValue::Type::Array: {
            out += '[';
            bool first = true;
            for (const JsonValue& item : v.items()) {
                if (!first) out += ',';
                first = false;
                newline(depth + 1);
                write(item, out, indent, depth + 1);
            }
            if (!v.items().empty()) newline(depth);
            out += ']';
            break;
        }
        case JsonValue::Type::Object: {
            out += '{';
            bool first = true;
            for (const auto& m : v.members()) {
                if (!first) out += ',';
                first = false;
                newline(depth + 1);
                writeString(m.first, out);
                out += indent > 0 ? ": " : ":";
                write(m.second, out, indent, depth + 1);
            }
            if (!v.members().empty()) newline(depth);
            out += '}';
            break;
        }
    }
}

}  // namespace

JsonParse parseJson(const std::string& text) { return Reader(text).run(); }

JsonParse parseJsonObjectIn(const std::string& text) {
    // Fast path: the whole reply is the object, as asked.
    JsonParse whole = parseJson(text);
    if (whole.ok && whole.value.isObject()) return whole;

    // Otherwise the first '{' whose matching '}' closes a parseable object.
    // Brace matching skips string contents, so a '}' inside a name is not one.
    for (std::size_t start = text.find('{'); start != std::string::npos;
         start = text.find('{', start + 1)) {
        int depth = 0;
        bool inString = false;
        for (std::size_t i = start; i < text.size(); ++i) {
            const char c = text[i];
            if (inString) {
                if (c == '\\') ++i;
                else if (c == '"') inString = false;
                continue;
            }
            if (c == '"') inString = true;
            else if (c == '{') ++depth;
            else if (c == '}' && --depth == 0) {
                JsonParse p = parseJson(text.substr(start, i - start + 1));
                if (p.ok) return p;
                break;
            }
        }
    }
    JsonParse none;
    none.error = whole.error.empty() ? "no JSON object in the reply" : whole.error;
    return none;
}

std::string writeJson(const JsonValue& v, int indent) {
    std::string out;
    write(v, out, indent, 0);
    return out;
}

}  // namespace des
