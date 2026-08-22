// ============================================================================
// Station.cpp
// ============================================================================

#include "Station.hpp"
#include <cassert>
#include <sstream>

Station::Station(std::string name, int capacity,
                 std::unique_ptr<IQueueRule> rule,
                 std::unique_ptr<IDistribution> service)
    : m_name(std::move(name)),
      m_resource(m_name, capacity),
      m_queue(m_name + "Queue", std::move(rule)),
      m_service(std::move(service)) {
    assert(m_service != nullptr && "a station needs a service distribution");
}

void Station::reset() {
    m_resource.reset();
    m_queue.reset();
    m_stats.reset();
    m_service->reset();   // Deterministic rewinds; everyone else no-ops
    // m_next is topology, not run state -- it survives.
}

std::string Station::describe() const {
    std::ostringstream os;
    os << m_name
       << " [c=" << m_resource.capacity()
       << ", " << m_queue.ruleName()
       << ", " << m_service->describe()
       << ", next=" << (m_next ? m_next->name() : std::string("exit"))
       << "]";
    return os.str();
}
