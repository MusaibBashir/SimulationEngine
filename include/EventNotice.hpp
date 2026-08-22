// ============================================================================
// EventNotice.hpp  --  the RECORD of a scheduled event
// ============================================================================
// Theory: "A record specifying (event type, event time) -- e.g. (A, 10), (B, 7).
// The FEL is a list of event notices."
// And: an Event is "an INSTANTANEOUS occurrence that changes the state."
// Instantaneous -- it has a time, never a duration. Duration belongs to
// Activity and Delay, which is why they are separate types.
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
//
// [2] Includes: "Common.hpp".
//     Forward-declare `class Entity;` and `class Resource;` -- you only store
//     pointers to them, so no need to include their headers.
//
// [3] class EventNotice, private data:
//
//     [3a] EventType m_type
//     [3b] SimTime   m_time      : WHEN it is scheduled to occur.
//     [3c] Entity*   m_entity    : which entity this concerns. nullptr for
//                                  system-level events like EndSimulation.
//     [3d] Resource* m_resource  : which resource is involved, or nullptr.
//
// [4] PUBLIC INTERFACE:
//
//     [4a] Constructor (EventType, SimTime, Entity* = nullptr, Resource* = nullptr)
//          Note the two DEFAULT ARGUMENTS: a simple event costs two arguments
//          to build, a detailed one costs four. Defaults go last, always.
//
//     [4b..4e] Four const getters: type(), time(), entity(), resource().
//          All one-liners, all fine to define inline in the header.
//
//     [4f] *** THE IMPORTANT LINE ***
//          bool operator>(const EventNotice& other) const;
//          Body is a single comparison of m_time against other.m_time.
//
//          WHY GREATER-THAN AND NOT LESS-THAN, write this comment:
//            std::priority_queue is a MAX-heap by default -- it hands you the
//            "largest" element. You want the EARLIEST event. So you feed it
//            std::greater as the comparator, which inverts the order, and
//            std::greater needs operator> to exist. That one line is the reason
//            the whole FEL works. Get it backwards and your simulation runs
//            time in reverse while compiling perfectly.
//
//          // TODO v2: tie-break. Two events at exactly t=10.0 currently have
//          // an arbitrary order. Add an m_sequenceNumber member and compare it
//          // when times are equal, so runs are reproducible.
//
// [5] NO SETTERS. Deliberate.
//     An event notice is IMMUTABLE once scheduled. If a scheduled event must
//     change, you cancel it and schedule a new one -- you do not mutate a record
//     that is already sitting inside a heap, because mutating a heap element
//     breaks the heap invariant silently. Write that as a comment so future-you
//     does not "helpfully" add setTime().
//
// [6] Close class with semicolon.

#pragma once
#include "Common.hpp"
#include <cstdint>  // for uint64_t

class Entity;
class Resource;

class EventNotice {
private:
    EventType m_type;
    SimTime   m_time;
    Entity*   m_entity;
    Resource* m_resource;
    // The tie-break counter. A mutable static, which is a wart: it makes
    // constructing an EventNotice have a side effect, and it is not thread-safe.
    // It stays for now because moving it into FutureEventList would mean giving
    // EventNotice a setter, and an event notice is meant to be immutable. See
    // the v3 plan.
    static uint64_t s_nextSequenceNumber;
    uint64_t m_sequenceNumber;

public:
    EventNotice(EventType type, SimTime time, Entity* entity = nullptr, Resource* resource = nullptr);

    // v2.1: without this, sequence numbers keep climbing across replications.
    // Ordering still works, but two runs of the same model in one process are
    // no longer bit-identical, which defeats the point of seeding.
    static void resetSequenceCounter();

    EventType type() const { return m_type; }
    SimTime time() const { return m_time; }
    Entity* entity() const { return m_entity; }
    Resource* resource() const { return m_resource; }

    bool operator>(const EventNotice& other) const;
};
