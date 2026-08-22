// ============================================================================
// SimulationSystem.cpp  --  the engine
// ============================================================================

#include "SimulationSystem.hpp"
#include "Activity.hpp"
#include "Build.hpp"
#include "Nodes.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <utility>
#include <cassert>

namespace des {


SimulationSystem::SimulationSystem(unsigned seed) : m_rng(seed) {}

// ------------------------------------------------------------------- setup --

void SimulationSystem::setTermination(std::unique_ptr<ITerminationRule> rule) {
    assert(rule != nullptr);
    m_termination = std::move(rule);
}

void SimulationSystem::setWarmUp(SimTime t) {
    assert(t >= 0.0);
    m_warmUp = t;
}

void SimulationSystem::setObservationInterval(SimTime dt) {
    assert(dt > 0.0);
    m_observeInterval = dt;
}

bool SimulationSystem::enableTrace(const std::string& path, TraceLevel level, bool markdown) {
    return m_trace.open(path, level, markdown);
}

// --------------------------------------------------------- entity lifetime --

Entity* SimulationSystem::createEntity() {
    const EntityId id = m_nextEntityId++;
    auto e = std::make_unique<Entity>(id, m_clock.now());
    Entity* raw = e.get();                // address BEFORE the move: after
    m_entities.emplace(id, std::move(e)); // std::move the local is null
    return raw;
}

void SimulationSystem::destroyEntity(EntityId id) {
    m_entities.erase(id);
}

// ------------------------------------------------------------------ state --

// ---------------------------------------------------------- NodeContext --
// The narrow facade a node sees. Six operations, and nothing else: a node
// cannot reach the event list, the statistics, the entity table or the clock.
// That is the whole reason this class exists rather than making the engine's
// members public.

SimTime NodeContext::now() const { return m_sim.m_clock.now(); }
RandomStream& NodeContext::rng() { return m_sim.m_rng; }
Trace& NodeContext::trace() { return m_sim.m_trace; }
Entity* NodeContext::createEntity() { return m_sim.createEntity(); }
void NodeContext::destroy(Entity* e) { m_sim.destroyEntity(e->id()); }

void NodeContext::route(Entity* e, INode* to) {
    if (to == nullptr) { m_sim.disposeEntity(e); return; }
    // Direct call, not an event: moving between blocks takes no simulated time
    // unless a block says otherwise. A long chain of Assign/Decide/Record blocks
    // therefore recurses -- fine for any sane flowchart, and the alternative
    // (a zero-delay event for every hop) would bloat the FEL for no benefit.
    to->enter(*this, e);
}

void NodeContext::scheduleReturn(SimTime at, Entity* e, INode* node) {
    m_sim.scheduleEvent(EventType::Departure, at, e, node);
}

// ------------------------------------------------------------------ state --

std::size_t SimulationSystem::stillWaitingCount() const {
    std::size_t n = 0;
    for (std::size_t i = 0; i < m_model.stationCount(); ++i)
        n += m_model.stationAt(i).stillWaiting();
    return n;
}

void SimulationSystem::refreshState() {
    // SystemState is a SNAPSHOT summed across every process block. The blocks
    // remain the single source of truth; SystemState never decides anything.
    int queued = 0;
    int busy   = 0;
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        const Station& s = m_model.stationAt(i);
        queued += static_cast<int>(s.queue().length());
        busy   += s.resource().unitsBusy();
    }
    m_state.setNumberInQueue(queued);
    m_state.setNumberInSystem(queued + busy);
    m_state.setServerStatus(busy > 0 ? ResourceState::Busy : ResourceState::Idle);
}

void SimulationSystem::updateAllIntegrals(SimTime upTo) {
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        Station& s = m_model.stationAt(i);
        s.stats().updateTimeIntegrals(upTo,
                                      static_cast<int>(s.queue().length()),
                                      s.resource().unitsBusy());
    }
    m_stats.updateTimeIntegrals(upTo, m_state.numberInQueue(), m_state.numberInSystem());
}

void SimulationSystem::disposeEntity(Entity* e) {
    // The entity has left the system. Record it against the system-wide
    // statistics, then destroy it -- LAST, because everything above reads
    // through the pointer and after destruction it dangles.
    const SimTime wait     = e->attribute("waitTime");
    const SimTime inSystem = m_clock.now() - e->creationTime();
    m_stats.recordDeparture(m_clock.now(), wait, inSystem);

    if (m_trace.isOn()) {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4)
           << "exits; total wait " << wait << ", time in system " << inSystem;
        m_trace.event(m_clock.now(), "Exit", e->id(), "-", os.str(), 0, 0);
    }

    // A temporary batch that reaches an exit without being separated takes its
    // members with it -- they are still in the system in every sense that
    // matters, and leaking them would grow the entity table forever.
    for (Entity* member : e->members()) destroyEntity(member->id());
    destroyEntity(e->id());
    refreshState();
}

void SimulationSystem::initialise() {
    m_model.validate();          // catch modelling mistakes BEFORE the run
    assert(m_termination != nullptr && "no termination rule set");

    // Everything holding RUN STATE gets reset. Configuration survives.
    m_clock.reset();
    m_stats.reset();
    m_state.reset();
    m_rng.reset();
    m_fel.clear();
    m_entities.clear();
    m_nextEntityId = 1;
    m_model.reset();
    m_warmUpEnded = 0.0;
    m_observations.clear();

    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        m_model.stationAt(i).queue().setRandomStream(&m_rng);
    }

    if (m_trace.isOn()) {
        std::ostringstream os;
        os << "**Seed** " << m_rng.seed() << "  \n"
           << "**Termination** " << m_termination->describe() << "  \n"
           << "**Model**\n\n```\n" << m_model.describe() << "\n```";
        m_trace.note(os.str());
    }

    scheduleEvent(EventType::Arrival, 0.0);   // carries no entity -- see handleArrival
    if (m_warmUp > 0.0)          scheduleEvent(EventType::WarmUpEnd, m_warmUp);
    if (m_observeInterval > 0.0) scheduleEvent(EventType::Observe, m_observeInterval);
    m_initialised = true;
    refreshState();
}

void SimulationSystem::scheduleEvent(EventType type, SimTime t, Entity* e, INode* node) {
    // Scheduling into the PAST is the most common DES bug. Catch it where the
    // mistake is made, not ten thousand events later.
    assert(t >= m_clock.now());
    m_fel.schedule(EventNotice(type, t, e, node));
}

void SimulationSystem::run() {
    assert(m_initialised && "call initialise() before run()");

    while (!m_fel.isEmpty() && !m_termination->isMet(*this)) {
        EventNotice notice = m_fel.popImminent();

        // ORDER IS EVERYTHING: close the integrals for the interval that just
        // ended -- using the OLD state -- then move the clock, then let the
        // handler change state. Swap any two and every time average is wrong.
        updateAllIntegrals(notice.time());
        m_clock.advanceTo(notice.time());

        switch (notice.type()) {
            case EventType::Arrival:       handleArrival(notice);   break;
            case EventType::Departure:     handleDeparture(notice); break;
            case EventType::EndSimulation: return;
            case EventType::WarmUpEnd:     handleWarmUpEnd();       break;
            case EventType::Observe:       handleObservation();     break;
            case EventType::StartService:  break;   // reserved
        }
    }
    // The clock JUMPS event to event. Nothing is simulated in between because
    // nothing happens in between -- that is why DES is fast, and why the FEL
    // had to be sorted.
    //
    // The switch stays -- it dispatches on the KIND OF EVENT, of which there are
    // six, and that is genuinely all this loop does now.
    //
    // What v6 changed is the other axis. Five versions running, this project
    // declined to build an IEventHandler hierarchy because handler objects
    // outside this class would have needed its internals made public. v6 builds
    // it anyway, because nodes must live outside the engine and Batch is exactly
    // the "handler that carries state" that was named as the trigger.
    //
    // The resolution was not to widen this class's public interface. It was
    // NodeContext: a narrow, role-specific facade holding the six operations a
    // node may perform. The general lesson -- when an abstraction says it needs
    // your internals, publish an interface for its ROLE, not your whole class.
}

// ------------------------------------------------------------------ routing --

void SimulationSystem::handleWarmUpEnd() {
    // *** WARM-UP REMOVAL, the whole of it. ***
    // Throw away every measurement taken so far and start again from now. The
    // SYSTEM STATE is deliberately untouched: entities in service stay in
    // service, queues keep their contents. Measurement now begins from a
    // realistically loaded system rather than an empty one.
    m_stats.restartAt(m_clock.now());
    // EVERY block, not just the process ones -- a Record or Decide counter that
    // spanned the warm-up while the utilisation next to it did not would be two
    // numbers in one report meaning different periods.
    for (std::size_t i = 0; i < m_model.nodeCount(); ++i)
        m_model.nodeAt(i).resetStatistics(m_clock.now());
    m_warmUpEnded = m_clock.now();

    if (m_trace.isOn()) {
        m_trace.event(m_clock.now(), "WarmUpEnd", 0, "-",
                      "statistics discarded; measurement starts here",
                      0, m_state.numberInSystem());
    }
}

void SimulationSystem::handleObservation() {
    // Sample on a FIXED GRID, not at events, so replication i's observation k
    // can be averaged with replication j's observation k. Event times never
    // line up; grid points do.
    m_observations.push_back(static_cast<double>(m_state.numberInSystem()));
    scheduleEvent(EventType::Observe, m_clock.now() + m_observeInterval);
}

void SimulationSystem::handleArrival(const EventNotice& /*notice*/) {
    // *** THE ARRIVAL EVENT CARRIES NO ENTITY; THE ENTITY IS BORN HERE. ***
    // Creating it earlier and attaching it to a future Arrival would stamp
    // creationTime before it actually arrived, inflating every time-in-system.
    Entity* arriving = createEntity();
    arriving->setAttribute("waitTime", 0.0);
    for (const auto& a : m_model.arrivalAttributes())
        arriving->setAttribute(a.name, a.distribution->draw(m_rng));
    m_stats.recordArrival(m_clock.now());

    if (m_trace.isOn()) {
        m_trace.event(m_clock.now(), "Arrival", arriving->id(),
                      m_model.entry()->name(), "enters the system", 0, 0);
    }

    // *** FEED THE SIMULATION. *** Forget this and the run stops after one
    // entity -- the classic first-run bug.
    scheduleEvent(EventType::Arrival, m_clock.now() + m_model.interarrival().draw(m_rng));

    // Hand it to the first block. From here the FLOWCHART decides everything;
    // the engine's only remaining job is to move the clock.
    NodeContext ctx(*this);
    ctx.route(arriving, m_model.entry());
    refreshState();
}

void SimulationSystem::handleDeparture(const EventNotice& notice) {
    // A block asked to be called back at this time -- a service or a delay has
    // finished. Which block, and what that means, is entirely the block's
    // business. v5's engine knew how service worked; this one does not.
    Entity* e    = notice.entity();
    INode*  node = notice.node();
    assert(e != nullptr && node != nullptr);

    NodeContext ctx(*this);
    node->onScheduledEvent(ctx, e);
    refreshState();
}

// ------------------------------------------------------------------ report --

const StationResults& RunResults::station(const std::string& name) const {
    for (const auto& s : stations) {
        if (s.name == name) return s;
    }
    throw ModelError("results: no station named '" + name + "'");
}

SimulationSystem& SimulationSystem::execute() {
    initialise();
    run();
    return *this;
}

SimulationSystem& SimulationSystem::stopWhen(std::unique_ptr<ITerminationRule> rule) {
    setTermination(std::move(rule));
    return *this;
}
SimulationSystem& SimulationSystem::stopAt(SimTime t) { return stopWhen(timeLimit(t)); }
SimulationSystem& SimulationSystem::stopAfter(int entities) { return stopWhen(entityLimit(entities)); }
SimulationSystem& SimulationSystem::warmUpFor(SimTime t) { setWarmUp(t); return *this; }
SimulationSystem& SimulationSystem::traceTo(const std::string& path, TraceLevel level) {
    enableTrace(path, level);
    return *this;
}

RunResults SimulationSystem::results() const {
    // The measured period, NOT the clock. With a warm-up the two differ, and
    // dividing by the clock would understate every time average by exactly the
    // fraction of the run that was discarded. Doing it here once means no caller
    // can get it wrong.
    const SimTime measured = measuredTime();

    RunResults r;
    r.simulatedTime         = m_clock.now();
    r.warmUpDiscarded       = m_warmUpEnded;
    r.measuredTime          = measured;
    r.arrived               = m_stats.numberArrived();
    r.exited                = m_stats.numberServed();
    r.averageWait           = m_stats.averageWaitingTime();
    r.averageTimeInSystem   = m_stats.averageTimeInSystem();
    r.maxWait               = m_stats.maxWaitingTime();
    r.averageNumberInQueue  = m_stats.timeAverageA(measured);
    r.averageNumberInSystem = m_stats.timeAverageB(measured);
    r.stillWaitingAtStop    = stillWaitingCount();

    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        const Station& s = m_model.stationAt(i);
        StationResults sr;
        sr.name               = s.name();
        sr.capacity           = s.resource().capacity();
        sr.served             = s.stats().numberServed();
        sr.averageWait        = s.stats().averageWaitingTime();
        sr.averageTimeHere    = s.stats().averageTimeInSystem();
        sr.maxWait            = s.stats().maxWaitingTime();
        sr.averageQueueLength = s.stats().timeAverageA(measured);
        sr.maxQueueLength     = s.queue().maxLengthObserved();
        sr.utilisation        = s.stats().utilisation(measured, sr.capacity);
        r.stations.push_back(std::move(sr));
    }
    return r;
}

void SimulationSystem::report() const {
    // v5: report() prints results(). It does not recompute anything -- one place
    // knows how a number is derived, and printing is just a view of it.
    const RunResults r = results();
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "=== simulation report =====================================\n";
    std::cout << "seed                     : " << m_rng.seed() << "\n";
    std::cout << "termination              : "
              << (m_termination ? m_termination->describe() : "<none>") << "\n";
    std::cout << "total simulated time     : " << r.simulatedTime << "\n";
    std::cout << "warm-up discarded        : " << r.warmUpDiscarded << "\n";
    std::cout << "measured period          : " << r.measuredTime << "\n";
    std::cout << "entities arrived         : " << r.arrived << "\n";
    std::cout << "entities exited          : " << r.exited << "\n";
    std::cout << "average total wait       : " << r.averageWait << "\n";
    std::cout << "average time in system   : " << r.averageTimeInSystem << "\n";
    std::cout << "max total wait           : " << r.maxWait << "\n";
    std::cout << "time-avg in queue (Lq)   : " << r.averageNumberInQueue << "\n";
    std::cout << "time-avg in system (L)   : " << r.averageNumberInSystem << "\n";
    std::cout << "still waiting at stop    : " << r.stillWaitingAtStop << "\n";
    std::cout << "live entity objects      : " << m_entities.size() << "\n";
    std::cout << "\n";
    std::cout << "  station        cap   served    avg wait   time-avg Q    util   maxQ\n";
    std::cout << "  -----------------------------------------------------------------\n";
    for (const StationResults& s : r.stations) {
        std::cout << "  " << std::setw(12) << std::left << s.name << std::right
                  << std::setw(5)  << s.capacity
                  << std::setw(9)  << s.served
                  << std::setw(12) << s.averageWait
                  << std::setw(13) << s.averageQueueLength
                  << std::setw(8)  << s.utilisation
                  << std::setw(7)  << s.maxQueueLength
                  << "\n";
    }
    std::cout << "===========================================================\n";
}

}  // namespace des
