// ============================================================================
// EventNotice.cpp
// ============================================================================

#include "EventNotice.hpp"

namespace des {


EventNotice::EventNotice(EventType type, SimTime time, Entity* entity, INode* node)
    : m_type(type), m_time(time), m_entity(entity), m_node(node) {}
    // m_sequenceNumber stays 0 until FutureEventList::schedule() stamps it.
    // A notice that was never scheduled has no order, which is honest.

bool EventNotice::operator>(const EventNotice& other) const {
    // Exact floating-point equality is the right test here: we are asking
    // whether two events were scheduled for the identical instant, not whether
    // two computed quantities are close. Ties fall back to scheduling order, so
    // a run is reproducible.
    if (m_time != other.m_time) return m_time > other.m_time;
    return m_sequenceNumber > other.m_sequenceNumber;
}

}  // namespace des
