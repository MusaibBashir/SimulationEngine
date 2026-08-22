// ============================================================================
// TerminationRule.hpp  --  v3 step 6: stopping conditions become a hierarchy
// ============================================================================
// TerminationCondition.hpp has been flagging this since v1, and v1 explicitly
// said DON'T DO IT YET: two criteria joined by an OR is the right amount of
// machinery for two criteria. Now there is a third (idle-and-drained) and a
// need to compose them, so the hierarchy is earned rather than anticipated.
//
// Compare the two shapes and notice what changed:
//   v1:  bool isMet(SimTime now, int served) const;         // hardcoded OR
//   v3:  virtual bool isMet(const SimulationSystem&) const; // asks the system
// Passing the whole system means a rule can look at anything -- queue length,
// utilisation, a confidence interval in v4 -- without the signature changing
// again. That is the real payoff, and it is worth more than the polymorphism.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Common.hpp"

class SimulationSystem;

class ITerminationRule {
public:
    virtual ~ITerminationRule() = default;
    virtual bool isMet(const SimulationSystem& sim) const = 0;
    virtual std::string describe() const = 0;
};

class TimeLimit : public ITerminationRule {
    SimTime m_maxTime;
public:
    explicit TimeLimit(SimTime maxTime);
    bool isMet(const SimulationSystem& sim) const override;
    std::string describe() const override;
    SimTime maxTime() const { return m_maxTime; }
};

class EntityLimit : public ITerminationRule {
    int m_maxEntities;
public:
    explicit EntityLimit(int maxEntities);
    bool isMet(const SimulationSystem& sim) const override;
    std::string describe() const override;
};

// Stop once the system has drained: nothing in service, nothing waiting.
// Impossible to express in the v1 two-field struct, which is the concrete
// reason the hierarchy exists.
class DrainedRule : public ITerminationRule {
public:
    bool isMet(const SimulationSystem& sim) const override;
    std::string describe() const override;
};

// Composite: met when ANY child is met. The v1 hardcoded OR, now a first-class
// object you can build at runtime out of however many rules you like.
class AnyOf : public ITerminationRule {
    std::vector<std::unique_ptr<ITerminationRule>> m_rules;
public:
    AnyOf() = default;
    AnyOf& add(std::unique_ptr<ITerminationRule> rule);
    bool isMet(const SimulationSystem& sim) const override;
    std::string describe() const override;
    bool empty() const { return m_rules.empty(); }
};
