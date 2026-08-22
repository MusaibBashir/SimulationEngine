// ============================================================================
// 01 — Hello, queue.  The smallest useful program.
// ============================================================================
// One server, customers arriving at random, first-come-first-served.
// Read this one first; every other example is this shape with more parts.
// ============================================================================

#include <iostream>
#include "des.hpp"

using namespace des;   // everything lives in namespace des

int main() {
    // The engine. The seed makes the run REPRODUCIBLE -- same seed, same
    // numbers, every time. Change it to get a different sample.
    SimulationSystem sim(12345u);

    // Describe the system. The engine knows nothing about tellers or customers;
    // it only knows what you put in the model.
    sim.model()
       .arrivals(exponential(1.0))                       // one every ~1.0 min
       .station("Teller", /*capacity=*/1, FIFO,
                exponential(0.8))                        // service ~0.8 min
       .entryAt("Teller");                               // where arrivals land

    // *** exponential() takes the MEAN, not the rate. ***
    // exponential(0.8) means "0.8 minutes on average". Passing 1.25 because
    // "the rate is 1.25/min" gives a model that is wrong and runs happily.

    // Stop after an 8-hour day of SIMULATED time. It runs in milliseconds,
    // because the clock jumps from event to event and nothing is computed
    // in between.
    sim.stopAt(480.0);

    // execute() is initialise() then run(). Then print.
    sim.execute().report();

    // ------------------------------------------------------------------
    // Getting numbers out programmatically: results() hands you everything
    // with the divisions already done.
    // ------------------------------------------------------------------
    const RunResults r = sim.results();
    std::cout << "\ncustomers served      : " << r.exited << "\n"
              << "average wait          : " << r.averageWait << " min\n"
              << "longest anyone waited : " << r.maxWait << " min\n"
              << "teller utilisation    : " << r.station("Teller").utilisation << "\n"
              << "longest the queue got : " << r.station("Teller").maxQueueLength << "\n";

    std::cout << R"(
Utilisation lands near 0.80 because rho = 0.8 / 1.0 = 0.8: the server is
busy 80% of the time.

THAT NUMBER IS THE ONE TO CHECK BEFORE ANY MODEL RUNS.

    rho = mean service time / (capacity * mean interarrival time)

If rho >= 1, work arrives faster than it can be done, the queue grows for
as long as you run, and every average is a function of run length rather
than a property of the system. Try changing the service time to 1.4 and
re-running: the engine refuses, with a message saying why. It used to run
and print confident nonsense.
)";
    return 0;
}
