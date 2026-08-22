// ============================================================================
// SystemState.cpp
// ============================================================================
// [1] Include "SystemState.hpp".
//
// [2] Constructor definition if not inlined: zero the counts, Idle the server.
//
// [3] The setters are one-liners; inline them in the header OR define them here.
//     Pick one policy for the whole project and stick to it -- inconsistency
//     between files costs more than either choice.
//
// [4] void SystemState::reset()
//     // TODO v2: back to the t=0 values. Note this is the third reset() in the
//     // project (Clock, Statistics, SystemState). Three is the point at which a
//     // pattern is real: in v3 consider an IResettable interface, or a single
//     // SimulationSystem::reset() that calls all three. Do NOT abstract it yet
//     // -- just notice it and write the observation down here.
//
// [5] Copy the DELIBERATE SMELL paragraph from the header into your notes and
//     make the (a)-view vs (b)-snapshot decision before you write v2's
//     handleArrival. Deciding it after the handlers exist means rewriting them.

#include "SystemState.hpp"

void SystemState::reset() {
    // Assign a fresh default-constructed object over ourselves. The in-class
    // initialisers in SystemState.hpp are now the ONLY place that knows the
    // t=0 values. Listing them again here would be a second source of truth,
    // and the two would drift the first time a member is added.
    // Same idiom as Statistics::reset().
    *this = SystemState{};
}

