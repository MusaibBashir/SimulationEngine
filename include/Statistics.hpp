// ============================================================================
// Statistics.hpp  --  the STATISTICAL ACCUMULATORS
// ============================================================================
// Theory: "Variables that store the statistics of interest."
// Everything the simulation is actually FOR ends up in this class.
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
// [2] Include "Common.hpp".
//
// [3] class Statistics, private data. Two kinds -- keep them visually separated
//     with a blank line and a comment, because they update differently:
//
//     --- COUNTERS (bumped at discrete moments) ---
//     [3a] int m_numberArrived
//     [3b] int m_numberServed
//     [3c] SimTime m_totalWaitingTime    : summed over all served entities
//     [3d] SimTime m_totalTimeInSystem   : summed over all served entities
//     [3e] SimTime m_maxWaitingTime
//
//     --- TIME INTEGRALS (accumulated over intervals) ---
//     [3f] SimTime m_areaUnderQueueLength : the integral of L(t) dt
//     [3g] SimTime m_areaUnderServerBusy  : the integral of B(t) dt
//     [3h] SimTime m_lastUpdateTime       : when the integrals were last brought
//                                           up to date. Without this you cannot
//                                           know the width of the next rectangle.
//
// [4] PUBLIC INTERFACE:
//     [4a] Default constructor. Zero EVERY member explicitly. Uninitialised
//          accumulators produce plausible-looking garbage, which is worse than
//          a crash because you will believe it.
//     [4b] One const getter per member. Tedious; write them all.
//
// [5] V1 STUBS -- the update routines:
//
//     [5a] void recordArrival(SimTime t);
//          v2: increment m_numberArrived.
//
//     [5b] void recordDeparture(SimTime t, SimTime waitTime, SimTime timeInSystem);
//          v2: increment m_numberServed; add to m_totalWaitingTime and
//          m_totalTimeInSystem; update m_maxWaitingTime if this one is larger.
//
//     [5c] void updateTimeIntegrals(SimTime now, int queueLength, int serversBusy);
//          *** THE ONE THAT IS EASY TO GET WRONG. *** v2 body:
//            - dt = now - m_lastUpdateTime
//            - m_areaUnderQueueLength += queueLength * dt
//            - m_areaUnderServerBusy  += serversBusy * dt
//            - m_lastUpdateTime = now
//          The values passed in are the state DURING the interval just ended,
//          i.e. the state BEFORE whatever is about to happen. So this must be
//          called BEFORE the state changes, at the top of every event handler,
//          without exception. Write that in capitals above the declaration.
//          One missed call and every time-average in your report is wrong, with
//          no error message.
//
//     [5d] void reset();
//
// [6] DERIVED REPORTING METHODS -- declare now, implement in v2:
//     [6a] double averageWaitingTime() const
//          total waiting time / number served. Guard number served == 0.
//     [6b] double averageTimeInSystem() const
//     [6c] double timeAverageQueueLength(SimTime totalTime) const
//          m_areaUnderQueueLength / totalTime.
//     [6d] double serverUtilisation(SimTime totalTime, int capacity) const
//          m_areaUnderServerBusy / (totalTime * capacity). Dimensionless, and
//          it must land in [0,1] -- a value above 1 means you double-counted,
//          which is a useful self-check to remember.
//
// [7] Close class with semicolon.
//
// ---------------- DESIGN RULE ----------------
// AVERAGES ARE NEVER STORED. There is no m_averageWait member and there must
// not be. Averages are computed from accumulators on demand, at report time.
// Same rule as Resource::unitsAvailable() and Activity::endTime(): if it can be
// derived, deriving it is the only way it cannot go stale.

#pragma once

#include "Common.hpp"

class Statistics {
private:
    // --- COUNTERS (bumped at discrete moments) ---
    int m_numberArrived{0};
    int m_numberServed{0};
    SimTime m_totalWaitingTime{0.0};
    SimTime m_totalTimeInSystem{0.0};
    SimTime m_maxWaitingTime{0.0};

    // --- TIME INTEGRALS (accumulated over intervals) ---
    SimTime m_areaUnderQueueLength{0.0};
    SimTime m_areaUnderServerBusy{0.0};
    SimTime m_lastUpdateTime{0.0};

public:
    Statistics()=default;

    int numberArrived() const { return m_numberArrived; }
    int numberServed() const { return m_numberServed; }
    SimTime totalWaitingTime() const { return m_totalWaitingTime; }
    SimTime totalTimeInSystem() const { return m_totalTimeInSystem; }
    SimTime maxWaitingTime() const { return m_maxWaitingTime; }

    SimTime areaUnderQueueLength() const { return m_areaUnderQueueLength; }
    SimTime areaUnderServerBusy() const { return m_areaUnderServerBusy; }
    SimTime lastUpdateTime() const { return m_lastUpdateTime; }

    void recordArrival(SimTime t);
    void recordDeparture(SimTime t, SimTime waitTime, SimTime timeInSystem);
    // MUST BE CALLED BEFORE ANY STATE CHANGES AT TOP OF EVENT HANDLER
    void updateTimeIntegrals(SimTime now, int queueLength, int serversBusy);
    void reset();

    double averageWaitingTime() const;
    double averageTimeInSystem() const;
    double timeAverageQueueLength(SimTime totalTime) const;
    double serverUtilisation(SimTime totalTime, int capacity) const;
};

