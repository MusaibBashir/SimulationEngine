// ============================================================================
// Station.hpp  --  the PROCESS node: seize, delay, release
// ============================================================================
// Arena calls this Process; this project has called it Station since v3 and the
// name is kept so existing models still compile. `Process` is an alias.
//
// It bundles the four things a service point owns:
//     a Resource (capacity c) + a queue + a service time + where entities go next
//
// v6: it is now an INode, and the seize/queue/release logic lives HERE rather
// than in SimulationSystem. That is the real structural change of this version.
// The engine used to know how service worked; now it only knows how to move time
// forward, and every behaviour is a node.

#pragma once

#include <map>
#include <memory>
#include <string>
#include "Common.hpp"
#include "Node.hpp"
#include "Resource.hpp"
#include "EntityQueue.hpp"
#include "Statistics.hpp"
#include "Distribution.hpp"
#include "Delay.hpp"

namespace des {

class Station : public INode {
private:
    Resource    m_resource;    // by value: the station OWNS its servers
    EntityQueue m_queue;       // and its waiting line
    std::unique_ptr<IDistribution> m_service;

    // v4.1: if non-empty, service duration is read from the entity's attribute
    // of this name. Job shops need it -- a job carries its own processing time,
    // and SPT then sequences on the number that is actually used.
    std::string m_serviceAttribute;

    Statistics m_stats;

    // v6: the Delay each queued entity is inside now lives with the QUEUE it is
    // waiting in, rather than in one map on the engine. A delay is a fact about
    // this queue, and moving it here is what let SimulationSystem stop knowing
    // anything about waiting.
    std::map<EntityId, Delay> m_waitingSince;

public:
    Station(std::string name, int capacity,
            std::unique_ptr<IQueueRule> rule,
            std::unique_ptr<IDistribution> service);

    // --- INode ---
    void enter(NodeContext& ctx, Entity* e) override;
    void onScheduledEvent(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;
    double loadPerVisit() const override;

    // --- configuration ---
    void setServiceFromAttribute(const std::string& attributeName);
    bool usesServiceAttribute() const { return !m_serviceAttribute.empty(); }
    const std::string& serviceAttributeName() const { return m_serviceAttribute; }
    void setRandomStream(RandomStream* rng) { m_queue.setRandomStream(rng); }

    // --- access ---
    Resource&    resource()       { return m_resource; }
    const Resource& resource() const { return m_resource; }
    EntityQueue& queue()          { return m_queue; }
    const EntityQueue& queue() const { return m_queue; }
    Statistics&  stats()          { return m_stats; }
    const Statistics& stats() const { return m_stats; }
    IDistribution& serviceDistribution() { return *m_service; }
    const IDistribution& serviceDistribution() const { return *m_service; }
    std::size_t stillWaiting() const { return m_waitingSince.size(); }

    SimTime drawService(const Entity& e, RandomStream& rng);

private:
    // Pull the next waiting entity into service, if any and if a server is free.
    void startNextService(NodeContext& ctx);
    void beginService(NodeContext& ctx, Entity* e, SimTime waited);
};

// Arena's name for the same block.
using Process = Station;

}  // namespace des
