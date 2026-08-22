// ============================================================================
// EntityQueue.cpp
// ============================================================================
// Entity.hpp is included HERE, not in the header -- the header only stores
// Entity*, so it forward-declares. That keeps Entity.hpp out of every file that
// merely passes a queue around.

#include "EntityQueue.hpp"
#include "Entity.hpp"
#include "RandomStream.hpp"
#include <cassert>

namespace des {


EntityQueue::EntityQueue(const std::string& name, QueueDiscipline discipline)
    : m_name(name), m_rule(makeQueueRule(discipline)) {}

EntityQueue::EntityQueue(const std::string& name, std::unique_ptr<IQueueRule> rule)
    : m_name(name), m_rule(std::move(rule)) {
    assert(m_rule != nullptr && "a queue needs a discipline");
}

void EntityQueue::reset() {
    m_waiting.clear();
    m_maxLengthObserved = 0;
    // m_name, m_rule and m_rng are configuration, not run state.
}

void EntityQueue::resetStatistics() {
    m_maxLengthObserved = m_waiting.size();   // the current length is a real
                                              // observation in the new window
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

bool EntityQueue::remove(Entity* e) {
    for (auto it = m_waiting.begin(); it != m_waiting.end(); ++it) {
        if (*it == e) { m_waiting.erase(it); return true; }
    }
    return false;
}

Entity* EntityQueue::pop() {
    if (isEmpty()) return nullptr;   // the documented contract

    // The v2 switch is gone. The rule picks an index; the queue does the
    // removing, so the queue stays the only thing that can touch its own deque.
    assert(m_rng != nullptr && "queue needs a RandomStream -- call setRandomStream()");
    const std::size_t idx = m_rule->selectIndex(m_waiting, *m_rng);
    assert(idx < m_waiting.size() && "queue rule returned an out-of-range index");

    Entity* selected = m_waiting[idx];
    m_waiting.erase(m_waiting.begin() + static_cast<std::ptrdiff_t>(idx));
    return selected;
}

}  // namespace des
