// ============================================================================
// Activity.cpp
// ============================================================================
// [1] Include "Activity.hpp".
// [2] Constructor definition if not inlined -- three members from three params.
// [3] Everything else (name, startTime, duration, endTime) is a one-line const
//     getter and belongs inline in the header. This file may end up holding only
//     the constructor.
//     // v2: an Activity will not be constructed with a hand-supplied duration
//     // any more -- it will take a distribution and DRAW the duration. Expect
//     // that constructor to move here and grow.

#include "Activity.hpp"
#include <utility>

Activity::Activity(std::string name, SimTime start, SimTime duration)
    : m_name(std::move(name)), m_startTime(start), m_duration(duration) {}
