// ============================================================================
// SimulationSystem.cpp  --  the biggest file in v1, and still only storage
// ============================================================================
// [1] Include "SimulationSystem.hpp" first, then <algorithm> and <iostream>.
//
// [2] IMPLEMENT FOR REAL IN v1 (these are the factories -- no simulation logic):
//
//     [2a] SimulationSystem::SimulationSystem(TerminationCondition t)
//          Member-init list: m_termination from the parameter, m_nextEntityId
//          to 1. The five by-value members default-construct themselves; do not
//          list them.
//
//     [2b] Entity* SimulationSystem::createEntity()
//          Four lines:
//            auto e = std::make_unique<Entity>(m_nextEntityId, m_clock.now());
//            ++m_nextEntityId;
//            Entity* raw = e.get();          <- grab the address BEFORE moving
//            m_entities.push_back(std::move(e));
//            return raw;
//          The order matters: after std::move the local unique_ptr is null, so
//          calling .get() on it afterwards returns nullptr. Try it wrong once
//          and watch it happen -- that is the clearest possible demonstration of
//          what "move" actually does.
//
//     [2c] Resource* SimulationSystem::addResource(const std::string& name, int capacity)
//          Identical four-line pattern into m_resources.
//     [2d] EntityQueue* SimulationSystem::addQueue(const std::string& name, QueueDiscipline d)
//          Identical pattern into m_queues.
//          (Three copies of the same four lines. Notice it. A templated
//          helper could collapse them -- that is a v3 exercise, not a v1 one.)
//
//     [2e] Resource* SimulationSystem::resource(const std::string& name)
//          Range-for over m_resources; if the name matches, return .get();
//          after the loop, return nullptr.
//     [2f] EntityQueue* SimulationSystem::queue(const std::string& name)
//          Same shape.
//
//     [2g] The const accessors are one-liners; leave them inline in the header.
//
// [3] v1 STUBS -- write each signature with an EMPTY body and its TODO comment.
//     Getting all of these to compile as empty functions IS the v1 finish line.
//
//     [3a] void SimulationSystem::initialise()
//          // TODO v2 -- the "Initialization" row of the theory table:
//          //   m_clock.reset(); m_stats.reset(); m_state.reset();
//          //   create the first entity and schedule its Arrival at t=0
//          //   schedule EndSimulation at m_termination.maxTime()
//
//     [3b] void SimulationSystem::scheduleEvent(EventType type, SimTime t, Entity* e, Resource* r)
//          // TODO v2: construct an EventNotice from the four arguments and pass
//          // it to m_fel.schedule(). Two lines. This wrapper exists so that no
//          // other code ever touches the FEL directly.
//
//     [3c] void SimulationSystem::run()
//          // TODO v2 -- THE MAIN LOOP, about eight lines:
//          //   while (!m_fel.isEmpty() &&
//          //          !m_termination.isMet(m_clock.now(), m_stats.numberServed()))
//          //     EventNotice notice = m_fel.popImminent();
//          //     m_stats.updateTimeIntegrals(notice.time(),
//          //                                 <current queue length>,
//          //                                 <current servers busy>);   // BEFORE
//          //     m_clock.advanceTo(notice.time());
//          //     switch (notice.type()) { Arrival -> handleArrival(notice);
//          //                              Departure -> handleDeparture(notice);
//          //                              EndSimulation -> break out; }
//          //
//          // The clock JUMPS from event to event. Nothing is simulated in
//          // between, because by definition nothing happens in between. That
//          // single idea is what makes discrete-event simulation fast, and it
//          // is why the FEL had to be sorted.
//          //
//          // v3: this switch becomes virtual dispatch on an IEventHandler.
//          // Write the switch first and let it get ugly -- the ugliness is the
//          // argument for the refactor, and an argument you have felt is worth
//          // more than one you were told.
//
//     [3d] void SimulationSystem::handleArrival(const EventNotice& notice)
//          // TODO v2:
//          //   record the arrival in m_stats
//          //   CREATE THE NEXT ENTITY AND SCHEDULE ITS ARRIVAL at
//          //     now + interarrivalDraw   <- this self-feeding step is how a DES
//          //     keeps running; forget it and the simulation stops after one
//          //     customer, which is the classic first-run bug
//          //   if the resource has a free unit: seize it, build an Activity for
//          //     the service, schedule a Departure at activity.endTime()
//          //   else: push the entity into the queue and start a Delay
//          //   update m_state
//
//     [3e] void SimulationSystem::handleDeparture(const EventNotice& notice)
//          // TODO v2:
//          //   release the resource
//          //   record the departure: wait time and time-in-system, computed
//          //     from the entity's creationTime and the current clock
//          //   if the queue is non-empty: pop the next entity per the discipline,
//          //     end its Delay, seize the resource, schedule its Departure
//          //   update m_state
//
//     [3f] void SimulationSystem::report() const
//          // TODO v2: print number served, average wait, average time in system,
//          // time-average queue length, utilisation, max queue length.
//          // v4: this becomes a Report object that can emit text or CSV. For now
//          // std::cout is honest and sufficient.

#include "SimulationSystem.hpp"
#include "Activity.hpp"   // v2: service durations are Activities
#include <iostream>
#include <iomanip>
#include <utility>        // std::move
#include <cassert>
#include <cmath>          // std::isinf

SimulationSystem::SimulationSystem(TerminationCondition termination, unsigned seed)
    : m_termination(termination), m_rng(seed) {}
// m_nextEntityId and the model parameters are initialised by their in-class
// initialisers in the header. Repeating them here would be a second place that
// knows the starting values.

// ---------------------------------------------------------------- factories --

Entity* SimulationSystem::createEntity() {
    auto e = std::make_unique<Entity>(m_nextEntityId, m_clock.now());
    ++m_nextEntityId;
    Entity* raw = e.get();               // grab the address BEFORE the move --
    m_entities.push_back(std::move(e));  // after std::move the local is null
    return raw;
}

Resource* SimulationSystem::addResource(const std::string& name, int capacity) {
    auto r = std::make_unique<Resource>(name, capacity);
    Resource* raw = r.get();
    m_resources.push_back(std::move(r));
    return raw;
}

EntityQueue* SimulationSystem::addQueue(const std::string& name, QueueDiscipline d) {
    auto q = std::make_unique<EntityQueue>(name, d);
    EntityQueue* raw = q.get();
    m_queues.push_back(std::move(q));
    return raw;
}

void SimulationSystem::setModel(const std::string& serverName, const std::string& queueName) {
    m_serverName = serverName;
    m_queueName  = queueName;
}

void SimulationSystem::setMeanInterarrival(SimTime mean) {
    assert(mean > 0.0);
    m_meanInterarrival = mean;
}

void SimulationSystem::setMeanService(SimTime mean) {
    assert(mean > 0.0);
    m_meanService = mean;
}

// ------------------------------------------------------------------ lookup --

Resource* SimulationSystem::resource(const std::string& name) {
    for (const auto& r : m_resources) {
        if (r->name() == name) return r.get();
    }
    return nullptr;
}

EntityQueue* SimulationSystem::queue(const std::string& name) {
    for (const auto& q : m_queues) {
        if (q->name() == name) return q.get();
    }
    return nullptr;
}

// ----------------------------------------------------------------- running --

void SimulationSystem::refreshState() {
    // SystemState is treated as a SNAPSHOT, refreshed from the authoritative
    // objects after every state change -- resolution (b) of the duplication
    // flagged in SystemState.hpp. Resource and EntityQueue remain the single
    // source of truth; SystemState never decides anything, it only reports.
    const int inQueue = static_cast<int>(m_line->length());
    m_state.setNumberInQueue(inQueue);
    m_state.setNumberInSystem(inQueue + m_server->unitsBusy());
    m_state.setServerStatus(m_server->state());
}

void SimulationSystem::initialise() {
    // The "Initialization" row of the theory table: state and FEL at t = 0.
    m_clock.reset();
    m_stats.reset();
    m_state.reset();
    m_rng.reset();
    m_activeDelays.clear();

    // Resolve the model ONCE, here, instead of doing a string lookup inside
    // every event handler.
    m_server = resource(m_serverName);
    m_line   = queue(m_queueName);
    assert(m_server != nullptr && "initialise(): no such resource -- call addResource first");
    assert(m_line   != nullptr && "initialise(): no such queue -- call addQueue first");

    m_line->setRandomStream(&m_rng);   // only the Random discipline uses it

    // The first arrival carries NO entity. See handleArrival for why.
    scheduleEvent(EventType::Arrival, 0.0);

    // Only schedule the stop event if there is a finite time limit. Putting
    // infinity on the FEL would let advanceTo() move the clock to infinity if
    // the list ever drained.
    if (!std::isinf(m_termination.maxTime())) {
        scheduleEvent(EventType::EndSimulation, m_termination.maxTime());
    }
}

void SimulationSystem::scheduleEvent(EventType type, SimTime t, Entity* e, Resource* r) {
    // Scheduling into the PAST is the most common DES bug. Catch it at the
    // moment of the mistake, not ten thousand events later.
    assert(t >= m_clock.now());
    m_fel.schedule(EventNotice(type, t, e, r));
    // Default arguments (= nullptr) live in the HEADER only, never here.
}

void SimulationSystem::run() {
    assert(m_server != nullptr && "run(): call initialise() first");

    while (!m_fel.isEmpty() &&
           !m_termination.isMet(m_clock.now(), m_stats.numberServed()))
    {
        EventNotice notice = m_fel.popImminent();

        // ORDER IS EVERYTHING. Close the integral for the interval that just
        // ended -- using the OLD state -- then move the clock, then let the
        // handler change the state. Swap any two of these three and every time
        // average in the report goes quietly wrong.
        m_stats.updateTimeIntegrals(notice.time(),
                                    static_cast<int>(m_line->length()),
                                    m_server->unitsBusy());
        m_clock.advanceTo(notice.time());

        switch (notice.type()) {
            case EventType::Arrival:       handleArrival(notice);   break;
            case EventType::Departure:     handleDeparture(notice); break;
            case EventType::EndSimulation: return;
            case EventType::StartService:  /* v3: seize/release as its own event */ break;
        }
    }
    // The clock JUMPS from event to event. Nothing is simulated in between,
    // because by definition nothing happens in between -- which is why
    // discrete-event simulation is fast, and why the FEL had to be sorted.
    //
    // v3: this switch becomes virtual dispatch on an IEventHandler.
}

void SimulationSystem::handleArrival(const EventNotice& /*notice*/) {
    // *** THE ARRIVAL EVENT CARRIES NO ENTITY, AND THE ENTITY IS BORN HERE. ***
    // The obvious alternative -- create the next entity now and attach it to a
    // future Arrival -- stamps it with creationTime = now even though it will
    // not arrive until later, so every time-in-system comes out inflated by one
    // interarrival gap. Creating on arrival makes the timestamp correct by
    // construction rather than by remembering to fix it up.
    Entity* arriving = createEntity();
    m_stats.recordArrival(m_clock.now());

    // *** FEED THE SIMULATION. *** Schedule the next arrival. Forget this line
    // and the run stops after one customer -- the classic first-run bug.
    scheduleEvent(EventType::Arrival, m_clock.now() + m_rng.exponential(m_meanInterarrival));

    if (m_server->isAvailable()) {
        m_server->seize();
        // An Activity: its duration is drawn NOW, so its end can be scheduled
        // NOW. That is exactly what distinguishes an activity from a delay.
        const Activity service("Service", m_clock.now(), m_rng.exponential(m_meanService));
        arriving->setAttribute("waitTime", 0.0);   // served immediately
        scheduleEvent(EventType::Departure, service.endTime(), arriving, m_server);
    } else {
        m_line->push(arriving);
        // A Delay: its end is unknown right now and will be decided by the
        // system, in handleDeparture, whenever a server frees up.
        m_activeDelays.emplace(arriving->id(), Delay(m_clock.now()));
    }

    refreshState();
}

void SimulationSystem::handleDeparture(const EventNotice& notice) {
    Entity*   finished = notice.entity();
    Resource* server   = notice.resource();
    assert(finished != nullptr && server != nullptr);

    server->release();

    const SimTime wait     = finished->attribute("waitTime");
    const SimTime inSystem = m_clock.now() - finished->creationTime();
    m_stats.recordDeparture(m_clock.now(), wait, inSystem);

    if (!m_line->isEmpty()) {
        Entity* nextInLine = m_line->pop();   // obeys the queue discipline
        assert(nextInLine != nullptr);

        // End that entity's Delay. Its waiting time falls out of this, which is
        // the whole reason the Delay class exists rather than a bare timestamp.
        auto it = m_activeDelays.find(nextInLine->id());
        assert(it != m_activeDelays.end());
        it->second.end(m_clock.now());
        nextInLine->setAttribute("waitTime", it->second.duration());
        m_activeDelays.erase(it);

        server->seize();
        const Activity service("Service", m_clock.now(), m_rng.exponential(m_meanService));
        scheduleEvent(EventType::Departure, service.endTime(), nextInLine, server);
    }

    refreshState();
}

void SimulationSystem::report() const {
    const SimTime total = m_clock.now();

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "--- simulation report -------------------------------------\n";
    std::cout << "seed                     : " << m_rng.seed() << "\n";
    std::cout << "total simulated time     : " << total << "\n";
    std::cout << "entities arrived         : " << m_stats.numberArrived() << "\n";
    std::cout << "entities served          : " << m_stats.numberServed() << "\n";
    // Ask Statistics for the derived values instead of recomputing them here.
    // Doing the division at the call site would put the same formula in two
    // places -- and the guarded versions live in Statistics for a reason.
    std::cout << "average waiting time     : " << m_stats.averageWaitingTime() << "\n";
    std::cout << "average time in system   : " << m_stats.averageTimeInSystem() << "\n";
    std::cout << "max waiting time         : " << m_stats.maxWaitingTime() << "\n";
    std::cout << "time-average queue length: " << m_stats.timeAverageQueueLength(total) << "\n";
    std::cout << "max queue length observed: " << (m_line ? m_line->maxLengthObserved() : 0) << "\n";
    std::cout << "server utilisation       : "
              << m_stats.serverUtilisation(total, m_server ? m_server->capacity() : 0) << "\n";
    std::cout << "still waiting at stop    : " << m_activeDelays.size() << "\n";
    std::cout << "-----------------------------------------------------------\n";
    // v4: this becomes a Report object that can emit text or CSV.
}
