// ============================================================================
// 05 — A chain of stations, and finding the bottleneck.
// ============================================================================
// Real systems are rarely one queue. A restaurant is at least three:
//
//     arrive -> Host (seat you) -> Waiters (feed you) -> Cashier -> leave
//
// `connect(from, to)` routes entities from one station to the next. A station
// with nothing connected after it is an exit -- that is how entities leave.
//
// Each station gets its OWN capacity, queue discipline and service time, and
// its own statistics. That last part is the point: a system-wide average would
// hide which station is the problem, and the whole reason to build the model is
// to find out.
// ============================================================================

#include <iostream>
#include <memory>
#include "SimulationSystem.hpp"

int main() {
    SimulationSystem sim(7u);
    Model& m = sim.model();

    // A party arrives every ~15 minutes.
    m.setInterarrival(std::make_unique<Exponential>(15.0));

    // One host, seating takes ~2 min.
    m.addStation("Host", 1, QueueDiscipline::FIFO,
                 std::make_unique<Exponential>(2.0));

    // Three waiters. A meal takes 20 to 60 minutes, most often 35 --
    // Triangular is the right shape when you have a plausible minimum, maximum
    // and most-likely value but no data. Which is most coursework.
    m.addStation("Waiters", 3, QueueDiscipline::FIFO,
                 std::make_unique<Triangular>(20.0, 35.0, 60.0));

    // One cashier, 1 to 4 minutes, uniformly.
    m.addStation("Cashier", 1, QueueDiscipline::FIFO,
                 std::make_unique<Uniform>(1.0, 4.0));

    // The routing. Without these two lines you have three unconnected queues
    // and every entity would leave straight after being seated.
    m.connect("Host", "Waiters");
    m.connect("Waiters", "Cashier");
    m.setEntry("Host");

    // *** DO THIS ARITHMETIC BEFORE YOU RUN ANYTHING. ***
    // Waiters: mean meal (20+35+60)/3 = 38.3 min, shared over 3 waiters
    //          = 12.8 min of waiter-work per party.
    // Parties arrive every 15 min. rho = 12.8 / 15 = 0.85. Stable, but busy.
    // Change arrivals to every 10 min and rho = 1.28: the queue grows without
    // bound forever and every number below becomes meaningless -- while still
    // being printed to four decimal places.

    sim.setTermination(std::make_unique<TimeLimit>(6000.0));
    sim.setWarmUp(600.0);
    sim.enableTrace("trace_05.md", TraceLevel::Events);

    sim.initialise();
    sim.run();
    sim.report();

    std::cout << R"(
--- reading the per-station table ---------------------------------------
Look at the `util` column. Host and Cashier sit around 13-16%; Waiters sit
around 82%. The waiters are the BOTTLENECK, and everything else is idle
capacity.

That tells you where to spend money. Hiring a second cashier would improve
nothing at all -- the cashier is already idle 84% of the time. Hiring a
fourth waiter is the only change that would move the customer's experience.

Now look at `avg wait` per station. Almost the entire customer wait is
accumulated at one station. A single system-wide "average wait" number
would be perfectly accurate and completely useless for deciding anything.

Open trace_05.md to watch a single party move Host -> Waiters -> Cashier.
--------------------------------------------------------------------------
)";
    return 0;
}
