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
#include <utility>

namespace des {


Statistics::Statistics(std::string labelA, std::string labelB)
    : m_labelA(std::move(labelA)), m_labelB(std::move(labelB)) {}

void Statistics::recordArrival(SimTime /*t*/) {
    ++m_numberArrived;
}

void Statistics::recordDeparture(SimTime /*t*/, SimTime waitTime, SimTime timeInSystem) {
    ++m_numberServed;
    m_totalWaitingTime  += waitTime;
    m_totalTimeInSystem += timeInSystem;
    if (waitTime > m_maxWaitingTime) m_maxWaitingTime = waitTime;
}

void Statistics::updateTimeIntegrals(SimTime now, int valueA, int valueB) {
    // One rectangle added to a running integral: width is the time since we were
    // last here, height is the state that held throughout that width.
    const SimTime dt = now - m_lastUpdateTime;
    m_areaA += valueA * dt;
    m_areaB += valueB * dt;
    m_lastUpdateTime = now;
}

void Statistics::reset() {
    // Assign a fresh object over ourselves so the in-class initialisers stay the
    // only place that knows the zero values -- but keep the labels, which are
    // configuration rather than run state.
    std::string a = std::move(m_labelA);
    std::string b = std::move(m_labelB);
    *this = Statistics{};
    m_labelA = std::move(a);
    m_labelB = std::move(b);
}

void Statistics::restartAt(SimTime now) {
    reset();
    m_lastUpdateTime = now;   // <- the whole difference. Area accumulated from
                              // here on is divided by (clock - now), not by the
                              // clock, so the transient never enters any average.
}

double Statistics::averageWaitingTime() const {
    if (m_numberServed == 0) return 0.0;   // FP divide-by-zero yields NaN, not a
    return m_totalWaitingTime / m_numberServed;   // crash, and prints as "nan"
}

double Statistics::averageTimeInSystem() const {
    if (m_numberServed == 0) return 0.0;
    return m_totalTimeInSystem / m_numberServed;
}

double Statistics::timeAverageA(SimTime elapsed) const {
    if (elapsed <= 0.0) return 0.0;
    return m_areaA / elapsed;
}

double Statistics::timeAverageB(SimTime elapsed) const {
    if (elapsed <= 0.0) return 0.0;
    return m_areaB / elapsed;
}

double Statistics::utilisation(SimTime elapsed, int capacity) const {
    if (elapsed <= 0.0 || capacity <= 0) return 0.0;
    // Dimensionless and MUST land in [0,1]. Above 1 is double counting.
    return m_areaB / (elapsed * capacity);
}

}  // namespace des
