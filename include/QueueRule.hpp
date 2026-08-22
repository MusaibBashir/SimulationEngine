// ============================================================================
// QueueRule.hpp  --  v3 step 2: queue disciplines become strategy objects
// ============================================================================
// The motivation is concrete, and you wrote it yourself: Priority, SPT and EDD
// were three copies of one scan loop, differing only in an attribute name and a
// comparison direction. v2.1 collapsed them into one file-local helper. This is
// the principled version of the same move.
//
// *** WHAT THIS BUYS AND WHAT IT COSTS -- the actual lesson. ***
// Buys : adding a discipline no longer means editing EntityQueue. Open for
//        extension, closed for modification, and here that is not a slogan --
//        EntityQueue.cpp genuinely stops changing.
// Costs: you can no longer `switch` over an enum and let -Wswitch tell you that
//        a new discipline is unhandled. The compiler stops being your checklist.
//
// That trade is why abstraction is a decision and not an upgrade.

#pragma once

#include <deque>
#include <string>
#include <memory>
#include <cstddef>
#include "Common.hpp"

class Entity;
class RandomStream;

class IQueueRule {
public:
    virtual ~IQueueRule() = default;

    // Returns the INDEX of the entity to serve next, not the entity itself.
    // Deliberate: the rule decides *who*, the queue does the removing. A rule
    // that could erase from the deque could also corrupt m_maxLengthObserved,
    // and there would be two places responsible for the queue's invariants.
    virtual std::size_t selectIndex(const std::deque<Entity*>& waiting,
                                    RandomStream& rng) const = 0;

    virtual std::string name() const = 0;
};

// --- the two positional rules ------------------------------------------------
class FifoRule : public IQueueRule {
public:
    std::size_t selectIndex(const std::deque<Entity*>&, RandomStream&) const override;
    std::string name() const override { return "FIFO"; }
};

class LifoRule : public IQueueRule {
public:
    std::size_t selectIndex(const std::deque<Entity*>&, RandomStream&) const override;
    std::string name() const override { return "LIFO"; }
};

// --- the attribute rules -----------------------------------------------------
// Priority, SPT and EDD are all "scan for the extreme value of some attribute".
// Once that is said out loud there are only TWO classes here, not three, and
// the difference between SPT and EDD is a string.
class HighestAttributeRule : public IQueueRule {
    std::string m_attribute;
public:
    explicit HighestAttributeRule(std::string attribute);
    std::size_t selectIndex(const std::deque<Entity*>&, RandomStream&) const override;
    std::string name() const override;
};

class LowestAttributeRule : public IQueueRule {
    std::string m_attribute;
public:
    explicit LowestAttributeRule(std::string attribute);
    std::size_t selectIndex(const std::deque<Entity*>&, RandomStream&) const override;
    std::string name() const override;
};

class RandomRule : public IQueueRule {
public:
    std::size_t selectIndex(const std::deque<Entity*>&, RandomStream&) const override;
    std::string name() const override { return "Random"; }
};

// Bridge from the v1/v2 enum to the v3 objects, so existing model-building code
// keeps working. A factory like this is the usual way to introduce a hierarchy
// without breaking every caller on the same day.
std::unique_ptr<IQueueRule> makeQueueRule(QueueDiscipline d);
