// ============================================================================
// Nodes.cpp
// ============================================================================

#include "Nodes.hpp"
#include "Entity.hpp"
#include "RandomStream.hpp"
#include "Trace.hpp"
#include "ModelError.hpp"
#include <algorithm>
#include <functional>
#include <cassert>
#include <iomanip>
#include <sstream>

namespace des {

// ------------------------------------------------------------------- Delay --

DelayNode::DelayNode(std::string name, std::unique_ptr<IDistribution> duration)
    : INode(std::move(name)), m_duration(std::move(duration)) {
    if (!m_duration) throw ModelError("delay '" + m_name + "': null duration");
}

void DelayNode::enter(NodeContext& ctx, Entity* e) {
    m_stats.recordArrival(ctx.now());
    ++m_inTransit;
    const SimTime d = m_duration->draw(ctx.rng());
    e->setAttribute("delayStart", ctx.now());
    ctx.scheduleReturn(ctx.now() + d, e, this);
    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4) << "delayed " << d;
        ctx.trace().event(ctx.now(), "Delay", e->id(), m_name, os.str(),
                          0, m_inTransit);
    }
}

void DelayNode::onScheduledEvent(NodeContext& ctx, Entity* e) {
    --m_inTransit;
    // No waiting happens in a Delay -- nobody competes for anything -- so the
    // wait component is 0 and only the elapsed time is recorded.
    m_stats.recordDeparture(ctx.now(), 0.0, ctx.now() - e->attribute("delayStart"));
    ctx.route(e, m_next);
}

void DelayNode::resetStatistics(SimTime now) {
    m_stats.restartAt(now);
}

void DelayNode::reset() {
    m_stats.reset();
    m_duration->reset();
    m_inTransit = 0;
}

std::string DelayNode::describe() const {
    return "Delay " + m_name + " [" + m_duration->describe() + ", next=" +
           (m_next ? m_next->name() : std::string("exit")) + "]";
}

// ------------------------------------------------------------------ Assign --

AssignNode::AssignNode(std::string name) : INode(std::move(name)) {}

AssignNode& AssignNode::set(const std::string& attribute, std::unique_ptr<IDistribution> value) {
    if (!value) throw ModelError("assign '" + m_name + "': null value for '" + attribute + "'");
    if (attribute == "waitTime" || attribute == "waitHere" || attribute == "stationEntry")
        throw ModelError("assign '" + m_name + "': '" + attribute + "' is reserved by the engine");
    m_rules.push_back(Rule{attribute, std::move(value)});
    return *this;
}

void AssignNode::enter(NodeContext& ctx, Entity* e) {
    for (const auto& r : m_rules) e->setAttribute(r.name, r.value->draw(ctx.rng()));
    ++m_count;
    if (ctx.trace().isOn() && !m_rules.empty()) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4);
        for (std::size_t i = 0; i < m_rules.size(); ++i) {
            if (i) os << ", ";
            os << m_rules[i].name << "=" << e->attribute(m_rules[i].name);
        }
        ctx.trace().event(ctx.now(), "Assign", e->id(), m_name, os.str(), 0, 0);
    }
    // Instant: the entity carries straight on. No event is scheduled, so an
    // Assign costs no simulated time at all -- which is the point of it.
    ctx.route(e, m_next);
}

void AssignNode::resetStatistics(SimTime now) {
    (void)now; m_count = 0;
}

void AssignNode::reset() {
    m_count = 0;
    for (auto& r : m_rules) r.value->reset();
}

std::string AssignNode::describe() const {
    std::ostringstream os;
    os << "Assign " << m_name << " [";
    for (std::size_t i = 0; i < m_rules.size(); ++i) {
        if (i) os << ", ";
        os << m_rules[i].name << " ~ " << m_rules[i].value->describe();
    }
    os << ", next=" << (m_next ? m_next->name() : std::string("exit")) << "]";
    return os.str();
}

// ------------------------------------------------------------------ Decide --

DecideNode::DecideNode(std::string name, double probability)
    : INode(std::move(name)), m_byChance(true) {
    if (probability < 0.0 || probability > 1.0)
        throw ModelError("decide '" + m_name + "': probability must be in [0,1]");
    m_branches.push_back(Branch{probability, nullptr, nullptr, 0});
}

DecideNode::DecideNode(std::string name, Condition condition)
    : INode(std::move(name)), m_byChance(false) {
    if (!condition) throw ModelError("decide '" + m_name + "': null condition");
    m_branches.push_back(Branch{-1.0, std::move(condition), nullptr, 0});
}

DecideNode::DecideNode(std::string name, bool byChance)
    : INode(std::move(name)), m_byChance(byChance) {}

DecideNode& DecideNode::addBranch(double probability, INode* target) {
    if (!m_byChance)
        throw ModelError("decide '" + m_name + "': this block branches by condition; "
                         "a Decide is all-chance or all-condition, never mixed");
    if (probability < 0.0 || probability > 1.0)
        throw ModelError("decide '" + m_name + "': probability must be in [0,1]");
    double total = probability;
    for (const auto& b : m_branches) total += b.probability;
    if (total > 1.0 + 1e-9)
        throw ModelError("decide '" + m_name + "': branch probabilities sum to more than 1");
    m_branches.push_back(Branch{probability, nullptr, target, 0});
    return *this;
}

DecideNode& DecideNode::addBranch(Condition condition, INode* target) {
    if (m_byChance)
        throw ModelError("decide '" + m_name + "': this block branches by chance; "
                         "a Decide is all-chance or all-condition, never mixed");
    if (!condition) throw ModelError("decide '" + m_name + "': null condition");
    m_branches.push_back(Branch{-1.0, std::move(condition), target, 0});
    return *this;
}

void DecideNode::setTrueBranch(INode* n) {
    if (m_branches.empty())
        throw ModelError("decide '" + m_name + "': no branch to attach a target to");
    m_branches[0].target = n;
}

long long DecideNode::tookFalse() const {
    long long other = m_fellThrough;
    for (std::size_t i = 1; i < m_branches.size(); ++i) other += m_branches[i].taken;
    return other;
}

void DecideNode::enter(NodeContext& ctx, Entity* e) {
    Branch* chosen = nullptr;

    if (m_byChance) {
        // ONE draw, walked against a cumulative probability. Drawing once per
        // branch would burn several random numbers and, worse, would not give
        // the branch probabilities you actually asked for.
        const double u = ctx.rng().uniform(0.0, 1.0);
        double cumulative = 0.0;
        for (auto& b : m_branches) {
            cumulative += b.probability;
            if (u < cumulative) { chosen = &b; break; }
        }
    } else {
        // First matching condition wins, so ORDER IS MEANINGFUL. Put the most
        // specific condition first.
        for (auto& b : m_branches) {
            if (b.condition(*e)) { chosen = &b; break; }
        }
    }

    INode* target = m_next;
    if (chosen) { ++chosen->taken; target = chosen->target; }
    else        { ++m_fellThrough; }

    if (ctx.trace().isOn()) {
        ctx.trace().event(ctx.now(), "Decide", e->id(), m_name,
                          std::string(chosen ? "branch -> " : "fell through -> ") +
                          (target ? target->name() : std::string("exit")), 0, 0);
    }
    ctx.route(e, target);
}

void DecideNode::resetStatistics(SimTime now) {
    (void)now;
    for (auto& b : m_branches) b.taken = 0;
    m_fellThrough = 0;
}

void DecideNode::reset() {
    for (auto& b : m_branches) b.taken = 0;
    m_fellThrough = 0;
}

std::string DecideNode::describe() const {
    std::ostringstream os;
    os << "Decide " << m_name << " [" << (m_byChance ? "chance" : "condition");
    for (const auto& b : m_branches) {
        os << ", ";
        if (m_byChance) os << b.probability << " -> ";
        else            os << "when -> ";
        os << (b.target ? b.target->name() : std::string("exit"));
    }
    os << ", else -> " << (m_next ? m_next->name() : std::string("exit")) << "]";
    return os.str();
}

// ------------------------------------------------------------------- Batch --

BatchNode::BatchNode(std::string name, std::size_t size, bool permanent)
    : INode(std::move(name)), m_size(size), m_permanent(permanent) {
    if (size < 2) throw ModelError("batch '" + m_name + "': size must be at least 2");
}

BatchNode::BatchNode(std::string name, std::size_t size, bool permanent,
                     Rule rule, std::string attribute)
    : INode(std::move(name)), m_size(size), m_permanent(permanent),
      m_rule(rule), m_attribute(std::move(attribute)) {
    if (size < 2) throw ModelError("batch '" + m_name + "': size must be at least 2");
    if (m_rule != Rule::AnyEntity && m_attribute.empty())
        throw ModelError("batch '" + m_name + "': this rule needs an attribute name");
}

std::vector<std::size_t> BatchNode::selectMembers() const {
    std::vector<std::size_t> chosen;

    if (m_rule == Rule::AnyEntity) {
        if (m_waiting.size() < m_size) return chosen;
        for (std::size_t i = 0; i < m_size; ++i) chosen.push_back(i);
        return chosen;
    }

    if (m_rule == Rule::SameAttribute) {
        // Group by value; the first group to reach `size` goes. Scanning in
        // arrival order means the OLDEST complete group wins, so nothing is
        // starved by a later, busier value.
        for (std::size_t i = 0; i < m_waiting.size(); ++i) {
            const double v = m_waiting[i]->attribute(m_attribute);
            std::vector<std::size_t> group;
            for (std::size_t j = i; j < m_waiting.size(); ++j)
                if (m_waiting[j]->attribute(m_attribute) == v) group.push_back(j);
            if (group.size() >= m_size) {
                group.resize(m_size);
                return group;
            }
        }
        return chosen;
    }

    // DistinctAttribute: one of each. Take the OLDEST entity of each distinct
    // value until we have `size` different values. Oldest-first matters: taking
    // the newest would let an entity sit forever while its own kind kept
    // arriving and being picked ahead of it.
    std::vector<double> valuesTaken;
    for (std::size_t i = 0; i < m_waiting.size() && chosen.size() < m_size; ++i) {
        const double v = m_waiting[i]->attribute(m_attribute);
        bool already = false;
        for (double t : valuesTaken) if (t == v) { already = true; break; }
        if (already) continue;
        valuesTaken.push_back(v);
        chosen.push_back(i);
    }
    if (chosen.size() < m_size) chosen.clear();
    return chosen;
}

bool BatchNode::tryFormBatch(NodeContext& ctx) {
    const std::vector<std::size_t> chosen = selectMembers();
    if (chosen.empty()) return false;

    // The representative is a NEW entity whose creation time is the OLDEST
    // member's, so time-in-system measures how long the first arrival waited for
    // the group. Using now() here understates it by exactly the batching delay.
    Entity* rep = ctx.createEntity();
    SimTime oldest = m_waiting[chosen.front()]->creationTime();
    SimTime totalWait = 0.0;
    std::vector<Entity*> members;
    for (std::size_t idx : chosen) {
        Entity* member = m_waiting[idx];
        oldest = std::min(oldest, member->creationTime());
        totalWait += member->attribute("waitTime");
        // One waiting-time observation PER MEMBER, which is what Arena reports
        // for a batching station queue.
        m_queueStats.recordDeparture(ctx.now(), ctx.now() - m_arrivedAt[idx],
                                     ctx.now() - m_arrivedAt[idx]);
        members.push_back(member);
        rep->addMember(member);
    }
    rep->setType(m_waiting[chosen.front()]->type());
    rep->setCreationTime(oldest);
    rep->setAttribute("waitTime", totalWait / static_cast<double>(members.size()));
    rep->setAttribute("batchSize", static_cast<double>(members.size()));

    // Remove the chosen entries, highest index first so the earlier ones do not
    // shift underneath us.
    std::vector<std::size_t> order = chosen;
    std::sort(order.begin(), order.end(), std::greater<std::size_t>());
    for (std::size_t idx : order) {
        m_waiting.erase(m_waiting.begin() + static_cast<std::ptrdiff_t>(idx));
        m_arrivedAt.erase(m_arrivedAt.begin() + static_cast<std::ptrdiff_t>(idx));
    }

    ++m_batchesFormed;
    m_stats.recordDeparture(ctx.now(), 0.0, ctx.now() - oldest);

    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << "batch of " << members.size() << " formed as entity " << rep->id()
           << (m_permanent ? " (permanent)" : " (temporary)");
        ctx.trace().event(ctx.now(), "Batch", rep->id(), m_name, os.str(),
                          m_waiting.size(), 0);
    }

    if (m_permanent) {
        // Members are consumed: the batch IS the thing from now on.
        for (Entity* member : members) ctx.destroy(member);
        rep->clearMembers();
    }

    ctx.route(rep, m_next);
    return true;
}

void BatchNode::enter(NodeContext& ctx, Entity* e) {
    m_stats.recordArrival(ctx.now());
    m_queueStats.recordArrival(ctx.now());
    m_waiting.push_back(e);
    m_arrivedAt.push_back(ctx.now());
    m_maxQueue = std::max(m_maxQueue, m_waiting.size());

    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << "held for batch (" << m_waiting.size() << " waiting, need " << m_size << ")";
        ctx.trace().event(ctx.now(), "Batch", e->id(), m_name, os.str(),
                          m_waiting.size(), 0);
    }

    // Loop: one arrival can complete more than one batch when a matched rule has
    // been holding several near-complete groups.
    while (tryFormBatch(ctx)) {}
}

void BatchNode::resetStatistics(SimTime now) {
    m_stats.restartAt(now);
    m_queueStats.restartAt(now);
    m_batchesFormed = 0;
    m_maxQueue = m_waiting.size();
}

void BatchNode::reset() {
    m_waiting.clear();
    m_arrivedAt.clear();
    m_batchesFormed = 0;
    m_maxQueue = 0;
    m_stats.reset();
    m_queueStats.reset();
}

std::string BatchNode::describe() const {
    std::ostringstream os;
    os << "Batch " << m_name << " [size " << m_size << ", "
       << (m_permanent ? "permanent" : "temporary");
    if (m_rule == Rule::SameAttribute)     os << ", same " << m_attribute;
    if (m_rule == Rule::DistinctAttribute) os << ", one of each " << m_attribute;
    os << ", next=" << (m_next ? m_next->name() : std::string("exit")) << "]";
    return os.str();
}

// ---------------------------------------------------------------- Separate --

SeparateNode::SeparateNode(std::string name) : INode(std::move(name)), m_duplicates(0) {}

SeparateNode::SeparateNode(std::string name, int copies)
    : INode(std::move(name)), m_duplicates(copies) {
    if (copies < 1) throw ModelError("separate '" + m_name + "': copies must be >= 1");
}

void SeparateNode::enter(NodeContext& ctx, Entity* e) {
    ++m_processed;

    if (m_duplicates > 0) {
        // DUPLICATE. Send the original on, then the copies. Copies inherit the
        // attributes but get their own ids and their own creation time of now --
        // they did not exist before this instant.
        // Route the ORIGINAL last: the copies are made from it, and routing it
        // first could send it somewhere that destroys it before they are built.
        const double share = m_percentToDuplicates / 100.0;
        for (int i = 0; i < m_duplicates; ++i) {
            Entity* copy = ctx.createEntity();
            copy->copyAttributesFrom(*e);
            copy->setType(e->type());
            copy->setCreationTime(e->creationTime());   // it is the same work
            copy->setAttribute("costShare", share / m_duplicates);
            if (ctx.trace().isOn())
                ctx.trace().event(ctx.now(), "Duplicate", copy->id(), m_name,
                                  "copy of entity " + std::to_string(e->id()), 0, 0);
            ctx.route(copy, m_duplicateTo ? m_duplicateTo : m_next);
        }
        e->setAttribute("costShare", 1.0 - share);
        ctx.route(e, m_next);
        return;
    }

    // SPLIT a temporary batch back into its members.
    if (!e->isBatch())
        throw ModelError("separate '" + m_name + "': entity " + std::to_string(e->id()) +
                         " is not a temporary batch. A permanent Batch consumes its "
                         "members, so there is nothing left to separate.");

    std::vector<Entity*> members = e->members();
    e->clearMembers();
    if (ctx.trace().isOn())
        ctx.trace().event(ctx.now(), "Separate", e->id(), m_name,
                          "split into " + std::to_string(members.size()) + " members", 0, 0);
    ctx.destroy(e);                       // the representative is done
    for (Entity* m : members) ctx.route(m, m_next);
}

void SeparateNode::resetStatistics(SimTime now) {
    (void)now; m_processed = 0;
}

void SeparateNode::reset() { m_processed = 0; }

std::string SeparateNode::describe() const {
    std::ostringstream os;
    os << "Separate " << m_name << " [";
    if (m_duplicates > 0) os << "duplicate x" << m_duplicates;
    else                  os << "split batch";
    os << ", next=" << (m_next ? m_next->name() : std::string("exit")) << "]";
    return os.str();
}

// ------------------------------------------------------------------ Record --

RecordNode::RecordNode(std::string name)
    : INode(std::move(name)), m_what(What::Count) {}

RecordNode::RecordNode(std::string name, std::string attribute)
    : INode(std::move(name)), m_what(What::Attribute), m_attribute(std::move(attribute)) {}

std::unique_ptr<RecordNode> RecordNode::timeInSystem(std::string name) {
    auto r = std::make_unique<RecordNode>(std::move(name));
    r->m_what = What::TimeInSystem;
    return r;
}

void RecordNode::enter(NodeContext& ctx, Entity* e) {
    double value = 0.0;
    switch (m_what) {
        case What::Count:        value = 1.0; break;
        case What::Attribute:    value = e->attribute(m_attribute); break;
        case What::TimeInSystem: value = ctx.now() - e->creationTime(); break;
    }
    if (m_count == 0) { m_min = value; m_max = value; }
    else { m_min = std::min(m_min, value); m_max = std::max(m_max, value); }
    m_total += value;
    ++m_count;

    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4) << "recorded " << value;
        ctx.trace().event(ctx.now(), "Record", e->id(), m_name, os.str(), 0, 0);
    }
    ctx.route(e, m_next);
}

void RecordNode::resetStatistics(SimTime now) {
    (void)now; m_count = 0; m_total = 0.0; m_min = 0.0; m_max = 0.0;
}

void RecordNode::reset() { m_count = 0; m_total = 0.0; m_min = 0.0; m_max = 0.0; }

std::string RecordNode::describe() const {
    std::string what = m_what == What::Count ? "count"
                     : m_what == What::Attribute ? ("attribute:" + m_attribute)
                     : "time in system";
    return "Record " + m_name + " [" + what + ", next=" +
           (m_next ? m_next->name() : std::string("exit")) + "]";
}

// ----------------------------------------------------------------- Dispose --

DisposeNode::DisposeNode(std::string name) : INode(std::move(name)) {}

void DisposeNode::setNext(INode*) {
    throw ModelError("dispose '" + m_name + "' is an exit -- nothing follows it");
}

void DisposeNode::enter(NodeContext& ctx, Entity* e) {
    ++m_count;
    ctx.route(e, nullptr);   // nullptr means "leaves the system"
}

void DisposeNode::resetStatistics(SimTime now) {
    (void)now; m_count = 0;
}

void DisposeNode::reset() { m_count = 0; }

std::string DisposeNode::describe() const {
    return "Dispose " + m_name;
}

}  // namespace des
