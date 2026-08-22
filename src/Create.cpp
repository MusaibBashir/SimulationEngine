// ============================================================================
// Create.cpp
// ============================================================================

#include "Create.hpp"
#include "Entity.hpp"
#include "RandomStream.hpp"
#include "Trace.hpp"
#include "ModelError.hpp"
#include <sstream>

namespace des {

CreateNode::CreateNode(std::string name, std::string entityType,
                       std::unique_ptr<IDistribution> interarrival,
                       long long maxArrivals, SimTime firstAt, int entitiesPerArrival)
    : INode(std::move(name)),
      m_entityType(std::move(entityType)),
      m_interarrival(std::move(interarrival)),
      m_firstAt(firstAt),
      m_maxArrivals(maxArrivals),
      m_perArrival(entitiesPerArrival) {
    if (!m_interarrival) throw ModelError("create '" + m_name + "': null interarrival");
    if (m_perArrival < 1) throw ModelError("create '" + m_name + "': entities per arrival >= 1");
    if (m_firstAt < 0.0)  throw ModelError("create '" + m_name + "': first arrival cannot be negative");
}

void CreateNode::enter(NodeContext&, Entity*) {
    throw ModelError("create '" + m_name + "' is a source -- nothing routes into it");
}

void CreateNode::onScheduledEvent(NodeContext& ctx, Entity* /*unused*/) {
    // *** SCHEDULE THE NEXT ARRIVAL FIRST. ***
    // The entities below are routed synchronously and may travel a long way
    // through the flowchart before this function resumes. Scheduling first means
    // the stream is fed no matter what happens downstream -- and forgetting it
    // is the classic first-run bug where a simulation stops after one entity.
    if (!exhausted()) {
        const SimTime next = ctx.now() + m_interarrival->draw(ctx.rng());
        ctx.scheduleNextArrival(next, this);
    }

    for (int i = 0; i < m_perArrival && !exhausted(); ++i) {
        Entity* e = ctx.createEntity();
        e->setType(m_entityType);
        e->setAttribute("waitTime", 0.0);
        ctx.registerArrival(e);        // counts it as demand, stamps attributes
        ++m_created;

        if (ctx.trace().isOn()) {
            ctx.trace().event(ctx.now(), "Create", e->id(), m_name,
                              m_entityType + " enters the system", 0, 0);
        }
        ctx.route(e, m_next);
    }
}

void CreateNode::reset() {
    m_created = 0;
    m_interarrival->reset();
}

void CreateNode::resetStatistics(SimTime) {
    // m_created is NOT reset. It is the arrival COUNT for the whole run, and a
    // source capped at 1000 must not start again at zero when the warm-up ends
    // -- that would let it create 1000 more.
}

std::string CreateNode::describe() const {
    std::ostringstream os;
    os << "Create " << m_name << " [" << m_entityType
       << " ~ " << m_interarrival->describe();
    if (m_maxArrivals >= 0) os << ", max " << m_maxArrivals;
    if (m_firstAt > 0.0)    os << ", first at " << m_firstAt;
    if (m_perArrival > 1)   os << ", " << m_perArrival << " per arrival";
    os << ", next=" << (m_next ? m_next->name() : std::string("exit")) << "]";
    return os.str();
}

}  // namespace des
