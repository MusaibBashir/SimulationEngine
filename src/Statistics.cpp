// ============================================================================
// Statistics.cpp
// ============================================================================
// [1] Include "Statistics.hpp".
//
// [2] Constructor: zero every single member EXPLICITLY in the member-init list.
//     All eight of them. Uninitialised accumulators do not crash -- they produce
//     numbers that look like results.
//
// [3] v1 STUB BODIES:
//
//     [3a] void Statistics::recordArrival(SimTime t)
//          // TODO v2: ++m_numberArrived;
//          // (t is unused for now. Keep the parameter -- the signature is the
//          // interface and you will want the time in v4 for arrival-rate traces.
//          // Silence the unused-parameter warning by omitting the parameter
//          // NAME in the definition, keeping only the type.)
//
//     [3b] void Statistics::recordDeparture(SimTime t, SimTime wait, SimTime inSystem)
//          // TODO v2, four lines: ++m_numberServed; add wait to the total; add
//          // inSystem to its total; if wait > m_maxWaitingTime, replace it.
//
//     [3c] void Statistics::updateTimeIntegrals(SimTime now, int queueLength, int serversBusy)
//          // TODO v2, four lines:
//          //   SimTime dt = now - m_lastUpdateTime;
//          //   m_areaUnderQueueLength += queueLength * dt;
//          //   m_areaUnderServerBusy  += serversBusy * dt;
//          //   m_lastUpdateTime = now;
//          //
//          // *** CALL THIS AT THE TOP OF EVERY EVENT HANDLER, BEFORE ANY STATE
//          // CHANGES. *** The arguments describe the interval that just ENDED,
//          // so they must be the OLD values. Update state first and you
//          // attribute the new queue length to the old time interval, and every
//          // time-average in your report is silently wrong.
//          //
//          // Sanity check for v2: for a single server, m_areaUnderServerBusy
//          // divided by total time must land in [0,1]. If it exceeds 1 you are
//          // calling this twice per event.
//
//     [3d] void Statistics::reset()
//          // TODO v2: same as the constructor. Consider having the constructor
//          // simply call reset() so there is ONE place that knows the zero
//          // values -- a small, real application of don't-repeat-yourself.
//
// [4] DERIVED REPORTING METHODS -- TODO v2:
//     [4a] averageWaitingTime()      : total wait / served. GUARD served == 0.
//                                      Integer-divide-by-zero crashes; floating
//                                      point silently yields NaN and prints
//                                      "nan" in your report. Guard it.
//     [4b] averageTimeInSystem()     : same shape, same guard.
//     [4c] timeAverageQueueLength(T) : m_areaUnderQueueLength / T. Guard T == 0.
//     [4d] serverUtilisation(T, cap) : m_areaUnderServerBusy / (T * cap).
//
//     Cross-check to run in v2: Little's Law. L = lambda * W, i.e.
//     timeAverageQueueLength should approximately equal (arrival rate) x
//     (average wait). If those two disagree, your accumulators are wrong, not
//     the theory. This is the single most useful test in the whole project --
//     write it down now so you remember to run it.

#include "Statistics.hpp"

void Statistics::recordArrival(SimTime /*t*/) {
    // TODO v2: ++m_numberArrived;
    // t is unused for now. KEEP THE PARAMETER -- the signature is the interface,
    // and v4 will want arrival times for rate traces. Commenting out the name is
    // how you keep the parameter without earning a -Wunused-parameter warning.
}

void Statistics::recordDeparture(SimTime /*t*/, SimTime /*waitTime*/, SimTime /*timeInSystem*/) {
    // TODO v2 -- four lines:
    //   ++m_numberServed;
    //   m_totalWaitingTime  += waitTime;
    //   m_totalTimeInSystem += timeInSystem;
    //   if (waitTime > m_maxWaitingTime) m_maxWaitingTime = waitTime;
}

void Statistics::updateTimeIntegrals(SimTime /*now*/, int /*queueLength*/, int /*serversBusy*/) {
    // TODO v2 -- four lines:
    //   SimTime dt = now - m_lastUpdateTime;
    //   m_areaUnderQueueLength += queueLength * dt;
    //   m_areaUnderServerBusy  += serversBusy * dt;
    //   m_lastUpdateTime = now;
    //
    // *** CALL THIS AT THE TOP OF EVERY EVENT HANDLER, BEFORE ANY STATE
    // CHANGE. *** The arguments describe the interval that just ENDED, so they
    // must be the OLD queue length and OLD busy count. Update the state first
    // and you attribute the new values to the old interval -- every time
    // average in the report is then silently wrong, with no error message.
    //
    // Self-check for v2: m_areaUnderServerBusy / (totalTime * capacity) must
    // land in [0, 1]. Above 1 means you are calling this twice per event.
}

void Statistics::reset() {
    // Assign a fresh default-constructed object over ourselves. The in-class
    // initialisers in Statistics.hpp are the ONLY place that knows the zero
    // values -- listing all eight again here would be a second source of truth
    // that drifts the first time a member is added.
    *this = Statistics{};
}

double Statistics::averageWaitingTime() const {
    // TODO v2:
    //   if (m_numberServed == 0) return 0.0;   // GUARD IT
    //   return m_totalWaitingTime / m_numberServed;
    // Floating-point division by zero does not crash -- it yields NaN, and your
    // report cheerfully prints "nan". Guard, do not rely on a crash.
    return 0.0;   // v1 placeholder
}

double Statistics::averageTimeInSystem() const {
    // TODO v2: same shape and same zero-guard as averageWaitingTime(),
    //          over m_totalTimeInSystem.
    return 0.0;   // v1 placeholder
}

double Statistics::timeAverageQueueLength(SimTime /*totalTime*/) const {
    // TODO v2:
    //   if (totalTime <= 0.0) return 0.0;
    //   return m_areaUnderQueueLength / totalTime;
    //
    // v2 acceptance test -- LITTLE'S LAW:  L = lambda * W
    // This value should approximately equal (arrival rate) x averageWaitingTime().
    // If the two disagree, your accumulators are wrong, not the theory. It is
    // the single most useful test in the whole project.
    return 0.0;   // v1 placeholder
}

double Statistics::serverUtilisation(SimTime /*totalTime*/, int /*capacity*/) const {
    // TODO v2:
    //   if (totalTime <= 0.0 || capacity <= 0) return 0.0;
    //   return m_areaUnderServerBusy / (totalTime * capacity);
    // Dimensionless, and it MUST land in [0, 1]. A value above 1 is a
    // double-counting bug, not a busy server.
    return 0.0;   // v1 placeholder
}

