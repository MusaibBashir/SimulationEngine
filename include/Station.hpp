// ============================================================================
// Station.hpp  --  v3 step 3/4: one place where service happens
// ============================================================================
// v2 had a single cached m_server and m_line inside SimulationSystem. That was
// fine for one server and broke the moment you wanted a restaurant: host, then
// waiter, then cashier. A Station bundles the four things a service point owns:
//
//     a Resource (capacity c)  +  a queue  +  a service distribution
//     +  where entities go NEXT
//
// The last one is the whole point. Routing is what turns a queue into a network.

#pragma once

#include <string>
#include <memory>
#include "Common.hpp"
#include "Resource.hpp"
#include "EntityQueue.hpp"
#include "Statistics.hpp"
#include "Distribution.hpp"

class Station {
private:
    std::string m_name;
    Resource    m_resource;    // by value: the station OWNS its servers
    EntityQueue m_queue;       // by value: and its waiting line
    std::unique_ptr<IDistribution> m_service;

    // Where an entity goes after being served here. nullptr means "leaves the
    // system". A raw pointer because the Model owns every Station -- same
    // owner/observer rule as everywhere else in this project.
    Station* m_next{nullptr};

    // Per-station statistics. Utilisation and queue length are properties of a
    // station, not of the system: a restaurant can have an idle host and a
    // swamped kitchen, and one system-wide number would hide exactly that.
    Statistics m_stats;

public:
    Station(std::string name, int capacity,
            std::unique_ptr<IQueueRule> rule,
            std::unique_ptr<IDistribution> service);

    Station(const Station&) = delete;
    Station& operator=(const Station&) = delete;

    const std::string& name() const { return m_name; }
    Resource&    resource()       { return m_resource; }
    const Resource& resource() const { return m_resource; }
    EntityQueue& queue()          { return m_queue; }
    const EntityQueue& queue() const { return m_queue; }
    Statistics&  stats()          { return m_stats; }
    const Statistics& stats() const { return m_stats; }

    IDistribution& serviceDistribution() { return *m_service; }
    const IDistribution& serviceDistribution() const { return *m_service; }

    Station* next() const { return m_next; }
    void setNext(Station* next) { m_next = next; }
    bool isExit() const { return m_next == nullptr; }

    // Run state resets; name, capacity, rule and distribution survive.
    void reset();

    std::string describe() const;
};
