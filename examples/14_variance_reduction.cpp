// ============================================================================
// 14 — Getting a narrower answer for the same amount of computing
// ============================================================================
// Example 08 said: one run is one sample, so report an interval. The obvious
// way to narrow that interval is to run more replications -- but halving it
// costs FOUR TIMES the compute, so that road runs out fast.
//
// Variance reduction gets you a narrower interval for the SAME compute, by
// being cleverer about which random numbers you use. Two techniques:
//
//   ANTITHETIC VARIATES    run each replication twice, the second time with
//                          every uniform replaced by 1-u, and average the pair.
//   COMMON RANDOM NUMBERS  compare two designs driven by the SAME draws, so the
//                          luck common to both cancels in the difference.
//
// Both depend on something example 13 showed: every variate here is ONE uniform
// pushed through a MONOTONE inverse CDF. A generator that consumes an unknown
// number of uniforms per variate cannot be paired with anything.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <cmath>
#include "des.hpp"

using namespace des;

namespace {

// An M/M/c queue: arrivals every 1.0 minute on average.
Experiment::Builder queueWith(SimTime meanService, int servers) {
    return [meanService, servers](SimulationSystem& s) {
        s.model().arrivals(exponential(1.0))
                 .station("Server", servers, FIFO, exponential(meanService))
                 .entryAt("Server");
        s.stopAt(5000.0);
    };
}

void line(const char* label, const Experiment::Estimate& e, int runs) {
    std::cout << "  " << std::setw(34) << std::left << label << std::right
              << std::setw(9) << e.mean << "  +/- " << std::setw(7) << e.halfWidth
              << "   [" << std::setw(8) << e.low() << "," << std::setw(9) << e.high() << "]"
              << "   " << runs << " runs\n";
}

}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // ---------------------------------------------------------------------
    // 1. ANTITHETIC VARIATES
    //    Same total compute -- 40 runs either way. The paired version splits
    //    them into 20 mirrored pairs instead of 40 independent runs.
    // ---------------------------------------------------------------------
    std::cout << "############ 1. Antithetic variates ############\n\n";
    std::cout << "M/M/1, rho = 0.8. Theory says Wq = 3.2000.\n\n";

    Experiment plain("plain", queueWith(0.8, 1));
    plain.replications(40).baseSeed(3000u).warmUp(500.0).separateStreams();
    plain.run();

    Experiment anti("antithetic", queueWith(0.8, 1));
    anti.replications(20).baseSeed(3000u).warmUp(500.0)
        .separateStreams().antitheticPairs();
    anti.run();

    line("40 independent replications", Experiment::estimate(plain.waits()), 40);
    line("20 antithetic pairs",         Experiment::estimate(anti.waits()),  40);

    std::cout << R"(
The pairs give a narrower interval for identical compute. A run whose
arrival stream was unluckily busy is paired with one where the same
uniforms, mirrored, make it unluckily quiet -- so the pair average is
closer to the truth than either half. The two halves are NEGATIVELY
correlated, and that is exactly what shrinks the variance of their mean.

Note the pair is ONE observation, not two. Counting the halves as
independent would understate the interval, which is the one direction you
must never err in.
)";

    // ---------------------------------------------------------------------
    // 2. COMMON RANDOM NUMBERS
    //    Two designs. Do they differ? Compare them the naive way, then paired.
    // ---------------------------------------------------------------------
    std::cout << "\n############ 2. Common random numbers ############\n\n";
    std::cout << "Design A: one server at mean 0.8.   Design B: two at mean 1.6.\n";
    std::cout << "Same total capacity, so rho = 0.8 in both. Which queues less?\n\n";

    Experiment A("one fast server", queueWith(0.8, 1));
    Experiment B("two slow servers", queueWith(1.6, 2));
    for (Experiment* e : {&A, &B}) {
        // *** THE SAME BASE SEED FOR BOTH. *** That is the whole technique:
        // replication i of A and replication i of B see the same arrival
        // stream, because separateStreams() gives arrivals a stream of their
        // own that does not shift when the service distribution changes.
        e->replications(20).baseSeed(7000u).warmUp(500.0).separateStreams();
        e->run();
    }

    const auto ea = Experiment::estimate(A.waits());
    const auto eb = Experiment::estimate(B.waits());
    line("design A, on its own", ea, 20);
    line("design B, on its own", eb, 20);

    const bool overlap = ea.low() < eb.high() && eb.low() < ea.high();
    std::cout << "\n  intervals overlap: " << std::boolalpha << overlap
              << "   -> compared this way, you CANNOT say which is better\n\n";

    const auto cmp = Experiment::compare(A, B, &ReplicationResult::averageWait);
    std::cout << "  paired difference (A - B)         "
              << std::setw(9) << cmp.meanDifference
              << "  +/- " << std::setw(7) << cmp.halfWidth
              << "   [" << std::setw(8) << (cmp.meanDifference - cmp.halfWidth)
              << "," << std::setw(9) << (cmp.meanDifference + cmp.halfWidth) << "]\n";
    std::cout << "  difference is significant: " << cmp.differsSignificantly << "\n";

    const double unpaired = std::sqrt(ea.halfWidth * ea.halfWidth + eb.halfWidth * eb.halfWidth);
    std::cout << "  an UNPAIRED comparison would give  +/- " << unpaired
              << "  (" << (unpaired / cmp.halfWidth) << "x wider)\n";

    std::cout << R"(
--- what to notice ------------------------------------------------------
THE TWO SEPARATE INTERVALS OVERLAP, so on that evidence the designs are
indistinguishable and 20 replications was not enough to answer the
question.

THE PAIRED DIFFERENCE IS TINY AND CLEARLY NON-ZERO. Same runs, same
compute, same numbers -- looked at as differences rather than as two
independent means.

Why it works: both designs saw the SAME arrival stream in each
replication. Whatever bad luck was in that stream inflated both designs'
waits together, so it cancels in A-B. What is left is the difference the
design actually makes.

*** TWO DESIGNS WHOSE INTERVALS OVERLAP HAVE NOT BEEN SHOWN TO DIFFER --
BUT THEY MAY STILL DIFFER. *** Overlapping intervals are weak evidence,
not evidence of no difference, and a paired comparison is often the only
one your compute budget can afford. Whenever you are comparing rather than
measuring, pair.

WHAT BREAKS IT: without separateStreams() every distribution shares one
stream, so changing the service time shifts every subsequent draw and
replication i of A has nothing in common with replication i of B. The
pairing silently becomes worthless -- the numbers still print. Try
deleting .separateStreams() above and watch the paired half-width grow to
about the unpaired one.

AND THE HONEST LIMIT: this engine gives streams to arrivals, arrival
attributes and each Process's service time. Delay durations and Decide
draws still share the common stream, so a model whose variants differ in
THOSE will pair less well. Stated rather than hidden.
)";
    return 0;
}
