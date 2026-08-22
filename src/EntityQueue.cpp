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

EntityQueue::EntityQueue(const std::string& name, QueueDiscipline discipline)
    : m_name(name), m_discipline(discipline), m_maxLengthObserved(0) {
    // deque default-constructs empty
}

void EntityQueue::push(Entity* /*e*/) {
    // TODO v2 -- three lines:
    //   assert(e != nullptr);
    //   m_waiting.push_back(e);
    //   if (m_waiting.size() > m_maxLengthObserved)
    //       m_maxLengthObserved = m_waiting.size();
    // The third line is a statistic riding along with the operation. It lives
    // here rather than in Statistics because max queue length is knowledge the
    // queue already has -- no need to tell anyone else about it.
}

Entity* EntityQueue::pop() {
    // TODO v2 -- THE INTERESTING ONE.
    //
    // It needs the full Entity definition (to read attributes), so add
    //     #include "Entity.hpp"
    // at the top of THIS FILE -- not the header. That is precisely the payoff
    // of the forward declaration in EntityQueue.hpp: the cost lands in one
    // .cpp instead of in every file that merely passes a queue around.
    //
    //   if (isEmpty()) return nullptr;            // the documented contract
    //   switch (m_discipline) {
    //     FIFO     : take front(), pop_front()
    //     LIFO     : take back(),  pop_back()
    //     Priority : scan for MAX attribute("priority"),    erase at that index
    //     SPT      : scan for MIN attribute("serviceTime"), erase at that index
    //     EDD      : scan for MIN attribute("dueDate"),     erase at that index
    //     Random   : uniform index in [0, size), erase there (needs the v2 RNG)
    //   }
    //
    // Priority / SPT / EDD are the SAME loop with a different attribute name
    // and a flipped comparison. Write all three out longhand anyway. Seeing the
    // triplication with your own eyes is what makes the v3 refactor (one scan
    // taking a comparator, or a discipline strategy object) feel necessary
    // rather than clever.
    return nullptr;   // v1 placeholder
}