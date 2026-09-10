// ============================================================================
// RunSetup.hpp  --  v12: how to run a model, as a struct
// ============================================================================
// v11 left a model file carrying no run length, so `des run` had to take one on
// the command line and a regression harness would have had to store one beside
// each model. An expectation whose meaning depends on a number kept somewhere
// else stops being an expectation the moment that number changes.
//
// The [Run] module that fills this in from a document arrives with
// readRunSetup(); the struct comes first because RunController is written
// against it.

#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Common.hpp"

namespace des {

class ModelDocument;
struct Diagnostic;

struct RunSetup {
    std::optional<SimTime> length;          // nothing = no time limit
    SimTime                warmUp{0.0};
    int                    replications{1};
    unsigned               baseSeed{12345u};
    bool                   stopWhenDrained{false};
    std::optional<int>     maxEntities;
    bool                   separateStreams{false};
    bool                   antithetic{false};

    // NOT a column on [Run]. Welch's warm-up grid is an experiment-level
    // instrument rather than a property of the model, and Experiment sets it
    // directly. Exposing it in the schema would put a diagnostic tool in the
    // model file next to the model.
    SimTime observeInterval{0.0};
};

// Reads a [Run] row -- Arena's Run Setup, in the document where the model is.
// A document with no [Run] is not an error: it means the defaults. Never
// throws.
//
// v15 allows MANY. Arena keeps one Run Setup per model and makes you edit it
// to try a longer horizon or more replications; a file can hold several named
// ones, and choosing between them beats editing the same numbers back and
// forth and losing what they were. `which` is a position, and out of range
// reads as the defaults rather than as an error, because a caller holding a
// stale index is not a broken model.
RunSetup readRunSetup(const ModelDocument& doc, std::vector<Diagnostic>& out,
                      std::size_t which = 0);

// The Name of each [Run] row, in document order, with a stand-in for any that
// has none -- a row without a name is still a row you may want to run.
std::vector<std::string> runNames(const ModelDocument& doc);

}  // namespace des
