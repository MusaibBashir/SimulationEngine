// ============================================================================
// 03 — More than one server at a station (M/M/c), and a trap about metrics.
// ============================================================================
// A station's `capacity` is how many entities it can serve AT ONCE. Three bank
// tellers sharing one queue is capacity 3 -- not three stations.
//
// The experiment: hold the TOTAL service capacity fixed and change how it is
// split. One fast server, or two at half speed, or three at a third speed? All
// three keep up with the same arrival rate, so all three have rho = 0.8.
//
// Most people are confident about the answer. Most people are half wrong,
// because the answer depends on WHICH NUMBER YOU MEASURE.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <memory>
#include "des.hpp"

using namespace des;

namespace {

void runWith(int servers, SimTime meanService, double theoryWq, double theoryW) {
    SimulationSystem sim(2024u);
    Model& m = sim.model();
    m.setInterarrival(exponential(1.0));
    m.station("Servers", servers, QueueDiscipline::FIFO,
                 exponential(meanService));
    m.setEntry("Servers");
    sim.setTermination(timeLimit(50000.0));
    sim.setWarmUp(2000.0);          // see example 07 for what this is
    sim.initialise();
    sim.run();

    const Station& s = sim.model().stationAt(0);
    const SimTime  T = sim.measuredTime();
    std::cout << "  c=" << servers
              << "  service " << std::setw(5) << meanService
              << " |  Wq " << std::setw(7) << sim.statistics().averageWaitingTime()
              << " (theory " << std::setw(6) << theoryWq << ")"
              << " |  W " << std::setw(7) << sim.statistics().averageTimeInSystem()
              << " (theory " << std::setw(6) << theoryW << ")"
              << " |  rho " << std::setw(6) << s.stats().utilisation(T, servers)
              << "\n";
}

}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Arrivals every 1.0 min. Same TOTAL capacity in every row\n"
                 "(c / meanService is constant), split three different ways.\n"
                 "Theory values are Erlang-C.\n\n";

    //       c  mean service   Wq       W
    runWith(1,  0.8,          3.2000,  4.0000);
    runWith(2,  1.6,          2.8444,  4.4444);
    runWith(3,  2.4,          2.5888,  4.9888);

    std::cout << R"(
--- what to notice ------------------------------------------------------
The two columns disagree, and that is the whole point of this example.

  WAITING TIME (Wq) gets BETTER with more servers: 3.20 -> 2.84 -> 2.59.
  TIME IN SYSTEM (W) gets WORSE:                   4.00 -> 4.44 -> 4.99.

Both are correct, and they are not in tension -- they are measuring
different things. With three slow servers you are more likely to find one
free immediately (P(wait) drops from 0.80 to 0.65), so you queue less. But
once you are being served, a third-speed server holds you three times as
long. Less waiting, more serving, and serving wins.

So which arrangement is "better" is not a question the simulation can
answer. It depends on whether your customers hate queueing or hate the
total errand taking a long time. A bank lobby and a drive-through get
opposite answers from the same arithmetic.

*** THE LESSON FOR YOUR COURSEWORK. *** Decide which metric your
objective actually is BEFORE you run anything. It is very easy to sweep a
parameter, find the setting that minimises the number you happened to
print, and write it up as optimal -- when a different, equally reasonable
metric would have chosen the opposite end of the range.

(A separate and genuinely universal result: merging several SEPARATE
queues into ONE shared queue in front of the same servers always helps.
That is pooling, and this engine models the shared-queue version already.
Don't confuse it with the comparison above, which is a different question.)
--------------------------------------------------------------------------
)";
    return 0;
}
