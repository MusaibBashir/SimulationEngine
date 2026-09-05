#include "Value.hpp"
#include <cmath>
#include <sstream>
#include "Diagnostic.hpp"

namespace des {

double asNumber(const Value& v) {
    if (const double* d = std::get_if<double>(&v)) return *d;
    throw ExpressionError("expected a number, got the text '" + std::get<std::string>(v) + "'");
}

const std::string& asText(const Value& v) {
    if (const std::string* s = std::get_if<std::string>(&v)) return *s;
    throw ExpressionError("expected text, got a number");
}

bool truthy(const Value& v) { return asNumber(v) != 0.0; }

std::string formatValue(const Value& v) {
    if (const std::string* s = std::get_if<std::string>(&v)) return *s;
    const double d = std::get<double>(v);
    std::ostringstream out;
    // A whole number prints as "2", not "2.000000" -- these strings end up in
    // trace lines and error messages that get read by a person.
    if (d == std::floor(d) && std::fabs(d) < 1e15) out << static_cast<long long>(d);
    else                                           out << d;
    return out.str();
}

bool hasErrors(const std::vector<Diagnostic>& diagnostics) {
    for (const Diagnostic& d : diagnostics)
        if (d.severity == Severity::Error) return true;
    return false;
}

std::string formatDiagnostics(const std::vector<Diagnostic>& diagnostics) {
    std::ostringstream out;
    for (const Diagnostic& d : diagnostics) {
        out << "col " << (d.span.offset + 1) << ": " << d.message << "\n";
    }
    return out.str();
}

}  // namespace des
