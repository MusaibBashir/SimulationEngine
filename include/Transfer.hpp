// ============================================================================
// Transfer.hpp  --  v16: Arena's Station and Route
// ============================================================================
// A Route holds an entity for its route time and then delivers it to a
// Station named in its Destination -- no line is drawn, and none is needed.
// This is how a car in the ride-hailing model drives to the zone its trip ends
// in: three Routes, three Stations, and the Station is where it "is" next.
//
// A Route has NO Next. Where it goes is its Destination, held apart from
// next() so the routing-loop check and the visit ratios stop at a Route
// rather than following a teleport as if it were a wire.
// ============================================================================

#pragma once
#include <string>
#include "Expression.hpp"
#include "LevelIntegral.hpp"
#include "Node.hpp"

namespace des {

class StationNode : public INode {
    long long m_arrivals{0};
public:
    explicit StationNode(std::string name) : INode(std::move(name)) {}
    void enter(NodeContext& ctx, Entity* e) override;
    void reset() override { m_arrivals = 0; }
    void resetStatistics(SimTime) override { m_arrivals = 0; }
    std::string describe() const override;
    long long arrivals() const { return m_arrivals; }
};

class RouteNode : public INode {
    ExpressionPtr m_time;
    StationNode*  m_destination{nullptr};   // non-owning: the Model owns it
    LevelIntegral m_inTransit;
    long long     m_travelled{0};
public:
    RouteNode(std::string name, ExpressionPtr time);
    void setNext(INode* n) override;                     // refuses: see above
    void setDestination(StationNode* s) { m_destination = s; }
    StationNode* destination() const { return m_destination; }

    void enter(NodeContext& ctx, Entity* e) override;
    void onScheduledEvent(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;

    const IExpression& timeExpression() const { return *m_time; }
    IExpression& timeExpression() { return *m_time; }
    long long travelled() const { return m_travelled; }
    long long inTransit() const { return m_inTransit.level(); }
    double averageInTransit(SimTime now) const { return m_inTransit.average(now); }
};

}  // namespace des
