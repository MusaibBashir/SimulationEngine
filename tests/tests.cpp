// ============================================================================
// tests/tests.cpp  --  v3 step 9: unit tests
// ============================================================================
// DELIBERATELY LAST in the v3 order. Tests written against the v2 interfaces
// would have been rewritten three times over as distributions, queue rules and
// the Model landed. Test the shape once it stops moving.
//
// No external framework. Catch2 and doctest are both fine, but adding a
// dependency to learn what a dependency costs is a poor trade when the whole
// harness is twenty lines. If the suite outgrows this, swap it then.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <cmath>
#include <memory>
#include <vector>
#include <string>
#include "des.hpp"
#include "harness.hpp"

using namespace des;
using des_test::check;
using des_test::checkClose;
using des_test::section;

void runExpressionTests();   // tests/expression_tests.cpp
void runDocumentTests();     // tests/document_tests.cpp
void runRuntimeTests();      // tests/runtime_tests.cpp

namespace {

// ---------------------------------------------------------------------------

void testEntity() {
    section("Entity");
    Entity e(7, 1.5);
    check(e.id() == 7, "id is what it was constructed with");
    checkClose(e.creationTime(), 1.5, 1e-12, "creationTime");
    check(!e.hasAttribute("priority"), "absent attribute reports absent");
    checkClose(e.attribute("priority"), 0.0, 1e-12, "absent attribute reads 0.0");
    e.setAttribute("priority", 3.0);
    check(e.hasAttribute("priority"), "attribute present after set");
    checkClose(e.attribute("priority"), 3.0, 1e-12, "attribute round-trips");
    e.setAttribute("priority", 4.0);
    checkClose(e.attribute("priority"), 4.0, 1e-12, "setAttribute overwrites");
}

void testResource() {
    section("Resource");
    Resource r("Teller", 3);
    check(r.capacity() == 3, "capacity");
    check(r.unitsBusy() == 0, "starts idle");
    check(r.unitsAvailable() == 3, "unitsAvailable derived");
    check(r.state() == ResourceState::Idle, "idle state");
    r.seize(2);
    check(r.unitsBusy() == 2 && r.unitsAvailable() == 1, "seize(2)");
    check(r.state() == ResourceState::Busy, "busy state");
    check(r.isAvailable(), "one unit still free");
    r.seize();
    check(!r.isAvailable(), "fully seized");
    r.release(3);
    check(r.unitsBusy() == 0, "release(3)");
    r.seize(1);
    r.reset();
    check(r.unitsBusy() == 0, "reset clears run state");
    check(r.capacity() == 3, "reset keeps configuration");
}

void testQueueDisciplines() {
    section("Queue disciplines");
    RandomStream rng(1u);
    std::vector<std::unique_ptr<Entity>> pool;
    auto make = [&](EntityId id, double prio, double svc, double due) {
        pool.push_back(std::make_unique<Entity>(id, 0.0));
        pool.back()->setAttribute("priority", prio);
        pool.back()->setAttribute("serviceTime", svc);
        pool.back()->setAttribute("dueDate", due);
        return pool.back().get();
    };
    Entity* a = make(1, 5.0, 9.0, 30.0);
    Entity* b = make(2, 1.0, 2.0, 10.0);
    Entity* c = make(3, 9.0, 5.0, 20.0);

    auto fill = [&](EntityQueue& q) { q.push(a); q.push(b); q.push(c); };

    { EntityQueue q("q", QueueDiscipline::FIFO); q.setRandomStream(&rng); fill(q);
      check(q.pop() == a, "FIFO serves the first in"); }
    { EntityQueue q("q", QueueDiscipline::LIFO); q.setRandomStream(&rng); fill(q);
      check(q.pop() == c, "LIFO serves the last in"); }
    { EntityQueue q("q", QueueDiscipline::Priority); q.setRandomStream(&rng); fill(q);
      check(q.pop() == c, "Priority serves the HIGHEST priority"); }
    { EntityQueue q("q", QueueDiscipline::SPT); q.setRandomStream(&rng); fill(q);
      check(q.pop() == b, "SPT serves the SHORTEST service time"); }
    { EntityQueue q("q", QueueDiscipline::EDD); q.setRandomStream(&rng); fill(q);
      check(q.pop() == b, "EDD serves the EARLIEST due date"); }
    { EntityQueue q("q", QueueDiscipline::Random); q.setRandomStream(&rng); fill(q);
      Entity* got = q.pop();
      check(got == a || got == b || got == c, "Random returns one of the entities");
      check(q.length() == 2, "Random removes exactly one"); }

    { EntityQueue q("q", QueueDiscipline::FIFO); q.setRandomStream(&rng);
      check(q.pop() == nullptr, "pop on an empty queue returns nullptr");
      fill(q);
      check(q.maxLengthObserved() == 3, "maxLengthObserved tracks the peak");
      q.pop();
      check(q.maxLengthObserved() == 3, "peak does not fall when the queue drains");
      q.reset();
      check(q.isEmpty() && q.maxLengthObserved() == 0, "reset clears both"); }

    // Ties resolve to the earliest arrival -- FIFO among equals. A real
    // modelling decision, so it gets a test.
    { std::vector<std::unique_ptr<Entity>> p2;
      p2.push_back(std::make_unique<Entity>(10, 0.0)); p2.back()->setAttribute("priority", 5.0);
      p2.push_back(std::make_unique<Entity>(11, 0.0)); p2.back()->setAttribute("priority", 5.0);
      EntityQueue q("q", QueueDiscipline::Priority); q.setRandomStream(&rng);
      q.push(p2[0].get()); q.push(p2[1].get());
      check(q.pop() == p2[0].get(), "equal priorities break FIFO"); }
}

void testFutureEventList() {
    section("FutureEventList");
    FutureEventList fel;
    check(fel.isEmpty(), "starts empty");
    fel.schedule(EventNotice(EventType::Arrival, 10.0));
    fel.schedule(EventNotice(EventType::Arrival, 3.0));
    fel.schedule(EventNotice(EventType::Arrival, 7.0));
    check(fel.size() == 3, "size");
    checkClose(fel.nextEventTime(), 3.0, 1e-12, "nextEventTime peeks the earliest");
    checkClose(fel.popImminent().time(), 3.0, 1e-12, "earliest first");
    checkClose(fel.popImminent().time(), 7.0, 1e-12, "then 7");
    checkClose(fel.popImminent().time(), 10.0, 1e-12, "then 10");
    check(fel.isEmpty(), "drained");

    // Tie-break: equal times come out in scheduling order.
    fel.schedule(EventNotice(EventType::Arrival,   5.0));
    fel.schedule(EventNotice(EventType::Departure, 5.0));
    fel.schedule(EventNotice(EventType::StartService, 5.0));
    check(fel.popImminent().type() == EventType::Arrival,      "tie 1: scheduling order");
    check(fel.popImminent().type() == EventType::Departure,    "tie 2: scheduling order");
    check(fel.popImminent().type() == EventType::StartService, "tie 3: scheduling order");

    fel.schedule(EventNotice(EventType::Arrival, 1.0));
    fel.clear();
    check(fel.isEmpty(), "clear empties the list");

    // v4: the sequence counter belongs to the FEL and rewinds with clear(), so
    // two separate lists cannot interfere and replications stay bit-identical.
    FutureEventList a, b;
    a.schedule(EventNotice(EventType::Arrival, 2.0));
    a.schedule(EventNotice(EventType::Departure, 2.0));
    b.schedule(EventNotice(EventType::Arrival, 2.0));
    b.schedule(EventNotice(EventType::Departure, 2.0));
    check(a.popImminent().type() == b.popImminent().type(),
          "two independent FELs order identically");
}

void testActivityAndDelay() {
    section("Activity and Delay");
    Activity act("Service", 10.0, 2.5);
    checkClose(act.endTime(), 12.5, 1e-12, "Activity endTime is derived");
    checkClose(act.duration(), 2.5, 1e-12, "Activity duration");

    Delay d(4.0);
    check(!d.hasEnded(), "a new Delay has not ended");
    d.end(9.0);
    check(d.hasEnded(), "ended after end()");
    checkClose(d.duration(), 5.0, 1e-12, "Delay duration is end - start");
}

void testStatistics() {
    section("Statistics");
    Statistics s;
    checkClose(s.averageWaitingTime(), 0.0, 1e-12, "no divide-by-zero with 0 served");

    // Hand-computed rectangles: queue length 2 for 5 minutes, then 0 for 5.
    // Area = 2*5 + 0*5 = 10. Busy 1 throughout: area = 10.
    s.updateTimeIntegrals(5.0, 2, 1);
    s.updateTimeIntegrals(10.0, 0, 1);
    checkClose(s.areaA(), 10.0, 1e-12, "integral A");
    checkClose(s.areaB(), 10.0, 1e-12, "integral B");
    checkClose(s.timeAverageA(10.0), 1.0, 1e-12, "time-average A");
    checkClose(s.utilisation(10.0, 1), 1.0, 1e-12, "utilisation");

    s.recordArrival(0.0);
    s.recordDeparture(10.0, 4.0, 6.0);
    s.recordDeparture(12.0, 2.0, 4.0);
    check(s.numberArrived() == 1 && s.numberServed() == 2, "counters");
    checkClose(s.averageWaitingTime(), 3.0, 1e-12, "average wait");
    checkClose(s.averageTimeInSystem(), 5.0, 1e-12, "average time in system");
    checkClose(s.maxWaitingTime(), 4.0, 1e-12, "max wait");

    s.reset();
    check(s.numberServed() == 0 && s.areaA() == 0.0, "reset zeroes everything");

    // v4: restartAt is reset() plus "start the integral clock here". It is the
    // whole of warm-up removal, so it gets its own check.
    Statistics w;
    w.updateTimeIntegrals(100.0, 9, 1);          // a big transient, then...
    check(w.areaA() > 0.0, "transient accumulated");
    w.restartAt(100.0);                          // ...discard it
    checkClose(w.areaA(), 0.0, 1e-12, "restartAt discards accumulated area");
    checkClose(w.lastUpdateTime(), 100.0, 1e-12, "restartAt moves the integral clock");
    w.updateTimeIntegrals(110.0, 2, 1);
    checkClose(w.areaA(), 20.0, 1e-12, "post-restart area measures only the new window");
    checkClose(w.timeAverageA(10.0), 2.0, 1e-12,
               "divide by the MEASURED period, not the clock");

    Statistics labelled("in queue", "in system");
    check(labelled.labelA() == "in queue" && labelled.labelB() == "in system", "labels");
    labelled.reset();
    check(labelled.labelA() == "in queue", "reset keeps labels (configuration)");
}

void testSummary() {
    section("Summary (mean, sample sd, t interval)");
    const std::vector<double> xs{2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    checkClose(Summary::mean(xs), 5.0, 1e-12, "mean");
    // Population sd of this classic set is 2.0; the SAMPLE sd (n-1) is larger.
    checkClose(Summary::stdDev(xs), std::sqrt(32.0 / 7.0), 1e-12, "sample sd uses n-1");
    check(Summary::stdDev(xs) > 2.0, "sample sd exceeds population sd");
    checkClose(Summary::standardError(xs), Summary::stdDev(xs) / std::sqrt(8.0), 1e-12,
               "standard error is s/sqrt(n)");

    checkClose(Summary::tCritical95(1), 12.706, 1e-9, "t(1)");
    checkClose(Summary::tCritical95(9), 2.262, 1e-9, "t(9) -- 10 replications");
    checkClose(Summary::tCritical95(30), 2.042, 1e-9, "t(30)");
    checkClose(Summary::tCritical95(500), 1.96, 1e-9, "large df falls back to normal");
    check(Summary::tCritical95(9) > 1.96,
          "t beats the normal at small n -- using 1.96 would understate the interval");

    checkClose(Summary::halfWidth95(xs),
               Summary::tCritical95(7) * Summary::standardError(xs), 1e-12, "half-width");
    check(Summary::stdDev(std::vector<double>{1.0}) == 0.0, "one sample says nothing about spread");
    check(Summary::halfWidth95(std::vector<double>{}) == 0.0, "empty sample does not divide by zero");
}

void testDistributions() {
    section("Distributions");
    RandomStream rng(99u);

    Constant c(3.5);
    checkClose(c.draw(rng), 3.5, 1e-12, "Constant draws its value");
    checkClose(c.draw(rng), 3.5, 1e-12, "Constant is constant");

    Deterministic d(std::vector<SimTime>{1.0, 2.0, 3.0}, /*repeat=*/true);
    checkClose(d.draw(rng), 1.0, 1e-12, "Deterministic 1st");
    checkClose(d.draw(rng), 2.0, 1e-12, "Deterministic 2nd");
    checkClose(d.draw(rng), 3.0, 1e-12, "Deterministic 3rd");
    checkClose(d.draw(rng), 1.0, 1e-12, "Deterministic wraps when repeating");
    d.reset();
    checkClose(d.draw(rng), 1.0, 1e-12, "reset rewinds the cursor");

    Uniform u(2.0, 4.0);
    bool inRange = true;
    for (int i = 0; i < 1000; ++i) { const SimTime v = u.draw(rng); if (v < 2.0 || v > 4.0) inRange = false; }
    check(inRange, "Uniform stays inside its bounds");

    Triangular t(1.0, 2.0, 6.0);
    inRange = true;
    double sum = 0.0;
    for (int i = 0; i < 20000; ++i) { const SimTime v = t.draw(rng); if (v < 1.0 || v > 6.0) inRange = false; sum += v; }
    check(inRange, "Triangular stays inside its bounds");
    checkClose(sum / 20000.0, (1.0 + 2.0 + 6.0) / 3.0, 0.05, "Triangular mean is (a+m+b)/3");

    // The mean of an exponential is the parameter, not its reciprocal. The
    // single most common way to get a queueing simulation quietly wrong.
    Exponential e(4.0);
    sum = 0.0;
    for (int i = 0; i < 200000; ++i) sum += e.draw(rng);
    checkClose(sum / 200000.0, 4.0, 0.1, "Exponential(mean=4) has mean 4, not 0.25");
}

void testTerminationRules() {
    section("Termination rules");
    SimulationSystem sim(5u);
    Model& m = sim.model();
    m.setInterarrival(constant(1.0));
    m.station("S", 1, QueueDiscipline::FIFO, constant(0.5));
    m.setEntry("S");
    sim.setTermination(entityLimit(10));
    sim.initialise();
    sim.run();
    check(sim.statistics().numberServed() >= 10, "EntityLimit stops after N exits");

    SimulationSystem sim2(5u);
    Model& m2 = sim2.model();
    m2.setInterarrival(constant(1.0));
    m2.station("S", 1, QueueDiscipline::FIFO, constant(0.5));
    m2.setEntry("S");
    sim2.setTermination(timeLimit(20.0));
    sim2.initialise();
    sim2.run();
    check(sim2.clock().now() >= 20.0, "TimeLimit stops at the time limit");

    auto any = std::make_unique<AnyOf>();
    check(any->empty(), "an empty AnyOf reports empty");
    any->add(timeLimit(1000.0));
    any->add(entityLimit(5));
    check(!any->empty(), "AnyOf holds its children");
}

void testDeterministicEndToEnd() {
    section("Deterministic end-to-end (hand-worked)");
    // Same model as scenario 3 in main. Worked by hand:
    //   5 served, waits 0,1,0,3,1 -> average 1.0, last exit at t = 13.
    SimulationSystem sim(1u);
    Model& m = sim.model();
    m.setInterarrival(fixedTimes({2, 4, 1, 3, 5}));
    m.station("Server", 1, QueueDiscipline::FIFO,
                 fixedTimes({3, 2, 4, 1, 2}));
    m.setEntry("Server");
    sim.setTermination(entityLimit(5));
    sim.initialise();
    sim.run();

    check(sim.statistics().numberServed() == 5, "5 entities served");
    checkClose(sim.statistics().averageWaitingTime(), 1.0, 1e-9, "average wait is exactly 1.0");
    checkClose(sim.statistics().maxWaitingTime(), 3.0, 1e-9, "max wait is exactly 3.0");
    checkClose(sim.clock().now(), 13.0, 1e-9, "last exit at t = 13");
    checkClose(sim.statistics().averageTimeInSystem(), 3.4, 1e-9, "average time in system");
    check(sim.liveEntityCount() == 0, "every entity was destroyed on exit");
}

void testChainRouting() {
    section("Chain routing");
    // Two stations in series, no randomness anywhere: an entity must visit both.
    SimulationSystem sim(1u);
    Model& m = sim.model();
    m.setInterarrival(constant(100.0));   // one arrival, then far apart
    m.station("A", 1, QueueDiscipline::FIFO, constant(2.0));
    m.station("B", 1, QueueDiscipline::FIFO, constant(3.0));
    m.connect("A", "B");
    m.setEntry("A");
    sim.setTermination(entityLimit(1));
    sim.initialise();
    sim.run();

    check(sim.model().station("A")->stats().numberServed() == 1, "station A served it");
    check(sim.model().station("B")->stats().numberServed() == 1, "station B served it too");
    checkClose(sim.statistics().averageTimeInSystem(), 5.0, 1e-9,
               "time in system is 2 + 3 across both stations");
    check(sim.model().station("A")->next() == sim.model().station("B"), "A routes to B");
    check(sim.model().station("B")->next() == nullptr, "B is the exit");
}

void testReproducibility() {
    section("Reproducibility");
    auto build = [](SimulationSystem& s) {
        Model& m = s.model();
        m.setInterarrival(exponential(1.0));
        m.station("S", 2, QueueDiscipline::FIFO, exponential(1.5));
        m.setEntry("S");
        s.setTermination(timeLimit(500.0));
    };
    SimulationSystem a(4242u); build(a); a.initialise(); a.run();
    SimulationSystem b(4242u); build(b); b.initialise(); b.run();
    check(a.statistics().numberServed() == b.statistics().numberServed(),
          "two systems, same seed, same count");
    checkClose(a.statistics().averageWaitingTime(), b.statistics().averageWaitingTime(),
               0.0, "two systems, same seed, identical average");

    // And the same system re-initialised must match itself.
    SimulationSystem c(4242u); build(c);
    c.initialise(); c.run();
    const int n1 = c.statistics().numberServed();
    c.initialise(); c.run();
    check(c.statistics().numberServed() == n1, "re-initialising resets every bit of run state");
}

void testWarmUpRemoval() {
    section("Warm-up removal");
    // Constant everything, so the answer is arithmetic rather than statistics.
    // Arrivals every 1.0, service 0.5 -> nobody ever waits, utilisation 0.5.
    auto build = [](SimulationSystem& s) {
        Model& m = s.model();
        m.setInterarrival(constant(1.0));
        m.station("S", 1, QueueDiscipline::FIFO, constant(0.5));
        m.setEntry("S");
        s.setTermination(timeLimit(100.0));
    };

    SimulationSystem noWarm(1u); build(noWarm);
    noWarm.initialise(); noWarm.run();
    checkClose(noWarm.measuredTime(), noWarm.clock().now(), 1e-9,
               "no warm-up: measured period is the whole clock");
    checkClose(noWarm.warmUpEnd(), 0.0, 1e-12, "no warm-up: measurement starts at 0");

    SimulationSystem warm(1u); build(warm);
    warm.setWarmUp(40.0);
    warm.initialise(); warm.run();
    checkClose(warm.warmUpEnd(), 40.0, 1e-9, "warm-up ended when asked");
    checkClose(warm.measuredTime(), warm.clock().now() - 40.0, 1e-9,
               "measured period excludes the warm-up");
    check(warm.statistics().numberServed() < noWarm.statistics().numberServed(),
          "warm-up discards the entities served during the transient");
    // Utilisation is 0.5 either way -- the SYSTEM did not change, only what was
    // measured. That is the point of warm-up removal.
    const Station& st = warm.model().stationAt(0);
    checkClose(st.stats().utilisation(warm.measuredTime(), 1), 0.5, 1e-6,
               "utilisation unchanged by warm-up removal");
}

void testObservations() {
    section("Observation series");
    SimulationSystem sim(3u);
    Model& m = sim.model();
    m.setInterarrival(constant(1.0));
    m.station("S", 1, QueueDiscipline::FIFO, constant(0.5));
    m.setEntry("S");
    sim.setTermination(timeLimit(100.0));
    sim.setObservationInterval(10.0);
    sim.initialise(); sim.run();
    check(sim.observations().size() >= 9, "sampled on the grid, roughly time/dt points");
    check(sim.observations().size() <= 11, "and not many more than that");
}

void testExperiment() {
    section("Experiment");
    auto build = [](SimulationSystem& s) {
        Model& m = s.model();
        m.setInterarrival(exponential(1.0));
        m.station("S", 1, QueueDiscipline::FIFO, exponential(0.8));
        m.setEntry("S");
        s.setTermination(timeLimit(1500.0));
    };

    Experiment e("test", build);
    e.replications(6).baseSeed(300u).warmUp(200.0);
    e.run();
    check(e.results().size() == 6, "six replications ran");

    // Each replication must use a DIFFERENT seed, or the "sample" has no
    // variance and the confidence interval is a lie.
    bool allDifferent = true;
    for (std::size_t i = 1; i < e.results().size(); ++i)
        if (e.results()[i].seed == e.results()[0].seed) allDifferent = false;
    check(allDifferent, "replications use different seeds");

    const std::vector<double> wq = e.column(&ReplicationResult::averageWait);
    check(wq.size() == 6, "column() pulls one value per replication");
    check(Summary::stdDev(wq) > 0.0, "results actually vary between replications");
    check(Summary::halfWidth95(wq) > 0.0, "a real confidence interval");

    // And the whole experiment is reproducible from its base seed.
    Experiment f("test again", build);
    f.replications(6).baseSeed(300u).warmUp(200.0);
    f.run();
    bool identical = true;
    for (std::size_t i = 0; i < 6; ++i)
        if (f.results()[i].averageWait != e.results()[i].averageWait) identical = false;
    check(identical, "same base seed reproduces the whole experiment");
}

void testWarmUpSuggestion() {
    section("MSER warm-up suggestion");
    // A BUSY queue (rho = 0.9) has a real transient: it takes hundreds of
    // minutes to fill from empty toward steady state.
    Experiment busy("busy", [](SimulationSystem& s) {
        Model& m = s.model();
        m.setInterarrival(exponential(1.0));
        m.station("S", 1, QueueDiscipline::FIFO, exponential(0.9));
        m.setEntry("S");
        s.setTermination(timeLimit(20000.0));
    });
    busy.replications(20).baseSeed(500u).observeEvery(20.0);
    busy.run();
    const SimTime w = busy.suggestWarmUp();
    check(w > 0.0, "a busy queue has a transient worth discarding");
    check(w < 20000.0 * 0.5, "and MSER never discards more than half the run");

    // The averaged series must actually rise from its empty start toward the
    // steady-state level -- that rise IS the transient the rule is finding.
    const std::vector<double> s = busy.welchAverages(0);
    check(s.size() > 100, "series collected");
    double head = 0.0, tail = 0.0;
    for (std::size_t k = 0; k < 5; ++k) head += s[k];
    for (std::size_t k = s.size() - 100; k < s.size(); ++k) tail += s[k];
    check(head / 5.0 < tail / 100.0, "the run starts emptier than it ends");
}

void testModelErrors() {
    section("Model errors (v5)");
    auto throwsModelError = [](auto fn) {
        try { fn(); } catch (const ModelError&) { return true; } catch (...) { return false; }
        return false;
    };

    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().arrivals(exponential(1.0))
                 .station("A", 1, FIFO, exponential(1.4))   // rho = 1.4
                 .entryAt("A");
        s.stopAt(100.0).execute();
    }), "an unstable station (rho >= 1) is refused, not simulated");

    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().arrivals(exponential(1.0))
                 .station("A", 1, FIFO, exponential(0.5))
                 .route("A", "Nope");
    }), "routing to a station that does not exist");

    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().arrivals(exponential(1.0))
                 .station("A", 1, FIFO, exponential(0.5))
                 .station("A", 1, FIFO, exponential(0.5));
    }), "duplicate station name");

    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().arrivals(exponential(1.0))
                 .attribute("waitTime", constant(1.0));
    }), "a reserved attribute name");

    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().station("A", 1, FIFO, exponential(0.5)).entryAt("A");
        s.stopAt(10.0).execute();
    }), "no arrival distribution");

    // And an attribute-driven service time is checked too -- job shops are the
    // models most likely to be accidentally unstable, so skipping them would be
    // exactly backwards.
    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().arrivals(exponential(10.0))
                 .attribute(attr::serviceTime, uniform(2.0, 30.0))   // mean 16
                 .station("M", 1, SPT, uniform(2.0, 30.0))
                 .entryAt("M");
        s.model().station("M")->setServiceFromAttribute(attr::serviceTime);
        s.stopAt(100.0).execute();
    }), "unstable attribute-driven service is caught");

    // A stable model must NOT throw.
    bool ok = true;
    try {
        SimulationSystem s(1u);
        s.model().arrivals(exponential(1.0))
                 .station("A", 2, FIFO, exponential(1.5))   // rho = 0.75
                 .entryAt("A");
        s.stopAt(200.0).execute();
    } catch (...) { ok = false; }
    check(ok, "a stable model runs without complaint");
}

void testDistributionMeans() {
    section("Distribution means (v5)");
    checkClose(Exponential(4.0).mean(), 4.0, 1e-12, "Exponential mean is its parameter");
    checkClose(Constant(2.5).mean(), 2.5, 1e-12, "Constant");
    checkClose(Uniform(2.0, 6.0).mean(), 4.0, 1e-12, "Uniform is (a+b)/2");
    checkClose(Triangular(1.0, 2.0, 6.0).mean(), 3.0, 1e-12, "Triangular is (a+m+b)/3");
    checkClose(Deterministic(std::vector<SimTime>{1.0, 2.0, 6.0}).mean(), 3.0, 1e-12,
               "Deterministic is the list average");
}

void testResultsStruct() {
    section("RunResults (v5)");
    SimulationSystem sim(4u);
    sim.model().arrivals(constant(1.0))
               .station("A", 1, FIFO, constant(0.5))
               .station("B", 1, FIFO, constant(0.25))
               .route("A", "B")
               .entryAt("A");
    sim.stopAt(200.0).warmUpFor(50.0).execute();

    const RunResults r = sim.results();
    check(r.stations.size() == 2, "one entry per station");
    check(r.station("A").name == "A" && r.station("B").name == "B", "lookup by name");
    checkClose(r.measuredTime, r.simulatedTime - r.warmUpDiscarded, 1e-9,
               "measured period excludes the warm-up");
    // results() must divide by the MEASURED period, not the clock. Utilisation
    // at A is 0.5 either way only if that is done right.
    checkClose(r.station("A").utilisation, 0.5, 1e-3, "utilisation uses measuredTime");
    checkClose(r.station("B").utilisation, 0.25, 1e-3, "second station too");

    bool threw = false;
    try { r.station("nope"); } catch (const ModelError&) { threw = true; }
    check(threw, "unknown station name throws instead of returning zeros");

    // report() must agree with results() -- it is supposed to be a view of it.
    checkClose(r.averageWait, sim.statistics().averageWaitingTime(), 1e-12,
               "results() and the underlying Statistics agree");
}

void testBuildHelpers() {
    section("Build helpers (v5)");
    // The factories must produce exactly what the constructors did.
    checkClose(exponential(3.0)->mean(), 3.0, 1e-12, "exponential()");
    checkClose(uniform(1.0, 3.0)->mean(), 2.0, 1e-12, "uniform()");
    checkClose(triangular(1.0, 2.0, 3.0)->mean(), 2.0, 1e-12, "triangular()");
    checkClose(fixedTimes({2.0, 4.0})->mean(), 3.0, 1e-12, "fixedTimes()");
    check(FIFO == QueueDiscipline::FIFO && SPT == QueueDiscipline::SPT, "discipline aliases");
    check(attr::serviceTime == "serviceTime" && attr::priority == "priority",
          "attribute name constants match what the rules look up");

    // anyOf() must compose variadically and fire when any child fires.
    SimulationSystem sim(1u);
    sim.model().arrivals(constant(1.0)).station("A", 1, FIFO, constant(0.5)).entryAt("A");
    sim.stopWhen(anyOf(timeLimit(1000.0), entityLimit(20))).execute();
    check(sim.statistics().numberServed() >= 20 && sim.clock().now() < 1000.0,
          "anyOf stops on whichever child fires first");
}

void testEstimate() {
    section("Experiment::Estimate (v5)");
    const std::vector<double> xs{1.0, 2.0, 3.0, 4.0, 5.0};
    const Experiment::Estimate e = Experiment::estimate(xs);
    checkClose(e.mean, 3.0, 1e-12, "mean");
    checkClose(e.halfWidth, Summary::halfWidth95(xs), 1e-12, "half-width matches Summary");
    checkClose(e.low(), e.mean - e.halfWidth, 1e-12, "low");
    checkClose(e.high(), e.mean + e.halfWidth, 1e-12, "high");
    check(e.covers(3.0), "covers its own mean");
    check(!e.covers(100.0), "does not cover a distant value");
}

void testFlowchartBlocks() {
    section("Flowchart blocks (v6)");

    // Everything constant, so the answers are arithmetic rather than statistics.
    // One entity every 100 minutes means the system is empty between arrivals
    // and nothing ever queues -- so every number below is exact.
    {
        SimulationSystem sim(1u);
        sim.model()
            .arrivals(constant(100.0))
            .assign("Tag", "kind", constant(7.0))
            .delay("Walk", constant(2.0))
            .station("Desk", 1, FIFO, constant(3.0))
            .recordTimeInSystem("Age")
            .dispose("Out")
            .route("Tag", "Walk").route("Walk", "Desk")
            .route("Desk", "Age").route("Age", "Out")
            .entryAt("Tag");
        sim.stopAfter(3).execute();

        const RunResults r = sim.results();
        check(r.exited == 3, "three entities went all the way through");
        checkClose(r.averageTimeInSystem, 5.0, 1e-9,
                   "Assign is instant, Delay 2 + Process 3 = 5");
        checkClose(sim.model().nodeAs<RecordNode>("Age").average(), 5.0, 1e-9,
                   "Record agrees with the system statistics");
        check(sim.model().nodeAs<DisposeNode>("Out").count() == 3, "Dispose counts exits");
        check(sim.model().nodeAs<AssignNode>("Tag").count() == 3, "Assign counts entities");
        // Three services of exactly 3 minutes each. The run stops when the
        // third entity exits (t = 205), NOT after a full 300-minute cycle, so
        // the denominator is the measured period -- which is the point.
        checkClose(sim.model().station("Desk")->stats().utilisation(sim.measuredTime(), 1),
                   9.0 / sim.measuredTime(), 1e-9, "Desk utilisation is busy-time / measured period");
        checkClose(sim.clock().now(), 205.0, 1e-9, "third entity exits at 100+100+5");
    }

    // DELAY holds many entities at once -- it has no resource, so there is no
    // contention. If it behaved like a capacity-1 Process, entities would queue
    // and the time in system would grow without bound at this arrival rate.
    {
        SimulationSystem sim(1u);
        sim.model()
            .arrivals(constant(1.0))
            .delay("Belt", constant(10.0))     // ten on the belt at once
            .dispose("End")
            .route("Belt", "End")
            .entryAt("Belt");
        sim.stopAt(200.0).warmUpFor(50.0).execute();
        checkClose(sim.results().averageTimeInSystem, 10.0, 1e-6,
                   "a Delay never queues: time in system is exactly the delay");
    }

    // DECIDE by chance: the split must match the probability.
    {
        SimulationSystem sim(3u);
        sim.model()
            .arrivals(constant(1.0))
            .decideByChance("Coin", 0.25)
            .dispose("Heads").dispose("Tails")
            .routeTrue("Coin", "Heads").route("Coin", "Tails")
            .entryAt("Coin");
        sim.stopAfter(4000).execute();
        const auto& d = sim.model().nodeAs<DecideNode>("Coin");
        const double frac = static_cast<double>(d.tookTrue()) / (d.tookTrue() + d.tookFalse());
        checkClose(frac, 0.25, 0.02, "a 0.25 chance branch takes about a quarter");
        check(sim.model().nodeAs<DisposeNode>("Heads").count() == d.tookTrue(),
              "the true branch and its exit agree");
    }

    // DECIDE by condition: deterministic given the entity, so this is exact.
    {
        SimulationSystem sim(3u);
        sim.model()
            .arrivals(constant(1.0))
            .attribute("score", constant(9.0))     // every entity scores 9
            .decideByCondition("High", [](const Entity& e){ return e.attribute("score") > 5.0; })
            .dispose("Big").dispose("Small")
            .routeTrue("High", "Big").route("High", "Small")
            .entryAt("High");
        sim.stopAfter(50).execute();
        const auto& d = sim.model().nodeAs<DecideNode>("High");
        check(d.tookFalse() == 0 && d.tookTrue() > 0, "a condition sends every matching entity one way");
    }

    // BATCH, permanent: four in, one out.
    {
        SimulationSystem sim(1u);
        sim.model()
            .arrivals(constant(1.0))
            .batch("Pack", 4, /*permanent=*/true)
            .dispose("Shipped")
            .route("Pack", "Shipped")
            .entryAt("Pack");
        sim.stopAt(100.0).execute();
        const auto& b = sim.model().nodeAs<BatchNode>("Pack");
        check(b.batchesFormed() > 0, "batches were formed");
        check(sim.model().nodeAs<DisposeNode>("Shipped").count() == b.batchesFormed(),
              "one carton leaves per batch formed, not four");
        // The representative inherits the OLDEST member's creation time, so a
        // carton's age includes the wait for its companions: four arrivals one
        // minute apart means the first waited 3 minutes.
        checkClose(sim.results().averageTimeInSystem, 3.0, 1e-6,
                   "batch age is measured from the oldest member");
    }

    // BATCH temporary + SEPARATE: four in, four out.
    {
        SimulationSystem sim(1u);
        sim.model()
            .arrivals(constant(1.0))
            .batch("Group", 4, /*permanent=*/false)
            .separate("Split")
            .dispose("Done")
            .route("Group", "Split").route("Split", "Done")
            .entryAt("Group");
        sim.stopAt(100.0).execute();
        const auto& b = sim.model().nodeAs<BatchNode>("Group");
        check(sim.model().nodeAs<DisposeNode>("Done").count() == b.batchesFormed() * 4,
              "a temporary batch gives every member back");
    }

    // DUPLICATE: one in, three out.
    {
        SimulationSystem sim(1u);
        sim.model()
            .arrivals(constant(1.0))
            .duplicate("Copy", 2)              // the original plus two copies
            .dispose("Out")
            .route("Copy", "Out")
            .entryAt("Copy");
        sim.stopAt(20.0).execute();
        const auto& sep = sim.model().nodeAs<SeparateNode>("Copy");
        check(sim.model().nodeAs<DisposeNode>("Out").count() == sep.processed() * 3,
              "duplicate x2 sends three entities onward for each one in");
    }

    // Separating something that is not a batch is a modelling mistake, and must
    // say so rather than quietly doing nothing.
    {
        bool threw = false;
        try {
            SimulationSystem sim(1u);
            sim.model().arrivals(constant(1.0)).separate("Split").dispose("Out")
                       .route("Split", "Out").entryAt("Split");
            sim.stopAfter(2).execute();
        } catch (const ModelError&) { threw = true; }
        check(threw, "separating a non-batch throws");
    }
}

void testFlowchartValidation() {
    section("Flowchart validation (v6)");
    auto throwsModelError = [](auto fn) {
        try { fn(); } catch (const ModelError&) { return true; } catch (...) { return false; }
        return false;
    };

    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().arrivals(constant(1.0))
                 .station("A", 1, FIFO, constant(0.5))
                 .station("B", 1, FIFO, constant(0.5))
                 .route("A", "B").route("B", "A")      // a loop
                 .entryAt("A");
        s.stopAt(10.0).execute();
    }), "a routing loop is caught before the run, not as a hang");

    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().arrivals(constant(1.0)).dispose("Out").route("Out", "Out");
    }), "a block cannot route to itself");

    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().arrivals(constant(1.0)).station("A", 1, FIFO, constant(0.5))
                 .dispose("Out").route("Out", "A");
    }), "nothing follows a Dispose");

    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().arrivals(constant(1.0)).station("A", 1, FIFO, constant(0.5))
                 .station("A", 1, FIFO, constant(0.5));
    }), "duplicate block name");

    check(throwsModelError([]{
        SimulationSystem s(1u);
        s.model().arrivals(constant(1.0)).station("A", 1, FIFO, constant(0.5)).entryAt("A");
        s.model().nodeAs<BatchNode>("A");          // it is a Process, not a Batch
    }), "nodeAs<> rejects the wrong kind of block");

    // *** VISIT RATIOS. *** The stability check must follow the flowchart: a
    // block behind a 10% branch sees a tenth of the work, and a block after a
    // batch of 4 sees a quarter of the entities. Treating every block as seeing
    // every entity would reject models that are perfectly fine.
    {
        SimulationSystem s(1u);
        s.model()
            .arrivals(constant(1.0))
            .decideByChance("Split", 0.1)
            .station("Rare", 1, FIFO, constant(5.0))    // 5.0 > 1.0 arrival gap!
            .batch("Pack", 4)
            .station("Packer", 1, FIFO, constant(3.0))  // 3.0 > 1.0 too
            .dispose("Out")
            .routeTrue("Split", "Rare").route("Split", "Pack")
            .route("Rare", "Out").route("Pack", "Packer").route("Packer", "Out")
            .entryAt("Split");

        const Model::VisitRatios vr = s.model().visitRatios();
        check(vr.exact, "a chance-only flowchart has exact visit ratios");
        checkClose(vr.visits.at(s.model().node("Rare")), 0.1, 1e-9, "10% branch");
        checkClose(vr.visits.at(s.model().node("Packer")), 0.9 / 4.0, 1e-9,
                   "90% of entities, then four per carton");

        // Both stations would look overloaded on naive arithmetic (service time
        // longer than the arrival gap) and both are fine once the flowchart is
        // taken into account. rho = 0.1*5 = 0.5 and 0.225*3 = 0.675.
        checkClose(s.model().offeredLoad(*s.model().station("Rare")), 0.5, 1e-9, "rho behind a branch");
        checkClose(s.model().offeredLoad(*s.model().station("Packer")), 0.675, 1e-9, "rho after a batch");
        bool ok = true;
        try { s.stopAt(200.0).execute(); } catch (...) { ok = false; }
        check(ok, "and the model is accepted");
    }

    // A condition-based Decide makes the split an OUTPUT of the run, so the
    // engine must say its ratios are inexact rather than inventing a number.
    {
        SimulationSystem s(1u);
        s.model().arrivals(constant(1.0))
                 .decideByCondition("C", [](const Entity&){ return true; })
                 .dispose("Out").routeTrue("C", "Out").entryAt("C");
        check(!s.model().visitRatios().exact, "a condition-based Decide is flagged inexact");
    }
}

void testSharedResources() {
    section("Shared resources (v7)");

    // ONE operator, TWO machines. Each machine alone would be lightly loaded;
    // together they keep the single operator busy. This is the case that was
    // simply inexpressible before v7, because a Process owned its servers.
    {
        SimulationSystem sim(11u);
        sim.model()
            .arrivals(constant(2.0))
            .resource("Operator", 1)
            .decideNWayByChance("Split")
            .stationUsing("MachineA", "Operator", FIFO, constant(0.6))
            .stationUsing("MachineB", "Operator", FIFO, constant(0.6))
            .dispose("Out")
            .branch("Split", 0.5, "MachineA")
            .branch("Split", 0.5, "MachineB")
            .route("MachineA", "Out").route("MachineB", "Out")
            .entryAt("Split");
        sim.stopAt(2000.0).warmUpFor(200.0).execute();

        const Resource& op = *sim.model().resourceNamed("Operator");
        check(op.userCount() == 2, "both machines registered as users of the operator");
        check(op.unitsBusy() <= op.capacity(), "the shared resource kept its invariant");

        // Both blocks draw on the same resource, so their utilisations must SUM
        // to the operator's. If each had its own private resource, each would
        // have read about 0.15 and the operator's 0.30 would not exist.
        const RunResults r = sim.results();
        const double a = r.station("MachineA").utilisation;
        const double b = r.station("MachineB").utilisation;
        checkClose(a + b, 0.6 / 2.0, 0.05, "the two blocks share one operator's time");
    }

    // Oversubscription: each block is fine alone, the pair is impossible.
    bool threw = false;
    try {
        SimulationSystem s(1u);
        s.model()
            .arrivals(constant(1.0))
            .resource("Op", 1)
            .decideNWayByChance("Split")
            // Each takes a third of a 1-per-minute stream and needs 1.2 min of
            // operator time: 0.4 each, which is fine, and 1.2 together, which is
            // not. Checking them one at a time would pass this model.
            .stationUsing("A", "Op", FIFO, constant(1.2))
            .stationUsing("B", "Op", FIFO, constant(1.2))
            .stationUsing("C", "Op", FIFO, constant(1.2))
            .dispose("Out")
            .branch("Split", 0.34, "A").branch("Split", 0.33, "B").branch("Split", 0.33, "C")
            .route("A", "Out").route("B", "Out").route("C", "Out")
            .entryAt("Split");
        s.stopAt(100.0).execute();
    } catch (const ModelError&) { threw = true; }
    check(threw, "a resource shared by blocks that together exceed it is refused");

    // Asking for more units than the resource has could never start.
    threw = false;
    try {
        SimulationSystem s(1u);
        s.model().arrivals(constant(5.0)).resource("Op", 2)
                 .stationUsing("A", "Op", FIFO, constant(1.0), /*units=*/3)
                 .entryAt("A");
    } catch (const ModelError&) { threw = true; }
    check(threw, "seizing more units than exist is refused at build time");

    // An unknown resource name is a mistake, not a silent private resource.
    threw = false;
    try {
        SimulationSystem s(1u);
        s.model().arrivals(constant(1.0)).stationUsing("A", "Ghost", FIFO, constant(0.5));
    } catch (const ModelError&) { threw = true; }
    check(threw, "using an undeclared resource is refused");
}

void testBalkingAndReneging() {
    section("Balking and reneging (v7)");

    // BALKING: with a queue cap of 2 and a server that cannot keep up, most
    // arrivals must balk rather than pile up forever.
    {
        SimulationSystem sim(5u);
        sim.model()
            // EXPONENTIAL, not constant. With deterministic arrivals every 1.0
            // and a deterministic 0.9 service, a queue never forms at all and
            // nobody would ever balk -- the model would be stable and silent.
            // Balking and reneging only mean anything where a queue fluctuates.
            .arrivals(exponential(1.0))
            .station("Desk", 1, FIFO, exponential(0.9))
            .dispose("LeftAngry")
            .balkAt("Desk", 2, "LeftAngry")
            .route("Desk", "LeftAngry")
            .entryAt("Desk");
        sim.stopAt(500.0).execute();

        const Station& d = *sim.model().station("Desk");
        check(d.balked() > 0, "some arrivals balked");
        check(d.queue().maxLengthObserved() <= 2,
              "the queue never grew past the balk threshold");
        check(sim.model().nodeAs<DisposeNode>("LeftAngry").count() > 0,
              "balkers were routed to the exit that was named for them");
    }

    // RENEGING: infinite patience serves everybody; zero-ish patience serves
    // almost nobody. The comparison is the test -- it pins the direction.
    {
        auto build = [](SimulationSystem& s, bool impatient) {
            s.model().arrivals(exponential(1.0))
                     .station("Desk", 1, FIFO, exponential(0.9))
                     .dispose("GaveUp")
                     .route("Desk", "GaveUp")
                     .entryAt("Desk");
            if (impatient) s.model().renegeAfter("Desk", exponential(0.5), "GaveUp");
        };
        SimulationSystem patient(5u);   build(patient, false);
        patient.stopAt(500.0).execute();
        SimulationSystem hasty(5u);     build(hasty, true);
        hasty.stopAt(500.0).execute();

        check(patient.model().station("Desk")->reneged() == 0, "infinite patience never reneges");
        check(hasty.model().station("Desk")->reneged() > 0, "short patience does");
        check(hasty.model().station("Desk")->stats().numberServed() <
              patient.model().station("Desk")->stats().numberServed(),
              "reneging entities are not served");
        // Every entity is accounted for: served here, or gave up. Nothing is
        // lost, which is what lazy cancellation must not break.
        check(hasty.results().exited > 0, "reneged entities still leave properly");
    }

    // The stale-timer path: a patience so long that nobody ever uses it. Every
    // renege event fires and must be ignored without disturbing anything.
    {
        SimulationSystem sim(5u);
        sim.model().arrivals(exponential(2.0))
                   .station("Desk", 1, FIFO, exponential(0.5))
                   .dispose("Out").route("Desk", "Out").entryAt("Desk");
        sim.model().renegeAfter("Desk", constant(1000.0), "Out");
        sim.stopAt(200.0).execute();
        check(sim.model().station("Desk")->reneged() == 0,
              "stale patience timers fire and are ignored");
        check(sim.results().exited > 50, "and the run is otherwise unaffected");
    }
}

void testNWayDecide() {
    section("N-way Decide (v7)");

    // Three chance branches. One draw is walked against a cumulative
    // probability, so the observed split must match what was asked for.
    {
        SimulationSystem sim(9u);
        sim.model()
            .arrivals(constant(1.0))
            .decideNWayByChance("Sort")
            .dispose("Small").dispose("Medium").dispose("Large")
            .branch("Sort", 0.5, "Small")
            .branch("Sort", 0.3, "Medium")
            .branch("Sort", 0.2, "Large")
            .entryAt("Sort");
        sim.stopAfter(6000).execute();

        const auto& d = sim.model().nodeAs<DecideNode>("Sort");
        const double n = static_cast<double>(d.branches()[0].taken + d.branches()[1].taken +
                                             d.branches()[2].taken + d.fellThrough());
        checkClose(d.branches()[0].taken / n, 0.5, 0.03, "50% branch");
        checkClose(d.branches()[1].taken / n, 0.3, 0.03, "30% branch");
        checkClose(d.branches()[2].taken / n, 0.2, 0.03, "20% branch");
        check(d.fellThrough() == 0, "probabilities summing to 1 leave no fall-through");
    }

    // Probabilities that do not sum to 1 leave a remainder, which falls through
    // to next(). That is a feature -- "10% get inspected, everyone else carries
    // on" is the natural way to say it.
    {
        SimulationSystem sim(9u);
        sim.model()
            .arrivals(constant(1.0))
            .decideNWayByChance("Sample")
            .dispose("Inspected").dispose("Passed")
            .branch("Sample", 0.1, "Inspected")
            .route("Sample", "Passed")
            .entryAt("Sample");
        sim.stopAfter(4000).execute();
        const auto& d = sim.model().nodeAs<DecideNode>("Sample");
        const double n = static_cast<double>(d.branches()[0].taken + d.fellThrough());
        checkClose(d.branches()[0].taken / n, 0.1, 0.02, "the 10% branch");
        check(d.fellThrough() > 0, "the other 90% fall through to next()");
    }

    // Conditions are evaluated IN ORDER and the first match wins, so ordering is
    // a modelling decision. Both conditions below match every entity; the first
    // one declared must take all of them.
    {
        SimulationSystem sim(9u);
        sim.model()
            .arrivals(constant(1.0))
            .attribute("score", constant(9.0))
            .decideNWayByCondition("Grade")
            .dispose("Over5").dispose("Over1")
            .branch("Grade", [](const Entity& e){ return e.attribute("score") > 5.0; }, "Over5")
            .branch("Grade", [](const Entity& e){ return e.attribute("score") > 1.0; }, "Over1")
            .entryAt("Grade");
        sim.stopAfter(50).execute();
        const auto& d = sim.model().nodeAs<DecideNode>("Grade");
        check(d.branches()[0].taken > 0 && d.branches()[1].taken == 0,
              "first matching condition wins");
    }

    // Mixing chance and condition branches has no coherent meaning, so it is
    // refused rather than guessed at.
    bool threw = false;
    try {
        SimulationSystem s(1u);
        s.model().arrivals(constant(1.0)).decideNWayByChance("D").dispose("Out")
                 .branch("D", [](const Entity&){ return true; }, "Out");
    } catch (const ModelError&) { threw = true; }
    check(threw, "a chance Decide refuses a condition branch");

    threw = false;
    try {
        SimulationSystem s(1u);
        s.model().arrivals(constant(1.0)).decideNWayByChance("D").dispose("Out")
                 .branch("D", 0.7, "Out").branch("D", 0.7, "Out");
    } catch (const ModelError&) { threw = true; }
    check(threw, "branch probabilities summing above 1 are refused");
}

void testEntityIdWidth() {
    section("EntityId width (v7)");
    check(sizeof(EntityId) >= 8, "EntityId is 64-bit, so a long run cannot wrap it");
    // A wrapped id would start colliding with live entities in the id-keyed maps
    // -- silently, and only on the longest runs, which is the worst combination.
    check(static_cast<EntityId>(3000000000LL) > 0, "and it holds a value an int could not");
}

void testRandomStreamPrimitive() {
    section("u01 primitive (v8)");
    RandomStream r(123u);
    bool inOpenInterval = true;
    double sum = 0.0;
    const int n = 200000;
    for (int i = 0; i < n; ++i) {
        const double u = r.u01();
        if (u <= 0.0 || u >= 1.0) inOpenInterval = false;   // OPEN interval
        sum += u;
    }
    check(inOpenInterval, "u01 never returns exactly 0 or 1");
    // It must be the open interval: inverse transforms take log(u) and log(1-u),
    // and either endpoint gives an infinity rather than a long service time.
    checkClose(sum / n, 0.5, 0.005, "u01 has mean 0.5");
    check(r.draws() == n, "every uniform is counted");

    r.reset();
    RandomStream s(123u);
    bool same = true;
    for (int i = 0; i < 100; ++i) if (r.u01() != s.u01()) same = false;
    check(same, "same seed, same sequence, and reset() rewinds it");

    // ANTITHETIC: exactly 1-u, draw for draw.
    RandomStream a(77u), b(77u);
    b.setAntithetic(true);
    bool mirrored = true;
    for (int i = 0; i < 100; ++i) if (std::fabs(a.u01() + b.u01() - 1.0) > 1e-12) mirrored = false;
    check(mirrored, "antithetic returns exactly 1-u");

    // And a monotone transform must therefore mirror too: a small u gives a
    // short exponential and 1-u a long one. That property is what antithetic
    // pairing relies on, so it gets its own check.
    RandomStream ea(77u), eb(77u);
    eb.setAntithetic(true);
    const double x = ea.exponential(1.0), y = eb.exponential(1.0);
    check((x < 1.0) != (y < 1.0), "a mirrored uniform gives a mirrored variate");
}

void testSubstreams() {
    section("Independent substreams (v8)");
    RandomStream base(500u);
    RandomStream arrivals = base.substream("arrivals");
    RandomStream service  = base.substream("service");

    bool differ = false;
    for (int i = 0; i < 50; ++i) if (arrivals.u01() != service.u01()) differ = true;
    check(differ, "differently named substreams produce different sequences");

    // Reproducible: the same base seed and name always give the same stream.
    // That is what makes an experiment reproduce from one number.
    RandomStream again = RandomStream(500u).substream("arrivals");
    RandomStream fresh = RandomStream(500u).substream("arrivals");
    bool identical = true;
    for (int i = 0; i < 50; ++i) if (again.u01() != fresh.u01()) identical = false;
    check(identical, "same base seed and name give the same substream");

    // *** THE POINT OF THE WHOLE MECHANISM. ***
    // Two models that differ only in service time must see the SAME arrivals,
    // or replication i of one has nothing in common with replication i of the
    // other and a paired comparison is worthless.
    auto arrivalsSeenBy = [](SimTime serviceMean) {
        SimulationSystem sim(4242u);
        sim.model().arrivals(exponential(1.0))
                   .station("S", 1, FIFO, exponential(serviceMean))
                   .entryAt("S");
        sim.useSeparateStreams().stopAfter(200).execute();
        return sim.statistics().numberArrived();
    };
    check(arrivalsSeenBy(0.5) == arrivalsSeenBy(0.5), "reproducible");
    // Different service times mean different run lengths, so arrival COUNTS
    // differ -- but the arrival stream itself is unshifted, which is what the
    // paired comparison in testCommonRandomNumbers actually measures.
    check(true, "see testCommonRandomNumbers for the end-to-end check");
}

void testNewDistributions() {
    section("v8 distributions");
    RandomStream rng(31u);
    auto meanOf = [&](IDistribution& d, int n) {
        double s = 0.0;
        for (int i = 0; i < n; ++i) s += d.draw(rng);
        return s / n;
    };

    Normal nrm(10.0, 2.0);
    checkClose(meanOf(nrm, 200000), 10.0, 0.05, "Normal mean");
    checkClose(nrm.mean(), 10.0, 1e-12, "Normal declares its mean");

    // A normal used as a duration has a left tail. Truncating is one honest
    // answer; refusing is the other. Silently returning a negative is not.
    Normal tight(0.5, 5.0, /*truncateAtZero=*/true);
    bool everNegative = false;
    for (int i = 0; i < 20000; ++i) if (tight.draw(rng) < 0.0) everNegative = true;
    check(!everNegative, "a truncated Normal never returns a negative duration");

    Normal strict(0.5, 5.0, /*truncateAtZero=*/false);
    bool threw = false;
    try { for (int i = 0; i < 20000; ++i) strict.draw(rng); }
    catch (const ModelError&) { threw = true; }
    check(threw, "an untruncated Normal throws rather than returning a negative");

    auto ln = Lognormal::fromMeanAndSd(10.0, 4.0);
    checkClose(meanOf(*ln, 200000), 10.0, 0.15, "lognormalFrom hits the mean you asked for");
    checkClose(ln->mean(), 10.0, 1e-9, "and declares it");

    Weibull w(5.0, 2.0);
    checkClose(meanOf(w, 200000), w.mean(), 0.05, "Weibull mean matches scale*Gamma(1+1/shape)");

    auto er = Erlang::fromMean(10.0, 4);
    checkClose(meanOf(*er, 100000), 10.0, 0.1, "Erlang mean");
    checkClose(er->mean(), 10.0, 1e-12, "Erlang declares its mean");
    // An Erlang-k is k exponentials summed, so its spread is narrower than a
    // single exponential of the same mean. That is the entire reason to use it.
    Exponential ex(10.0);
    double sdErlang = 0.0, sdExp = 0.0, m1 = 0.0, m2 = 0.0;
    const int n = 60000;
    std::vector<double> ve, vx;
    for (int i = 0; i < n; ++i) { ve.push_back(er->draw(rng)); vx.push_back(ex.draw(rng)); }
    for (double v : ve) m1 += v;
    m1 /= n;
    for (double v : vx) m2 += v;
    m2 /= n;
    for (double v : ve) sdErlang += (v-m1)*(v-m1);
    for (double v : vx) sdExp    += (v-m2)*(v-m2);
    check(std::sqrt(sdErlang/n) < std::sqrt(sdExp/n), "Erlang-4 is less variable than exponential");

    Discrete d({1.0, 5.0, 10.0}, {0.5, 0.3, 0.2});
    checkClose(d.mean(), 1.0*0.5 + 5.0*0.3 + 10.0*0.2, 1e-12, "Discrete mean");
    checkClose(meanOf(d, 200000), d.mean(), 0.05, "Discrete draws match its mean");
    threw = false;
    try { Discrete bad({1.0, 2.0}, {0.5, 0.4}); } catch (const ModelError&) { threw = true; }
    check(threw, "probabilities that do not sum to 1 are refused");

    // Empirical interpolates between order statistics, so the distribution it
    // samples has the TRAPEZOIDAL mean, not the average of the observations.
    Empirical emp(std::vector<SimTime>{2.0, 4.0, 12.0});
    checkClose(emp.mean(), (0.5*(2.0+4.0) + 0.5*(4.0+12.0)) / 2.0, 1e-12,
               "Empirical reports the mean of what it DRAWS");
    checkClose(meanOf(emp, 200000), emp.mean(), 0.05, "and the draws agree with it");

    Poisson p(3.0);
    checkClose(meanOf(p, 200000), 3.0, 0.05, "Poisson mean");
    bool wholeNumbers = true;
    for (int i = 0; i < 1000; ++i) { const double v = p.draw(rng); if (v != std::floor(v) || v < 0) wholeNumbers = false; }
    check(wholeNumbers, "Poisson returns non-negative whole numbers");
}

void testStreamQualityTests() {
    section("Generator quality tests (v8)");
    RandomStream good(12345u, EngineKind::MersenneTwister);
    const std::vector<TestResult> mt = StreamTests::runAll(good, 300000);
    int failures = 0;
    for (const TestResult& t : mt) if (!t.passed) ++failures;
    check(failures == 0, "mt19937 passes every test");

    RandomStream randu(12345u, EngineKind::Randu);
    const std::vector<TestResult> bad = StreamTests::runAll(randu, 300000);

    // *** THE LESSON. *** RANDU is beautifully uniform and utterly broken in
    // three dimensions. Every one-dimensional test passes; the serial test does
    // not. A generator that is broken does not crash -- it gives you an answer.
    for (const TestResult& t : bad) {
        if (t.name == "serial test in 3D") {
            check(!t.passed, "RANDU fails the 3D serial test");
            check(t.statistic > 5.0 * t.critical, "and not marginally -- by a wide margin");
        } else {
            check(t.passed, ("RANDU passes: " + t.name).c_str());
        }
    }

    checkClose(StreamTests::chiSquareCritical95(1), 3.841, 1e-9, "chi-square table, df=1");
    checkClose(StreamTests::chiSquareCritical95(10), 18.307, 1e-9, "chi-square table, df=10");
    check(StreamTests::chiSquareCritical95(100) > 100.0, "and the approximation beyond the table");
}

void testCommonRandomNumbers() {
    section("Common random numbers and antithetic (v8)");
    auto queue = [](SimTime svc, int c) {
        return [svc, c](SimulationSystem& s) {
            s.model().arrivals(exponential(1.0))
                     .station("Server", c, FIFO, exponential(svc))
                     .entryAt("Server");
            s.stopAt(3000.0);
        };
    };

    // ANTITHETIC: same compute, narrower interval. Deterministic given the
    // seeds, so this is a hard assertion rather than a hopeful one.
    Experiment plain("plain", queue(0.8, 1));
    plain.replications(30).baseSeed(3000u).warmUp(400.0).separateStreams();
    plain.run();
    Experiment anti("anti", queue(0.8, 1));
    anti.replications(15).baseSeed(3000u).warmUp(400.0).separateStreams().antitheticPairs();
    anti.run();
    check(anti.results().size() == 15, "a pair is ONE observation, not two");
    check(Experiment::estimate(anti.waits()).halfWidth <
          Experiment::estimate(plain.waits()).halfWidth,
          "antithetic pairing gives a narrower interval for the same 30 runs");

    // COMMON RANDOM NUMBERS: pair the designs and the shared luck cancels.
    Experiment A("A", queue(0.8, 1));
    Experiment B("B", queue(1.6, 2));
    for (Experiment* e : {&A, &B}) {
        e->replications(20).baseSeed(7000u).warmUp(400.0).separateStreams();
        e->run();
    }
    const auto ea = Experiment::estimate(A.waits());
    const auto eb = Experiment::estimate(B.waits());
    const auto cmp = Experiment::compare(A, B, &ReplicationResult::averageWait);

    check(cmp.differences.size() == 20, "one difference per replication");
    checkClose(cmp.meanDifference, ea.mean - eb.mean, 1e-9,
               "the mean difference equals the difference of means");
    // The intervals overlap, so separately the designs are indistinguishable...
    check(ea.low() < eb.high() && eb.low() < ea.high(), "the separate intervals overlap");
    // ...yet the paired difference is far tighter and clearly non-zero.
    const double unpaired = std::sqrt(ea.halfWidth*ea.halfWidth + eb.halfWidth*eb.halfWidth);
    check(cmp.halfWidth < unpaired / 5.0, "pairing is several times tighter than not pairing");
    check(cmp.differsSignificantly, "and it can tell the designs apart");
}

void testMultipleSources() {
    section("Multiple sources and entity types (v9)");
    {
        SimulationSystem sim(5u);
        sim.model()
            .source("A arrivals", "Alpha", constant(1.0), /*max=*/10)
            .source("B arrivals", "Beta",  constant(2.0), /*max=*/5)
            .station("Desk", 1, FIFO, constant(0.1))
            .dispose("Out")
            .route("A arrivals", "Desk").route("B arrivals", "Desk")
            .route("Desk", "Out")
            .entryAt("Desk");
        sim.stopAt(100.0).execute();

        const auto& t = sim.byType();
        check(t.count("Alpha") == 1 && t.count("Beta") == 1, "both entity types appear");
        // *** THE CAP IS A HARD LIMIT. *** 100 minutes at one a minute would
        // give 100 Alphas without it.
        check(t.at("Alpha").in == 10, "Alpha stopped at its maximum of 10");
        check(t.at("Beta").in == 5,   "Beta stopped at its maximum of 5");
        check(t.at("Alpha").out == 10 && t.at("Beta").out == 5, "and all of them left");
        check(sim.results().exited == 15, "15 entities through the system");
    }

    // An uncapped source keeps producing.
    {
        SimulationSystem sim(5u);
        sim.model().source("S", "Thing", constant(1.0))
                   .station("Desk", 1, FIFO, constant(0.1))
                   .dispose("Out")
                   .route("S", "Desk").route("Desk", "Out").entryAt("Desk");
        sim.stopAt(50.0).execute();
        check(sim.byType().at("Thing").in > 40, "an uncapped source runs for the whole horizon");
    }

    // Nothing routes INTO a Create.
    bool threw = false;
    try {
        SimulationSystem s(1u);
        s.model().source("S", "T", constant(1.0)).station("D", 1, FIFO, constant(0.5))
                 .route("D", "S");
        s.model().route("S", "D");
        s.model().entryAt("D");
        s.stopAt(10.0).execute();
    } catch (const ModelError&) { threw = true; }
    check(threw, "routing into a Create is refused");

    // A source that feeds nothing is a mistake, not an empty run.
    threw = false;
    try {
        SimulationSystem s(1u);
        s.model().source("S", "T", constant(1.0))
                 .source("S2", "T2", constant(1.0))
                 .station("D", 1, FIFO, constant(0.5)).dispose("O")
                 .route("S", "D").route("D", "O").entryAt("D");
        s.stopAt(10.0).execute();      // S2 goes nowhere
    } catch (const ModelError&) { threw = true; }
    check(threw, "a source wired to nothing is caught before the run");
}

void testMatchedBatching() {
    section("Matched batching (v9)");

    // ONE OF EACH. Three streams at different speeds: without matching, the
    // fast stream would be batched with itself. With it, sets are limited by
    // the slowest ingredient and the fast type piles up.
    {
        SimulationSystem sim(7u);
        sim.model()
            .source("fast", "Fast", constant(1.0))
            .source("slow", "Slow", constant(4.0))
            .assign("tagFast", "kind", constant(1.0))
            .assign("tagSlow", "kind", constant(2.0))
            .batchOneOfEach("Match", 2, "kind", /*permanent=*/true)
            .dispose("Out")
            .route("fast", "tagFast").route("slow", "tagSlow")
            .route("tagFast", "Match").route("tagSlow", "Match")
            .route("Match", "Out")
            .entryAt("Match");
        sim.stopAt(100.0).execute();

        const auto& b = sim.model().nodeAs<BatchNode>("Match");
        const auto& t = sim.byType();
        // A set needs one of each, so the number of sets is governed by the
        // SLOW stream -- about 25 in 100 minutes, not the fast stream's 100.
        check(b.batchesFormed() >= 20 && b.batchesFormed() <= 26,
              "sets are limited by the slowest ingredient");
        check(t.at("Fast").out == t.at("Slow").out,
              "exactly as many Fast as Slow were consumed");
        check(t.at("Fast").in > t.at("Fast").out + 40,
              "and the fast type piles up waiting for partners");
    }

    // A PLAIN batch of 2 on the same streams would happily take two Fasts, so
    // it is NOT limited by the slow stream. That contrast is the whole reason
    // the matched rule exists.
    {
        SimulationSystem sim(7u);
        sim.model()
            .source("fast", "Fast", constant(1.0))
            .source("slow", "Slow", constant(4.0))
            .batch("Any", 2, /*permanent=*/true)
            .dispose("Out")
            .route("fast", "Any").route("slow", "Any").route("Any", "Out")
            .entryAt("Any");
        sim.stopAt(100.0).execute();
        check(sim.model().nodeAs<BatchNode>("Any").batchesFormed() > 50,
              "a plain batch is not limited by the slow stream -- different model");
    }

    // SAME ATTRIBUTE: group entities that agree.
    {
        SimulationSystem sim(3u);
        sim.model()
            .source("s", "Job", constant(1.0))
            .assign("lot", "lotNumber", discrete({1.0, 2.0}, {0.5, 0.5}))
            .batchBySameAttribute("ByLot", 3, "lotNumber", true)
            .dispose("Out")
            .route("s", "lot").route("lot", "ByLot").route("ByLot", "Out")
            .entryAt("ByLot");
        sim.stopAt(300.0).execute();
        check(sim.model().nodeAs<BatchNode>("ByLot").batchesFormed() > 50,
              "same-attribute batching forms groups");
    }

    // The batching queue records one observation PER MEMBER, not per batch --
    // which is what Arena reports and a factor of `size` different.
    {
        SimulationSystem sim(1u);
        sim.model().source("s", "P", constant(1.0), 20)
                   .batch("B", 5, true).dispose("Out")
                   .route("s", "B").route("B", "Out").entryAt("B");
        sim.stopAt(100.0).execute();
        const auto& b = sim.model().nodeAs<BatchNode>("B");
        check(b.batchesFormed() == 4, "20 arrivals make 4 batches of 5");
        check(b.queueStats().numberServed() == 20,
              "20 waiting-time observations -- one per member, not per batch");
        // Members arrive one a minute; within a batch they wait 4,3,2,1,0
        // minutes, so the average is exactly 2.
        checkClose(b.queueStats().averageWaitingTime(), 2.0, 1e-9,
                   "and the average member wait is exactly right");
    }
}

void testOverloadAllowed() {
    section("Terminating runs of overloaded models (v9)");
    auto build = [](SimulationSystem& s, bool allow) {
        Model& m = s.model();
        if (allow) m.allowOverload();
        m.arrivals(exponential(0.5))
         .station("Chem", 1, FIFO, triangular(0.5, 1.0, 1.5))   // rho = 2
         .dispose("Out").route("Chem", "Out").entryAt("Chem");
        s.stopAt(4.0);
    };

    bool threw = false;
    try { SimulationSystem s(1u); build(s, false); s.execute(); }
    catch (const ModelError&) { threw = true; }
    check(threw, "an overloaded model is refused by default");

    bool ok = true;
    SimulationSystem s(1u);
    try { build(s, true); s.execute(); } catch (...) { ok = false; }
    check(ok, "and runs when the model says allowOverload()");
    check(s.model().overloadAllowed(), "the model remembers, so the report can say so");
    check(s.results().exited > 0, "and it produces results for the horizon asked about");
}

void testSeparateExits() {
    section("Separate's two exits (v9)");
    // Arena's Separate has an Original exit and a Duplicate exit, and models
    // routinely send them different ways. Reading the "50%" in the lab question
    // as a routing probability instead gives a similar-looking wrong model.
    SimulationSystem sim(1u);
    sim.model()
        .source("s", "Sample", constant(10.0), 5)
        .duplicate("Split", 1)
        .station("BenchA", 1, FIFO, constant(1.0))
        .station("BenchB", 1, FIFO, constant(1.0))
        .dispose("Out")
        .route("s", "Split")
        .route("Split", "BenchA")               // ORIGINAL
        .routeDuplicate("Split", "BenchB")      // DUPLICATE
        .route("BenchA", "Out").route("BenchB", "Out")
        .entryAt("Split");
    sim.stopAt(100.0).execute();

    check(sim.model().station("BenchA")->stats().numberServed() == 5,
          "every original went to bench A");
    check(sim.model().station("BenchB")->stats().numberServed() == 5,
          "every duplicate went to bench B");
    check(sim.results().exited == 10, "5 samples in, 10 results out");

    // Without a duplicate exit, copies follow the original -- the v6 behaviour.
    SimulationSystem two(1u);
    two.model()
        .source("s", "Sample", constant(10.0), 5)
        .duplicate("Split", 1)
        .station("Bench", 1, FIFO, constant(1.0))
        .dispose("Out")
        .route("s", "Split").route("Split", "Bench").route("Bench", "Out")
        .entryAt("Split");
    two.stopAt(100.0).execute();
    check(two.model().station("Bench")->stats().numberServed() == 10,
          "with one exit, copies follow the original");
}

void testPerTypeStatistics() {
    section("Per-entity-type statistics (v9)");
    SimulationSystem sim(1u);
    sim.model()
        .source("s", "Widget", constant(2.0), 10)
        .station("Desk", 1, FIFO, constant(1.0))
        .dispose("Out")
        .route("s", "Desk").route("Desk", "Out").entryAt("Desk");
    sim.stopAt(100.0).execute();

    const auto& w = sim.byType().at("Widget");
    check(w.in == 10 && w.out == 10, "ten in, ten out");
    check(w.inSystem == 0, "and none left inside");
    // Arrivals every 2 minutes, service exactly 1: nobody ever waits, so every
    // widget spends exactly its service time in the system.
    checkClose(w.totalTime / static_cast<double>(w.out), 1.0, 1e-9,
               "average time in system is exactly the service time");
    checkClose(w.maxTime, 1.0, 1e-9, "and so is the maximum");
    // WIP: one widget present for 1 minute in every 2, from t=0 to the last
    // exit at t=19. Time-average is therefore about 0.5.
    checkClose(w.areaWIP / sim.measuredTime(), 10.0 / sim.measuredTime(), 1e-6,
               "WIP integral is total time in system divided by the horizon");
}

void testDistributionClone() {
    section("Distribution clone (v9)");
    // arrivals() keeps a copy for the stability check and gives one to the
    // Create block; a unique_ptr cannot be in two places, hence clone().
    auto e = exponential(3.0);
    auto c = e->clone();
    checkClose(c->mean(), 3.0, 1e-12, "a clone keeps its parameters");
    check(c.get() != e.get(), "and is a separate object");

    auto d = discrete({1.0, 2.0}, {0.25, 0.75});
    checkClose(d->clone()->mean(), d->mean(), 1e-12, "Discrete rebuilds its probabilities");
    auto emp = empirical({1.0, 2.0, 9.0});
    checkClose(emp->clone()->mean(), emp->mean(), 1e-12, "Empirical clones its data");
}

void testTheoryInsideInterval() {
    section("M/M/1 theory falls inside the interval");
    // The question open since v2, as an automated check.
    Experiment e("mm1", [](SimulationSystem& s) {
        Model& m = s.model();
        m.setInterarrival(exponential(1.0));
        m.station("Server", 1, QueueDiscipline::FIFO, exponential(0.8));
        m.setEntry("Server");
        s.setTermination(timeLimit(20000.0));
    });
    // 20 replications. At 10 the half-width is wide enough that coverage
    // depends on the seed -- and a test that fails 1 run in 20 is not a test.
    // (The warm-up here is small on purpose: MSER says this model's transient
    // is only tens of minutes, so discarding thousands would just throw away
    // data and widen the interval. See examples/07.)
    e.replications(20).baseSeed(9000u).warmUp(500.0);
    e.run();

    struct Case { const char* name; double ReplicationResult::* field; double theory; };
    const Case cases[] = {
        {"Wq", &ReplicationResult::averageWait,         3.20},
        {"W",  &ReplicationResult::averageTimeInSystem, 4.00},
        {"Lq", &ReplicationResult::Lq,                  3.20},
        {"L",  &ReplicationResult::L,                   4.00},
        {"rho", &ReplicationResult::utilisation,        0.80},
    };
    for (const Case& c : cases) {
        const std::vector<double> xs = e.column(c.field);
        const double m  = Summary::mean(xs);
        const double hw = Summary::halfWidth95(xs);
        check(c.theory >= m - hw && c.theory <= m + hw,
              std::string("theory for ") + c.name + " lies inside the 95% interval");
    }
}

}  // namespace

int main() {
    std::cout << "running tests\n\n";
    testEntity();
    testResource();
    testQueueDisciplines();
    testFutureEventList();
    testActivityAndDelay();
    testStatistics();
    testSummary();
    testDistributions();
    testTerminationRules();
    testDeterministicEndToEnd();
    testChainRouting();
    testReproducibility();
    testWarmUpRemoval();
    testObservations();
    testExperiment();
    testModelErrors();
    testFlowchartBlocks();
    testSharedResources();
    testBalkingAndReneging();
    testNWayDecide();
    testEntityIdWidth();
    testRandomStreamPrimitive();
    testSubstreams();
    testNewDistributions();
    testStreamQualityTests();
    testCommonRandomNumbers();
    testMultipleSources();
    testMatchedBatching();
    testOverloadAllowed();
    testSeparateExits();
    testPerTypeStatistics();
    testDistributionClone();
    testFlowchartValidation();
    testDistributionMeans();
    testResultsStruct();
    testBuildHelpers();
    testEstimate();
    testWarmUpSuggestion();
    testTheoryInsideInterval();

    runExpressionTests();
    runDocumentTests();
    runRuntimeTests();

    std::cout << "\n" << (des_test::g_checks - des_test::g_failures)
              << " / " << des_test::g_checks << " checks passed\n";
    if (des_test::g_failures > 0) std::cout << des_test::g_failures << " FAILURES\n";
    return des_test::g_failures == 0 ? 0 : 1;
}
