// ============================================================================
// 06 — The five distributions, and why the shape matters as much as the mean.
// ============================================================================
//   Exponential(mean)          random, memoryless. The default for arrivals.
//   Constant(v)                no randomness at all.
//   Uniform(low, high)         equally likely anywhere in a range.
//   Triangular(low, mode, high) min / most likely / max. Best when you have no
//                              data but do have an opinion.
//   Deterministic({...})       a fixed list, in order. For hand-checking.
//
// *** Exponential takes the MEAN, not the rate. *** Exponential(4.0) means
// "4 minutes on average". Passing 0.25 because "the rate is 0.25/min" gives a
// model that is wrong by a factor of 16 and still runs happily.
//
// The experiment below holds the MEAN service time fixed at 0.8 and changes
// only the SHAPE. If service time variability didn't matter, all four rows
// would be the same. They are not, and the difference is enormous.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include "des.hpp"

using namespace des;

namespace {

void trial(const std::string& label, std::function<std::unique_ptr<IDistribution>()> makeService) {
    Experiment e(label, [&makeService](SimulationSystem& s) {
        Model& m = s.model();
        m.setInterarrival(exponential(1.0));
        m.station("S", 1, QueueDiscipline::FIFO, makeService());
        m.setEntry("S");
        s.setTermination(timeLimit(20000.0));
    });
    e.replications(8).baseSeed(600u).warmUp(3000.0);
    e.run();

    const std::vector<double> wq = e.column(&ReplicationResult::averageWait);
    const std::vector<double> ut = e.column(&ReplicationResult::utilisation);
    std::cout << "  " << std::setw(26) << std::left << label << std::right
              << std::setw(9) << Summary::mean(wq)
              << "  +/- " << std::setw(6) << Summary::halfWidth95(wq)
              << std::setw(10) << Summary::mean(ut) << "\n";
}

}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Arrivals Exponential(1.0). Service MEAN is 0.8 in every row.\n";
    std::cout << "Only the shape of the service distribution changes.\n\n";
    std::cout << "  service distribution         mean Wq    +/-        rho\n";
    std::cout << "  ---------------------------------------------------------\n";

    trial("Constant(0.8)",              []{ return constant(0.8); });
    trial("Uniform(0.4, 1.2)",          []{ return uniform(0.4, 1.2); });
    trial("Triangular(0.2,0.6,1.6)",    []{ return triangular(0.2, 0.6, 1.6); });
    trial("Exponential(0.8)",           []{ return exponential(0.8); });

    std::cout << R"(
--- what to notice ------------------------------------------------------
Utilisation is 0.8 in every row -- identical, because it depends only on
the mean. The server is busy exactly as much in all four systems.

The WAIT is not identical. It roughly doubles from Constant to Exponential
even though the workload never changed. Variability alone creates queues.

This is the Pollaczek-Khinchine result: for an M/G/1 queue,
    Wq = (rho / (1 - rho)) * (mean service / 2) * (1 + CV^2)
where CV is the coefficient of variation of the service time. Constant has
CV = 0, exponential has CV = 1 -- exactly a factor of two between them, and
that is what the table shows.

The practical version: if you want shorter queues and cannot add capacity,
MAKE SERVICE MORE CONSISTENT. Reducing variability is free capacity, and it
is invisible to anyone who only looks at averages.

Deterministic is not in the table because it is not a shape -- it is a
fixed list, for when you want to check a run by hand. See example 02.
--------------------------------------------------------------------------
)";
    return 0;
}
