// ============================================================================
// Transfer.cpp
// ============================================================================

#include "Transfer.hpp"
#include <iomanip>
#include <sstream>
#include "Entity.hpp"
#include "ModelError.hpp"
#include "Trace.hpp"

namespace des {

void StationNode::enter(NodeContext& ctx, Entity* e) {
    ++m_arrivals;
    ctx.route(e, m_next);
}

std::string StationNode::describe() const {
    return "Station " + m_name + " [next=" +
           (m_next ? m_next->name() : std::string("exit")) + "]";
}

RouteNode::RouteNode(std::string name, ExpressionPtr time)
    : INode(std::move(name)), m_time(std::move(time)) {
    if (!m_time) throw ModelError("route '" + m_name + "': null route time");
}

void RouteNode::setNext(INode*) {
    throw ModelError("route '" + m_name + "' has no Next -- it goes to its Destination");
}

void RouteNode::enter(NodeContext& ctx, Entity* e) {
    if (m_destination == nullptr)
        throw ModelError("route '" + m_name + "' has no Destination station");
    EvalContext ectx = ctx.evaluationContext(e);
    const SimTime t = static_cast<SimTime>(asNumber(m_time->evaluate(ectx)));
    if (t < 0.0) {
        std::ostringstream os;
        os << "route '" << m_name << "': the route time came out negative (" << t << ")";
        throw ModelError(os.str());
    }
    m_inTransit.change(ctx.now(), +1);
    ctx.scheduleReturn(ctx.now() + t, e, this);
    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4) << "to " << m_destination->name()
           << ", " << t;
        ctx.trace().event(ctx.now(), "Route", e->id(), m_name, os.str(), 0,
                          static_cast<int>(m_inTransit.level()));
    }
}

void RouteNode::onScheduledEvent(NodeContext& ctx, Entity* e) {
    m_inTransit.change(ctx.now(), -1);
    ++m_travelled;
    ctx.route(e, m_destination);
}

void RouteNode::reset() {
    m_inTransit.reset();
    m_travelled = 0;
    m_time->reset();
}

void RouteNode::resetStatistics(SimTime now) {
    m_inTransit.restart(now);
    m_travelled = 0;
}

std::string RouteNode::describe() const {
    return "Route " + m_name + " [" + m_time->describe() + " to " +
           (m_destination ? m_destination->name() : std::string("?")) + "]";
}

}  // namespace des
