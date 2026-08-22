// ============================================================================
// Nodes.hpp  --  the rest of the block set
// ============================================================================
// Process (seize-delay-release) lives in Station.hpp. These are the blocks that
// make a flowchart out of a queue.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "Common.hpp"
#include "Node.hpp"
#include "Distribution.hpp"
#include "Statistics.hpp"

namespace des {

// ---------------------------------------------------------------------------
// DELAY -- hold the entity for a time. No resource, so no queue and no
// contention: a hundred entities can be in a Delay at once. Use it for travel,
// cooling, curing, paperwork -- anything that takes time but competes for
// nothing. Modelling those as a Process with huge capacity would work and would
// also report a meaningless utilisation.
// ---------------------------------------------------------------------------
class DelayNode : public INode {
    std::unique_ptr<IDistribution> m_duration;
    Statistics m_stats;
    int m_inTransit{0};
public:
    DelayNode(std::string name, std::unique_ptr<IDistribution> duration);
    void enter(NodeContext& ctx, Entity* e) override;
    void onScheduledEvent(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;
    const Statistics& stats() const { return m_stats; }
    int inTransit() const { return m_inTransit; }
};

// ---------------------------------------------------------------------------
// ASSIGN -- set attributes on the entity passing through. Instant.
// Two forms: a fixed value, or a draw from a distribution.
// ---------------------------------------------------------------------------
class AssignNode : public INode {
public:
    struct Rule { std::string name; std::unique_ptr<IDistribution> value; };
private:
    std::vector<Rule> m_rules;
    long long m_count{0};
public:
    explicit AssignNode(std::string name);
    AssignNode& set(const std::string& attribute, std::unique_ptr<IDistribution> value);
    void enter(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;
    long long count() const { return m_count; }
};

// ---------------------------------------------------------------------------
// DECIDE -- branch. Two flavours, and the difference matters:
//
//   BY CHANCE     : a fixed probability. "10% of parts fail inspection."
//   BY CONDITION  : a predicate on the entity. "priority > 3 goes to the
//                   express lane." Deterministic given the entity.
//
// The engine can compute visit ratios for chance branches, so the stability
// check still works. It cannot for conditions -- the fraction taking each branch
// is an output of the simulation, not an input -- and it says so rather than
// guessing.
// ---------------------------------------------------------------------------
class DecideNode : public INode {
public:
    using Condition = std::function<bool(const Entity&)>;
private:
    bool m_byChance{true};
    double m_probability{0.5};   // probability of taking the TRUE branch
    Condition m_condition;
    INode* m_ifTrue{nullptr};
    long long m_tookTrue{0}, m_tookFalse{0};
public:
    // byChance: `probability` of going to the true branch.
    DecideNode(std::string name, double probability);
    // byCondition: predicate decides.
    DecideNode(std::string name, Condition condition);

    void setTrueBranch(INode* n) { m_ifTrue = n; }
    INode* trueBranch() const { return m_ifTrue; }
    // setNext() is the FALSE branch, so a Decide with only a true branch set
    // falls through to whatever comes next -- which reads the way people expect.

    void enter(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;

    bool isByChance() const { return m_byChance; }
    double probability() const { return m_probability; }
    long long tookTrue() const { return m_tookTrue; }
    long long tookFalse() const { return m_tookFalse; }
};

// ---------------------------------------------------------------------------
// BATCH -- accumulate `size` entities, then send ONE representative onward
// carrying the members. Palletising, packing, a table of four waiting to be
// seated, a truck that leaves when full.
//
// PERMANENT vs TEMPORARY is the decision to get right:
//   Temporary : members are kept, and a later Separate can split them back out.
//               Use when the grouping is transport and the items still matter.
//   Permanent : members are consumed. The batch IS the thing from now on.
//               Use when ten parts become one assembly.
//
// The representative's creation time is the OLDEST member's, so time-in-system
// measures how long the first arrival waited for the group -- which is what you
// wanted to know, and is easy to get silently wrong by using "now".
// ---------------------------------------------------------------------------
class BatchNode : public INode {
    std::size_t m_size;
    bool m_permanent;
    std::vector<Entity*> m_waiting;
    long long m_batchesFormed{0};
    Statistics m_stats;
public:
    BatchNode(std::string name, std::size_t size, bool permanent = false);
    void enter(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;
    std::size_t size() const { return m_size; }
    bool isPermanent() const { return m_permanent; }
    long long batchesFormed() const { return m_batchesFormed; }
    std::size_t waitingForBatch() const { return m_waiting.size(); }
};

// ---------------------------------------------------------------------------
// SEPARATE -- the inverse of a temporary Batch: send every member onward
// individually. The representative is destroyed.
//
// Also does Arena's other Separate job: DUPLICATE, making `copies` clones that
// take the same route. Copies share the original's attributes.
// ---------------------------------------------------------------------------
class SeparateNode : public INode {
    int m_duplicates{0};   // 0 means "split a batch" rather than "duplicate"
    long long m_processed{0};
public:
    // Split a batch back into its members.
    explicit SeparateNode(std::string name);
    // Duplicate: the original plus `copies` clones.
    SeparateNode(std::string name, int copies);
    void enter(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;
    long long processed() const { return m_processed; }
};

// ---------------------------------------------------------------------------
// RECORD -- tally something without changing it. Counts entities passing, and
// optionally accumulates a named attribute or the entity's time in system so
// far, so you can ask "how long had the parts that failed inspection been in the
// system?" without post-processing a trace.
// ---------------------------------------------------------------------------
class RecordNode : public INode {
public:
    enum class What { Count, Attribute, TimeInSystem };
private:
    What m_what;
    std::string m_attribute;
    long long m_count{0};
    double m_total{0.0}, m_min{0.0}, m_max{0.0};
public:
    explicit RecordNode(std::string name);
    RecordNode(std::string name, std::string attribute);
    // Returns a unique_ptr because INode is deliberately non-copyable: a node
    // is an identity in a graph, not a value. Copying one would give two nodes
    // that other nodes' next-pointers disagree about.
    static std::unique_ptr<RecordNode> timeInSystem(std::string name);

    void enter(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;

    long long count() const { return m_count; }
    double total() const { return m_total; }
    double average() const { return m_count ? m_total / static_cast<double>(m_count) : 0.0; }
    double minimum() const { return m_min; }
    double maximum() const { return m_max; }
};

// ---------------------------------------------------------------------------
// DISPOSE -- leave the system. Routing to nullptr does the same thing; this
// exists so a flowchart can name its exits, and so several exits can be counted
// separately ("scrapped" vs "shipped").
// ---------------------------------------------------------------------------
class DisposeNode : public INode {
    long long m_count{0};
public:
    explicit DisposeNode(std::string name);
    void enter(NodeContext& ctx, Entity* e) override;
    void setNext(INode*) override;   // a Dispose has no next; this throws
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;
    long long count() const { return m_count; }
};

}  // namespace des
