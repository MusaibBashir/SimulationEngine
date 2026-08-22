// ============================================================================
// TerminationCondition.cpp
// ============================================================================
// [1] Include "TerminationCondition.hpp".
// [2] Constructor definition: two members from two parameters.
// [3] v1 STUB BODY:
//     bool TerminationCondition::isMet(SimTime now, int entitiesServed) const
//     // TODO v2, one line: return now >= m_maxTime || entitiesServed >= m_maxEntities;
//     // stay a single expression with no branching on "was a limit set".

#include "TerminationCondition.hpp"

TerminationCondition::TerminationCondition(SimTime maxTime, int maxEntities)
    : m_maxTime(maxTime), m_maxEntities(maxEntities) {}

bool TerminationCondition::isMet(SimTime now, int entitiesServed) const {
    // The infinity / INT_MAX sentinels the caller supplies are what let this
    // stay a single expression, with no branching on "was a limit even set".
    return now >= m_maxTime || entitiesServed >= m_maxEntities;
}

