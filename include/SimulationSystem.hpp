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
#include <string>
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
#include "EvalContext.hpp"
#include "Model.hpp"
#include "Entity.hpp"
#include "Delay.hpp"
#include "Node.hpp"
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

class SimulationSystem : public IModelState {
private:
    Clock m_clock;
    FutureEventList m_fel;
    SystemState m_state;
    Statistics m_stats;            // SYSTEM-wide: arrivals, exits, time in system
    RandomStream m_rng;            // the shared fallback stream

    // v8: one INDEPENDENT stream per named distribution, derived from the base
    // seed. Turning this on is what makes two model variants comparable --
    // change the service distribution and the arrival pattern stays put,
    // because arrivals draw from their own stream.
    bool m_separateStreams{false};
    bool m_antithetic{false};
    std::map<std::string, RandomStream> m_streams;
    void assignStreams();
    Trace m_trace;
    Model m_model;

    std::unique_ptr<ITerminationRule> m_termination;

    // v2.1: keyed by id so a departed entity can actually be destroyed.
    std::unordered_map<EntityId, std::unique_ptr<Entity>> m_entities;
    EntityId m_nextEntityId{1};

    // The Delay each waiting entity is currently inside. A delay is a fact about
    // the entity's relationship to the system, not an intrinsic property of the
    // entity, so it lives here and not on Entity.

    bool m_initialised{false};

    // --- v4: warm-up removal and observation series ---
    SimTime m_warmUp{0.0};              // 0 means no warm-up period
    SimTime m_warmUpEnded{0.0};         // when measurement actually began
    SimTime m_observeInterval{0.0};     // 0 means collect nothing
    std::vector<double> m_observations; // number-in-system, sampled on a grid

    void handleWarmUpEnd();
    void handleObservation();

    // --- internal steps of run(), not interface ---
    void handleDeparture(const EventNotice& notice);

    // v6: NodeContext is the only part of this class a node can see. It is a
    // friend so it can call the four private operations below and nothing else.
    friend class NodeContext;

    // Called by NodeContext::route() when a node routes to nullptr: the entity
    // has left the system. Records exit statistics, then destroys it.
    void disposeEntity(Entity* e);

    void refreshState();
    std::size_t stillWaitingCount() const;

    // v9: per-entity-type accounting -- Arena's NumberIn / NumberOut / WIP /
    // TotalTime, which a model with three arrival streams needs and a model with
    // one does not miss.
    struct TypeStats {
        long long in{0};
        long long out{0};
        int     inSystem{0};
        SimTime totalTime{0.0};
        SimTime maxTime{0.0};
        SimTime areaWIP{0.0};        // integral of number-in-system over time
        SimTime lastUpdate{0.0};
    };
    std::map<std::string, TypeStats> m_byType;
    void noteArrival(Entity* e);
    void noteExit(Entity* e);
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
    // v10: the narrow interface the expression layer asked for. Note what is
    // NOT here -- the FEL, the entity table, the statistics objects.
    double  queueLength(const std::string& blockName) const override;
    double  resourceBusy(const std::string& name) const override;
    double  resourceCapacity(const std::string& name) const override;
    double  numberInSystem() const override;
    SimTime now() const override;

    double variableAverage(const std::string& name) const;

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

    // v8: give every distribution in the model its own stream, derived from the
    // base seed and the distribution's role. Required for common random numbers
    // to mean anything; harmless otherwise.
    SimulationSystem& useSeparateStreams(bool on = true);

    // v8: draw 1-u everywhere. Run a replication normally, then again with this
    // set, and average the pair: the two are negatively correlated, so the
    // average of the pair has lower variance than two independent runs.
    SimulationSystem& useAntithetic(bool on = true);
    bool isAntithetic() const { return m_antithetic; }
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
                       Entity* e = nullptr, INode* node = nullptr);
    void initialise();
    void run();
    void report() const;

    // v9: the same numbers laid out the way Arena lays them out -- tally
    // variables, discrete-change variables, outputs -- so a run can be put
    // beside an Arena report and read line for line.
    void reportArenaStyle() const;

    // v5 conveniences. No new behaviour -- execute() is initialise() then run(),
    // and results() is the numbers report() prints, returned instead of printed.
    SimulationSystem& execute();
    RunResults results() const;
    const std::map<std::string, TypeStats>& byType() const { return m_byType; }

    // Chaining versions of the setters, so a whole study reads as one statement.
    SimulationSystem& stopWhen(std::unique_ptr<ITerminationRule> rule);
    SimulationSystem& stopAt(SimTime t);
    SimulationSystem& stopAfter(int entities);
    SimulationSystem& warmUpFor(SimTime t);
    SimulationSystem& traceTo(const std::string& path,
                              TraceLevel level = TraceLevel::Events);
};

}  // namespace des
