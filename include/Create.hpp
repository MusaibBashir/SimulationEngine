// ============================================================================
// Create.hpp  --  v9: entities enter the system HERE
// ============================================================================
// Through v8 a model had exactly one arrival stream, described by
// Model::arrivals(). That covers one queue and nothing else. A model with three
// kinds of ball arriving separately, each capped at 1000, needs three sources --
// which is Arena's Create module, and this is it.
//
// The design point worth noticing: a Create is an INode like any other. Its
// scheduled callback makes an entity, sends it to the block it feeds, and
// reschedules itself. That let SimulationSystem::handleArrival be DELETED --
// the engine no longer has a special case for arrivals, because arrivals stopped
// being special. Every version of this project that generalised something got
// smaller in the middle; this is the clearest instance.

#pragma once

#include <memory>
#include <string>
#include "Common.hpp"
#include "Node.hpp"
#include "Distribution.hpp"
#include "Expression.hpp"

namespace des {

class CreateNode : public INode {
private:
    std::string m_entityType;
    ExpressionPtr m_interarrival;
    SimTime   m_firstAt{0.0};
    long long m_maxArrivals{-1};      // -1 means "keep going until time runs out"
    long long m_created{0};
    long long m_arrivals{0};          // v16: arrival EVENTS; Max Arrivals counts these
    int       m_perArrival{1};        // entities per arrival event (Arena's
                                      // "Entities per Arrival")
    ExpressionPtr m_perArrivalExpr;   // v16: when set, evaluated at each arrival
    // v16: following an arrival schedule. The event at First At then only
    // STARTS the stream -- an arrival exactly at the start of every
    // replication would be one more than the schedule says.
    bool      m_scheduled{false};
    bool      m_started{false};

public:
    CreateNode(std::string name, std::string entityType,
               std::unique_ptr<IDistribution> interarrival,
               long long maxArrivals = -1, SimTime firstAt = 0.0,
               int entitiesPerArrival = 1);
    CreateNode(std::string name, std::string entityType,
               ExpressionPtr interarrival,
               long long maxArrivals = -1, SimTime firstAt = 0.0,
               int entitiesPerArrival = 1);

    // Nothing routes INTO a Create. Saying so is better than quietly accepting
    // an entity and losing it.
    void enter(NodeContext& ctx, Entity* e) override;
    void onScheduledEvent(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;

    const std::string& entityType() const { return m_entityType; }
    SimTime firstAt() const { return m_firstAt; }
    long long created() const { return m_created; }
    long long maxArrivals() const { return m_maxArrivals; }
    int entitiesPerArrival() const { return m_perArrival; }
    void setPerArrival(ExpressionPtr e) { m_perArrivalExpr = std::move(e); }
    const IExpression* perArrivalExpression() const { return m_perArrivalExpr.get(); }
    long long arrivals() const { return m_arrivals; }
    IExpression& interarrival() { return *m_interarrival; }
    const IExpression& interarrival() const { return *m_interarrival; }
    // v16: Max Arrivals counts arrival EVENTS, as Arena's does. Per Arrival =
    // 20 with Max Arrivals = 1 is twenty entities at once -- how a fleet is
    // put into the world -- where counting entities made it one.
    bool scheduled() const { return m_scheduled; }
    bool exhausted() const { return m_maxArrivals >= 0 && m_arrivals >= m_maxArrivals; }
};

}  // namespace des
