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
#include "SimulationSystem.hpp"
#include "Activity.hpp"

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
    EventNotice::resetSequenceCounter();
    fel.schedule(EventNotice(EventType::Arrival,   5.0));
    fel.schedule(EventNotice(EventType::Departure, 5.0));
    fel.schedule(EventNotice(EventType::StartService, 5.0));
    check(fel.popImminent().type() == EventType::Arrival,      "tie 1: scheduling order");
    check(fel.popImminent().type() == EventType::Departure,    "tie 2: scheduling order");
    check(fel.popImminent().type() == EventType::StartService, "tie 3: scheduling order");

    fel.schedule(EventNotice(EventType::Arrival, 1.0));
    fel.clear();
    check(fel.isEmpty(), "clear empties the list");
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
    checkClose(s.areaUnderQueueLength(), 10.0, 1e-12, "queue-length integral");
    checkClose(s.areaUnderServerBusy(), 10.0, 1e-12, "busy integral");
    checkClose(s.timeAverageQueueLength(10.0), 1.0, 1e-12, "time-average queue length");
    checkClose(s.serverUtilisation(10.0, 1), 1.0, 1e-12, "utilisation");

    s.recordArrival(0.0);
    s.recordDeparture(10.0, 4.0, 6.0);
    s.recordDeparture(12.0, 2.0, 4.0);
    check(s.numberArrived() == 1 && s.numberServed() == 2, "counters");
    checkClose(s.averageWaitingTime(), 3.0, 1e-12, "average wait");
    checkClose(s.averageTimeInSystem(), 5.0, 1e-12, "average time in system");
    checkClose(s.maxWaitingTime(), 4.0, 1e-12, "max wait");

    s.reset();
    check(s.numberServed() == 0 && s.areaUnderQueueLength() == 0.0, "reset zeroes everything");
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
    m.setInterarrival(std::make_unique<Constant>(1.0));
    m.addStation("S", 1, QueueDiscipline::FIFO, std::make_unique<Constant>(0.5));
    m.setEntry("S");
    sim.setTermination(std::make_unique<EntityLimit>(10));
    sim.initialise();
    sim.run();
    check(sim.statistics().numberServed() >= 10, "EntityLimit stops after N exits");

    SimulationSystem sim2(5u);
    Model& m2 = sim2.model();
    m2.setInterarrival(std::make_unique<Constant>(1.0));
    m2.addStation("S", 1, QueueDiscipline::FIFO, std::make_unique<Constant>(0.5));
    m2.setEntry("S");
    sim2.setTermination(std::make_unique<TimeLimit>(20.0));
    sim2.initialise();
    sim2.run();
    check(sim2.clock().now() >= 20.0, "TimeLimit stops at the time limit");

    auto any = std::make_unique<AnyOf>();
    check(any->empty(), "an empty AnyOf reports empty");
    any->add(std::make_unique<TimeLimit>(1000.0));
    any->add(std::make_unique<EntityLimit>(5));
    check(!any->empty(), "AnyOf holds its children");
}

void testDeterministicEndToEnd() {
    section("Deterministic end-to-end (hand-worked)");
    // Same model as scenario 3 in main. Worked by hand:
    //   5 served, waits 0,1,0,3,1 -> average 1.0, last exit at t = 13.
    SimulationSystem sim(1u);
    Model& m = sim.model();
    m.setInterarrival(std::make_unique<Deterministic>(std::vector<SimTime>{2, 4, 1, 3, 5}));
    m.addStation("Server", 1, QueueDiscipline::FIFO,
                 std::make_unique<Deterministic>(std::vector<SimTime>{3, 2, 4, 1, 2}));
    m.setEntry("Server");
    sim.setTermination(std::make_unique<EntityLimit>(5));
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
    m.setInterarrival(std::make_unique<Constant>(100.0));   // one arrival, then far apart
    m.addStation("A", 1, QueueDiscipline::FIFO, std::make_unique<Constant>(2.0));
    m.addStation("B", 1, QueueDiscipline::FIFO, std::make_unique<Constant>(3.0));
    m.connect("A", "B");
    m.setEntry("A");
    sim.setTermination(std::make_unique<EntityLimit>(1));
    sim.initialise();
    sim.run();

    check(sim.model().station("A")->stats().numberServed() == 1, "station A served it");
    check(sim.model().station("B")->stats().numberServed() == 1, "station B served it too");
    checkClose(sim.statistics().averageTimeInSystem(), 5.0, 1e-9,
               "time in system is 2 + 3 across both stations");
    check(sim.model().station("A")->next() == sim.model().station("B"), "A routes to B");
    check(sim.model().station("B")->isExit(), "B is the exit");
}

void testReproducibility() {
    section("Reproducibility");
    auto build = [](SimulationSystem& s) {
        Model& m = s.model();
        m.setInterarrival(std::make_unique<Exponential>(1.0));
        m.addStation("S", 2, QueueDiscipline::FIFO, std::make_unique<Exponential>(1.5));
        m.setEntry("S");
        s.setTermination(std::make_unique<TimeLimit>(500.0));
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

}  // namespace

int main() {
    std::cout << "running tests\n\n";
    testEntity();
    testResource();
    testQueueDisciplines();
    testFutureEventList();
    testActivityAndDelay();
    testStatistics();
    testDistributions();
    testTerminationRules();
    testDeterministicEndToEnd();
    testChainRouting();
    testReproducibility();

    std::cout << "\n" << (g_checks - g_failures) << " / " << g_checks << " checks passed\n";
    if (g_failures > 0) std::cout << g_failures << " FAILURES\n";
    return g_failures == 0 ? 0 : 1;
}
