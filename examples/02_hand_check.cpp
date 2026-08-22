// ============================================================================
// 02 — Proving the simulator is right, by hand.
// ============================================================================
// Random numbers make a simulation impossible to check by inspection. So don't
// use them: `Deterministic` hands out a fixed list of values in order, which
// turns the whole run into arithmetic you can do on paper.
//
// This is the single most useful technique in the engine for coursework. Build
// a tiny deterministic version of your model FIRST, verify it by hand, and only
// then switch the distributions to random ones. If the deterministic version is
// wrong, no amount of statistics on the random version will save you.
// ============================================================================

#include <iostream>
#include <memory>
#include <vector>
#include "des.hpp"

using namespace des;

int main() {
    SimulationSystem sim(1u);   // seed is irrelevant: nothing here is random
    Model& m = sim.model();

    // Interarrival times, in order: the 1st customer arrives at t=0, the next
    // 2 minutes later, the next 4 after that, and so on.
    m.setInterarrival(std::make_unique<Deterministic>(
        std::vector<SimTime>{2, 4, 1, 3, 5}));

    // Service times, in order, for the 1st, 2nd, 3rd... customer SERVED.
    m.addStation("Server", 1, QueueDiscipline::FIFO,
                 std::make_unique<Deterministic>(
                     std::vector<SimTime>{3, 2, 4, 1, 2}));
    m.setEntry("Server");

    // Stop after 5 customers have LEFT. EntityLimit counts exits, not arrivals.
    sim.setTermination(entityLimit(5));

    // Write every event to a file you can read.
    sim.enableTrace("trace_02.md", TraceLevel::Events);

    sim.initialise();
    sim.run();
    sim.report();

    std::cout << R"(
--- the same run, worked by hand ---------------------------------------
  t=0   C1 arrives, server free   -> service 3, leaves at t=3
  t=2   C2 arrives, server busy   -> joins the queue
  t=3   C1 leaves (waited 0)      -> C2 starts, waited 1, service 2
  t=5   C2 leaves (waited 1)
  t=6   C3 arrives, server free   -> service 4, leaves at t=10
  t=7   C4 arrives, server busy   -> joins the queue
  t=10  C3 leaves (waited 0)      -> C4 starts, waited 3, service 1
        C5 also ARRIVES at t=10. Two events at the same instant: the
        departure was scheduled first, so it happens first. That tie-break
        is why two runs with the same seed give identical answers.
  t=11  C4 leaves (waited 3)      -> C5 starts, waited 1, service 2
  t=13  C5 leaves (waited 1)

  5 served. Waits 0, 1, 0, 3, 1 -> average 1.0000, maximum 3.0000.
  Last exit at t = 13.0000.
------------------------------------------------------------------------
Compare those to the report above, then open trace_02.md and compare it
line by line. If they match, you can trust the model you build next.
)";
    return 0;
}
