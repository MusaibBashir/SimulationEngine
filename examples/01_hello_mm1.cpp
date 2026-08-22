// ============================================================================
// 01 — Hello, queue.  The smallest useful program.
// ============================================================================
// One server, customers arriving at random, first-come-first-served.
// If you read one example, read this one: every other example is this shape
// with more parts.
//
// The five steps never change:
//   1. make a SimulationSystem (the engine) with a seed
//   2. describe your system in its Model
//   3. say when to stop
//   4. initialise() then run()
//   5. report()
// ============================================================================

#include <iostream>
#include <memory>
#include "SimulationSystem.hpp"

int main() {
    // 1. The engine. The seed makes the run REPRODUCIBLE -- same seed, same
    //    numbers, every time. Change it to get a different sample.
    SimulationSystem sim(/*seed=*/12345u);

    // 2. The model: what you are simulating. The engine knows nothing about
    //    tellers or customers; it only knows what you put in here.
    Model& m = sim.model();

    //    Customers arrive on average every 1.0 minutes.
    //    *** Exponential takes the MEAN, not the rate. *** Exponential(1.0)
    //    means "one every minute on average", not "one per minute rate = 1".
    //    For the exponential they happen to be reciprocals, so this is the
    //    single easiest thing in queueing to get backwards.
    m.setInterarrival(std::make_unique<Exponential>(1.0));

    //    One station called "Teller": capacity 1 (one server), first-in
    //    first-out queue, service taking 0.8 minutes on average.
    m.addStation("Teller", /*capacity=*/1, QueueDiscipline::FIFO,
                 std::make_unique<Exponential>(0.8));

    //    Where arrivals land. With one station this is optional -- the first
    //    station added becomes the entry by default -- but be explicit.
    m.setEntry("Teller");

    // 3. Stop after 8 hours of simulated time. Simulated: this runs in
    //    milliseconds of real time, because the clock JUMPS from event to
    //    event and nothing is computed in between.
    sim.setTermination(std::make_unique<TimeLimit>(480.0));

    // 4. initialise() resets everything and schedules the first arrival.
    //    run() processes events until the termination rule says stop.
    sim.initialise();
    sim.run();

    // 5. Print what happened.
    sim.report();

    // You can also pull individual numbers out.
    std::cout << "\nutilisation was "
              << sim.model().station("Teller")->stats()
                    .utilisation(sim.measuredTime(), 1)
              << " -- with arrivals every 1.0 min and service taking 0.8,\n"
                 "the server is busy 80% of the time. rho = 0.8 / 1.0 = 0.8.\n";

    // A WARNING YOU WILL NEED: if service is SLOWER than arrivals (rho >= 1)
    // the queue grows forever and every average is meaningless. The simulator
    // will happily run it and report confident numbers. Always compute rho by
    // hand before trusting anything.
    return 0;
}
