// ============================================================================
// 10 — Stopping rules, tracing, priorities, and reproducibility.
// ============================================================================
// The remaining pieces, in one file:
//   * the four termination rules, and how to combine them
//   * both trace formats and what they are for
//   * the Priority discipline, driven by an assigned attribute
//   * seeds: why the same seed twice is a feature, not a limitation
// ============================================================================

#include <iostream>
#include <iomanip>
#include <memory>
#include "des.hpp"

using namespace des;

namespace {

void build(SimulationSystem& s, QueueDiscipline rule = QueueDiscipline::FIFO) {
    Model& m = s.model();
    m.setInterarrival(exponential(2.0));
    m.station("Desk", 1, rule, exponential(1.6));
    m.setEntry("Desk");
}

void showStop(const char* label, std::unique_ptr<ITerminationRule> rule) {
    SimulationSystem sim(77u);
    build(sim);
    sim.setTermination(std::move(rule));
    sim.initialise();
    sim.run();
    std::cout << "  " << std::setw(34) << std::left << label << std::right
              << "stopped at t=" << std::setw(10) << sim.clock().now()
              << " after " << std::setw(6) << sim.statistics().numberServed()
              << " served\n";
}

}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(2);

    // --- 1. the four termination rules ------------------------------------
    std::cout << "--- termination rules ---------------------------------------\n";

    // Stop at a clock time. The usual choice for a steady-state study.
    showStop("TimeLimit(500)", timeLimit(500.0));

    // Stop after N entities have LEFT the system. Use this when you want the
    // same amount of DATA from every run rather than the same duration.
    showStop("EntityLimit(100)", entityLimit(100));

    // Stop when the system empties. This is how you model a TERMINATING system
    // -- a clinic that closes, a batch of jobs that finishes -- as opposed to a
    // steady-state one. Note it needs something else alongside it in practice,
    // because a system fed by endless arrivals may never empty.
    {
        auto any = std::make_unique<AnyOf>();
        any->add(whenDrained());
        any->add(timeLimit(10000.0));   // safety net
        showStop("AnyOf[Drained | TimeLimit(10000)]", std::move(any));
    }

    // AnyOf stops when ANY child stops. Combine freely.
    {
        auto any = std::make_unique<AnyOf>();
        any->add(timeLimit(500.0));
        any->add(entityLimit(50));
        showStop("AnyOf[TimeLimit(500) | Entity(50)]", std::move(any));
    }

    // --- 2. tracing --------------------------------------------------------
    std::cout << "\n--- tracing --------------------------------------------------\n";
    {
        SimulationSystem sim(3u);
        build(sim);
        sim.setTermination(entityLimit(12));

        // markdown=true  -> a table that renders in a report or on GitHub
        // markdown=false -> aligned plain text, easier to diff and grep
        sim.enableTrace("trace_10.md", TraceLevel::Events, /*markdown=*/true);

        sim.initialise();
        sim.run();
        std::cout << "  trace_10.md written -- one row per event.\n";
        std::cout << "  TraceLevel::Off costs nothing: no file is opened and every\n"
                     "  trace call returns immediately, so leave the calls in place.\n";
        std::cout << "  Traces DIFF. Trace a run before a change and after it; an\n"
                     "  empty diff proves the behaviour is identical, which is far\n"
                     "  stronger than 'the averages still look about right'.\n";
    }

    // --- 3. priority, driven by an assigned attribute ----------------------
    std::cout << "\n--- priority ------------------------------------------------\n";
    {
        SimulationSystem sim(5u);
        Model& m = sim.model();
        m.setInterarrival(exponential(2.0));

        // QueueDiscipline::Priority serves the HIGHEST value of an attribute
        // called exactly "priority". Something must put it there:
        m.assignOnArrival("priority", uniform(1.0, 5.0));

        m.station("Desk", 1, QueueDiscipline::Priority,
                     exponential(1.6));
        m.setEntry("Desk");
        sim.setTermination(timeLimit(5000.0));
        sim.setWarmUp(500.0);
        sim.initialise();
        sim.run();

        std::cout << "  mean wait " << sim.statistics().averageWaitingTime()
                  << ", MAX wait " << sim.statistics().maxWaitingTime() << "\n";
        std::cout << "  Priority barely changes the MEAN -- reordering a queue does\n"
                     "  not change how much work is in it. What it changes is who\n"
                     "  waits. Compare the max against FIFO: low-priority customers\n"
                     "  pay for the high-priority ones, and in a busy system they can\n"
                     "  be starved indefinitely. Always report the max, not just the\n"
                     "  mean, when you use a priority rule.\n";
    }

    // --- 4. when you get the model wrong ----------------------------------
    // Mistakes YOU make are reported as ModelError exceptions, not asserts.
    // That matters: asserts vanish in a release build (-DNDEBUG), so a typo'd
    // station name would have silently done nothing and the model would have
    // run with entities leaving early. Exceptions are checked in every build.
    std::cout << "\n--- model errors ---------------------------------------------\n";
    {
        // (a) a system that cannot keep up
        try {
            SimulationSystem bad(1u);
            bad.model().arrivals(exponential(1.0))
                       .station("Desk", 1, FIFO, exponential(1.4))
                       .entryAt("Desk");
            bad.stopAt(1000.0).execute();
        } catch (const ModelError& e) {
            std::cout << "  caught: " << e.what() << "\n\n";
        }

        // (b) a station name that does not exist
        try {
            SimulationSystem bad(1u);
            bad.model().arrivals(exponential(2.0))
                       .station("Desk", 1, FIFO, exponential(1.0))
                       .route("Desk", "Cashier");     // never added
        } catch (const ModelError& e) {
            std::cout << "  caught: " << e.what() << "\n";
        }

        // (c) forgetting to say where arrivals go is caught the same way, as is
        // a routing loop, a duplicate station name, and using a reserved
        // attribute name. All at initialise(), before any events run.
    }

    // --- 5. seeds and reproducibility --------------------------------------
    std::cout << "\n--- seeds ----------------------------------------------------\n";
    {
        SimulationSystem a(999u); build(a);
        a.setTermination(timeLimit(2000.0));
        a.initialise(); a.run();

        SimulationSystem b(999u); build(b);
        b.setTermination(timeLimit(2000.0));
        b.initialise(); b.run();

        SimulationSystem c(1000u); build(c);
        c.setTermination(timeLimit(2000.0));
        c.initialise(); c.run();

        std::cout << "  seed 999  : Wq = " << a.statistics().averageWaitingTime() << "\n";
        std::cout << "  seed 999  : Wq = " << b.statistics().averageWaitingTime()
                  << "   <- identical, by design\n";
        std::cout << "  seed 1000 : Wq = " << c.statistics().averageWaitingTime()
                  << "   <- a different sample of the same system\n";
        std::cout << R"(
  Same seed reproducing exactly is what makes a bug findable: you can add a
  trace, re-run, and see the identical sequence of events.

  It is also the trap. Different seeds give different answers, so ONE seed
  is one sample -- see example 08. Never pick a seed because it gave you a
  number you liked; that is choosing your data.

  You can also re-run the same object: initialise() resets the clock,
  statistics, queues, servers and event list, so calling initialise() and
  run() again gives the identical result rather than continuing.
)";
    }
    return 0;
}
