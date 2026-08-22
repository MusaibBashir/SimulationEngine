// ============================================================================
// QueueRule.cpp
// ============================================================================

#include "QueueRule.hpp"
#include "Entity.hpp"
#include "RandomStream.hpp"
#include <cassert>

namespace {
// One scan, parameterised. This is what the three duplicated loops in v2 turned
// into: a single algorithm that both attribute rules share.
std::size_t scanExtreme(const std::deque<Entity*>& q,
                        const std::string& attribute,
                        bool wantLargest) {
    assert(!q.empty());
    std::size_t bestIdx = 0;
    double best = q[0]->attribute(attribute);
    for (std::size_t i = 1; i < q.size(); ++i) {
        const double val = q[i]->attribute(attribute);
        if (wantLargest ? (val > best) : (val < best)) {
            best    = val;
            bestIdx = i;
        }
    }
    // Ties resolve to the EARLIEST index, i.e. FIFO among equals. That is a
    // real modelling decision, not an accident of the loop -- `>` rather than
    // `>=` is what makes it so. Say it out loud so nobody "tidies" it later.
    return bestIdx;
}
}  // namespace

std::size_t FifoRule::selectIndex(const std::deque<Entity*>& q, RandomStream&) const {
    assert(!q.empty());
    return 0;
}

std::size_t LifoRule::selectIndex(const std::deque<Entity*>& q, RandomStream&) const {
    assert(!q.empty());
    return q.size() - 1;
}

HighestAttributeRule::HighestAttributeRule(std::string attribute)
    : m_attribute(std::move(attribute)) {}

std::size_t HighestAttributeRule::selectIndex(const std::deque<Entity*>& q,
                                              RandomStream&) const {
    return scanExtreme(q, m_attribute, /*wantLargest=*/true);
}

std::string HighestAttributeRule::name() const { return "Highest(" + m_attribute + ")"; }

LowestAttributeRule::LowestAttributeRule(std::string attribute)
    : m_attribute(std::move(attribute)) {}

std::size_t LowestAttributeRule::selectIndex(const std::deque<Entity*>& q,
                                             RandomStream&) const {
    return scanExtreme(q, m_attribute, /*wantLargest=*/false);
}

std::string LowestAttributeRule::name() const { return "Lowest(" + m_attribute + ")"; }

std::size_t RandomRule::selectIndex(const std::deque<Entity*>& q, RandomStream& rng) const {
    assert(!q.empty());
    return rng.uniformIndex(q.size());
}

std::unique_ptr<IQueueRule> makeQueueRule(QueueDiscipline d) {
    switch (d) {
        case QueueDiscipline::FIFO:     return std::make_unique<FifoRule>();
        case QueueDiscipline::LIFO:     return std::make_unique<LifoRule>();
        case QueueDiscipline::Priority: return std::make_unique<HighestAttributeRule>("priority");
        case QueueDiscipline::SPT:      return std::make_unique<LowestAttributeRule>("serviceTime");
        case QueueDiscipline::EDD:      return std::make_unique<LowestAttributeRule>("dueDate");
        case QueueDiscipline::Random:   return std::make_unique<RandomRule>();
    }
    // This factory is now the ONE place a -Wswitch warning still fires when a
    // new enumerator is added -- which is exactly why the enum was kept rather
    // than deleted. The compiler stays a checklist for one file instead of none.
    assert(false && "unhandled QueueDiscipline");
    return std::make_unique<FifoRule>();
}
