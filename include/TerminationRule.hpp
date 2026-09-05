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
#include <optional>
#include <string>
#include <vector>
#include "Common.hpp"

namespace des {


class SimulationSystem;

class ITerminationRule {
public:
    virtual ~ITerminationRule() = default;
    virtual bool isMet(const SimulationSystem& sim) const = 0;
    virtual std::string describe() const = 0;

    // v12: how far through this rule the run is, as a fraction, or NOTHING.
    //
    // Nothing means CANNOT TELL. It does not mean zero, and the difference is
    // the whole reason this returns an optional: a whenDrained() run has no
    // knowable fraction, and a progress bar that reads 0% for its whole
    // duration and then jumps to 100% is not an approximation, it is a lie the
    // caller has no way to detect. This is the fourth time this project has
    // had to write that rule down, after VisitRatios::exact, v9's silently
    // zero WIP, and v10's unknowable arrival mean.
    //
    // Defaulted, so a rule that cannot answer says so by saying nothing.
    virtual std::optional<double> progress(const SimulationSystem&) const {
        return std::nullopt;
    }
};

class TimeLimit : public ITerminationRule {
    SimTime m_maxTime;
public:
    explicit TimeLimit(SimTime maxTime);
    bool isMet(const SimulationSystem& sim) const override;
    std::string describe() const override;
    std::optional<double> progress(const SimulationSystem& sim) const override;
    SimTime maxTime() const { return m_maxTime; }
};

class EntityLimit : public ITerminationRule {
    int m_maxEntities;
public:
    explicit EntityLimit(int maxEntities);
    bool isMet(const SimulationSystem& sim) const override;
    std::string describe() const override;
    std::optional<double> progress(const SimulationSystem& sim) const override;
};

// Stop once the system has drained: nothing in service, nothing waiting.
// Impossible to express in the v1 two-field struct, which is the concrete
// reason the hierarchy exists.
class DrainedRule : public ITerminationRule {
public:
    bool isMet(const SimulationSystem& sim) const override;
    std::string describe() const override;
    // NO progress() override, deliberately. Whether a system will next be
    // empty is not knowable in advance, so the honest answer is the base
    // class's nothing. Adding one here that returned 0 until the moment it
    // returned 1 is the exact lie the base comment describes.
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
    std::optional<double> progress(const SimulationSystem& sim) const override;
    bool empty() const { return m_rules.empty(); }
};

}  // namespace des
