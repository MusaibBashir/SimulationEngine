// ============================================================================
// Clock.cpp
// ============================================================================
// [1] Include "Clock.hpp", then <cassert>.
//
// [2] v1 STUB BODIES:
//
//     [2a] void Clock::advanceTo(SimTime t)
//          // TODO v2, two lines:
//          //   assert(t >= m_now);   <- THE ENTIRE REASON THIS CLASS EXISTS
//          //   m_now = t;
//          // Use >= not > : several events legitimately occur at the same
//          // instant (a departure and an arrival at t=10.0), and advancing to
//          // "now" must be a legal no-op.
//
//     [2b] void Clock::reset()
//          // TODO v2: m_now = 0.0;  For the next replication in v4.
//
// [3] now() stayed inline in the header.

#include "Clock.hpp"
#include <cassert>

void Clock::advanceTo(SimTime /*t*/) {
    // TODO v2 -- two lines, and they are the entire reason Clock is a class
    // rather than a bare double:
    //   assert(t >= m_now);   // >= not > : several events legitimately share
    //                         // one instant, so advancing to "now" must be a
    //                         // legal no-op, not an assertion failure
    //   m_now = t;
    // Restore the parameter name when you use it.
}

void Clock::reset() {
    // TODO v2: m_now = 0.0;
    // Not needed until v4 runs multiple replications, but the interface is
    // fixed now so the call sites never have to change.
}

