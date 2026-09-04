// ============================================================================
// Station.cpp  --  the Process block
// ============================================================================

#include "Station.hpp"
#include "Entity.hpp"
#include "RandomStream.hpp"
#include "Activity.hpp"
#include "Trace.hpp"
#include "ModelError.hpp"
#include <cassert>
#include <iomanip>
#include <limits>
#include <sstream>

namespace des {

Station::Station(std::string name, Resource* resource, int unitsNeeded,
                 std::unique_ptr<IQueueRule> rule,
                 std::unique_ptr<IDistribution> service)
    : INode(std::move(name)),
      m_resource(resource),
      m_unitsNeeded(unitsNeeded),
      m_queue(m_name + "Queue", std::move(rule)),
      m_service(service ? ExpressionPtr(std::make_unique<DistributionExpression>(
                              std::move(service)))
                        : ExpressionPtr()) {
    if (!m_resource) throw ModelError("process '" + m_name + "': null resource");
    if (!m_service)  throw ModelError("process '" + m_name + "': null service distribution");
    if (m_unitsNeeded < 1 || m_unitsNeeded > m_resource->capacity())
        throw ModelError("process '" + m_name + "' needs " + std::to_string(m_unitsNeeded) +
                         " units of a resource with capacity " +
                         std::to_string(m_resource->capacity()) + " -- it could never start");
    m_resource->addUser(this);
}

Station::Station(std::string name, Resource* resource, int unitsNeeded,
                 std::unique_ptr<IQueueRule> rule,
                 ExpressionPtr service)
    : INode(std::move(name)),
      m_resource(resource),
      m_unitsNeeded(unitsNeeded),
      m_queue(m_name + "Queue", std::move(rule)),
      m_service(std::move(service)) {
    if (!m_resource) throw ModelError("process '" + m_name + "': null resource");
    if (!m_service)  throw ModelError("process '" + m_name + "': null service expression");
    if (m_unitsNeeded < 1 || m_unitsNeeded > m_resource->capacity())
        throw ModelError("process '" + m_name + "' needs " + std::to_string(m_unitsNeeded) +
                         " units of a resource with capacity " +
                         std::to_string(m_resource->capacity()) + " -- it could never start");
    m_resource->addUser(this);
}

void Station::setServiceFromAttribute(const std::string& a) { m_serviceAttribute = a; }

void Station::setBalking(std::size_t queueLength, INode* balkTo) {
    m_balkAt = queueLength;
    m_balkTo = balkTo;
}

void Station::setReneging(std::unique_ptr<IDistribution> patience, INode* renegeTo) {
    if (!patience) throw ModelError("process '" + m_name + "': null patience distribution");
    m_patience = std::make_unique<DistributionExpression>(std::move(patience));
    m_renegeTo = renegeTo;
}
void Station::setReneging(ExpressionPtr patience, INode* renegeTo) {
    if (!patience) throw ModelError("process '" + m_name + "': null patience expression");
    m_patience = std::move(patience);
    m_renegeTo = renegeTo;
}

void Station::setUnitsNeeded(int units) {
    if (units < 1 || units > m_resource->capacity())
        throw ModelError("process '" + m_name + "': units must be between 1 and the "
                         "resource's capacity");
    m_unitsNeeded = units;
}

SimTime Station::drawService(NodeContext& ctx, const Entity& e) {
    if (m_serviceAttribute.empty()) {
        EvalContext ectx = ctx.evaluationContext(&e);
        return static_cast<SimTime>(asNumber(m_service->evaluate(ectx)));
    }
    assert(e.hasAttribute(m_serviceAttribute) &&
           "station reads service time from an attribute the entity does not have");
    const SimTime t = e.attribute(m_serviceAttribute);
    assert(t >= 0.0 && "negative service time");
    return t;
}

double Station::loadPerVisit() const {
    if (!m_serviceAttribute.empty()) return 0.0;
    const auto mean = m_service->meanIfKnown();
    if (!mean) return 0.0;          // unknowable; loadIsKnown() says so
    return *mean * m_unitsNeeded / m_resource->capacity();
}

bool Station::loadIsKnown() const {
    // A service time read from an attribute was never checkable either, and
    // has said so by returning 0 since v5.
    if (!m_serviceAttribute.empty()) return true;
    return m_service->meanIfKnown().has_value();
}

// -------------------------------------------------------- resource user --

SimTime Station::headOfLineSince() const {
    // The earliest moment anybody in this queue started waiting. A shared
    // resource compares this across every block that uses it, so the entity that
    // has waited longest anywhere gets the freed unit.
    SimTime earliest = std::numeric_limits<SimTime>::infinity();
    for (const auto& kv : m_waitingSince) earliest = std::min(earliest, kv.second.startTime());
    return earliest;
}

void Station::startFromQueue(NodeContext& ctx) {
    if (m_queue.isEmpty() || m_resource->unitsAvailable() < m_unitsNeeded) return;

    Entity* next = m_queue.pop();   // obeys this block's own discipline
    assert(next != nullptr);

    auto it = m_waitingSince.find(next->id());
    assert(it != m_waitingSince.end());
    it->second.end(ctx.now());
    const SimTime waited = it->second.duration();
    m_waitingSince.erase(it);

    beginService(ctx, next, waited);
}

// -------------------------------------------------------------- service --

void Station::beginService(NodeContext& ctx, Entity* e, SimTime waited) {
    m_resource->seize(m_unitsNeeded);
    m_unitsHeld += m_unitsNeeded;
    e->setAttribute("waitHere", waited);
    e->setAttribute("waitTime", e->attribute("waitTime") + waited);

    const Activity service(m_name, ctx.now(), drawService(ctx, *e));
    ctx.scheduleReturn(service.endTime(), e, this);

    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4);
        if (waited > 0.0) os << "starts after waiting " << waited << ", ";
        else              os << "resource free, ";
        os << "service " << service.duration() << " until " << service.endTime();
        ctx.trace().event(ctx.now(), "Seize", e->id(), m_name, os.str(),
                          m_queue.length(), m_resource->unitsBusy());
    }
}

void Station::joinQueue(NodeContext& ctx, Entity* e) {
    m_queue.push(e);
    m_waitingSince.emplace(e->id(), Delay(ctx.now()));

    // v7: start the patience timer. See onRenegeTimeout for why nothing is ever
    // cancelled when the entity is served instead.
    if (m_patience) {
        EvalContext pctx = ctx.evaluationContext(e);
        const SimTime giveUpAt =
            ctx.now() + static_cast<SimTime>(asNumber(m_patience->evaluate(pctx)));
        ctx.scheduleRenegeCheck(giveUpAt, e, this);
    }

    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << "resource busy, queued at position " << m_queue.length();
        ctx.trace().event(ctx.now(), "Queue", e->id(), m_name, os.str(),
                          m_queue.length(), m_resource->unitsBusy());
    }
}

void Station::enter(NodeContext& ctx, Entity* e) {
    m_stats.recordArrival(ctx.now());
    e->setAttribute("stationEntry", ctx.now());

    if (m_resource->unitsAvailable() >= m_unitsNeeded) {
        beginService(ctx, e, 0.0);
        return;
    }

    // v7 BALKING: refuse to join a queue that is already too long. Real
    // customers do this, and a model without it overstates how much demand a
    // slow server actually absorbs -- it keeps every balker in the queue and
    // reports a wait nobody would have tolerated.
    if (m_balkAt > 0 && m_queue.length() >= m_balkAt) {
        ++m_balked;
        if (ctx.trace().isOn()) {
            std::ostringstream os;
            os << "queue of " << m_queue.length() << " too long, balks";
            ctx.trace().event(ctx.now(), "Balk", e->id(), m_name, os.str(),
                              m_queue.length(), m_resource->unitsBusy());
        }
        ctx.route(e, m_balkTo);   // nullptr means it leaves the system
        return;
    }

    joinQueue(ctx, e);
}

void Station::onScheduledEvent(NodeContext& ctx, Entity* e) {
    // Service here is complete.
    m_resource->release(m_unitsNeeded);
    m_unitsHeld -= m_unitsNeeded;

    const SimTime waitHere = e->attribute("waitHere");
    const SimTime timeHere = ctx.now() - e->attribute("stationEntry");
    m_stats.recordDeparture(ctx.now(), waitHere, timeHere);

    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4) << "done here after " << timeHere;
        ctx.trace().event(ctx.now(), "Release", e->id(), m_name, os.str(),
                          m_queue.length(), m_resource->unitsBusy());
    }

    // v7: offer the freed unit through the RESOURCE, not straight back to this
    // block's own queue. When the resource is shared, somebody at another block
    // may have been waiting longer, and helping ourselves first would silently
    // give this block priority.
    m_resource->offerFreedUnit(ctx);

    ctx.route(e, m_next);
}

void Station::onRenegeTimeout(NodeContext& ctx, Entity* e) {
    // *** LAZY CANCELLATION. ***
    // The patience timer was scheduled when the entity queued, and nothing
    // cancels it when the entity is served -- because a binary heap cannot
    // remove an arbitrary element, which FutureEventList.hpp has said since v1.
    //
    // Instead the event fires regardless and asks: is this entity STILL in my
    // queue? If not, it was served long ago and the timer is stale, so ignore
    // it. That is the standard answer to "my priority queue cannot cancel", it
    // costs one extra event per queued entity, and it is far simpler than any
    // structure that supports real cancellation.
    if (!m_queue.remove(e)) return;   // already served -- stale timer

    auto it = m_waitingSince.find(e->id());
    if (it != m_waitingSince.end()) {
        it->second.end(ctx.now());
        e->setAttribute("waitTime", e->attribute("waitTime") + it->second.duration());
        m_waitingSince.erase(it);
    }
    ++m_reneged;

    if (ctx.trace().isOn()) {
        ctx.trace().event(ctx.now(), "Renege", e->id(), m_name, "gave up waiting",
                          m_queue.length(), m_resource->unitsBusy());
    }
    ctx.route(e, m_renegeTo);
}

void Station::resetStatistics(SimTime now) {
    m_stats.restartAt(now);
    m_queue.resetStatistics();
    m_balked = 0;
    m_reneged = 0;
}

void Station::reset() {
    // NOTE: the resource is NOT reset here. It is shared, so several blocks
    // would each reset it and the last one would win -- harmless today, but the
    // sort of thing that becomes a bug the moment reset does more. The Model
    // owns the resources and resets them exactly once.
    m_queue.reset();
    m_stats.reset();
    m_service->reset();
    if (m_patience) m_patience->reset();
    m_waitingSince.clear();
    m_unitsHeld = 0;
    m_balked = 0;
    m_reneged = 0;
}

std::string Station::describe() const {
    std::ostringstream os;
    os << "Process " << m_name
       << " [resource " << m_resource->name() << " x" << m_unitsNeeded
       << " of " << m_resource->capacity()
       << ", " << m_queue.ruleName()
       << ", service " << (m_serviceAttribute.empty()
                           ? m_service->describe()
                           : "attribute:" + m_serviceAttribute);
    if (m_balkAt > 0)  os << ", balk at " << m_balkAt;
    if (m_patience)    os << ", patience " << m_patience->describe();
    os << ", next=" << (m_next ? m_next->name() : std::string("exit")) << "]";
    return os.str();
}

}  // namespace des
