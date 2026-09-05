// ============================================================================
// RunController.hpp  --  v12: a run a caller drives
// ============================================================================
// SimulationSystem::run() blocks until the stopping rule is met, which is the
// right shape for a program and the wrong one for a front end: there is no
// moment at which it can redraw, and no way to stop.
//
// v12 turns that inside out. stepOnce() does one event; this owns the loop
// around it, plus the replication loop above that. A caller advances a budget
// of events, redraws, and advances again -- so pausing is not calling, and
// cancelling is not calling ever again.
//
// NOTHING HERE IS ON THE INNER LOOP. A snapshot is built when it is asked for,
// so a run is bit-for-bit identical whether or not anyone was watching.

#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common.hpp"
#include "Experiment.hpp"
#include "RunSetup.hpp"

namespace des {

class ModelDocument;
struct Diagnostic;
class SimulationSystem;

enum class RunState { Ready, Running, Paused, Finished, Cancelled, Failed };

std::string describe(RunState state);

struct RunProgress {
    RunState  state{RunState::Ready};
    int       replication{0};        // 1-based; 0 before the first advance()
    int       replications{1};
    SimTime   now{0.0};
    long long eventsProcessed{0};    // across the whole study

    // Through the CURRENT RUN, or nothing when no rule can tell. With
    // antithetic pairing a replication is two runs, and the second half
    // restarts this at zero -- so a front end showing a per-replication bar
    // must say which half it is on, or the bar appears to go backwards.
    std::optional<double> fraction;
};

struct BlockSnapshot {
    std::string name;
    double      queueLength{0.0};
    long long   served{0};
    double      utilisation{0.0};
};

struct ResourceSnapshot {
    std::string name;
    double      busy{0.0};
    double      capacity{0.0};
};

struct VariableSnapshot {
    std::string name;
    double      value{0.0};
};

struct RunSnapshot {
    SimTime   now{0.0};
    SimTime   measuredTime{0.0};
    long long arrived{0};
    long long exited{0};
    double    numberInSystem{0.0};
    std::vector<BlockSnapshot>    blocks;
    std::vector<ResourceSnapshot> resources;
    std::vector<VariableSnapshot> variables;
};

// Reads a running system. Const, and it draws nothing: watching a run must not
// change it.
RunSnapshot snapshotOf(const SimulationSystem& sim);

}  // namespace des
