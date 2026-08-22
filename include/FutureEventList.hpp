// ============================================================================
// FutureEventList.hpp  --  the FEL
// ============================================================================
// Theory: "A list of events scheduled to occur AFTER the current simulation
// time, kept SORTED by event time."
// This is the heart of a discrete-event simulator. The clock does not tick --
// it JUMPS to the time of the earliest event in this list.
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
// [2] Includes: <queue>, <vector>, <functional> (for std::greater),
//     "EventNotice.hpp" (needed in full -- you store notices BY VALUE here),
//     "Common.hpp".
//
// [3] class FutureEventList, private data -- ONE member, one long line:
//
//     std::priority_queue< EventNotice,
//                          std::vector<EventNotice>,
//                          std::greater<EventNotice> >  m_fel;
//
//     Three template arguments, each doing a job:
//       1st : what is stored.
//       2nd : the underlying container the heap is built on (vector is the
//             default and the right choice -- contiguous, cache-friendly).
//       3rd : the comparator. std::greater flips the default max-heap into a
//             MIN-heap, so top() is the EARLIEST event. This is the argument
//             that consumes EventNotice::operator> from the previous file.
//
// [4] PUBLIC INTERFACE (v1):
//
//     [4a] Default constructor, written explicitly as `= default;`
//          The compiler would generate it anyway. Writing it states "I thought
//          about construction and the default is correct", which is different
//          from having not thought about it.
//     [4b] bool isEmpty() const        -> m_fel.empty()
//     [4c] std::size_t size() const    -> m_fel.size()
//
// [5] V1 STUBS:
//
//     [5a] void schedule(const EventNotice& e);
//          v2: push onto the heap. One line.
//          // TODO v2: assert e.time() >= currentClockTime. Scheduling an event
//          // in the PAST is the single most common DES bug and the assert
//          // catches it at the moment of the mistake instead of ten thousand
//          // events later.
//
//     [5b] EventNotice popImminent();
//          v2: copy top() into a local, call pop(), return the local.
//          THREE LINES AND THE ORDER MATTERS: top() returns a reference INTO the
//          heap; pop() destroys that element. Return top() directly and you
//          return a dangling reference. Copy first, pop second, return third.
//          "Imminent event" is the textbook's name for it -- use their word.
//
//     [5c] SimTime nextEventTime() const;
//          v2: return top().time(). Lets the caller peek without consuming.
//          Undefined if empty -- so document the precondition, or return
//          infinity from <limits>. Pick one and write it down.
//
// ---------------- DESIGN NOTE, THE POINT OF THIS FILE ----------------
// We WRAP std::priority_queue rather than letting SimulationSystem use one
// directly. That is encapsulation with a concrete, predictable payoff:
// a binary heap CANNOT cancel an arbitrary event (no way to find and remove a
// middle element). The day you need cancellation -- balking, reneging, machine
// breakdown pre-empting a service -- you swap the heap for a sorted list or an
// indexed structure, and ONLY THIS FILE CHANGES. Everyone else keeps calling
// schedule() and popImminent(). Write that comment here; it is the argument for
// every wrapper you will ever write.

#pragma once

#include <queue>
#include <vector>
#include <functional>
#include <cstddef>
#include "Common.hpp"
#include "EventNotice.hpp"

class FutureEventList {
private:
    std::priority_queue<EventNotice,
                        std::vector<EventNotice>,
                        std::greater<EventNotice>> m_fel;

public:
    FutureEventList() = default;

    bool isEmpty() const { return m_fel.empty(); }
    std::size_t size() const { return m_fel.size(); }

    void schedule(const EventNotice& e);
    EventNotice popImminent();
    SimTime nextEventTime() const;
};

