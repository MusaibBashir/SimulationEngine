// ============================================================================
// FutureEventList.cpp
// ============================================================================
// [1] Include "FutureEventList.hpp" first, then <cassert>.
//
// [2] v1 STUB BODIES:
//
//     [2a] void FutureEventList::schedule(const EventNotice& e)
//          // TODO v2: m_fel.push(e). One line.
//          // Add the past-scheduling assert once the system can tell you the
//          // current clock value (it must be passed in, or the FEL must be
//          // given a Clock reference -- a small design decision to make in v2;
//          // prefer passing it in, so the FEL stays dependency-free).
//
//     [2b] EventNotice FutureEventList::popImminent()
//          // TODO v2 -- THREE LINES AND THE ORDER IS NOT NEGOTIABLE:
//          //   EventNotice next = m_fel.top();   <- COPY, by value, not a ref
//          //   m_fel.pop();                      <- destroys the heap's element
//          //   return next;
//          // Write `const EventNotice& next = m_fel.top();` instead and you
//          // return a reference to an object that pop() just destroyed. It will
//          // often appear to work, which is the worst possible failure mode.
//          // Consider asserting !isEmpty() at the top.
//
//     [2c] SimTime FutureEventList::nextEventTime() const
//          // TODO v2: return m_fel.top().time();
//          // Document the empty-list contract you chose in the header and obey
//          // it here (assert, or return infinity from <limits>).

#include "FutureEventList.hpp"
#include <cassert>

void FutureEventList::schedule(const EventNotice& e) {
    m_fel.push(e);
}

EventNotice FutureEventList::popImminent() {
    assert(!m_fel.empty());
    EventNotice next = m_fel.top();
    m_fel.pop();
    return next;
}

SimTime FutureEventList::nextEventTime() const {
    assert(!m_fel.empty());
    return m_fel.top().time();
}

