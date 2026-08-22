// ============================================================================
// Delay.cpp
// ============================================================================
// [1] Include "Delay.hpp", then <cassert>.
// [2] Constructor definition if not inlined: start from the parameter,
//     m_ended = false, m_endTime = m_startTime as a placeholder.
//
// [3] v1 STUB BODIES:
//     [3a] void Delay::end(SimTime t)
//          // TODO v2: assert(t >= m_startTime); m_endTime = t; m_ended = true;
//     [3b] SimTime Delay::duration() const
//          // TODO v2: assert(m_ended); return m_endTime - m_startTime;
//          // The assert IS the theory: asking a delay how long it lasted before
//          // the system has ended it is a meaningless question, so the code
//          // refuses to answer rather than returning a plausible zero.

#include "Delay.hpp"
#include <cassert>

Delay::Delay(SimTime start)
    : m_startTime(start), m_endTime(start), m_ended(false) {}

void Delay::end(SimTime t) {
    // Called BY THE SYSTEM, when the waiting entity finally seizes a resource.
    // A delay never knows its own end -- that is what makes it a delay.
    assert(t >= m_startTime);
    m_endTime = t;
    m_ended = true;
}

SimTime Delay::duration() const {
    // Asking an unfinished delay how long it lasted is a meaningless question.
    // Refuse rather than returning a plausible zero.
    assert(m_ended);
    return m_endTime - m_startTime;
}

