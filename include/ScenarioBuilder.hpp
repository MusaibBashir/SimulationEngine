// ============================================================================
// ScenarioBuilder.hpp  --  v17: a Scenario becomes a model document
// ============================================================================
// Pure code, no language model anywhere near it. Everything the LLM is bad at
// -- Arena's column names, distribution syntax, wiring every Next, giving the
// flow somewhere to end -- happens here, where a test can watch it. So a model
// that came from a sentence is valid by construction, and the only thing left
// to doubt is whether the sentence was heard right.
//
// EVERY DEFAULT IS SAID OUT LOUD. One server when none was mentioned, a day's
// run when no length was given, a spread for "about four minutes": each goes
// into `assumptions`, because a number the person never said and never sees is
// a number they will trust by accident.

#pragma once
#include <string>
#include <vector>
#include "ModelDocument.hpp"
#include "Scenario.hpp"

namespace des {

struct BuiltScenario {
    ModelDocument            document;
    std::vector<std::string> assumptions;
};

// Takes a Scenario whose read produced no `invalid` issues: every name it
// refers to resolves. Missing facts are fine -- a step with no time builds
// with its Service blank, and the compiler then points at that cell.
BuiltScenario buildScenario(const Scenario& s);

// "check in desk" -> "CheckInDesk". A Name is an identifier in expressions, so
// it cannot hold a space; the person's words survive as capitals.
std::string blockNameFor(const std::string& words);

// The expression for a duration, in the model's time unit: EXPO(2), UNIF(1, 3).
std::string expressionFor(const TimeSpec& t, TimeUnit modelUnit,
                          std::vector<std::string>* assumptions = nullptr,
                          const std::string& what = "");

}  // namespace des
