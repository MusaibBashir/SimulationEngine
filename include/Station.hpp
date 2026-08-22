// ============================================================================
// Station.hpp  --  the PROCESS block: seize, delay, release
// ============================================================================
// Arena calls it Process; this project has called it Station since v3 and both
// names work.
//
// v7 changes what it OWNS. Through v6 a Process owned its servers, which made
// "one operator shared across three machines" inexpressible -- and that is an
// extremely common thing to want to model. Now the Model owns the resources and
// a Process holds a non-owning pointer to one. Several Processes can point at
// the same resource and compete for it.
//
// v7 also adds the two ways a customer refuses to wait:
//   BALKING  -- will not join a queue that is already too long
//   RENEGING -- joins, waits, then gives up

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

class Station : public INode, public IResourceUser {
private:
    Resource* m_resource{nullptr};   // NON-OWNING: the Model owns it, and other
                                     // Process blocks may point at the same one
    int m_unitsNeeded{1};
    EntityQueue m_queue;
    std::unique_ptr<IDistribution> m_service;
    std::string m_serviceAttribute;
    Statistics m_stats;

    // The Delay each queued entity is inside lives with the queue it waits in.
    std::map<EntityId, Delay> m_waitingSince;

    // --- v7: balking ---
    // Refuse to join if the queue is already this long. 0 = never balk.
    std::size_t m_balkAt{0};
    INode* m_balkTo{nullptr};
    long long m_balked{0};

    // --- v7: reneging ---
    // How long an entity will wait before giving up. null = infinite patience.
    std::unique_ptr<IDistribution> m_patience;
    INode* m_renegeTo{nullptr};
    long long m_reneged{0};

    // v7 FIX: how many units THIS block currently holds. Before v7 a Process
    // owned its resource, so resource().unitsBusy() was this block's own usage
    // and the statistics used it directly. With a shared resource that number
    // is the total across every block using it -- so two blocks sharing one
    // operator each reported the operator's whole utilisation, and the two
    // summed to twice the truth. Utilisation must be measured from what this
    // block holds, not from what the resource is doing.
    int m_unitsHeld{0};

public:
    Station(std::string name, Resource* resource, int unitsNeeded,
            std::unique_ptr<IQueueRule> rule,
            std::unique_ptr<IDistribution> service);

    // --- INode ---
    void enter(NodeContext& ctx, Entity* e) override;
    void onScheduledEvent(NodeContext& ctx, Entity* e) override;
    void onRenegeTimeout(NodeContext& ctx, Entity* e) override;
    void reset() override;
    void resetStatistics(SimTime now) override;
    std::string describe() const override;
    double loadPerVisit() const override;

    // --- IResourceUser: how a shared resource decides who gets a freed unit ---
    bool hasWaiting() const override { return !m_queue.isEmpty(); }
    SimTime headOfLineSince() const override;
    void startFromQueue(NodeContext& ctx) override;

    // --- configuration ---
    void setServiceFromAttribute(const std::string& attributeName);
    bool usesServiceAttribute() const { return !m_serviceAttribute.empty(); }
    const std::string& serviceAttributeName() const { return m_serviceAttribute; }
    void setRandomStream(RandomStream* rng) { m_queue.setRandomStream(rng); }
    void setBalking(std::size_t queueLength, INode* balkTo);
    void setReneging(std::unique_ptr<IDistribution> patience, INode* renegeTo);
    void setUnitsNeeded(int units);

    // --- access ---
    Resource&    resource()       { return *m_resource; }
    const Resource& resource() const { return *m_resource; }
    EntityQueue& queue()          { return m_queue; }
    const EntityQueue& queue() const { return m_queue; }
    Statistics&  stats()          { return m_stats; }
    const Statistics& stats() const { return m_stats; }
    IDistribution& serviceDistribution() { return *m_service; }
    const IDistribution& serviceDistribution() const { return *m_service; }
    std::size_t stillWaiting() const { return m_waitingSince.size(); }
    long long balked() const { return m_balked; }
    long long reneged() const { return m_reneged; }
    int unitsHeld() const { return m_unitsHeld; }
    int unitsNeeded() const { return m_unitsNeeded; }

    SimTime drawService(const Entity& e, RandomStream& rng);

private:
    void beginService(NodeContext& ctx, Entity* e, SimTime waited);
    void joinQueue(NodeContext& ctx, Entity* e);
};

using Process = Station;

}  // namespace des
