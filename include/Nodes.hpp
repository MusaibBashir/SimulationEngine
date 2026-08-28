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
#include "Expression.hpp"
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

    // v7: a Decide is now N-way. Each branch is either a probability or a
    // predicate; whatever matches no branch falls through to next().
    // v10: the condition is an EXPRESSION. A std::function still works -- it
    // arrives wrapped in a LambdaExpression -- so there is one evaluation path
    // rather than two kept alive in parallel. Branch is move-only as a result.
    struct Branch {
        double probability{-1.0};   // < 0 means this is a condition branch
        ExpressionPtr condition;
        INode* target{nullptr};
        long long taken{0};
    };

private:
    bool m_byChance{true};
    std::vector<Branch> m_branches;
    long long m_fellThrough{0};

public:
    // Two-way shorthands, unchanged from v6.
    DecideNode(std::string name, double probability);
    DecideNode(std::string name, Condition condition);
    DecideNode(std::string name, ExpressionPtr condition);
    // N-way: start empty and add branches.
    explicit DecideNode(std::string name, bool byChance);

    // *** A Decide is all-chance or all-condition, never mixed. ***
    // Mixing them has no coherent meaning: chance branches must share one draw
    // to sum correctly, conditions are evaluated in order, and a reader could
    // not tell which rule applied to which branch. Refused rather than guessed.
    DecideNode& addBranch(double probability, INode* target);
    DecideNode& addBranch(Condition condition, INode* target);
    DecideNode& addBranch(ExpressionPtr condition, INode* target);

    // The two-way spelling: branch 0 is "true".
    void setTrueBranch(INode* n);
    INode* trueBranch() const { return m_branches.empty() ? nullptr : m_branches[0].target; }

    void enter(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;

    bool isByChance() const { return m_byChance; }
    double probability() const { return m_branches.empty() ? 0.0 : m_branches[0].probability; }
    const std::vector<Branch>& branches() const { return m_branches; }
    long long fellThrough() const { return m_fellThrough; }
    long long tookTrue() const { return m_branches.empty() ? 0 : m_branches[0].taken; }
    long long tookFalse() const;
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
public:
    // v9: HOW entities are grouped. Arena calls this the batching Rule.
    enum class Rule {
        AnyEntity,          // the first `size` to show up
        SameAttribute,      // `size` entities that AGREE on an attribute --
                            // same lot number, same customer, same order
        DistinctAttribute   // `size` entities that DIFFER -- one of each type.
                            // "a set of three balls, one baseball, one
                            // basketball, one football" is this rule, and it is
                            // the one AnyEntity cannot fake: with three streams
                            // running at different speeds, taking the first
                            // three to arrive gives you three baseballs.
    };

private:
    std::size_t m_size;
    bool m_permanent;
    Rule m_rule{Rule::AnyEntity};
    std::string m_attribute;
    std::vector<Entity*> m_waiting;
    // When each waiting entity arrived, so the wait for a batch can be measured
    // per member. Arena reports one observation PER MEMBER at the batching
    // station queue, not one per batch, and the difference is a factor of `size`.
    std::vector<SimTime> m_arrivedAt;
    long long m_batchesFormed{0};
    Statistics m_stats;
    Statistics m_queueStats;     // waiting time, one observation per member
    std::size_t m_maxQueue{0};

    bool tryFormBatch(NodeContext& ctx);
    std::vector<std::size_t> selectMembers() const;

public:
    BatchNode(std::string name, std::size_t size, bool permanent = false);
    BatchNode(std::string name, std::size_t size, bool permanent,
              Rule rule, std::string attribute);
    void enter(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;
    std::size_t size() const { return m_size; }
    bool isPermanent() const { return m_permanent; }
    long long batchesFormed() const { return m_batchesFormed; }
    std::size_t waitingForBatch() const { return m_waiting.size(); }
    std::size_t maxQueueLength() const { return m_maxQueue; }
    // Waiting-time statistics for the batching station queue: one observation
    // per MEMBER, matching what Arena reports.
    const Statistics& queueStats() const { return m_queueStats; }
    const Statistics& stats() const { return m_stats; }
    Rule rule() const { return m_rule; }
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

    // v9: Arena's Separate block has TWO exit points, Original and Duplicate,
    // and models routinely send them different ways -- the sample to one test
    // bench and its duplicate to another. next() is the Original exit; this is
    // the Duplicate exit, and if it is not set the copies follow the original.
    INode* m_duplicateTo{nullptr};

    // Arena's "Percent Cost to Duplicates". Carried on the entity as an
    // attribute so a downstream Record can cost it. It does NOT affect routing,
    // which is the thing everyone reads it as the first time.
    double m_percentToDuplicates{50.0};

public:
    void setDuplicateExit(INode* n) { m_duplicateTo = n; }
    INode* duplicateExit() const { return m_duplicateTo; }
    void setPercentToDuplicates(double pct) { m_percentToDuplicates = pct; }
    double percentToDuplicates() const { return m_percentToDuplicates; }

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
