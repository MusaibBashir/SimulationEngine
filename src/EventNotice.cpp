// ============================================================================
// EventNotice.cpp
// ============================================================================
// This file may end up nearly empty, and that is a legitimate outcome.
//
// [1] Include "EventNotice.hpp".
//
// [2] Define the constructor here if you did not inline it. Remember: the
//     DEFAULT ARGUMENTS (= nullptr) appear in the header declaration ONLY,
//     never repeated in this definition.
//
// [3] Define bool EventNotice::operator>(const EventNotice& other) const
//     One line: compare m_time with other.m_time, greater-than.
//     // TODO v2: when the times are EQUAL, fall back to comparing a sequence
//     // number so that ties resolve deterministically and two runs with the
//     // same seed produce byte-identical output. Without it, "reproducible"
//     // is a claim you cannot actually make.
//
// [4] If everything here is a one-liner, consider deleting this .cpp and making
//     EventNotice header-only -- then remove it from CMakeLists.txt too. Small
//     value types are a normal and correct place for header-only. Make that call
//     consciously rather than keeping an empty file out of symmetry.

#include "EventNotice.hpp"

uint64_t EventNotice::s_nextSequenceNumber=0;

EventNotice::EventNotice(EventType type, SimTime time, Entity* entity, Resource* resource)
    : m_type(type), m_time(time), m_entity(entity), m_resource(resource), m_sequenceNumber(++s_nextSequenceNumber) {}

bool EventNotice::operator>(const EventNotice& other) const {
    // TODO v2: when the times are EQUAL, fall back to comparing a sequence
    // number so that ties resolve deterministically and two runs with the
    // same seed produce byte-identical output.
    if (m_time == other.m_time) {
        return m_sequenceNumber > other.m_sequenceNumber;
    }
    return m_time > other.m_time;
}

