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
#include <vector>
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
#include "ModelError.hpp"
#include <string>
#include <vector>

namespace des {


// v5: everything a run produced, in one object with the divisions already done.
// Before this, reading a station's utilisation meant
//     sim.model().stationAt(0).stats().utilisation(sim.measuredTime(), cap)
// which requires knowing that you divide by measuredTime() and not by the
// clock. Getting that wrong silently understates every time average, so the
// engine should not have been leaving it to the caller.
struct StationResults {
    std::string name;
    int    capacity{0};
    int    served{0};
    double averageWait{0.0};
    double averageTimeHere{0.0};
    double maxWait{0.0};
    double averageQueueLength{0.0};
    std::size_t maxQueueLength{0};
    double utilisation{0.0};
};

struct RunResults {
    SimTime simulatedTime{0.0};
    SimTime warmUpDiscarded{0.0};
    SimTime measuredTime{0.0};
    int     arrived{0};
    int     exited{0};
    double  averageWait{0.0};          // total, across every station visited
    double  averageTimeInSystem{0.0};
    double  maxWait{0.0};
    double  averageNumberInQueue{0.0}; // Lq
    double  averageNumberInSystem{0.0};// L
    std::size_t stillWaitingAtStop{0};
    std::vector<StationResults> stations;

    // Look a station up by name; throws ModelError if there is no such station,
    // rather than returning a zeroed struct that reads as a very idle server.
    const StationResults& station(const std::string& name) const;
};

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

    // --- v4: warm-up removal and observation series ---
    SimTime m_warmUp{0.0};              // 0 means no warm-up period
    SimTime m_warmUpEnded{0.0};         // when measurement actually began
    SimTime m_observeInterval{0.0};     // 0 means collect nothing
    std::vector<double> m_observations; // number-in-system, sampled on a grid

    void handleWarmUpEnd();
    void handleObservation();

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

    // v4: discard everything measured before time t. The system state is KEPT --
    // only the statistics are thrown away -- so measurement starts from a
    // realistically loaded system instead of an empty one. That bias is most of
    // why Wq has read a few percent high since v2.
    void setWarmUp(SimTime t);

    // v4: sample number-in-system every dt, for Welch's warm-up analysis.
    // Sampling on a fixed grid (rather than at events) is what lets several
    // replications be averaged index by index.
    void setObservationInterval(SimTime dt);
    const std::vector<double>& observations() const { return m_observations; }

    // Length of the MEASURED period. Not clock.now(): with warm-up removal the
    // averages divide by this, and passing the full clock would understate them.
    SimTime measuredTime() const { return m_clock.now() - m_warmUpEnded; }
    SimTime warmUpEnd() const { return m_warmUpEnded; }
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

    // v5 conveniences. No new behaviour -- execute() is initialise() then run(),
    // and results() is the numbers report() prints, returned instead of printed.
    SimulationSystem& execute();
    RunResults results() const;

    // Chaining versions of the setters, so a whole study reads as one statement.
    SimulationSystem& stopWhen(std::unique_ptr<ITerminationRule> rule);
    SimulationSystem& stopAt(SimTime t);
    SimulationSystem& stopAfter(int entities);
    SimulationSystem& warmUpFor(SimTime t);
    SimulationSystem& traceTo(const std::string& path,
                              TraceLevel level = TraceLevel::Events);
};

}  // namespace des
