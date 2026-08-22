// ============================================================================
// Station.cpp  --  the Process node
// ============================================================================

#include "Station.hpp"
#include "Entity.hpp"
#include "RandomStream.hpp"
#include "Activity.hpp"
#include "Trace.hpp"
#include <cassert>
#include <iomanip>
#include <sstream>

namespace des {

Station::Station(std::string name, int capacity,
                 std::unique_ptr<IQueueRule> rule,
                 std::unique_ptr<IDistribution> service)
    : INode(std::move(name)),
      m_resource(m_name, capacity),
      m_queue(m_name + "Queue", std::move(rule)),
      m_service(std::move(service)) {
    assert(m_service != nullptr && "a station needs a service distribution");
}

void Station::setServiceFromAttribute(const std::string& attributeName) {
    m_serviceAttribute = attributeName;
}

SimTime Station::drawService(const Entity& e, RandomStream& rng) {
    if (m_serviceAttribute.empty()) return m_service->draw(rng);
    assert(e.hasAttribute(m_serviceAttribute) &&
           "station reads service time from an attribute the entity does not have");
    const SimTime t = e.attribute(m_serviceAttribute);
    assert(t >= 0.0 && "negative service time");
    return t;
}

double Station::loadPerVisit() const {
    // Mean resource-time consumed per entity, divided by capacity. The Model
    // multiplies this by the arrival rate to get rho. Returns 0 when the service
    // time rides on the entity -- the Model looks that case up separately.
    if (!m_serviceAttribute.empty()) return 0.0;
    return m_service->mean() / m_resource.capacity();
}

void Station::beginService(NodeContext& ctx, Entity* e, SimTime waited) {
    m_resource.seize();
    e->setAttribute("waitHere", waited);
    e->setAttribute("waitTime", e->attribute("waitTime") + waited);

    // An Activity: the duration is drawn NOW, so its end can be scheduled NOW.
    // That is exactly what distinguishes an activity from a delay.
    const Activity service(m_name, ctx.now(), drawService(*e, ctx.rng()));
    ctx.scheduleReturn(service.endTime(), e, this);

    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4);
        if (waited > 0.0) os << "starts after waiting " << waited << ", ";
        else              os << "server free, ";
        os << "service " << service.duration() << " until " << service.endTime();
        ctx.trace().event(ctx.now(), "Seize", e->id(), m_name, os.str(),
                          m_queue.length(), m_resource.unitsBusy());
    }
}

void Station::enter(NodeContext& ctx, Entity* e) {
    m_stats.recordArrival(ctx.now());
    e->setAttribute("stationEntry", ctx.now());

    if (m_resource.isAvailable()) {
        beginService(ctx, e, 0.0);
        return;
    }

    // A Delay: its end is unknown now and will be decided by the system,
    // whenever a server here frees up.
    m_queue.push(e);
    m_waitingSince.emplace(e->id(), Delay(ctx.now()));

    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << "all " << m_resource.capacity() << " busy, queued at position "
           << m_queue.length();
        ctx.trace().event(ctx.now(), "Queue", e->id(), m_name, os.str(),
                          m_queue.length(), m_resource.unitsBusy());
    }
}

void Station::startNextService(NodeContext& ctx) {
    if (m_queue.isEmpty() || !m_resource.isAvailable()) return;

    Entity* next = m_queue.pop();   // obeys this station's discipline
    assert(next != nullptr);

    // End that entity's Delay. Its waiting time falls out of this, which is the
    // whole reason Delay is a class rather than a bare timestamp.
    auto it = m_waitingSince.find(next->id());
    assert(it != m_waitingSince.end());
    it->second.end(ctx.now());
    const SimTime waited = it->second.duration();
    m_waitingSince.erase(it);

    next->setAttribute("stationEntry", next->attribute("stationEntry"));
    beginService(ctx, next, waited);
}

void Station::onScheduledEvent(NodeContext& ctx, Entity* e) {
    // Service here is complete.
    m_resource.release();

    const SimTime waitHere = e->attribute("waitHere");
    const SimTime timeHere = ctx.now() - e->attribute("stationEntry");
    m_stats.recordDeparture(ctx.now(), waitHere, timeHere);

    if (ctx.trace().isOn()) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4) << "done here after " << timeHere;
        ctx.trace().event(ctx.now(), "Release", e->id(), m_name, os.str(),
                          m_queue.length(), m_resource.unitsBusy());
    }

    // Free server, so pull the next one in BEFORE routing this entity onward --
    // routing may take an arbitrary path through the model and we want the
    // server busy again as of this instant either way.
    startNextService(ctx);
    ctx.route(e, m_next);
}

void Station::resetStatistics(SimTime now) {
    m_stats.restartAt(now);
    m_queue.resetStatistics();
    // The resource, the queue contents and the waiting delays are STATE, not
    // statistics, and survive untouched. That is the whole point of warm-up
    // removal: measurement restarts from a loaded system.
}

void Station::reset() {
    m_resource.reset();
    m_queue.reset();
    m_stats.reset();
    m_service->reset();
    m_waitingSince.clear();
    // m_next is topology, not run state -- it survives.
}

std::string Station::describe() const {
    std::ostringstream os;
    os << "Process " << m_name
       << " [c=" << m_resource.capacity()
       << ", " << m_queue.ruleName()
       << ", service " << (m_serviceAttribute.empty()
                           ? m_service->describe()
                           : "attribute:" + m_serviceAttribute)
       << ", next=" << (m_next ? m_next->name() : std::string("exit"))
       << "]";
    return os.str();
}

}  // namespace des
