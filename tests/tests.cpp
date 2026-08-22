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

using namespace des;

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "\n";
    }
}

void checkClose(double got, double want, double tol, const std::string& what) {
    ++g_checks;
    if (std::fabs(got - want) > tol) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "  (got " << got << ", want " << want << ")\n";
    }
}

void section(const char* name) { std::cout << "[" << name << "]\n"; }

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
    testFlowchartValidation();
    testDistributionMeans();
    testResultsStruct();
    testBuildHelpers();
    testEstimate();
    testWarmUpSuggestion();
    testTheoryInsideInterval();

    std::cout << "\n" << (g_checks - g_failures) << " / " << g_checks << " checks passed\n";
    if (g_failures > 0) std::cout << g_failures << " FAILURES\n";
    return g_failures == 0 ? 0 : 1;
}
