// ============================================================================
// TerminationRule.cpp
// ============================================================================

#include <algorithm>
#include "TerminationRule.hpp"
#include "SimulationSystem.hpp"   // full definition needed: the rules query it
#include <cassert>
#include <sstream>

namespace des {


TimeLimit::TimeLimit(SimTime maxTime) : m_maxTime(maxTime) { assert(maxTime > 0.0); }

bool TimeLimit::isMet(const SimulationSystem& sim) const {
    return sim.clock().now() >= m_maxTime;
}

std::string TimeLimit::describe() const {
    std::ostringstream os; os << "TimeLimit(" << m_maxTime << ")"; return os.str();
}

std::optional<double> TimeLimit::progress(const SimulationSystem& sim) const {
    if (!(m_maxTime > 0.0)) return std::nullopt;
    return std::min(1.0, sim.now() / m_maxTime);
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

std::optional<double> EntityLimit::progress(const SimulationSystem& sim) const {
    if (m_maxEntities <= 0) return std::nullopt;
    const double done = static_cast<double>(sim.statistics().numberServed());
    return std::min(1.0, done / static_cast<double>(m_maxEntities));
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

std::optional<double> AnyOf::progress(const SimulationSystem& sim) const {
    // The LARGEST fraction any child knows. The run ends when the FIRST rule
    // is met, so the most advanced child is the honest estimate; averaging
    // with one that has barely started would report less progress than is
    // real. Nothing when no child can tell -- not zero.
    std::optional<double> best;
    for (const auto& r : m_rules) {
        const std::optional<double> f = r->progress(sim);
        if (f && (!best || *f > *best)) best = f;
    }
    return best;
}

std::optional<SimTime> AnyOf::stopTime() const {
    // The EARLIEST time any child knows, which is the mirror image of
    // progress() taking the largest fraction and the same argument: this
    // composite is met when the FIRST child is met, so the first knowable
    // deadline is the one the clock must stop on. A child that cannot say
    // (EntityLimit, Drained) may still end the run sooner -- canStep() is
    // what notices that -- but it cannot move the deadline.
    std::optional<SimTime> earliest;
    for (const auto& r : m_rules) {
        const std::optional<SimTime> t = r->stopTime();
        if (t && (!earliest || *t < *earliest)) earliest = t;
    }
    return earliest;
}

bool AnyOf::isMet(const SimulationSystem& sim) const {
    for (const auto& r : m_rules) {
        if (r->isMet(sim)) return true;
    }
    return false;
}

std::string AnyOf::describe() const {
    // An EMPTY AnyOf is never met, so the run ends when the event list empties.
    // "AnyOf[]" is technically what it is and tells a reader nothing; a report
    // header should say what actually stopped the run.
    if (m_rules.empty()) return "until the model runs out of events";
    std::ostringstream os;
    os << "AnyOf[";
    for (std::size_t i = 0; i < m_rules.size(); ++i) {
        if (i) os << " | ";
        os << m_rules[i]->describe();
    }
    os << "]";
    return os.str();
}

}  // namespace des
