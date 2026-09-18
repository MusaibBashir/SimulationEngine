// ============================================================================
// Match.cpp
// ============================================================================

#include "Match.hpp"
#include <algorithm>
#include <sstream>
#include "Entity.hpp"
#include "ModelError.hpp"
#include "Trace.hpp"

namespace des {

// ------------------------------------------------------------------ lanes --

MatchLaneNode::MatchLaneNode(std::string name, MatchNode* owner)
    : INode(std::move(name)), m_owner(owner) {
    if (m_owner == nullptr) throw ModelError("match lane '" + m_name + "' has no Match");
}

void MatchLaneNode::enter(NodeContext& ctx, Entity* e) {
    m_level.change(ctx.now(), +1);
    m_waiting.push_back(Waiting{e, ctx.now()});
    m_maxWaiting = std::max(m_maxWaiting, m_waiting.size());
    if (ctx.trace().isOn())
        ctx.trace().event(ctx.now(), "Match", e->id(), m_name, "waits for a match", 0,
                          static_cast<int>(m_waiting.size()));
    m_owner->tryMatch(ctx);
}

Entity* MatchLaneNode::take(std::size_t i, SimTime now) {
    const Waiting w = m_waiting[i];
    m_waiting.erase(m_waiting.begin() + static_cast<long>(i));
    m_level.change(now, -1);
    m_totalWait += now - w.since;
    ++m_matched;
    return w.entity;
}

void MatchLaneNode::reset() {
    m_waiting.clear();
    m_level.reset();
    m_maxWaiting = 0;
    m_totalWait = 0.0;
    m_matched = 0;
}

void MatchLaneNode::resetStatistics(SimTime now) {
    // Whoever is waiting still is; only what was measured is forgotten.
    m_level.restart(now);
    m_maxWaiting = m_waiting.size();
    m_totalWait = 0.0;
    m_matched = 0;
}

std::string MatchLaneNode::describe() const {
    return "MatchLane " + m_owner->name() + "." + m_name + " [next=" +
           (m_next ? m_next->name() : std::string("exit")) + "]";
}

// ------------------------------------------------------------------ match --

MatchNode::MatchNode(std::string name, std::string attribute)
    : INode(std::move(name)), m_attribute(std::move(attribute)) {}

void MatchNode::enter(NodeContext&, Entity*) {
    throw ModelError("match '" + m_name + "' is entered through one of its lanes, "
                     "not by its own name");
}

bool MatchNode::findMatch(std::vector<std::size_t>& picks) const {
    picks.assign(m_lanes.size(), 0);
    if (m_lanes.empty()) return false;
    for (const MatchLaneNode* l : m_lanes)
        if (l->waiting() == 0) return false;
    if (!byAttribute()) return true;          // the head of every lane

    // Every value lane 0 holds is a candidate; keep the one whose earliest
    // waiter anywhere has waited longest.
    bool found = false;
    SimTime bestSince = 0.0;
    std::vector<std::size_t> trial(m_lanes.size(), 0);
    const auto& first = m_lanes[0]->waitingList();
    for (std::size_t i = 0; i < first.size(); ++i) {
        const double v = first[i].entity->attribute(m_attribute);
        trial[0] = i;
        SimTime earliest = first[i].since;
        bool everywhere = true;
        for (std::size_t l = 1; l < m_lanes.size() && everywhere; ++l) {
            const auto& list = m_lanes[l]->waitingList();
            everywhere = false;
            for (std::size_t j = 0; j < list.size(); ++j) {
                if (list[j].entity->attribute(m_attribute) != v) continue;
                trial[l] = j;
                earliest = std::min(earliest, list[j].since);
                everywhere = true;
                break;
            }
        }
        if (!everywhere) continue;
        if (!found || earliest < bestSince) {
            found = true;
            bestSince = earliest;
            picks = trial;
        }
    }
    return found;
}

void MatchNode::tryMatch(NodeContext& ctx) {
    // Routing a matched entity can bring another into a lane in the same
    // instant. That arrival only queues; the loop below is already running
    // and will look again.
    if (m_matching) return;
    struct Guard {
        bool& flag;
        explicit Guard(bool& f) : flag(f) { flag = true; }
        ~Guard() { flag = false; }
    } guard(m_matching);

    std::vector<std::size_t> picks;
    while (findMatch(picks)) {
        // ALL taken before ANY is routed, so no lane is looked at half-emptied.
        std::vector<Entity*> matched(m_lanes.size(), nullptr);
        for (std::size_t l = 0; l < m_lanes.size(); ++l)
            matched[l] = m_lanes[l]->take(picks[l], ctx.now());
        ++m_matches;
        if (ctx.trace().isOn()) {
            std::ostringstream os;
            os << "matched";
            for (const Entity* e : matched) os << " #" << e->id();
            ctx.trace().event(ctx.now(), "Match", matched[0]->id(), m_name, os.str(), 0, 0);
        }
        for (std::size_t l = 0; l < m_lanes.size(); ++l)
            ctx.route(matched[l], m_lanes[l]->next());
    }
}

std::string MatchNode::describe() const {
    std::ostringstream os;
    os << "Match " << m_name << " [" << (byAttribute() ? "same " + m_attribute : "any")
       << ", lanes:";
    for (const MatchLaneNode* l : m_lanes) os << " " << l->name();
    os << "]";
    return os.str();
}

}  // namespace des
