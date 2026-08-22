// ============================================================================
// 15 — The lab problems, solved
// ============================================================================
// Three Arena coursework problems, built with this engine. Each one is set up
// exactly as the question describes, run, and the answers printed.
//
// Everything is in MINUTES for problem 1 and HOURS for problem 3, matching each
// question. Pick a time unit, state it, and use it everywhere -- mixing them is
// the most common way a simulation answer comes out wrong by a factor of 60.
// ============================================================================

#include <iostream>
#include <iomanip>
#include "des.hpp"

using namespace des;

namespace {
void title(const char* t) {
    std::cout << "\n\n################################################################\n"
              << "  " << t << "\n"
              << "################################################################\n";
}
}  // namespace

// ============================================================================
// PROBLEM 1 -- gear manufacturing
//   Metal plates arrive Expo(0.5 min). The machine works only on batches of
//   FIVE plates; process time Triangular(0.5, 1, 1.5) min. After processing the
//   plates are separated and dispatched. Run for 4 hours.
//
//   Asked: queueing time at the batching station and at the machine; entities
//   leaving the machine and the system; scheduled utilization of the machine.
// ============================================================================
void problem1() {
    title("PROBLEM 1 -- gear manufacturing (batch of 5), 4 hours");

    SimulationSystem sim(12345u);
    sim.model()
        .arrivals(exponential(0.5))                       // minutes
        .batch("Batching Station", 5, /*permanent=*/true)
        .station("Gear Machine", 1, FIFO, triangular(0.5, 1.0, 1.5))
        .dispose("Dispatch")
        .route("Batching Station", "Gear Machine")
        .route("Gear Machine", "Dispatch")
        .entryAt("Batching Station");

    sim.stopAt(240.0);            // 4 hours in minutes
    sim.execute().reportArenaStyle();

    const auto& batch = sim.model().nodeAs<BatchNode>("Batching Station");
    const Station& machine = *sim.model().station("Gear Machine");
    const SimTime T = sim.measuredTime();

    std::cout << R"(
--- ANSWERS -------------------------------------------------------------)" << "\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Batching station queue, average waiting time : "
              << batch.queueStats().averageWaitingTime() << " min"
              << "   (" << batch.queueStats().numberServed() << " plates)\n";
    std::cout << "Gear machine queue, average waiting time     : "
              << machine.stats().averageWaitingTime() << " min"
              << "   (" << machine.stats().numberServed() << " batches)\n";
    std::cout << "Entities leaving the gear machine            : "
              << machine.stats().numberServed() << " batches\n";
    std::cout << "Entities leaving the system                  : "
              << sim.results().exited << "\n";
    std::cout << "Gear machine scheduled utilization           : "
              << machine.stats().utilisation(T, 1) << "\n";

    std::cout << R"(
--- reading this ---------------------------------------------------------
THE BATCHING QUEUE IS WHERE ALL THE WAITING IS. A plate waits for four
companions before the machine even sees it, and that dominates the machine
queue by two orders of magnitude. Note the OBSERVATION COUNTS differ: the
batching queue records one observation per PLATE, the machine queue one per
BATCH -- five times fewer. Comparing the two averages without noticing that
is comparing different things.

*** A MODELLING TRAP IN THE QUESTION. ***
The words say the plates "are separated and are dispatched". That means a
TEMPORARY batch plus a Separate block, and roughly five times as many
entities leaving the system. But a PERMANENT batch -- which is what the
reference Arena model used -- consumes its members, so Separate has nothing
to split and quietly does nothing: one entity leaves per batch.

Both are defensible readings of an ambiguous question. What is NOT
defensible is not noticing which one you built. The two answers to "how
many entities leave the system" differ by a factor of five.

Below is the same model with a temporary batch, so you can see both.
)";

    SimulationSystem alt(12345u);
    alt.model()
        .arrivals(exponential(0.5))
        .batch("Batching Station", 5, /*permanent=*/false)   // TEMPORARY
        .station("Gear Machine", 1, FIFO, triangular(0.5, 1.0, 1.5))
        .separate("Separate Plates")                          // split it back
        .dispose("Dispatch")
        .route("Batching Station", "Gear Machine")
        .route("Gear Machine", "Separate Plates")
        .route("Separate Plates", "Dispatch")
        .entryAt("Batching Station");
    alt.stopAt(240.0).execute();
    std::cout << "  permanent batch : " << sim.results().exited
              << " entities leave the system\n";
    std::cout << "  temporary batch : " << alt.results().exited
              << " entities leave the system (the plates themselves)\n";
}

// ============================================================================
// PROBLEM 3 -- urine sample testing
//   Samples arrive Expo(0.5 hr). Chemicals are added: Tri(0.5, 1, 1.5) hr.
//   The sample is split in two, 50% to a duplicate. Both are tested in two
//   different settings, each Tri(0.5, 1, 1.5) hr. Finally dispatched at one
//   station. Run 4 hours.
//
//   Asked: queueing time and scheduled utilization for the three processes;
//   number of test results leaving the system.
// ============================================================================
void problem3() {
    title("PROBLEM 3 -- urine sample testing, 4 hours");

    std::cout << R"(*** THIS MODEL IS OVERLOADED, AND THAT IS THE INTERESTING PART. ***
Samples arrive every 0.5 hours on average, so 2 per hour. Adding chemicals
takes 1 hour on average with ONE station, so rho = 2.0. Work arrives twice
as fast as it can be done and the queue grows for as long as you run.

The engine refuses such a model by default, because for a STEADY-STATE
study "average wait" would be a function of run length rather than a
property of the system. Here the question is a TERMINATING one -- run for
four hours and report what happened -- which is a perfectly good question
about an overloaded shift. allowOverload() says so out loud, and the report
carries the caveat.
)";

    SimulationSystem sim(12345u);
    sim.model()
        .allowOverload()                                  // rho = 2 at chemicals
        .arrivals(exponential(0.5))                       // hours
        .station("Adding Chemicals", 1, FIFO, triangular(0.5, 1.0, 1.5))
        .duplicate("Separate Samples", 1)                 // original + 1 duplicate
        .station("Sample Process 1", 1, FIFO, triangular(0.5, 1.0, 1.5))
        .station("Sample Process 2", 1, FIFO, triangular(0.5, 1.0, 1.5))
        .dispose("Samples Exit")
        .route("Adding Chemicals", "Separate Samples")
        // *** The "50%" in the question is Arena's PERCENT COST TO DUPLICATES,
        // not a routing probability. *** It splits the cost of the work between
        // the original and its copy; it does not decide where either one goes.
        // The original goes to one test bench and the duplicate to the other --
        // two exits, not a coin flip. Reading it as 50/50 routing gives a model
        // that looks similar and is not the one described.
        .route("Separate Samples", "Sample Process 1")      // ORIGINAL exit
        .routeDuplicate("Separate Samples", "Sample Process 2")   // DUPLICATE exit
        .route("Sample Process 1", "Samples Exit")
        .route("Sample Process 2", "Samples Exit")
        .entryAt("Adding Chemicals");

    sim.stopAt(4.0);              // 4 hours
    sim.execute().reportArenaStyle();

    const SimTime T = sim.measuredTime();
    const Station& chem = *sim.model().station("Adding Chemicals");
    const Station& p1   = *sim.model().station("Sample Process 1");
    const Station& p2   = *sim.model().station("Sample Process 2");

    std::cout << std::fixed << std::setprecision(5);
    std::cout << "\n--- ANSWERS -------------------------------------------------------------\n";
    std::cout << "                        queue wait (hr)   scheduled utilization\n";
    auto row = [&](const char* n, const Station& s) {
        std::cout << "  " << std::setw(20) << std::left << n << std::right
                  << std::setw(14) << s.stats().averageWaitingTime()
                  << std::setw(22) << s.stats().utilisation(T, s.resource().capacity()) << "\n";
    };
    row("Adding Chemicals", chem);
    row("Sample Process 1", p1);
    row("Sample Process 2", p2);
    std::cout << "\nTest results leaving the system : " << sim.results().exited << "\n";

    std::cout << R"(
--- reading this ---------------------------------------------------------
ADDING CHEMICALS IS PINNED AT UTILIZATION 1.0 and its queue grows all
afternoon. That is not a simulation artefact, it is the answer: with one
station at this arrival rate the front of the process cannot cope, and
everything downstream is starved by it.

The two test settings sit near 0.7 each, and they are NOT the bottleneck.
Buying a third tester would change nothing. Adding a second chemicals
station is the only thing that would.

TEST RESULTS ARE ROUGHLY TWICE THE SAMPLES that get through, because
Separate duplicates each one. Every sample that clears chemicals produces
two results.

AND THE NUMBERS DEPEND ON THE RUN LENGTH. Run it for 8 hours instead of 4
and the chemicals queue will be twice as long. For an overloaded system
that is expected -- there is no steady state to converge to.
)";
}

// ============================================================================
// PROBLEM 5 -- matched batching of three ball types
//   Baseball, Basketball and Football each arrive Expo(1). A maximum of 1000
//   of each enters. Batches of THREE, one of each type, are made at a station
//   and dispatched. Run 4 hours.
//
//   Asked: WIP and queueing time at the batching station; numbers of each type
//   entering and leaving.
// ============================================================================
void problem5() {
    title("PROBLEM 5 -- one of each: matched batching, 4 hours");

    SimulationSystem sim(2024u);
    Model& m = sim.model();

    // THREE separate sources, each capped at 1000. A single arrival stream
    // cannot express this: the three types arrive independently.
    m.source("Baseball Arrivals",   "Baseball",   exponential(1.0), 1000)
     .source("Basketball Arrivals", "Basketball", exponential(1.0), 1000)
     .source("Football Arrivals",   "Football",   exponential(1.0), 1000)

    // *** ONE OF EACH. ***
    // Each ball carries a numeric type, and the batch needs three DIFFERENT
    // values. A plain batch of three would happily take three baseballs.
     .batchOneOfEach("Batching Station", 3, "ballType", /*permanent=*/true)
     .dispose("Dispatched")
     .route("Batching Station", "Dispatched")
     .entryAt("Batching Station");

    // Stamp the type number on each ball as it arrives. Assign blocks sit
    // between each source and the batching station.
    m.assign("Tag Baseball",   "ballType", constant(1.0))
     .assign("Tag Basketball", "ballType", constant(2.0))
     .assign("Tag Football",   "ballType", constant(3.0))
     .route("Baseball Arrivals",   "Tag Baseball")
     .route("Basketball Arrivals", "Tag Basketball")
     .route("Football Arrivals",   "Tag Football")
     .route("Tag Baseball",   "Batching Station")
     .route("Tag Basketball", "Batching Station")
     .route("Tag Football",   "Batching Station");

    sim.stopAt(240.0);            // 4 hours in minutes
    sim.execute().reportArenaStyle();

    const auto& batch = sim.model().nodeAs<BatchNode>("Batching Station");
    const RunResults r = sim.results();

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\n--- ANSWERS -------------------------------------------------------------\n";
    std::cout << "Batching station, average waiting time : "
              << batch.queueStats().averageWaitingTime() << " min\n";
    std::cout << "Batching station, WIP (time-average number in system) : "
              << r.averageNumberInSystem << "\n";
    std::cout << "Batching station, longest queue        : " << batch.maxQueueLength() << "\n";
    std::cout << "Sets of three dispatched              : " << batch.batchesFormed() << "\n\n";
    std::cout << "  type          entering   leaving\n";
    for (const auto& kv : sim.byType())
        std::cout << "  " << std::setw(12) << std::left << kv.first << std::right
                  << std::setw(10) << kv.second.in << std::setw(10) << kv.second.out << "\n";

    std::cout << R"(
--- reading this ---------------------------------------------------------
ALL THREE TYPES ARRIVE AT THE SAME RATE, so the batching station stays
roughly balanced and no type builds up badly. Change one stream to
Expo(0.5) and watch what happens: the two slower types are consumed as
fast as they arrive while the fast one piles up, because every set needs
one of each and the set can only go as fast as its slowest ingredient.

THAT IS THE WHOLE POINT OF MATCHED BATCHING. A plain batch of three would
take the first three balls to show up -- often three of the same kind --
and would never stall. It would also be modelling a different system.

The 1000-per-type cap never binds here: at one per minute for 240 minutes
only about 240 of each arrive. Shorten the run or speed up the streams and
the cap starts to matter.
)";
}

int main() {
    problem1();
    problem3();
    problem5();
    return 0;
}
