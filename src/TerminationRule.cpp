// ============================================================================
// TerminationRule.cpp
// ============================================================================

#include "TerminationRule.hpp"
#include "SimulationSystem.hpp"   // full definition needed: the rules query it
#include <cassert>
#include <sstream>

TimeLimit::TimeLimit(SimTime maxTime) : m_maxTime(maxTime) { assert(maxTime > 0.0); }

bool TimeLimit::isMet(const SimulationSystem& sim) const {
    return sim.clock().now() >= m_maxTime;
}

std::string TimeLimit::describe() const {
    std::ostringstream os; os << "TimeLimit(" << m_maxTime << ")"; return os.str();
}

EntityLimit::EntityLimit(int maxEntities) : m_maxEntities(maxEntities) {
    assert(maxEntities > 0);
}

bool EntityLimit::isMet(const SimulationSystem& sim) const {
    return sim.statistics().numberServed() >= m_maxEntities;
}

std::string EntityLimit::describe() const {
    std::ostringstream os; os << "EntityLimit(" << m_maxEntities << ")"; return os.str();
}

bool DrainedRule::isMet(const SimulationSystem& sim) const {
    return sim.state().numberInSystem() == 0 && sim.statistics().numberArrived() > 0;
}

std::string DrainedRule::describe() const { return "Drained"; }

AnyOf& AnyOf::add(std::unique_ptr<ITerminationRule> rule) {
    assert(rule != nullptr);
    m_rules.push_back(std::move(rule));
    return *this;   // returning *this lets callers chain .add().add()
}

bool AnyOf::isMet(const SimulationSystem& sim) const {
    for (const auto& r : m_rules) {
        if (r->isMet(sim)) return true;
    }
    return false;
}

std::string AnyOf::describe() const {
    std::ostringstream os;
    os << "AnyOf[";
    for (std::size_t i = 0; i < m_rules.size(); ++i) {
        if (i) os << " | ";
        os << m_rules[i]->describe();
    }
    os << "]";
    return os.str();
}
