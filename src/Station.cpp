// ============================================================================
// Station.cpp
// ============================================================================

#include "Station.hpp"
#include <cassert>
#include <sstream>
#include "Entity.hpp"
#include "RandomStream.hpp"

namespace des {


Station::Station(std::string name, int capacity,
                 std::unique_ptr<IQueueRule> rule,
                 std::unique_ptr<IDistribution> service)
    : m_name(std::move(name)),
      m_resource(m_name, capacity),
      m_queue(m_name + "Queue", std::move(rule)),
      m_service(std::move(service)) {
    assert(m_service != nullptr && "a station needs a service distribution");
}

void Station::setServiceFromAttribute(const std::string& attributeName) {
    m_serviceAttribute = attributeName;
}

SimTime Station::drawService(const Entity& e, RandomStream& rng) {
    if (m_serviceAttribute.empty()) return m_service->draw(rng);
    assert(e.hasAttribute(m_serviceAttribute) &&
           "station reads service time from an attribute the entity does not have");
    const SimTime t = e.attribute(m_serviceAttribute);
    assert(t >= 0.0 && "negative service time");
    return t;
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
       << ", service " << (m_serviceAttribute.empty()
                            ? m_service->describe()
                            : "attribute:" + m_serviceAttribute)
       << ", next=" << (m_next ? m_next->name() : std::string("exit"))
       << "]";
    return os.str();
}

}  // namespace des
