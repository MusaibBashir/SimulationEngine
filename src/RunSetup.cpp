#include "RunSetup.hpp"

#include <cstdlib>
#include "Diagnostic.hpp"
#include "ModelDocument.hpp"

namespace des {
namespace {

void complain(std::vector<Diagnostic>& out, Severity severity, std::size_t row,
              const std::string& column, const std::string& message) {
    out.push_back(Diagnostic{severity, SourceSpan{}, message,
                             CellRef{"Run", row, column}});
}

// True and the parsed value, or false when the text is not a number. strtod's
// end pointer is the whole check: "480x" must not read as 480.
bool asReal(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

}  // namespace

RunSetup readRunSetup(const ModelDocument& doc, std::vector<Diagnostic>& out) {
    RunSetup setup;
    const std::size_t rows = doc.rowCount("Run");
    if (rows == 0) return setup;

    for (std::size_t r = 1; r < rows; ++r)
        complain(out, Severity::Error, r, "Name",
                 "a model has at most one [Run]; this one is extra");

    const auto text = [&](const char* column) {
        return doc.cellOrDefault("Run", 0, column);
    };
    const auto number = [&](const char* column, double& into) {
        const std::string s = text(column);
        if (s.empty()) return false;
        if (!asReal(s, into)) {
            complain(out, Severity::Error, 0, column, "'" + s + "' is not a number");
            return false;
        }
        return true;
    };
    const auto flag = [&](const char* column) {
        const std::string s = text(column);
        if (s == "true")  return true;
        if (s == "false" || s.empty()) return false;
        complain(out, Severity::Error, 0, column, "'" + s + "' must be true or false");
        return false;
    };

    double v = 0.0;
    if (number("Length", v))       setup.length = v;
    if (number("Warm-up", v))      setup.warmUp = v;
    if (number("Replications", v)) setup.replications = static_cast<int>(v);
    if (number("Base Seed", v))    setup.baseSeed = static_cast<unsigned>(v);
    if (number("Max Entities", v)) setup.maxEntities = static_cast<int>(v);
    setup.stopWhenDrained = flag("Stop When Drained");
    setup.separateStreams = flag("Separate Streams");
    setup.antithetic      = flag("Antithetic");

    if (setup.replications < 1)
        complain(out, Severity::Error, 0, "Replications", "must be at least 1");

    // A WARNING, not a refusal. It looks like a model that runs forever, and
    // for a Create with Max Arrivals set it is not: the future event list
    // empties and the run ends on its own. Refusing it would reject a correct
    // model; saying nothing would leave an author waiting.
    if (!setup.length && !setup.maxEntities && !setup.stopWhenDrained)
        complain(out, Severity::Warning, 0, "Length",
                 "no stopping condition; this run ends only when the model "
                 "runs out of events");

    return setup;
}

}  // namespace des
