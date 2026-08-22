// ============================================================================
// SimulationSystem.hpp  --  the ENGINE
// ============================================================================
// Theory: "A collection of components/entities interacting together to achieve
// an outcome."
//
// v3: this class no longer knows what is being simulated. It owns a Model and
// runs it. There is not a single station name, capacity or distribution in this
// file -- change the Model and you have a different system without touching a
// line of the engine. That separation is the point of the whole version.
//
// Ownership, unchanged since v1: ONE owner (this class, and the Model it holds),
// many raw observing pointers. unique_ptr means "I own this"; a raw pointer
// means "I look at this".

#pragma once
#include <memory>
#include <string>
#include <map>
#include <unordered_map>
#include "Common.hpp"
#include "Clock.hpp"
#include "FutureEventList.hpp"
#include "SystemState.hpp"
#include "Statistics.hpp"
#include "RandomStream.hpp"
#include "TerminationRule.hpp"
#include "Trace.hpp"
#include "Model.hpp"
#include "Entity.hpp"
#include "Delay.hpp"

class SimulationSystem {
private:
    Clock m_clock;
    FutureEventList m_fel;
    SystemState m_state;
    Statistics m_stats;            // SYSTEM-wide: arrivals, exits, time in system
    RandomStream m_rng;            // the one source of randomness
    Trace m_trace;
    Model m_model;

    std::unique_ptr<ITerminationRule> m_termination;

    // v2.1: keyed by id so a departed entity can actually be destroyed.
    std::unordered_map<EntityId, std::unique_ptr<Entity>> m_entities;
    EntityId m_nextEntityId{1};

    // The Delay each waiting entity is currently inside. A delay is a fact about
    // the entity's relationship to the system, not an intrinsic property of the
    // entity, so it lives here and not on Entity.
    std::map<EntityId, Delay> m_activeDelays;

    bool m_initialised{false};

    // --- internal steps of run(), not interface ---
    void handleArrival(const EventNotice& notice);
    void handleDeparture(const EventNotice& notice);

    // v3: the routing primitive. Put an entity into a station -- seize a server
    // if one is free, otherwise queue it and open a Delay. Used both by arrival
    // (into the entry station) and by departure (into the NEXT station). Having
    // one function for both is what makes a chain work at all.
    void admit(Entity* e, Station* station);

    // Pull the next waiting entity at this station into service, if any.
    void startNextService(Station* station);

    void refreshState();
    void updateAllIntegrals(SimTime upTo);

    // SAFETY INVARIANT: an Entity may only be destroyed when nothing holds a raw
    // pointer to it -- not in any queue, no Delay in m_activeDelays, and its
    // event notice already popped off the FEL.
    void destroyEntity(EntityId id);

public:
    explicit SimulationSystem(unsigned seed = 12345u);

    SimulationSystem(const SimulationSystem&) = delete;
    SimulationSystem& operator=(const SimulationSystem&) = delete;

    // --- setup ---
    Model& model() { return m_model; }
    const Model& model() const { return m_model; }
    void setTermination(std::unique_ptr<ITerminationRule> rule);
    bool enableTrace(const std::string& path, TraceLevel level, bool markdown = true);

    // --- read-only views ---
    const Clock& clock() const { return m_clock; }
    const Statistics& statistics() const { return m_stats; }
    const SystemState& state() const { return m_state; }
    const FutureEventList& fel() const { return m_fel; }
    const RandomStream& randomStream() const { return m_rng; }
    std::size_t liveEntityCount() const { return m_entities.size(); }

    // --- running ---
    Entity* createEntity();
    void scheduleEvent(EventType type, SimTime t,
                       Entity* e = nullptr, Station* s = nullptr);
    void initialise();
    void run();
    void report() const;
};
