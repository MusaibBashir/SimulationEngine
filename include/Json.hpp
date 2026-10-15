// ============================================================================
// Json.hpp  --  v17: just enough JSON to talk to a language model
// ============================================================================
// A value, a reader and a writer, and nothing else. The model file is not JSON
// and never will be (DocumentFormat.hpp says why); this exists for the two
// places JSON is the only option -- the body of an HTTP request to an LLM, and
// the scenario the LLM answers with.
//
// NO LIBRARY. The package promises nothing to install, and every JSON library
// worth using is larger than everything here. What a 300-line reader cannot do
// -- \u surrogate pairs beyond the BMP, numbers past double precision -- an
// LLM's answer about queues does not need.

#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace des {

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() = default;                       // null
    static JsonValue boolean(bool b);
    static JsonValue number(double d);
    static JsonValue string(std::string s);
    static JsonValue array();
    static JsonValue object();

    Type type() const { return m_type; }
    bool isNull()   const { return m_type == Type::Null; }
    bool isBool()   const { return m_type == Type::Bool; }
    bool isNumber() const { return m_type == Type::Number; }
    bool isString() const { return m_type == Type::String; }
    bool isArray()  const { return m_type == Type::Array; }
    bool isObject() const { return m_type == Type::Object; }

    bool               asBool()   const { return m_bool; }
    double             asNumber() const { return m_number; }
    const std::string& asString() const { return m_string; }

    // Arrays.
    const std::vector<JsonValue>& items() const { return m_items; }
    void push(JsonValue v) { m_items.push_back(std::move(v)); }

    // Objects. Keys keep their insertion order, so a request body is written
    // in the order it was built and a test can compare it as text.
    const std::vector<std::pair<std::string, JsonValue>>& members() const { return m_members; }
    bool             has(const std::string& key) const;
    // The member, or a shared null when it is absent -- so a chain of lookups
    // into an answer that left something out reads as null, not as a crash.
    const JsonValue& operator[](const std::string& key) const;
    void             set(const std::string& key, JsonValue v);

private:
    Type        m_type{Type::Null};
    bool        m_bool{false};
    double      m_number{0.0};
    std::string m_string;
    std::vector<JsonValue> m_items;
    std::vector<std::pair<std::string, JsonValue>> m_members;
};

struct JsonParse {
    JsonValue   value;
    bool        ok{false};
    std::string error;        // "line 3: expected ':'" when !ok
};

// Never throws. Leading and trailing whitespace is allowed; anything else after
// the value is an error.
JsonParse parseJson(const std::string& text);

// The first {...} object in `text`, parsed. Small models wrap their JSON in a
// sentence or a ``` fence even when told not to, and refusing a correct answer
// over its packaging would make every call cost two.
JsonParse parseJsonObjectIn(const std::string& text);

// Keys in insertion order, numbers as short as round-trips. Compact by
// default; `indent` spaces per level for a file a person will open.
std::string writeJson(const JsonValue& v, int indent = 0);

}  // namespace des
