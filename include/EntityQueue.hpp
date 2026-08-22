// ============================================================================
// EntityQueue.hpp  --  the WAITING LINE
// ============================================================================
// Theory: "A list of entities waiting (for service, or for other reasons)"
// plus "Queue discipline: the rule deciding who is served next."
//
// *** THE FILE IS NOT CALLED Queue.hpp ON PURPOSE. *** The standard library
// already has <queue>; a header named Queue.hpp is a coin-flip over which one
// the compiler finds on a case-insensitive filesystem. Name collisions with the
// standard library are a real and boring source of lost evenings.
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
//
// [2] Includes: <deque>, <string>, <cstddef> (for size_t),
//     "Common.hpp" (QueueDiscipline), "Entity.hpp" (for the Entity* type).
//     -- Strictly you only need a FORWARD DECLARATION of Entity here, because
//        you only store pointers. Write `class Entity;` above the class instead
//        of including the header, and see that it still compiles. That is the
//        cheapest compile-time-dependency lesson in the project.
//
// [3] class EntityQueue, private data first:
//
//     [3a] std::string m_name
//     [3b] QueueDiscipline m_discipline    : which rule this queue obeys.
//     [3c] std::deque<Entity*> m_waiting   : the line itself.
//     [3d] std::size_t m_maxLengthObserved : a statistic that naturally belongs
//                                            to the queue, not to Statistics.
//
//     WHY std::deque AND NOT SOMETHING ELSE -- write this down as a comment:
//       vector : front-removal is O(n). FIFO pops the front constantly. No.
//       list   : no random access, so Priority/SPT/EDD cannot scan efficiently.
//       deque  : O(1) at BOTH ends (FIFO pops front, LIFO pops back) AND random
//                access for the scanning disciplines. It is the compromise that
//                serves all six disciplines in QueueDiscipline.
//
//     WHY Entity* AND NOT Entity -- write this down too:
//       An entity object exists in exactly ONE place: the SimulationSystem owns
//       it. The queue holds a non-owning reference to it. Storing Entity by
//       value would copy it, and then the copy in the queue and the original
//       would drift apart. This is the ownership question C++ forces you to
//       answer and Python lets you ignore forever.
//       v1 uses a raw pointer = "I observe this, I do not own it". Correct here.
//
// [4] PUBLIC INTERFACE (implemented in v1, these are pure storage):
//
//     [4a] Constructor (std::string name, QueueDiscipline discipline)
//          Init both, m_maxLengthObserved to 0, deque default-empty.
//     [4b] const std::string& name() const
//     [4c] QueueDiscipline discipline() const
//     [4d] std::size_t length() const        -> m_waiting.size()
//     [4e] bool isEmpty() const              -> m_waiting.empty()
//     [4f] std::size_t maxLengthObserved() const
//
// [5] V1 STUBS (declare, empty body + TODO in the .cpp):
//
//     [5a] void push(Entity* e);
//          v2: push_back, then if the new length exceeds m_maxLengthObserved,
//          update it. Two lines, and the second one is the statistic.
//
//     [5b] Entity* pop();
//          v2: THE INTERESTING ONE. Switch on m_discipline:
//            FIFO     -> take front
//            LIFO     -> take back
//            Priority -> scan for max "priority" attribute, erase at that index
//            SPT      -> scan for min "serviceTime" attribute
//            EDD      -> scan for min "dueDate" attribute
//            Random   -> uniform index from the RNG (which v1 does not have yet)
//          Returns nullptr when empty -- decide that contract NOW and write it
//          as a comment, because the caller must handle it.
//          When you write this switch in v2 and it gets ugly, that is your
//          motivation for the Strategy pattern in v3. Feel the pain first.
//
// [6] Close class with semicolon.

#pragma once
#include <deque>
#include <string>
#include <cstddef>
#include "Common.hpp"

class Entity;  // forward declaration, not include, because we only store pointers

class EntityQueue {
    private:
        std::string m_name;
        QueueDiscipline m_discipline;
        std::deque<Entity*> m_waiting;
        std::size_t m_maxLengthObserved;

    public:
        EntityQueue(const std::string& name, QueueDiscipline discipline);
        const std::string& name() const { return m_name; }
        QueueDiscipline discipline() const { return m_discipline; }
        std::size_t length() const { return m_waiting.size(); }
        bool isEmpty() const { return m_waiting.empty(); }
        std::size_t maxLengthObserved() const { return m_maxLengthObserved; }

        void push(Entity* e);
        Entity* pop();
};