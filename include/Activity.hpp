// ============================================================================
// Activity.hpp  --  an interval whose END IS KNOWN WHEN IT STARTS
// ============================================================================
// Theory: "A random interval of time whose END BECOMES KNOWN WHEN IT STARTS
// (a service duration: when service begins you draw its length and schedule the
// end). Preceded and succeeded by events."
//
// The practical consequence: the moment an activity begins you can push its end
// event onto the FEL. That is what makes it an activity.
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
// [2] Includes: <string>, "Common.hpp".
//
// [3] class Activity, private data:
//     [3a] std::string m_name    : "Service at teller"
//     [3b] SimTime m_startTime   : clock value when it began
//     [3c] SimTime m_duration    : DRAWN FROM A DISTRIBUTION AT THE START.
//                                  v1 takes it as a constructor argument;
//                                  v2 will get it from an RNG.
//
// [4] PUBLIC INTERFACE:
//     [4a] Constructor (std::string name, SimTime start, SimTime duration)
//          All three known at construction -- that is the definition of an
//          activity, expressed as a constructor signature.
//     [4b] const std::string& name() const
//     [4c] SimTime startTime() const
//     [4d] SimTime duration() const
//     [4e] SimTime endTime() const
//          ONE LINE: start + duration. DERIVED, NOT STORED -- same rule as
//          Resource::unitsAvailable(). This is the value you hand to
//          FutureEventList::schedule() as the time of the Departure event.
//
// [5] Close class with semicolon.
//
// ---------------- WHY THIS IS A SEPARATE TYPE FROM Delay ----------------
// See Delay.hpp. The two classes have deliberately DIFFERENT interfaces, and
// the difference IS the theory. Do not be tempted to merge them into one
// "TimeInterval" class with a flag -- the flag would be re-implementing the
// type system by hand, badly.

#pragma once

#include <string>
#include "Common.hpp"

class Activity {
private:
    std::string m_name;
    SimTime m_startTime;
    SimTime m_duration;

public:
    Activity(std::string name, SimTime start, SimTime duration);

    const std::string& name() const { return m_name; }
    SimTime startTime() const { return m_startTime; }
    SimTime duration() const { return m_duration; }
    SimTime endTime() const { return m_startTime + m_duration; }
};

