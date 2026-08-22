// ============================================================================
// TerminationCondition.hpp  --  when to STOP
// ============================================================================
// Theory: "The condition where the model should stop."
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
// [2] Includes: <limits> (for numeric_limits), "Common.hpp".
//
// [3] class TerminationCondition, private data. v1 supports two criteria:
//     [3a] SimTime m_maxTime
//          Stop when the clock reaches this. If the caller does not want a time
//          limit, they pass std::numeric_limits<SimTime>::infinity() -- a
//          SENTINEL that makes the comparison always false, so the code needs no
//          special case. Sentinels beat extra bool flags.
//     [3b] int m_maxEntities
//          Stop after this many DEPARTURES (not arrivals -- decide and document
//          which, because they differ). Sentinel: numeric_limits<int>::max().
//
// [4] PUBLIC INTERFACE:
//     [4a] Constructor (SimTime maxTime, int maxEntities).
//     [4b] SimTime maxTime() const
//     [4c] int maxEntities() const
//
// [5] V1 STUB:
//     [5a] bool isMet(SimTime now, int entitiesServed) const;
//          v2: return (now >= m_maxTime) || (entitiesServed >= m_maxEntities).
//          One line. The OR is why the sentinels work.
//
// [6] Close class with semicolon.
//
// ---------------- WHERE THIS GOES IN v3, AND WHY NOT NOW ----------------
// This is the natural first home for an ABSTRACT BASE CLASS in the project:
//     ITerminationCondition  with  virtual bool isMet(...) const = 0;
//     TimeBased : public ITerminationCondition
//     CountBased : public ITerminationCondition
//     SteadyStateDetected : public ITerminationCondition
// and SimulationSystem holding a std::unique_ptr<ITerminationCondition>.
//
// DO NOT DO THAT IN v1. Two hardcoded criteria and an OR is genuinely the right
// amount of machinery for two criteria. Build the hierarchy in v3, when you add
// the third and fourth condition and the if-chain starts to smell -- so that you
// EXPERIENCE why polymorphism beats a conditional, instead of being told it does.
// Premature abstraction is the most common way OOP is taught badly.

#pragma once

#include <limits>
#include "Common.hpp"

class TerminationCondition {
private:
    SimTime m_maxTime;
    int m_maxEntities;

public:
    TerminationCondition(SimTime maxTime, int maxEntities);

    SimTime maxTime() const { return m_maxTime; }
    int maxEntities() const { return m_maxEntities; }

    bool isMet(SimTime now, int entitiesServed) const;
};

