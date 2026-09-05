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

class RunController {
public:
    // Declared HERE and not taken from Experiment: Experiment is refactored
    // onto this class, so naming Experiment::Builder in this header would make
    // the two include each other.
    using ModelBuilder = std::function<void(SimulationSystem&)>;

    RunController(RunSetup setup, ModelBuilder build);
    ~RunController();

    RunController(const RunController&) = delete;
    RunController& operator=(const RunController&) = delete;

    RunState           state()   const { return m_state; }
    const std::string& failure() const { return m_failure; }

    // Do at most maxEvents events, rolling on to the next replication when one
    // ends. Returns how many it actually did, which is 0 when paused, finished,
    // cancelled or failed -- a front end that polls past the end is normal, not
    // an error.
    std::size_t advance(std::size_t maxEvents);

    void pause();
    void resume();
    void cancel();

    RunProgress progress() const;
    RunSnapshot snapshot() const;

    const std::vector<ReplicationResult>&   results() const { return m_results; }
    const std::vector<std::vector<double>>& series()  const { return m_series; }

    // Drives to the end. Loops on STATE rather than on advance()'s return
    // value: a replication that produced no events at all would return 0 and
    // strand a caller that trusted the count, with the study unfinished.
    void runToCompletion();

private:
    bool startRun();     // false when there is nothing left to start
    void finishRun();

    RunSetup     m_setup;
    ModelBuilder m_build;
    RunState     m_state{RunState::Ready};
    std::string  m_failure;

    int       m_replication{0};     // 0-based index of the one in progress
    bool      m_mirrorHalf{false};  // antithetic: the mirrored half of a pair
    long long m_events{0};

    std::unique_ptr<SimulationSystem> m_sim;
    ReplicationResult                 m_firstHalf;   // antithetic pairing
    std::vector<ReplicationResult>    m_results;
    std::vector<std::vector<double>>  m_series;
};

}  // namespace des
