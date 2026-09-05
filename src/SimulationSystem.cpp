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

SimulationSystem& SimulationSystem::useSeparateStreams(bool on) {
    m_separateStreams = on;
    return *this;
}

SimulationSystem& SimulationSystem::useAntithetic(bool on) {
    m_antithetic = on;
    return *this;
}

void SimulationSystem::assignStreams() {
    m_streams.clear();
    if (!m_separateStreams) {
        // One shared stream, as every version before v8. Every distribution
        // draws from it in whatever order events happen to fire.
        for (std::size_t i = 0; i < m_model.nodeCount(); ++i) (void)i;
        return;
    }

    // A stream per ROLE, named after what it drives. The name is what makes the
    // stream stable: as long as the arrival distribution is still called
    // "arrivals", it draws the same numbers no matter what else changed in the
    // model. That is the whole mechanism behind common random numbers.
    auto give = [&](IDistribution* d, const std::string& role) {
        if (!d) return;
        auto it = m_streams.emplace(role, m_rng.substream(role)).first;
        it->second.setAntithetic(m_antithetic);
        d->useStream(&it->second);
    };
    // v10: an expression may contain several sampling sites, and useStream
    // recurses into every one of them. That is what finally separates a Delay's
    // draws from a Decide's -- they shared a stream because they shared a code
    // path, not because anyone wanted them to.
    auto giveExpr = [&](IExpression* e, const std::string& role) {
        if (!e) return;
        auto it = m_streams.emplace(role, m_rng.substream(role)).first;
        it->second.setAntithetic(m_antithetic);
        e->useStream(&it->second);
    };

    // v9: the SOURCES own the interarrival distributions now. Model::interarrival()
    // is only the copy kept for the stability check, and streaming that copy
    // instead of the live one silently stopped arrivals having their own stream
    // -- which is precisely what common random numbers depends on. The
    // antithetic unit test caught it, which is what that test is for.
    for (std::size_t i = 0; i < m_model.sourceCount(); ++i) {
        CreateNode& c = m_model.sourceAt(i);
        giveExpr(&c.interarrival(), "arrivals:" + c.name());
    }
    for (const auto& a : m_model.arrivalAttributes())
        give(a.distribution.get(), "attribute:" + a.name);
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        Station& s = m_model.stationAt(i);
        giveExpr(&s.serviceExpression(), "service:" + s.name());
    }
    // v10: the complete job. Every other block that samples gets its own
    // stream too, named after what it drives. Before v10 these shared the
    // common stream because they shared a code path; now each is a distinct
    // expression and useStream() recurses into every sampling site beneath it.
    for (std::size_t i = 0; i < m_model.nodeCount(); ++i) {
        INode& n = m_model.nodeAt(i);
        if (auto* d = dynamic_cast<DelayNode*>(&n)) {
            giveExpr(&d->durationExpression(), "delay:" + d->name());
        } else if (auto* dec = dynamic_cast<DecideNode*>(&n)) {
            // A by-chance branch has no condition expression; it still draws
            // from the shared stream, which is what makes it a good witness
            // that the blocks above no longer disturb it.
            for (std::size_t b = 0; b < dec->branches().size(); ++b) {
                if (dec->branches()[b].condition) {
                    giveExpr(dec->branches()[b].condition.get(),
                             "decide:" + dec->name() + ":" + std::to_string(b));
                }
            }
        } else if (auto* a = dynamic_cast<AssignNode*>(&n)) {
            for (std::size_t r = 0; r < a->rules().size(); ++r) {
                giveExpr(a->rules()[r].value.get(),
                         "assign:" + a->name() + ":" + std::to_string(r));
            }
        }
    }
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

void SimulationSystem::noteArrival(Entity* e) {
    // Attributes the model stamps on everything that arrives -- priority, due
    // date, whatever this model says an entity carries.
    for (const auto& a : m_model.arrivalAttributes())
        e->setAttribute(a.name, a.distribution->draw(m_rng));
    e->setAttribute("counted", 1.0);   // so noteExit knows this one was demand
    m_stats.recordArrival(m_clock.now());
    TypeStats& t = m_byType[e->type()];
    ++t.in;
    ++t.inSystem;
}

void SimulationSystem::noteExit(Entity* e) {
    if (!e->hasAttribute("counted")) return;   // manufactured, not demand
    TypeStats& t = m_byType[e->type()];
    ++t.out;
    // This guard is load-bearing for a REAL, PRE-EXISTING reason, found while
    // fixing the v10 retype bug and deliberately left for its own version:
    //
    //   SeparateNode's duplicate path calls copyAttributesFrom(), which copies
    //   the internal "counted" mark along with everything else. So a duplicate
    //   is counted as an EXIT although it was never counted as an ARRIVAL, and
    //   the decrement below has nothing to match it.
    //
    // Fixing that changes NumberOut and WIP for every model using duplicate(),
    // which is a v9 behaviour change and not v10's to make. Until then the
    // guard stops the underflow. It should become an assert the moment the
    // duplicate accounting is corrected.
    if (t.inSystem > 0) --t.inSystem;
    const SimTime inSystem = m_clock.now() - e->creationTime();
    t.totalTime += inSystem;
    t.maxTime = std::max(t.maxTime, inSystem);
}

void SimulationSystem::retypeEntity(Entity* e, const std::string& type) {
    if (type == e->type()) return;
    // Only entities that were COUNTED as arrivals appear in m_byType. Batch
    // representatives and Separate duplicates are manufactured, never counted,
    // and must not move a count that was never added.
    if (e->hasAttribute("counted")) {
        TypeStats& from = m_byType[e->type()];
        // Same guard, same reason as noteExit: a Separate duplicate carries the
        // "counted" mark it should not, so its count may already be absent.
        if (from.inSystem > 0) {
            --from.inSystem;
            ++m_byType[type].inSystem;
        }
        // in/out are lifetime totals for the type an entity ARRIVED as, so they
        // deliberately do not move: the shop received one of the old type.
    }
    e->setType(type);
}

void SimulationSystem::destroyEntity(EntityId id) {
    // Every destruction is an exit for that entity type, whether it reached a
    // Dispose or was consumed by a permanent Batch. Arena counts both, and a
    // plate swallowed by a batch has certainly left the system.
    auto it = m_entities.find(id);
    if (it != m_entities.end()) noteExit(it->second.get());
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
void NodeContext::registerArrival(Entity* e) { m_sim.noteArrival(e); }
void NodeContext::destroy(Entity* e) { m_sim.destroyEntity(e->id()); }

// --- IModelState -----------------------------------------------------------
// An unknown name THROWS rather than reading zero. A typo'd block name in
// NQ() reading as "the queue is empty" is exactly the v9 bug where WIP was
// silently 0.0 while 32 balls were waiting: a value that is quietly zero gets
// copied into an answer.

double SimulationSystem::queueLength(const std::string& blockName) const {
    if (const Station* s = m_model.station(blockName))
        return static_cast<double>(s->queue().length());
    throw ModelError("NQ(" + blockName + "): no Process block named '" + blockName + "'");
}

double SimulationSystem::resourceBusy(const std::string& name) const {
    if (const Resource* r = m_model.resourceNamed(name)) return r->unitsBusy();
    // A Process with a private resource is addressable by the block's name --
    // there is no other name for it.
    if (const Station* s = m_model.station(name)) return s->resource().unitsBusy();
    throw ModelError("NR(" + name + "): no resource or Process block named '" + name + "'");
}

double SimulationSystem::resourceCapacity(const std::string& name) const {
    if (const Resource* r = m_model.resourceNamed(name)) return r->capacity();
    if (const Station* s = m_model.station(name)) return s->resource().capacity();
    throw ModelError("MR(" + name + "): no resource or Process block named '" + name + "'");
}

double SimulationSystem::numberInSystem() const {
    return static_cast<double>(m_state.numberInSystem());
}

SimTime SimulationSystem::now() const { return m_clock.now(); }

void SimulationSystem::reportStability(std::ostream& os) const {
    const Model::StabilityReport r = m_model.stability();
    if (r.checked) return;      // nothing to say; silence means verified
    os << "\n*** STABILITY NOT VERIFIED for:";
    for (const std::string& name : r.unverifiable) os << " " << name;
    os << "\n*** Their offered load depends on an expression whose mean cannot be\n"
          "*** computed before the run, so the check could not be applied. That is\n"
          "*** NOT the same as a load of zero.\n";
}

double SimulationSystem::variableAverage(const std::string& name) const {
    return m_model.variables().timeAverage(name);
}

// A node never builds an EvalContext itself: this is what keeps the variable
// store and the model-state implementation out of every node's reach.
void NodeContext::setEntityType(Entity* e, const std::string& type) {
    m_sim.retypeEntity(e, type);
}

EvalContext NodeContext::evaluationContext(const Entity* e) {
    return EvalContext(e, &m_sim.m_model.variables(), &m_sim, &m_sim.m_rng);
}

VariableStore& NodeContext::variables() { return m_sim.m_model.variables(); }

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

void NodeContext::scheduleRenegeCheck(SimTime at, Entity* e, INode* node) {
    m_sim.scheduleEvent(EventType::Renege, at, e, node);
}

void NodeContext::scheduleNextArrival(SimTime at, INode* source) {
    m_sim.scheduleEvent(EventType::Arrival, at, nullptr, source);
}

// ------------------------------------------------------------------ state --

std::size_t SimulationSystem::stillWaitingCount() const {
    std::size_t n = 0;
    for (std::size_t i = 0; i < m_model.stationCount(); ++i)
        n += m_model.stationAt(i).stillWaiting();
    return n;
}

void SimulationSystem::refreshState() {
    // SystemState is a SNAPSHOT. The blocks remain the single source of truth;
    // SystemState never decides anything, it only reports.
    int queued = 0;
    int busy   = 0;
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        const Station& s = m_model.stationAt(i);
        queued += static_cast<int>(s.queue().length());
        busy   += s.resource().unitsBusy();
    }
    // v9 FIX: also count entities held at a BATCH block waiting for companions.
    // Before this, "number in queue" meant "in a Process queue", so a model made
    // entirely of batching blocks -- which the ball-matching problem is --
    // reported a queue length and a WIP of exactly zero while 32 balls sat
    // waiting. A statistic that is silently zero is worse than one that is
    // missing, because it gets copied into an answer.
    for (std::size_t i = 0; i < m_model.nodeCount(); ++i)
        if (auto* b = dynamic_cast<const BatchNode*>(&m_model.nodeAt(i)))
            queued += static_cast<int>(b->waitingForBatch());

    // WIP is every entity the system is holding, wherever it is -- in service,
    // in a queue, on a conveyor, waiting for a batch. Counting only queues and
    // servers would miss anything in a Delay.
    int inSystem = 0;
    for (const auto& kv : m_byType) inSystem += kv.second.inSystem;

    m_state.setNumberInQueue(queued);
    m_state.setNumberInSystem(inSystem);
    m_state.setServerStatus(busy > 0 ? ResourceState::Busy : ResourceState::Idle);
}

void SimulationSystem::updateAllIntegrals(SimTime upTo) {
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        Station& s = m_model.stationAt(i);
        // unitsHeld(), not resource().unitsBusy(): with a shared resource the
        // latter is the total across every block using it.
        s.stats().updateTimeIntegrals(upTo,
                                      static_cast<int>(s.queue().length()),
                                      s.unitsHeld());
    }
    m_stats.updateTimeIntegrals(upTo, m_state.numberInQueue(), m_state.numberInSystem());
    // Variables are time-persistent, so their integrals close HERE with
    // everything else -- before the clock moves, never after.
    m_model.variables().updateIntegrals(upTo);
    // WIP per type: the same rectangle rule, one integral per entity type.
    for (auto& kv : m_byType) {
        TypeStats& t = kv.second;
        t.areaWIP += t.inSystem * (upTo - t.lastUpdate);
        t.lastUpdate = upTo;
    }
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
    // Wire the shorthand source to the entry block BEFORE validating -- the
    // caller may have said arrivals() before entryAt(), and the order in which
    // a model is described should not matter.
    m_model.wireSources();
    m_model.validate();          // catch modelling mistakes BEFORE the run
    assert(m_termination != nullptr && "no termination rule set");

    // Everything holding RUN STATE gets reset. Configuration survives.
    m_clock.reset();
    m_stats.reset();
    m_state.reset();
    m_rng.reset();
    m_rng.setAntithetic(m_antithetic);
    assignStreams();
    m_fel.clear();
    m_entities.clear();
    m_byType.clear();
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

    // v9: one Arrival event per SOURCE. There is no special arrival handling
    // left in the engine -- a Create is a block like any other, and its callback
    // makes the entity and reschedules itself.
    for (std::size_t i = 0; i < m_model.sourceCount(); ++i) {
        CreateNode& c = m_model.sourceAt(i);
        scheduleEvent(EventType::Arrival, c.firstAt(), nullptr, &c);
    }
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
            case EventType::Arrival: {
                // A source's turn to produce. Same shape as any other callback.
                NodeContext ctx(*this);
                if (notice.node()) notice.node()->onScheduledEvent(ctx, nullptr);
                refreshState();
                break;
            }
            case EventType::Departure:     handleDeparture(notice); break;
            case EventType::EndSimulation: return;
            case EventType::WarmUpEnd:     handleWarmUpEnd();       break;
            case EventType::Observe:       handleObservation();     break;
            case EventType::Renege: {
                // A patience timer. It fires whether or not the entity is still
                // waiting -- the block checks and ignores it if stale. See
                // Station::onRenegeTimeout for why nothing is ever cancelled.
                NodeContext ctx(*this);
                if (notice.node() && notice.entity())
                    notice.node()->onRenegeTimeout(ctx, notice.entity());
                refreshState();
                break;
            }
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
    // Per-type counters restart too, for the same reason every other statistic
    // does: a count spanning the warm-up next to a utilisation that does not is
    // two numbers in one report measuring different periods.
    for (auto& kv : m_byType) {
        TypeStats& t = kv.second;
        const int stillHere = t.inSystem;
        t = TypeStats{};
        t.inSystem   = stillHere;      // state survives; measurement restarts
        t.lastUpdate = m_clock.now();
    }
    // EVERY block, not just the process ones -- a Record or Decide counter that
    // spanned the warm-up while the utilisation next to it did not would be two
    // numbers in one report meaning different periods.
    for (std::size_t i = 0; i < m_model.nodeCount(); ++i)
        m_model.nodeAt(i).resetStatistics(m_clock.now());
    m_model.variables().resetStatistics(m_clock.now());
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

void SimulationSystem::reportArenaStyle(std::ostream& os) const {
    const RunResults r = results();
    const SimTime T = r.measuredTime;
    os << std::fixed;
    os << "\n============================================================\n";
    os << "Replication ended at time : " << std::setprecision(4) << r.simulatedTime << "\n";
    if (m_model.overloadAllowed()) {
        os << "*** OVERLOADED MODEL: at least one queue grows without bound.\n"
                     "*** These are TERMINATING-run results for this horizon only.\n"
                     "*** They are not steady-state values and will change if the\n"
                     "*** run length changes.\n";
    }

    os << "\nTALLY VARIABLES\n";
    os << std::setw(34) << std::left << "Identifier" << std::right
              << std::setw(12) << "Average" << std::setw(12) << "Minimum"
              << std::setw(12) << "Maximum" << std::setw(14) << "Observations" << "\n";
    os << std::string(84, '-') << "\n";

    auto tally = [&](const std::string& id, double avg, double mn, double mx, long long obs) {
        os << std::setw(34) << std::left << id << std::right
                  << std::setw(12) << std::setprecision(5) << avg
                  << std::setw(12) << mn << std::setw(12) << mx
                  << std::setw(14) << obs << "\n";
    };

    for (const auto& kv : m_byType) {
        const TypeStats& t = kv.second;
        const double avg = t.out ? t.totalTime / static_cast<double>(t.out) : 0.0;
        tally(kv.first + ".TotalTime", avg, 0.0, t.maxTime, t.out);
    }
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        const Station& s = m_model.stationAt(i);
        tally(s.name() + ".Queue.WaitingTime", s.stats().averageWaitingTime(),
              0.0, s.stats().maxWaitingTime(), s.stats().numberServed());
    }
    for (std::size_t i = 0; i < m_model.nodeCount(); ++i) {
        if (auto* b = dynamic_cast<const BatchNode*>(&m_model.nodeAt(i)))
            tally(b->name() + ".Queue.WaitingTime", b->queueStats().averageWaitingTime(),
                  0.0, b->queueStats().maxWaitingTime(), b->queueStats().numberServed());
    }

    os << "\nDISCRETE-CHANGE VARIABLES\n";
    os << std::setw(34) << std::left << "Identifier" << std::right
              << std::setw(12) << "Average" << std::setw(14) << "Final Value" << "\n";
    os << std::string(60, '-') << "\n";
    auto dcv = [&](const std::string& id, double avg, double final) {
        os << std::setw(34) << std::left << id << std::right
                  << std::setw(12) << std::setprecision(5) << avg
                  << std::setw(14) << final << "\n";
    };
    for (const auto& kv : m_byType)
        dcv(kv.first + ".WIP", T > 0 ? kv.second.areaWIP / T : 0.0, kv.second.inSystem);
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        const Station& s = m_model.stationAt(i);
        // Arena's "scheduled utilization" is busy resource-time divided by
        // (capacity x scheduled time) -- exactly what utilisation() computes,
        // because this engine has no resource schedules yet and every resource
        // is scheduled for the whole run.
        dcv(s.name() + ".Utilization", s.stats().utilisation(T, s.resource().capacity()),
            s.unitsHeld());
        dcv(s.name() + ".Queue.NumberInQueue", s.stats().timeAverageA(T),
            static_cast<double>(s.queue().length()));
    }

    os << "\nOUTPUTS\n";
    os << std::string(60, '-') << "\n";
    auto out = [&](const std::string& id, double v) {
        os << std::setw(40) << std::left << id << std::right
                  << std::setw(14) << std::setprecision(3) << v << "\n";
    };
    for (const auto& kv : m_byType) {
        out(kv.first + ".NumberIn",  static_cast<double>(kv.second.in));
        out(kv.first + ".NumberOut", static_cast<double>(kv.second.out));
    }
    for (std::size_t i = 0; i < m_model.stationCount(); ++i) {
        const Station& s = m_model.stationAt(i);
        out(s.name() + " Number In",  static_cast<double>(s.stats().numberArrived()));
        out(s.name() + " Number Out", static_cast<double>(s.stats().numberServed()));
        out(s.name() + ".ScheduledUtilization",
            s.stats().utilisation(T, s.resource().capacity()));
    }
    for (std::size_t i = 0; i < m_model.nodeCount(); ++i) {
        const INode& n = m_model.nodeAt(i);
        if (auto* d = dynamic_cast<const DisposeNode*>(&n))
            out(d->name() + ".NumberOut", static_cast<double>(d->count()));
        if (auto* b = dynamic_cast<const BatchNode*>(&n))
            out(b->name() + ".BatchesFormed", static_cast<double>(b->batchesFormed()));
        if (auto* rc = dynamic_cast<const RecordNode*>(&n)) {
            out(rc->name() + ".Count", static_cast<double>(rc->count()));
            out(rc->name() + ".Average", rc->average());
        }
    }
    out("System.NumberOut", static_cast<double>(r.exited));
    os << "============================================================\n";
}

void SimulationSystem::reportArenaStyle() const { reportArenaStyle(std::cout); }

void SimulationSystem::report(std::ostream& os) const {
    // v5: report() prints results(). It does not recompute anything -- one place
    // knows how a number is derived, and printing is just a view of it.
    const RunResults r = results();
    os << std::fixed << std::setprecision(4);
    // Before the numbers, not after: a reader who stops at the first table
    // should still have been told the check could not be applied.
    reportStability(os);
    os << "=== simulation report =====================================\n";
    os << "seed                     : " << m_rng.seed() << "\n";
    os << "termination              : "
              << (m_termination ? m_termination->describe() : "<none>") << "\n";
    os << "total simulated time     : " << r.simulatedTime << "\n";
    os << "warm-up discarded        : " << r.warmUpDiscarded << "\n";
    os << "measured period          : " << r.measuredTime << "\n";
    os << "entities arrived         : " << r.arrived << "\n";
    os << "entities exited          : " << r.exited << "\n";
    os << "average total wait       : " << r.averageWait << "\n";
    os << "average time in system   : " << r.averageTimeInSystem << "\n";
    os << "max total wait           : " << r.maxWait << "\n";
    os << "time-avg in queue (Lq)   : " << r.averageNumberInQueue << "\n";
    os << "time-avg in system (L)   : " << r.averageNumberInSystem << "\n";
    os << "still waiting at stop    : " << r.stillWaitingAtStop << "\n";
    os << "live entity objects      : " << m_entities.size() << "\n";
    os << "\n";
    os << "  station        cap   served    avg wait   time-avg Q    util   maxQ\n";
    os << "  -----------------------------------------------------------------\n";
    for (const StationResults& s : r.stations) {
        os << "  " << std::setw(12) << std::left << s.name << std::right
                  << std::setw(5)  << s.capacity
                  << std::setw(9)  << s.served
                  << std::setw(12) << s.averageWait
                  << std::setw(13) << s.averageQueueLength
                  << std::setw(8)  << s.utilisation
                  << std::setw(7)  << s.maxQueueLength
                  << "\n";
    }
    os << "===========================================================\n";
}

void SimulationSystem::report() const { report(std::cout); }

}  // namespace des
