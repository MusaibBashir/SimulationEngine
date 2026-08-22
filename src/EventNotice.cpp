// ============================================================================
// EventNotice.cpp
// ============================================================================

#include "EventNotice.hpp"

uint64_t EventNotice::s_nextSequenceNumber = 0;

EventNotice::EventNotice(EventType type, SimTime time, Entity* entity, Station* station)
    : m_type(type), m_time(time), m_entity(entity), m_station(station),
      m_sequenceNumber(++s_nextSequenceNumber) {}

void EventNotice::resetSequenceCounter() {
    // v2.1: without this, sequence numbers climb across replications and two
    // runs of the same model in one process stop being bit-identical.
    s_nextSequenceNumber = 0;
}

bool EventNotice::operator>(const EventNotice& other) const {
    // Exact floating-point equality is the right test here: we are asking
    // whether two events were scheduled for the identical instant, not whether
    // two computed quantities are close. Ties fall back to scheduling order, so
    // a run is reproducible.
    if (m_time != other.m_time) return m_time > other.m_time;
    return m_sequenceNumber > other.m_sequenceNumber;
}
