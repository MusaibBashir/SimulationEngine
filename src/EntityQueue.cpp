// ============================================================================
// EntityQueue.cpp
// ============================================================================
// [1] Include "EntityQueue.hpp" first, then "Entity.hpp" (you forward-declared
//     Entity in the header, so the FULL definition must arrive here -- pop()
//     will read entity attributes in v2). This split is the payoff of the
//     forward declaration: files that only pass queues around never pay the
//     cost of parsing Entity.hpp.
//
// [2] Constructor definition, if not inlined.
//
// [3] v1 STUB BODIES:
//
//     [3a] void EntityQueue::push(Entity* e)
//          // TODO v2, three lines:
//          //   guard against a null pointer (assert or early return -- decide)
//          //   m_waiting.push_back(e)
//          //   if (m_waiting.size() > m_maxLengthObserved) update it
//          The third line is a statistic riding along with the operation. Note
//          how naturally it lives here rather than in Statistics -- max queue
//          length is knowledge the queue already has.
//
//     [3b] Entity* EntityQueue::pop()
//          // TODO v2. Structure it as:
//          //   if empty -> return nullptr   (the contract you documented)
//          //   switch (m_discipline):
//          //     FIFO     : take m_waiting.front(), pop_front(), return it
//          //     LIFO     : take m_waiting.back(),  pop_back(),  return it
//          //     Priority : loop the deque tracking the index of the MAX
//          //                attribute("priority"); erase at begin()+index
//          //     SPT      : same loop, MIN attribute("serviceTime")
//          //     EDD      : same loop, MIN attribute("dueDate")
//          //     Random   : uniform index in [0, size), erase there
//          //                (needs the RNG that v2 introduces)
//          //
//          // Notice that Priority / SPT / EDD are the SAME loop with a
//          // different attribute name and a different comparison. Write all
//          // three out longhand anyway in v2. Seeing the triplication with your
//          // own eyes is what makes the v3 refactor -- one scan function taking
//          // a comparator, or a QueueDiscipline strategy object -- feel
//          // necessary rather than clever.

#include "EntityQueue.hpp"
#include "Entity.hpp"        // pop() reads attributes, so the FULL definition is
                             // needed HERE -- and only here. That is the payoff
                             // of the forward declaration in the header.
#include "RandomStream.hpp"  // same reasoning
#include <cassert>

EntityQueue::EntityQueue(const std::string& name, QueueDiscipline discipline)
    : m_name(name), m_discipline(discipline), m_maxLengthObserved(0), m_rng(nullptr) {
    // deque default-constructs empty
}

void EntityQueue::push(Entity* e) {
    assert(e != nullptr);
    m_waiting.push_back(e);
    // A statistic riding along with the operation. It lives here rather than in
    // Statistics because max queue length is knowledge the queue already has.
    if (m_waiting.size() > m_maxLengthObserved) {
        m_maxLengthObserved = m_waiting.size();
    }
}

// ----------------------------------------------------------------------------
// Local helper: scan the deque for the entity whose named attribute is the
// smallest (or largest), remove it, and return it.
//
// v2 NOTE ON DUPLICATION: Priority, SPT and EDD were three copies of the same
// loop differing only in an attribute name and a comparison direction. Three
// copies is where duplication stops being tolerable, so it is collapsed here
// into one function with two parameters. This is deliberately NOT the full v3
// abstraction (a discipline strategy object) -- it is the smallest change that
// removes the triplication, which is usually the right first move.
// ----------------------------------------------------------------------------
namespace {
Entity* extractBest(std::deque<Entity*>& q, const std::string& attribute, bool wantLargest) {
    auto bestIt = q.begin();
    double best = (*bestIt)->attribute(attribute);
    for (auto it = q.begin() + 1; it != q.end(); ++it) {
        const double val = (*it)->attribute(attribute);
        if (wantLargest ? (val > best) : (val < best)) {
            best   = val;
            bestIt = it;
        }
    }
    Entity* selected = *bestIt;
    q.erase(bestIt);   // erase invalidates iterators, but we are done with them
    return selected;
}
}  // anonymous namespace -- internal linkage, invisible outside this file

Entity* EntityQueue::pop() {
    if (isEmpty()) {
        return nullptr;   // the documented contract
    }

    switch (m_discipline) {
        case QueueDiscipline::FIFO: {
            Entity* selected = m_waiting.front();
            m_waiting.pop_front();
            return selected;
        }
        case QueueDiscipline::LIFO: {
            Entity* selected = m_waiting.back();
            m_waiting.pop_back();
            return selected;
        }
        case QueueDiscipline::Priority:
            return extractBest(m_waiting, "priority", /*wantLargest=*/true);
        case QueueDiscipline::SPT:
            return extractBest(m_waiting, "serviceTime", /*wantLargest=*/false);
        case QueueDiscipline::EDD:
            return extractBest(m_waiting, "dueDate", /*wantLargest=*/false);
        case QueueDiscipline::Random: {
            // Previously returned nullptr, which SILENTLY LOST an entity: the
            // caller saw "queue not empty" and then got nothing back. A
            // discipline that cannot be served must assert, never return a
            // plausible-looking null.
            assert(m_rng != nullptr && "Random discipline needs setRandomStream()");
            const std::size_t idx = m_rng->uniformIndex(m_waiting.size());
            auto it = m_waiting.begin() + static_cast<std::ptrdiff_t>(idx);
            Entity* selected = *it;
            m_waiting.erase(it);
            return selected;
        }
    }

    // Every enumerator is handled above, so this line is unreachable. It exists
    // only to satisfy compilers that cannot prove that. No default: label --
    // omitting it means -Wswitch warns when v3 adds a new discipline, which is
    // exactly the reminder you want.
    assert(false && "unhandled queue discipline");
    return nullptr;
}
