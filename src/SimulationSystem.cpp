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
#include <iostream>
#include <utility>   // std::move
// <algorithm> was included but never used -- the name lookups below are plain
// range-for loops. Removed. v2 may bring it back for std::find_if.

SimulationSystem::SimulationSystem(TerminationCondition termination)
    : m_termination(termination) {}
// m_nextEntityId is initialised to 1 by its in-class initialiser in the header.
// Repeating it here would be a second place that knows the starting value.

Entity* SimulationSystem::createEntity() {
    auto e = std::make_unique<Entity>(m_nextEntityId, m_clock.now());
    ++m_nextEntityId;
    Entity* raw = e.get();
    m_entities.push_back(std::move(e));
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

Resource* SimulationSystem::resource(const std::string& name) {
    for (const auto& r : m_resources) {
        if (r->name() == name) {
            return r.get();
        }
    }
    return nullptr;
}

EntityQueue* SimulationSystem::queue(const std::string& name) {
    for (const auto& q : m_queues) {
        if (q->name() == name) {
            return q.get();
        }
    }
    return nullptr;
}

void SimulationSystem::initialise() {
    // TODO v2 -- the "Initialization" row of the theory table: the state of the
    // system and the FEL at t = 0.
    //   m_clock.reset();  m_stats.reset();  m_state.reset();
    //   Entity* first = createEntity();
    //   scheduleEvent(EventType::Arrival, 0.0, first, nullptr);
    //   scheduleEvent(EventType::EndSimulation, m_termination.maxTime());
    // The EndSimulation event is a belt-and-braces stop: run() also checks the
    // termination condition each pass, but having it on the FEL means the loop
    // cannot overrun even if the queue never empties.
}

void SimulationSystem::scheduleEvent(EventType /*type*/, SimTime /*t*/,
                                     Entity* /*e*/, Resource* /*r*/) {
    // TODO v2 -- two lines:
    //   assert(t >= m_clock.now());   // scheduling into the PAST is the single
    //                                 // most common DES bug; catch it here, at
    //                                 // the moment of the mistake, not ten
    //                                 // thousand events later
    //   m_fel.schedule(EventNotice(type, t, e, r));
    //
    // This wrapper exists so no other code ever touches m_fel directly, and so
    // there is exactly ONE place that can enforce the assert above.
    // Default arguments (= nullptr) live in the HEADER only, never here.
}

void SimulationSystem::run() {
    // TODO v2 -- THE MAIN LOOP. About eight lines:
    //
    //   while (!m_fel.isEmpty() &&
    //          !m_termination.isMet(m_clock.now(), m_stats.numberServed()))
    //   {
    //       EventNotice notice = m_fel.popImminent();
    //
    //       m_stats.updateTimeIntegrals(notice.time(),
    //                                   <current queue length>,
    //                                   <current servers busy>);   // BEFORE!
    //       m_clock.advanceTo(notice.time());
    //
    //       switch (notice.type()) {
    //         case EventType::Arrival:       handleArrival(notice);   break;
    //         case EventType::Departure:     handleDeparture(notice); break;
    //         case EventType::EndSimulation: return;
    //         case EventType::StartService:  /* v3 */                 break;
    //       }
    //   }
    //
    // ORDER IS EVERYTHING: accumulate the integrals for the interval that just
    // ended, THEN move the clock, THEN change state. Swap any two and the
    // statistics go quietly wrong.
    //
    // The clock JUMPS from event to event -- nothing is simulated in between,
    // because by definition nothing happens in between. That single idea is why
    // discrete-event simulation is fast, and why the FEL had to be sorted.
    //
    // v3: this switch becomes virtual dispatch on an IEventHandler. Write the
    // switch first and let it get ugly. The ugliness is the argument for the
    // refactor, and an argument you have felt beats one you were told.
}

void SimulationSystem::handleArrival(const EventNotice& /*notice*/) {
    // TODO v2:
    //   m_stats.recordArrival(m_clock.now());
    //
    //   // *** FEED THE SIMULATION *** create the NEXT entity and schedule its
    //   // arrival at  now + interarrivalDraw. Forget this and the run stops
    //   // after one customer -- the classic first-run bug.
    //   Entity* next = createEntity();
    //   scheduleEvent(EventType::Arrival, m_clock.now() + interarrival(), next);
    //
    //   Resource* server = resource("Teller");
    //   if (server->isAvailable()) {
    //       server->seize();
    //       Activity service(name, m_clock.now(), serviceDraw());
    //       scheduleEvent(EventType::Departure, service.endTime(),
    //                     notice.entity(), server);
    //   } else {
    //       queue("TellerQueue")->push(notice.entity());
    //       // start a Delay for this entity -- and note you now need somewhere
    //       // to KEEP it. Deciding where (an attribute? a map in the system? a
    //       // member on Entity?) is a real v2 design decision. Think before you
    //       // reach for a global.
    //   }
    //   // update m_state
}

void SimulationSystem::handleDeparture(const EventNotice& /*notice*/) {
    // TODO v2:
    //   Resource* server = notice.resource();
    //   server->release();
    //
    //   SimTime inSystem = m_clock.now() - notice.entity()->creationTime();
    //   m_stats.recordDeparture(m_clock.now(), <that entity's wait>, inSystem);
    //
    //   EntityQueue* line = queue("TellerQueue");
    //   if (!line->isEmpty()) {
    //       Entity* nextInLine = line->pop();     // obeys the discipline
    //       // end that entity's Delay -- its wait time falls out of this
    //       server->seize();
    //       scheduleEvent(EventType::Departure,
    //                     m_clock.now() + serviceDraw(), nextInLine, server);
    //   }
    //   // update m_state
}

void SimulationSystem::report() const {
    // TODO v2 -- print, via std::cout:
    //   number arrived / number served
    //   average waiting time, average time in system, max waiting time
    //   time-average queue length, max queue length observed
    //   server utilisation
    //   total simulated time
    // Then hand-check it against a worked M/M/1 table from your notes, and
    // against Little's Law (see Statistics::timeAverageQueueLength).
    //
    // v4: this becomes a Report object that can emit text or CSV. std::cout is
    // honest and sufficient for now.
}

