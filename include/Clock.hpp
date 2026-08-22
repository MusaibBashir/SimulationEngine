// ============================================================================
// Clock.hpp  --  current simulation time
// ============================================================================
// Theory: "A variable that stores the current simulation time. (In Arena, TNOW
// gives the clock value.)"
//
// It is one double. We wrap it in a class anyway. The justification is at the
// bottom of this file, and it is not "because OOP".
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
// [2] Include "Common.hpp" for SimTime. Nothing else.
//
// [3] class Clock, private data:
//     [3a] SimTime m_now
//
// [4] PUBLIC INTERFACE:
//     [4a] Default constructor setting m_now to 0.0 -- via the member-init
//          list, or an in-class default initialiser on [3a]. Either is fine;
//          pick one style and use it everywhere in the project.
//     [4b] SimTime now() const     -> returns m_now. Inline.
//
// [5] V1 STUBS:
//     [5a] void advanceTo(SimTime t);
//          v2 body, two lines:
//            - assert(t >= m_now)   <- include <cassert>
//            - m_now = t
//     [5b] void reset();
//          v2: set m_now back to 0.0, for the next replication.
//
// [6] Close class with semicolon.
//
// ---------------- WHY THIS IS A CLASS AND NOT A double ----------------
// Because there is exactly one rule about simulation time -- IT NEVER GOES
// BACKWARDS -- and a bare double cannot enforce it. Wrapped, the only way to
// change time is advanceTo(), and advanceTo() asserts. Time travel becomes
// impossible by construction rather than by everybody remembering.
//
// That is the general test for "should this be a class?": is there an invariant
// to protect? If yes, wrap it. If no, a plain variable is the honest answer and
// wrapping it is ceremony. Here the answer is yes.

#pragma once

#include "Common.hpp"

namespace des {


class Clock {
private:
    SimTime m_now{0.0};

public:
    Clock() = default;

    SimTime now() const { return m_now; }

    void advanceTo(SimTime t);
    void reset();
};

}  // namespace des
