// ============================================================================
// EntityQueue.hpp  --  the WAITING LINE
// ============================================================================
// Theory: "A list of entities waiting", plus "Queue discipline: the rule
// deciding who is served next."
//
// NOT named Queue.hpp: the standard library already has <queue>, and a header
// named Queue.hpp is a coin-flip on a case-insensitive filesystem.
//
// v3: the discipline is now an IQueueRule object rather than an enum + switch.
// The queue still owns the deque and still enforces its own invariants; the
// rule only answers "which index next?". See QueueRule.hpp for what that trade
// buys and what it costs.

#pragma once
#include <deque>
#include <string>
#include <memory>
#include <cstddef>
#include "Common.hpp"
#include "QueueRule.hpp"

class Entity;
class RandomStream;

class EntityQueue {
private:
    std::string m_name;
    std::deque<Entity*> m_waiting;   // deque: O(1) at BOTH ends (FIFO pops the
                                     // front, LIFO the back) AND random access
                                     // for the attribute rules to scan
    std::size_t m_maxLengthObserved{0};
    std::unique_ptr<IQueueRule> m_rule;
    RandomStream* m_rng{nullptr};    // non-owning; only RandomRule needs it

public:
    EntityQueue(const std::string& name, QueueDiscipline discipline);
    EntityQueue(const std::string& name, std::unique_ptr<IQueueRule> rule);

    EntityQueue(const EntityQueue&) = delete;
    EntityQueue& operator=(const EntityQueue&) = delete;

    const std::string& name() const { return m_name; }
    std::string ruleName() const { return m_rule->name(); }
    std::size_t length() const { return m_waiting.size(); }
    bool isEmpty() const { return m_waiting.empty(); }
    std::size_t maxLengthObserved() const { return m_maxLengthObserved; }
    const std::deque<Entity*>& contents() const { return m_waiting; }

    void setRandomStream(RandomStream* rng) { m_rng = rng; }

    // v2.1: back to the t=0 condition. Clearing m_waiting also drops the
    // dangling Entity* left over from a previous replication.
    void reset();

    void push(Entity* e);

    // Returns nullptr if and only if the queue is empty. Any other failure
    // asserts -- a discipline that cannot choose must not silently lose an
    // entity, which is exactly the bug v2 shipped.
    Entity* pop();
};
