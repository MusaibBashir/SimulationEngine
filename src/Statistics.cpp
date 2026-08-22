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
    ++m_numberArrived;
    // t is still unused. The parameter stays because the signature is the
    // interface, and v4 wants arrival times for rate traces.
}

void Statistics::recordDeparture(SimTime /*t*/, SimTime waitTime, SimTime timeInSystem) {
    ++m_numberServed;
    m_totalWaitingTime  += waitTime;
    m_totalTimeInSystem += timeInSystem;
    if (waitTime > m_maxWaitingTime) {
        m_maxWaitingTime = waitTime;
    }
}

void Statistics::updateTimeIntegrals(SimTime now, int queueLength, int serversBusy) {
    // *** CALLED AT THE TOP OF run()'s LOOP, BEFORE THE CLOCK MOVES AND BEFORE
    // ANY STATE CHANGES. *** The arguments describe the interval that just
    // ENDED, so they must be the OLD queue length and OLD busy count.
    //
    // Geometrically: we are adding one rectangle to a running integral. Its
    // width is the time since we were last here; its height is the state that
    // held throughout that width.
    const SimTime dt = now - m_lastUpdateTime;
    m_areaUnderQueueLength += queueLength * dt;
    m_areaUnderServerBusy  += serversBusy * dt;
    m_lastUpdateTime = now;
}

void Statistics::reset() {
    // Assign a fresh default-constructed object over ourselves. The in-class
    // initialisers in Statistics.hpp are the ONLY place that knows the zero
    // values -- listing all eight again here would be a second source of truth
    // that drifts the first time a member is added.
    *this = Statistics{};
}

double Statistics::averageWaitingTime() const {
    if (m_numberServed == 0) return 0.0;   // guard: FP division by zero gives
                                           // NaN, not a crash, and the report
                                           // would cheerfully print "nan"
    return m_totalWaitingTime / m_numberServed;
}

double Statistics::averageTimeInSystem() const {
    if (m_numberServed == 0) return 0.0;
    return m_totalTimeInSystem / m_numberServed;
}

double Statistics::timeAverageQueueLength(SimTime totalTime) const {
    if (totalTime <= 0.0) return 0.0;
    // LITTLE'S LAW CHECK:  L = lambda * W. This should approximately equal
    // (effective arrival rate) x averageWaitingTime(). If they disagree, the
    // accumulators are wrong -- the theory is not.
    return m_areaUnderQueueLength / totalTime;
}

double Statistics::serverUtilisation(SimTime totalTime, int capacity) const {
    if (totalTime <= 0.0 || capacity <= 0) return 0.0;
    // Dimensionless, and it MUST land in [0, 1]. Above 1 is a double-counting
    // bug, not a busy server.
    return m_areaUnderServerBusy / (totalTime * capacity);
}
