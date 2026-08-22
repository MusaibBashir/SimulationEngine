// ============================================================================
// SimulationSystem.hpp  --  the SYSTEM. The owner of everything.
// ============================================================================
// Theory: "A collection of components/entities interacting together to achieve
// an outcome."
// Structurally: this class OWNS every object in the simulation. Everybody else
// holds non-owning raw pointers into what this class owns. One owner, many
// observers -- decide that once, here, and the memory story is settled forever.
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
// [2] Includes: <vector>, <memory> (unique_ptr), <string>, and every one of your
//     own headers whose objects it holds BY VALUE:
//       Clock.hpp, FutureEventList.hpp, SystemState.hpp, Statistics.hpp,
//       TerminationCondition.hpp, Entity.hpp, Resource.hpp, EntityQueue.hpp
//     This is the ONE file allowed to include almost everything -- it sits at
//     the top of the dependency graph. If any OTHER header grows an include list
//     this long, something has gone wrong there.
//
// [3] class SimulationSystem, private data:
//
//     --- owned by value (composition: they live and die with the system) ---
//     [3a] Clock m_clock
//     [3b] FutureEventList m_fel
//     [3c] SystemState m_state
//     [3d] Statistics m_stats
//     [3e] TerminationCondition m_termination
//
//     --- owned by pointer (variable in number, handed out as raw pointers) ---
//     [3f] std::vector<std::unique_ptr<Entity>>      m_entities
//     [3g] std::vector<std::unique_ptr<Resource>>    m_resources
//     [3h] std::vector<std::unique_ptr<EntityQueue>> m_queues
//          unique_ptr states "I OWN THIS" in the type itself. The queues hold
//          raw Entity* pointing INTO these vectors' objects; raw pointer means
//          "I observe, I do not own". The two pointer types together document
//          the entire ownership model with zero comments needed.
//          // NOTE v2: pushing into a vector<unique_ptr> reallocates the vector,
//          // but the OBJECTS do not move -- only the pointers-to-pointers do.
//          // So Entity* handed out earlier stay valid. Convince yourself of this
//          // before you rely on it; it is why unique_ptr and not plain objects.
//
//     [3i] EntityId m_nextEntityId   : the id counter, starting at 1.
//          Exactly ONE of these in the program. Entities do not mint their own.
//
// [4] PUBLIC INTERFACE -- v1 ACTUALLY IMPLEMENTS THESE (all pure storage):
//
//     [4a] explicit SimulationSystem(TerminationCondition termination)
//          Init m_termination from the parameter, m_nextEntityId to 1,
//          everything else default-constructs itself. Mark it `explicit`:
//          one parameter, so without it a TerminationCondition could implicitly
//          convert into a whole SimulationSystem, which is nonsense.
//
//     [4b] Entity* createEntity();
//          THE FACTORY. Three steps:
//            - make_unique<Entity>(m_nextEntityId, m_clock.now()), then bump
//              m_nextEntityId
//            - move it into m_entities  (std::move -- unique_ptr cannot be copied,
//              and the compiler will tell you so if you forget)
//            - return the raw pointer: m_entities.back().get()
//          THE ONLY PLACE IN THE PROGRAM WHERE AN ENTITY IS BORN.
//
//     [4c] Resource* addResource(const std::string& name, int capacity);
//          Same three-step pattern, into m_resources.
//     [4d] EntityQueue* addQueue(const std::string& name, QueueDiscipline d);
//          Same pattern, into m_queues.
//
//     [4e] Resource* resource(const std::string& name);
//          v1: linear scan comparing name(), return the raw pointer or nullptr.
//          // TODO v2: a std::map<std::string, Resource*> index. Linear is fine
//          // for five resources and wrong for five hundred. Note it, move on --
//          // do not optimise before the profile says so.
//     [4f] EntityQueue* queue(const std::string& name);
//
//     [4g] Const accessors, all one-liners returning const references:
//          const Clock& clock() const
//          const Statistics& statistics() const
//          const SystemState& state() const
//          const FutureEventList& fel() const
//          Return CONST references so callers can read but not meddle. Handing
//          out a non-const reference to m_stats would let anyone anywhere edit
//          your results, and encapsulation would be decorative.
//
// [5] V1 STUBS -- declare them all. THIS LIST IS THE ENTIRE v2 WORKLOAD:
//
//     [5a] void initialise();
//          The "Initialization" row of the table: set state and FEL at t = 0.
//          v2: reset clock/stats/state, create the first entity, schedule its
//          Arrival, and schedule the EndSimulation event at m_termination.maxTime().
//
//     [5b] void scheduleEvent(EventType type, SimTime t, Entity* e, Resource* r);
//          v2: build an EventNotice and hand it to m_fel.schedule().
//
//     [5c] void run();
//          THE MAIN LOOP. v2, and it is only about eight lines:
//            while FEL not empty AND termination not met:
//              notice = m_fel.popImminent()
//              m_stats.updateTimeIntegrals(notice.time(), ...)   <- BEFORE
//              m_clock.advanceTo(notice.time())
//              switch on notice.type() -> call the matching handler
//          The clock JUMPS to each event. Nothing happens between events, which
//          is precisely why discrete-event simulation is fast.
//          // v3: that switch becomes polymorphic dispatch on an IEventHandler.
//          // Write the switch first and let it get ugly.
//
//     [5d] void handleArrival(const EventNotice&);
//          v2 sketch: record arrival; create the NEXT arrival and schedule it
//          (this is how a DES keeps itself fed); if the resource is available,
//          seize it and schedule the Departure; otherwise push into the queue.
//     [5e] void handleDeparture(const EventNotice&);
//          v2 sketch: release the resource; record the departure statistics; if
//          the queue is non-empty, pop the next entity, seize, schedule its
//          Departure.
//     [5f] void report() const;
//          v2: print the derived quantities from m_stats.
//
// [6] *** RULE OF FIVE -- WRITE THESE TWO LINES IN v1 ***
//     Because this class holds unique_ptrs it is MOVE-ONLY. Explicitly delete
//     the copy operations in the public section:
//         SimulationSystem(const SimulationSystem&) = delete;
//         SimulationSystem& operator=(const SimulationSystem&) = delete;
//     Two lines. They turn "you accidentally copied the whole simulation" from a
//     mysterious runtime disaster into a clear compile error. State the intent;
//     let the compiler enforce it.

#pragma once
#include <vector>
#include <memory>
#include <string>
#include <map>
#include "Common.hpp"
#include "Clock.hpp"
#include "FutureEventList.hpp"
#include "SystemState.hpp"
#include "Statistics.hpp"
#include "TerminationCondition.hpp"
#include "RandomStream.hpp"
#include "Entity.hpp"
#include "Resource.hpp"
#include "EntityQueue.hpp"
#include "Delay.hpp"

class SimulationSystem {
private:
    // --- owned by value: they live and die with the system ---
    Clock m_clock;
    FutureEventList m_fel;
    SystemState m_state;
    Statistics m_stats;
    TerminationCondition m_termination;
    RandomStream m_rng;                       // v2: the ONE source of randomness

    // --- owned by pointer: variable in number ---
    std::vector<std::unique_ptr<Entity>> m_entities;
    std::vector<std::unique_ptr<Resource>> m_resources;
    std::vector<std::unique_ptr<EntityQueue>> m_queues;

    EntityId m_nextEntityId{1};

    // --- v2: model parameters ---
    SimTime m_meanInterarrival{1.0};
    SimTime m_meanService{0.8};

    // v2: which resource and queue this single-server model uses. Looked up ONCE
    // in initialise() and cached, instead of doing a string lookup inside every
    // event handler. The names are configurable so the engine is not welded to
    // the word "Teller".
    // v3 replaces all five of these with a proper model description object.
    std::string m_serverName{"Teller"};
    std::string m_queueName{"TellerQueue"};
    Resource*    m_server{nullptr};   // non-owning: m_resources owns it
    EntityQueue* m_line{nullptr};     // non-owning: m_queues owns it

    // v2: the Delay each waiting entity is currently inside. Keyed by entity id
    // rather than stored on Entity, because a delay is a fact about the entity's
    // relationship to THIS system, not an intrinsic property of the entity.
    std::map<EntityId, Delay> m_activeDelays;

    // --- internal steps of run(), not interface ---
    void handleArrival(const EventNotice& notice);
    void handleDeparture(const EventNotice& notice);
    void refreshState();   // v2: push the true counts into m_state

public:
    explicit SimulationSystem(TerminationCondition termination, unsigned seed = 12345u);

    SimulationSystem(const SimulationSystem&) = delete;
    SimulationSystem& operator=(const SimulationSystem&) = delete;

    // --- model building ---
    Entity* createEntity();
    Resource* addResource(const std::string& name, int capacity);
    EntityQueue* addQueue(const std::string& name, QueueDiscipline d);
    void setModel(const std::string& serverName, const std::string& queueName);
    void setMeanInterarrival(SimTime mean);
    void setMeanService(SimTime mean);

    // --- lookup ---
    Resource* resource(const std::string& name);
    EntityQueue* queue(const std::string& name);

    // --- read-only views ---
    const Clock& clock() const { return m_clock; }
    const Statistics& statistics() const { return m_stats; }
    const SystemState& state() const { return m_state; }
    const FutureEventList& fel() const { return m_fel; }
    const RandomStream& randomStream() const { return m_rng; }

    // --- running ---
    void initialise();
    void scheduleEvent(EventType type, SimTime t, Entity* e = nullptr, Resource* r = nullptr);
    void run();
    void report() const;
};
