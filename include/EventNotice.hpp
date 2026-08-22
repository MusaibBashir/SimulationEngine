// ============================================================================
// EventNotice.hpp  --  the RECORD of a scheduled event
// ============================================================================
// Theory: "A record specifying (event type, event time)". An event is
// INSTANTANEOUS -- it has a time, never a duration. Duration belongs to
// Activity and Delay, which is why those are separate types.
//
// v3 CHANGE: the notice now names a Station rather than a Resource. In a
// network, "where did this happen" is a station, and the station owns its
// resource. Naming the resource was only ever adequate because there was one.

#pragma once
#include <cstdint>
#include "Common.hpp"

class Entity;
class Station;

class EventNotice {
private:
    EventType m_type;
    SimTime   m_time;
    Entity*   m_entity;    // nullptr for system events like EndSimulation
    Station*  m_station;   // nullptr when the event is not at a station

    // v4: the mutable static is GONE. The number is now stamped by the FEL at
    // schedule() time through a private setter, and FutureEventList is the only
    // thing that can reach it.
    //
    // Why `friend` and not a public setSequenceNumber(): a public setter would
    // let ANY code renumber an event, including one already sitting in the heap
    // -- which silently breaks the heap invariant. friend narrows that power to
    // exactly the one class that needs it. Friendship is often described as
    // "breaking encapsulation"; used like this it is the opposite, because the
    // alternative was a setter open to everyone.
    friend class FutureEventList;
    uint64_t m_sequenceNumber{0};
    void setSequenceNumber(uint64_t n) { m_sequenceNumber = n; }

public:
    EventNotice(EventType type, SimTime time,
                Entity* entity = nullptr, Station* station = nullptr);

    EventType type() const { return m_type; }
    SimTime   time() const { return m_time; }
    Entity*   entity() const { return m_entity; }
    Station*  station() const { return m_station; }
    uint64_t  sequenceNumber() const { return m_sequenceNumber; }

    // *** THE LINE THE WHOLE FEL DEPENDS ON. ***
    // std::priority_queue is a MAX-heap. Feeding it std::greater inverts that
    // into a min-heap, so top() is the EARLIEST event -- and std::greater needs
    // operator> to exist. Get this backwards and the simulation compiles
    // perfectly and runs time in reverse.
    bool operator>(const EventNotice& other) const;

    // NO SETTERS, deliberately. Mutating an element already inside a heap
    // breaks the heap invariant silently. To change a scheduled event you
    // cancel it and schedule a new one.
};
