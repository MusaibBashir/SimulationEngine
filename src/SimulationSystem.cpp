// ============================================================================
// SimulationSystem.cpp  --  the engine
// ============================================================================

#include "SimulationSystem.hpp"
#include "Activity.hpp"
#include "Build.hpp"
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
    assert(m_activeDelays.find(id) == m_activeDelays.end() &&
           "destroying an entity that is still inside a Delay");
    m_entities.erase(id);
}

// ------------------------------------------------------------------ state --

void SimulationSystem::refreshState() {
    // SystemState is a SNAPSHOT summed across every station -- resolution (b) of
    // the duplication flagged back in v1. The stations remain the single source
    // of truth; SystemState never decides anything, it only reports.
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
    // Per station, because utilisation and queue length are station properties.
    // A restaurant can have an idle host and a swamped kitchen; one system-wide
    // number would hide precisely the thing you are trying to see.
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        Station& s = m_model.stationAt(i);
        s.stats().updateTimeIntegrals(upTo,
                                      static_cast<int>(s.queue().length()),
                                      s.resource().unitsBusy());
    }
    m_stats.updateTimeIntegrals(upTo, m_state.numberInQueue(), m_state.numberInSystem());
}

// --------------------------------------------------------------- lifecycle --

void SimulationSystem::initialise() {
    m_model.validate();          // catch modelling mistakes BEFORE the run
    assert(m_termination != nullptr && "no termination rule set");

    // Everything holding RUN STATE gets reset. Configuration survives.
    m_clock.reset();
    m_stats.reset();
    m_state.reset();
    m_rng.reset();
    m_fel.clear();
    m_activeDelays.clear();
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

void SimulationSystem::scheduleEvent(EventType type, SimTime t, Entity* e, Station* s) {
    // Scheduling into the PAST is the most common DES bug. Catch it where the
    // mistake is made, not ten thousand events later.
    assert(t >= m_clock.now());
    m_fel.schedule(EventNotice(type, t, e, s));
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
    // STILL NOT an IEventHandler hierarchy, and v4 sharpened the reason rather
    // than weakening it. The switch is six cases now, but the cost of the
    // abstraction is not the hierarchy -- it is that handler objects living
    // outside this class would need admit(), startNextService(), createEntity()
    // and refreshState() made public, or five friend declarations. Widening the
    // public interface to satisfy an abstraction is a worse trade than a switch
    // that fits on a screen, and -Wswitch still flags a new EventType for free.
    //
    // The thing that would actually change the answer: handlers that carry
    // STATE (pre-emption, balking, reneging). A switch cannot hold state; an
    // object can. Build it then, not before.
}

// ------------------------------------------------------------------ routing --

void SimulationSystem::admit(Entity* e, Station* station) {
    assert(e != nullptr && station != nullptr);

    // Per-station bookkeeping. "stationEntry" is when this entity reached THIS
    // station; "waitHere" is how long it waited here. Both are stamped as
    // attributes rather than kept in a side table because they are single
    // values with the same lifetime as the entity's visit.
    station->stats().recordArrival(m_clock.now());
    e->setAttribute("stationEntry", m_clock.now());

    if (station->resource().isAvailable()) {
        e->setAttribute("waitHere", 0.0);   // served immediately
        station->resource().seize();
        // An Activity: the duration is drawn NOW, so its end can be scheduled
        // NOW. That is exactly what distinguishes an activity from a delay.
        const Activity service(station->name(),
                               m_clock.now(),
                               station->drawService(*e, m_rng));
        scheduleEvent(EventType::Departure, service.endTime(), e, station);

        if (m_trace.isOn()) {
            std::ostringstream os;
            os << "server free, service " << std::fixed << std::setprecision(4)
               << service.duration() << " until " << service.endTime();
            m_trace.event(m_clock.now(), "Seize", e->id(), station->name(), os.str(),
                          station->queue().length(), station->resource().unitsBusy());
        }
    } else {
        station->queue().push(e);
        // A Delay: its end is unknown now and will be decided by the system,
        // whenever a server here frees up.
        m_activeDelays.emplace(e->id(), Delay(m_clock.now()));

        if (m_trace.isOn()) {
            std::ostringstream os;
            os << "all " << station->resource().capacity() << " busy, queued at position "
               << station->queue().length();
            m_trace.event(m_clock.now(), "Queue", e->id(), station->name(), os.str(),
                          station->queue().length(), station->resource().unitsBusy());
        }
    }
}

void SimulationSystem::startNextService(Station* station) {
    if (station->queue().isEmpty()) return;

    Entity* next = station->queue().pop();   // obeys the station's discipline
    assert(next != nullptr);

    // End that entity's Delay. Its waiting time falls out of this, which is the
    // whole reason Delay is a class rather than a bare timestamp.
    auto it = m_activeDelays.find(next->id());
    assert(it != m_activeDelays.end());
    it->second.end(m_clock.now());
    const SimTime waited = it->second.duration();
    m_activeDelays.erase(it);

    // "waitTime" accumulates across every station the entity visits; "waitHere"
    // is just this visit, and is consumed when service here completes.
    next->setAttribute("waitTime", next->attribute("waitTime") + waited);
    next->setAttribute("waitHere", waited);

    station->resource().seize();
    const Activity service(station->name(), m_clock.now(),
                           station->drawService(*next, m_rng));
    scheduleEvent(EventType::Departure, service.endTime(), next, station);

    if (m_trace.isOn()) {
        std::ostringstream os;
        os << "pulled from queue after waiting " << std::fixed << std::setprecision(4)
           << waited << ", service until " << service.endTime();
        m_trace.event(m_clock.now(), "Seize", next->id(), station->name(), os.str(),
                      station->queue().length(), station->resource().unitsBusy());
    }
}

// ----------------------------------------------------------------- handlers --

void SimulationSystem::handleWarmUpEnd() {
    // *** WELCH'S METHOD, the whole of it. ***
    // Throw away every measurement taken so far and start again from now. The
    // SYSTEM STATE is deliberately untouched: entities in service stay in
    // service, queues keep their contents. That is the point -- measurement now
    // begins from a realistically loaded system rather than an empty one, and
    // the start-up transient never enters an average.
    m_stats.restartAt(m_clock.now());
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        Station& s = m_model.stationAt(i);
        s.stats().restartAt(m_clock.now());
        s.queue().resetStatistics();
    }
    m_warmUpEnded = m_clock.now();

    if (m_trace.isOn()) {
        m_trace.event(m_clock.now(), "WarmUpEnd", 0, "-",
                      "statistics discarded; measurement starts here",
                      0, m_state.numberInSystem());
    }
}

void SimulationSystem::handleObservation() {
    // Sample on a FIXED GRID, not at events. Welch's method averages replication
    // i's observation k with replication j's observation k, so the observations
    // have to line up -- and event times never do.
    m_observations.push_back(static_cast<double>(m_state.numberInSystem()));
    scheduleEvent(EventType::Observe, m_clock.now() + m_observeInterval);
}

void SimulationSystem::handleArrival(const EventNotice& /*notice*/) {
    // *** THE ARRIVAL EVENT CARRIES NO ENTITY; THE ENTITY IS BORN HERE. ***
    // Creating it earlier and attaching it to a future Arrival would stamp
    // creationTime before it actually arrived, inflating every time-in-system.
    Entity* arriving = createEntity();
    arriving->setAttribute("waitTime", 0.0);
    // v4.1: stamp the model's arrival attributes -- priority, due date,
    // processing time, whatever this model says an entity carries.
    for (const auto& a : m_model.arrivalAttributes())
        arriving->setAttribute(a.name, a.distribution->draw(m_rng));
    m_stats.recordArrival(m_clock.now());

    if (m_trace.isOn()) {
        m_trace.event(m_clock.now(), "Arrival", arriving->id(),
                      m_model.entry()->name(), "enters the system",
                      m_model.entry()->queue().length(),
                      m_model.entry()->resource().unitsBusy());
    }

    // *** FEED THE SIMULATION. *** Forget this and the run stops after one
    // customer -- the classic first-run bug.
    scheduleEvent(EventType::Arrival, m_clock.now() + m_model.interarrival().draw(m_rng));

    admit(arriving, m_model.entry());
    refreshState();
}

void SimulationSystem::handleDeparture(const EventNotice& notice) {
    Entity*  finished = notice.entity();
    Station* here     = notice.station();
    assert(finished != nullptr && here != nullptr);

    here->resource().release();

    // Service at THIS station is complete: record it against this station.
    const SimTime waitHere  = finished->attribute("waitHere");
    const SimTime timeHere  = m_clock.now() - finished->attribute("stationEntry");
    here->stats().recordDeparture(m_clock.now(), waitHere, timeHere);

    Station* next = here->next();

    if (next != nullptr) {
        // A CHAIN. The entity is not done -- it moves to the next station and
        // is admitted there exactly as if it had just arrived. One function for
        // both paths is what makes networks work.
        if (m_trace.isOn()) {
            m_trace.event(m_clock.now(), "Move", finished->id(), here->name(),
                          "service done, routing to " + next->name(),
                          here->queue().length(), here->resource().unitsBusy());
        }
        startNextService(here);       // free server, so pull the next one here
        admit(finished, next);
    } else {
        // EXIT. The entity leaves the system.
        assert(finished->hasAttribute("waitTime") && "waitTime was never set");
        const SimTime wait     = finished->attribute("waitTime");
        const SimTime inSystem = m_clock.now() - finished->creationTime();
        m_stats.recordDeparture(m_clock.now(), wait, inSystem);

        if (m_trace.isOn()) {
            std::ostringstream os;
            os << std::fixed << std::setprecision(4)
               << "exits; total wait " << wait << ", time in system " << inSystem;
            m_trace.event(m_clock.now(), "Exit", finished->id(), here->name(), os.str(),
                          here->queue().length(), here->resource().unitsBusy());
        }

        startNextService(here);
        const EntityId id = finished->id();
        refreshState();
        // Destroy LAST: everything above still reads through `finished`, and
        // after this line that pointer dangles.
        destroyEntity(id);
        return;
    }

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
    r.stillWaitingAtStop    = m_activeDelays.size();

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
