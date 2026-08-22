// ============================================================================
// Delay.hpp  --  an interval whose duration is UNKNOWN WHEN IT BEGINS
// ============================================================================
// Theory: "A random interval of time whose DURATION IS UNKNOWN WHEN IT BEGINS
// (waiting time in a queue -- you cannot schedule its end, it is determined by
// the system). Delays add NO VALUE."
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
// [2] Include "Common.hpp".
//
// [3] class Delay, private data:
//     [3a] SimTime m_startTime  : when the entity joined the queue
//     [3b] SimTime m_endTime    : MEANINGLESS until m_ended is true
//     [3c] bool m_ended         : has the system ended this delay yet
//
// [4] PUBLIC INTERFACE:
//     [4a] Constructor (SimTime start)
//          ONE parameter. Compare with Activity's THREE. You physically cannot
//          supply a duration, because it does not exist yet. Set m_ended=false
//          and m_endTime=start as a harmless placeholder.
//     [4b] SimTime startTime() const
//     [4c] bool hasEnded() const
//
// [5] V1 STUBS:
//     [5a] void end(SimTime t);
//          v2: set m_endTime = t, set m_ended = true. Called by the system when
//          the entity finally gets a server -- the system decides, not the delay.
//     [5b] SimTime duration() const;
//          v2: return m_endTime - m_startTime.
//          PRECONDITION: only valid if m_ended. Assert it, or return 0. Write
//          the precondition as a comment above the declaration either way.
//
// [6] Close class with semicolon.
//
// ---------------- THE POINT ----------------
// Look at the two constructors side by side:
//     Activity(name, start, duration)   <- duration known now
//     Delay(start)                      <- duration unknowable now
// and at the fact that only Delay has end().
//
// The theory distinction between activity and delay is now STRUCTURAL: the type
// system will not let you write code that pretends a queue wait has a known
// length. That is what "modelling the domain in types" means, and it is the
// main reason we are doing this in a language with a real type system.

#pragma once

#include "Common.hpp"

namespace des {


class Delay {
private:
    SimTime m_startTime;
    SimTime m_endTime;
    bool m_ended;

public:
    explicit Delay(SimTime start);

    SimTime startTime() const { return m_startTime; }
    bool hasEnded() const { return m_ended; }

    // Precondition: only valid if m_ended
    void end(SimTime t);
    SimTime duration() const;
};

}  // namespace des
