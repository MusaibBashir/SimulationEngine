// ============================================================================
// Regression.hpp  --  v12: many models, run and diffed
// ============================================================================
// The standard is BYTE-IDENTICAL report text, not a mean inside a tolerance.
// A tolerance passes a real regression that happens to land inside the band,
// which is the "near-miss average" standard this project rejected in v10.
//
// Expectations are `.expected` rather than `.txt` on purpose: .gitignore
// excludes *.txt with one hand-maintained negation, and a silently ignored
// expectation file is a gate covering less than it appears to.

#pragma once
#include <string>
#include <vector>

namespace des {

struct RegressionOutcome {
    std::string model;        // path as the manifest gave it
    std::string detail;       // the first differing line, or why it could not run
    bool matched{false};
    bool hadExpectation{false};
};

struct RegressionReport {
    int cases{0};
    int failures{0};
    int missing{0};           // no stored expectation
    int captured{0};
    int refused{0};           // capture declined because one already exists
    std::vector<RegressionOutcome> outcomes;

    // A harness that ran nothing has not passed. It has not run.
    bool ok() const { return cases > 0 && failures == 0 && missing == 0; }
};

// dir holds `manifest` (one model path per line, # comments allowed) and one
// <stem>.expected per model. capture writes expectations instead of comparing;
// force allows overwriting one that already exists.
RegressionReport runRegression(const std::string& dir, bool capture, bool force);

}  // namespace des
