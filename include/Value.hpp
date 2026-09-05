// ============================================================================
// Value.hpp  --  v10: what an expression evaluates to
// ============================================================================
// A variant of double and string, and the string half exists for exactly ONE
// reason: Entity.Type == "Ball". Nothing else in the language needs text.
//
// BOOLEANS ARE DOUBLES, 0 and 1, the way Arena treats them. That is not
// laziness -- it is what makes a condition field and a value field the same
// kind of expression, which is the whole point of having one grammar. A
// separate bool type would fork every operator.
//
// There is NO coercion between the two halves. A string in a numeric position
// throws rather than reading as zero, because "a default return value is a
// place for a bug to hide" and a silent 0 in a duration field is a service time
// of zero that nobody notices.

#pragma once
#include <string>
#include <variant>
#include "ModelError.hpp"

namespace des {

using Value = std::variant<double, std::string>;

// Thrown when an expression cannot do its job AT RUN TIME -- a string in
// arithmetic, division by zero, an attribute read with no entity. Derives from
// ModelError because by then there is no cell to point at, and continuing would
// produce confident nonsense. Compare Diagnostic, which is for user errors
// caught BEFORE a run.
class ExpressionError : public ModelError {
public:
    explicit ExpressionError(const std::string& what) : ModelError(what) {}
};

inline bool isNumber(const Value& v) { return std::holds_alternative<double>(v); }
inline bool isText(const Value& v)   { return std::holds_alternative<std::string>(v); }

double             asNumber(const Value& v);
const std::string& asText(const Value& v);
bool               truthy(const Value& v);
std::string        formatValue(const Value& v);

}  // namespace des
