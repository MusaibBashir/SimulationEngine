// ============================================================================
// 08 — One run is one sample. Report an interval, not a number.
// ============================================================================
// *** IF YOU TAKE ONE THING FROM THESE EXAMPLES, TAKE THIS ONE. ***
//
// A simulation run is a random experiment. Running it once and writing down
// "Wq = 3.2864" is exactly like flipping a coin ten times, getting six heads,
// and reporting "P(heads) = 0.6000". The number is real; the confidence it
// implies is fiction.
//
// The fix is replications: run the model N times with DIFFERENT seeds, treat
// the N answers as a sample, and report mean +/- a confidence interval.
// `Experiment` does the bookkeeping.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <memory>
#include "des.hpp"

using namespace des;

namespace {
void buildMM1(SimulationSystem& s) {
    Model& m = s.model();
    m.setInterarrival(exponential(1.0));
    m.station("Server", 1, QueueDiscipline::FIFO, exponential(0.8));
    m.setEntry("Server");
    s.setTermination(timeLimit(20000.0));
}
}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // --- the wrong way ----------------------------------------------------
    {
        SimulationSystem sim(12345u);
        buildMM1(sim);
        sim.initialise();
        sim.run();
        std::cout << "ONE RUN, no warm-up: Wq = " << sim.statistics().averageWaitingTime()
                  << "\n  Four decimal places from a single sample. Every digit after\n"
                     "  the first is decoration.\n\n";
    }

    // --- the right way ----------------------------------------------------
    // Experiment takes a function that builds your model. It runs it N times,
    // giving each replication seed = baseSeed + r, so:
    //   - the runs differ (or there would be no variance to measure)
    //   - the whole study reproduces from one number
    Experiment e("M/M/1, properly", buildMM1);
    e.replications(20)      // 20 independent runs
     .baseSeed(9000u)       // seeds 9000..9019
     .warmUp(500.0);        // discard the transient -- example 07 shows this
                            // model's transient is short, so 500 is plenty and
                            // discarding more would only widen the interval
    e.run();
    e.report();

    // Pull one column out and do your own arithmetic on it if you want.
    const std::vector<double> wq = e.column(&ReplicationResult::averageWait);
    const double mean = Summary::mean(wq);
    const double half = Summary::halfWidth95(wq);

    std::cout << "\n--- the per-replication values --------------------------\n";
    for (const auto& r : e.results())
        std::cout << "  seed " << r.seed << "  Wq = " << r.averageWait << "\n";

    std::cout << "\n  spread across runs (sample sd): " << Summary::stdDev(wq) << "\n";
    std::cout << "  95% interval for the mean     : [" << (mean - half)
              << ", " << (mean + half) << "]\n";
    std::cout << "  M/M/1 theory                  : 3.2000  -> inside: "
              << std::boolalpha << (3.2 >= mean - half && 3.2 <= mean + half) << "\n";
    std::cout << "\n  Try changing replications(20) to replications(5) and re-run.\n"
                 "  The interval gets much wider, and whether it covers 3.2000\n"
                 "  starts to depend on the seed. That is not the simulator being\n"
                 "  unreliable -- it is five samples being too few to say much.\n";

    std::cout << R"(
--- what to notice ------------------------------------------------------
Look at the per-replication list. Those are runs of the SAME model that
differ only in the random seed, and they scatter by several tenths. Any
one of them, quoted alone, would have looked authoritative.

The interval contains the theoretical answer. That is what "the simulation
agrees with theory" actually looks like -- not two numbers matching to
four decimals, which would be luck.

HOW TO USE THIS IN COURSEWORK:
  * Never report a bare number from one run. Report mean +/- half-width,
    and say how many replications.
  * If the interval is too wide to answer your question, you need MORE
    REPLICATIONS or LONGER RUNS -- not a different seed until you get a
    number you like. Halving the interval takes four times the runs.
  * Two designs whose intervals OVERLAP have not been shown to differ.
    Saying "A is better than B" on overlapping intervals is unsupported.

Two details the arithmetic gets right, which matter at small N:
  * the sample standard deviation divides by (n-1), not n
  * the interval uses Student's t (2.093 at n=20, 2.262 at n=10), not
    1.96 -- the normal would make your interval too narrow and your
    claims correspondingly too strong
--------------------------------------------------------------------------
)";
    return 0;
}
