// ============================================================================
// SimulationSystem.cpp  --  the engine
// ============================================================================

#include "SimulationSystem.hpp"
#include "Activity.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <utility>
#include <cassert>

SimulationSystem::SimulationSystem(unsigned seed) : m_rng(seed) {}

// ------------------------------------------------------------------- setup --

void SimulationSystem::setTermination(std::unique_ptr<ITerminationRule> rule) {
    assert(rule != nullptr);
    m_termination = std::move(rule);
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
    EventNotice::resetSequenceCounter();
    m_model.reset();

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
            case EventType::StartService:  break;   // reserved
        }
    }
    // The clock JUMPS event to event. Nothing is simulated in between because
    // nothing happens in between -- that is why DES is fast, and why the FEL
    // had to be sorted.
    //
    // NOT abstracted into an IEventHandler hierarchy, deliberately: four cases
    // that fit on a screen do not need virtual dispatch, and -Wswitch still
    // tells us when a new EventType appears. Revisit when there are eight.
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
                               station->serviceDistribution().draw(m_rng));
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
                           station->serviceDistribution().draw(m_rng));
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

void SimulationSystem::handleArrival(const EventNotice& /*notice*/) {
    // *** THE ARRIVAL EVENT CARRIES NO ENTITY; THE ENTITY IS BORN HERE. ***
    // Creating it earlier and attaching it to a future Arrival would stamp
    // creationTime before it actually arrived, inflating every time-in-system.
    Entity* arriving = createEntity();
    arriving->setAttribute("waitTime", 0.0);
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

void SimulationSystem::report() const {
    const SimTime total = m_clock.now();
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "=== simulation report =====================================\n";
    std::cout << "seed                     : " << m_rng.seed() << "\n";
    std::cout << "termination              : "
              << (m_termination ? m_termination->describe() : "<none>") << "\n";
    std::cout << "total simulated time     : " << total << "\n";
    std::cout << "entities arrived         : " << m_stats.numberArrived() << "\n";
    std::cout << "entities exited          : " << m_stats.numberServed() << "\n";
    std::cout << "average total wait       : " << m_stats.averageWaitingTime() << "\n";
    std::cout << "average time in system   : " << m_stats.averageTimeInSystem() << "\n";
    std::cout << "max total wait           : " << m_stats.maxWaitingTime() << "\n";
    // The system-level Statistics object was fed (numberInQueue, numberInSystem)
    // rather than (queueLength, serversBusy), so its two integrals mean L_q and
    // L. Reusing the class this way is a small abuse of its member NAMES -- and
    // a fair sign that in v4 those should become areaUnder[A] / areaUnder[B]
    // with the meaning supplied by the caller.
    std::cout << "time-avg in queue (Lq)   : " << m_stats.timeAverageQueueLength(total) << "\n";
    std::cout << "time-avg in system (L)   : " << m_stats.serverUtilisation(total, 1) << "\n";
    std::cout << "still waiting at stop    : " << m_activeDelays.size() << "\n";
    std::cout << "live entity objects      : " << m_entities.size() << "\n";
    std::cout << "\n";
    std::cout << "  station        cap   served    avg wait   time-avg Q    util   maxQ\n";
    std::cout << "  -----------------------------------------------------------------\n";
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        const Station& s = m_model.stationAt(i);
        std::cout << "  " << std::setw(12) << std::left << s.name() << std::right
                  << std::setw(5)  << s.resource().capacity()
                  << std::setw(9)  << s.stats().numberArrived()
                  << std::setw(12) << s.stats().averageWaitingTime()
                  << std::setw(13) << s.stats().timeAverageQueueLength(total)
                  << std::setw(8)  << s.stats().serverUtilisation(total, s.resource().capacity())
                  << std::setw(7)  << s.queue().maxLengthObserved()
                  << "\n";
    }
    std::cout << "===========================================================\n";
}
