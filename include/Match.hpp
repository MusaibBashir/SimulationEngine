// ============================================================================
// Match.hpp  --  v16: Arena's Match
// ============================================================================
// Entities wait in two to five lanes until there is one in EVERY lane --
// or, by attribute, one in every lane with the same value of it -- and then
// one from each lane leaves at the same instant, each by its own lane's exit.
// A rider and a car in the same zone is the ride-hailing model's whole engine.
//
// Each lane is a block of its own with its own name, so other blocks route
// into "Riders_Waiting" rather than into "input 1 of the Match", and the
// report says Riders_Waiting rather than "queue 2". The Match itself cannot
// be routed to: an entity arriving there would not know which lane it is in.
//
// Which entities go, when several could:
//   Any       -- the head of each lane.
//   Attribute -- the value whose earliest waiter, across all lanes, has
//                waited longest; from each lane, the longest-waiting entity
//                with that value.
// ============================================================================

#pragma once
#include <deque>
#include <string>
#include <vector>
#include "LevelIntegral.hpp"
#include "Node.hpp"

namespace des {

class MatchNode;

class MatchLaneNode : public INode {
public:
    struct Waiting { Entity* entity; SimTime since; };

private:
    MatchNode*          m_owner;        // non-owning: the Model owns both
    std::deque<Waiting> m_waiting;
    LevelIntegral       m_level;
    std::size_t         m_maxWaiting{0};
    double              m_totalWait{0.0};
    long long           m_matched{0};

public:
    MatchLaneNode(std::string name, MatchNode* owner);
    void enter(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;

    const MatchNode& owner() const { return *m_owner; }
    const std::deque<Waiting>& waitingList() const { return m_waiting; }
    // Removes the i-th waiting entity and counts its wait.
    Entity* take(std::size_t i, SimTime now);

    std::size_t waiting() const { return m_waiting.size(); }
    std::size_t maxWaiting() const { return m_maxWaiting; }
    double averageWaiting(SimTime now) const { return m_level.average(now); }
    double averageWait() const {
        return m_matched ? m_totalWait / static_cast<double>(m_matched) : 0.0;
    }
    long long matched() const { return m_matched; }
};

class MatchNode : public INode {
    std::string                 m_attribute;   // empty: Any
    std::vector<MatchLaneNode*> m_lanes;       // non-owning, in lane order
    bool                        m_matching{false};
    long long                   m_matches{0};

public:
    MatchNode(std::string name, std::string attribute);
    void addLane(MatchLaneNode* lane) { m_lanes.push_back(lane); }
    void enter(NodeContext& ctx, Entity* e) override;   // refuses: route to a lane
    void reset() override { m_matches = 0; }
    void resetStatistics(SimTime) override { m_matches = 0; }
    std::string describe() const override;

    // Called by a lane after every arrival. Releases every match that is now
    // possible, which is normally zero or one.
    void tryMatch(NodeContext& ctx);

    const std::string& attribute() const { return m_attribute; }
    bool byAttribute() const { return !m_attribute.empty(); }
    std::size_t laneCount() const { return m_lanes.size(); }
    const MatchLaneNode& lane(std::size_t i) const { return *m_lanes[i]; }
    long long matches() const { return m_matches; }

private:
    // Which entity to take from each lane, or false when nothing matches.
    bool findMatch(std::vector<std::size_t>& picks) const;
};

}  // namespace des
