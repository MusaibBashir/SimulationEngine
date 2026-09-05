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

}  // namespace des
