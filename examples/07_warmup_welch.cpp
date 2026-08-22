// ============================================================================
// 07 — Warm-up: your run starts in the wrong state. How much to throw away?
// ============================================================================
// Every run starts with an empty system and an idle server. A real system at
// 11am is not empty. So the beginning of every run measures a situation that is
// not the one you care about, and averaging it in with the rest biases you.
//
// The fix is WARM-UP REMOVAL: run for a while, discard everything measured so
// far, then start measuring. The system keeps running throughout -- only the
// STATISTICS are thrown away, so measurement begins from a realistically loaded
// system. That is `sim.setWarmUp(t)`.
//
// The question is what t should be. Welch's method answers it, and this example
// also shows when the answer is "don't bother".
//
// The model here is deliberately BUSY: rho = 0.9. A lightly loaded system fills
// up almost immediately and has no transient worth removing -- which is itself
// worth knowing, and is why you measure rather than assume.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <memory>
#include "des.hpp"

using namespace des;

namespace {
// rho = 0.9.  Theory: Wq = rho / (mu - lambda) = 0.9 / (1/0.9 - 1) = 8.1000
Experiment::Builder busyQueue(SimTime runLength) {
    return [runLength](SimulationSystem& s) {
        Model& m = s.model();
        m.setInterarrival(exponential(1.0));
        m.station("Server", 1, QueueDiscipline::FIFO, exponential(0.9));
        m.setEntry("Server");
        s.setTermination(timeLimit(runLength));
    };
}

void row(SimTime runLength, SimTime warmUp) {
    Experiment e("row", busyQueue(runLength));
    e.replications(40).baseSeed(9000u);
    if (warmUp > 0.0) e.warmUp(warmUp);
    e.run();
    const std::vector<double> wq = e.column(&ReplicationResult::averageWait);
    std::cout << std::setw(9) << runLength << std::setw(9) << warmUp
              << std::setw(10) << Summary::mean(wq)
              << "  +/- " << std::setw(7) << Summary::halfWidth95(wq) << "\n";
}
}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // --- step 1: measure the transient ------------------------------------
    // Sample "how many are in the system" every 20 minutes, across 40 runs.
    // One run is far too noisy to see anything; averaging ACROSS runs is what
    // makes the trend visible, and is the whole idea of Welch's method.
    Experiment welch("warm-up analysis", busyQueue(40000.0));
    welch.replications(40).baseSeed(500u).observeEvery(20.0);
    welch.run();
    welch.writeWelchSeries("welch_07.csv", /*smoothing window=*/25);

    std::cout << "suggested warm-up (MSER): " << welch.suggestWarmUp() << "\n\n";
    std::cout << "welch_07.csv is written -- open it and plot column 2 against\n"
                 "column 1. You will see it start near 3 and climb to about 9\n"
                 "over the first few hundred minutes. THAT CLIMB IS THE TRANSIENT.\n"
                 "The system is filling up from empty toward its steady state.\n\n";

    // --- step 2: does removing it change the answer? ----------------------
    std::cout << "Wq for the same model, varying run length and warm-up.\n";
    std::cout << "Theory says 8.1000. 40 replications per row.\n\n";
    std::cout << " run len  warm-up        Wq       95% half-width\n";
    std::cout << " ------------------------------------------------\n";
    row( 1500.0,   0.0);
    row( 1500.0, 500.0);
    row( 3000.0,   0.0);
    row( 3000.0, 500.0);
    row(20000.0,   0.0);
    row(20000.0, 500.0);

    std::cout << R"(
--- what to notice, and it is not what people expect ---------------------
1. SHORT RUNS ARE BIASED LOW, and warm-up removal only partly fixes it.
   At run length 1500 you get about 7.1 against a true 8.1 -- 12% low.
   Removing the warm-up moves it toward the truth but does not get there,
   because after discarding 500 you have only 1000 minutes of data left.
   *** The real fix for a short run is a longer run, not a cleverer
   truncation. *** Warm-up removal cannot manufacture data you never
   collected.

2. WARM-UP REMOVAL IS NOT FREE. Compare the half-widths in each pair: the
   warmed rows are always WIDER. You threw away data, so your estimate is
   less precise. Discard the transient and no more.

3. BY RUN LENGTH 20000 IT BARELY MATTERS. The transient is a few hundred
   minutes out of twenty thousand, so leaving it in shifts the answer less
   than the noise. If your run is long relative to the transient, warm-up
   removal is a rounding correction.

4. AND AT LIGHTER LOAD IT MATTERS EVEN LESS. Re-run this file with service
   0.8 instead of 0.9 (rho = 0.8) and MSER will suggest almost nothing --
   that system reaches steady state in tens of minutes, not hundreds. The
   length of the transient is a property of YOUR model. Measure it.

--- about suggestWarmUp() ------------------------------------------------
It uses MSER: pick the truncation point that minimises the standard error
of the remaining mean, trading bias against the data you give up.

It is still a heuristic, and Welch's method is properly finished BY EYE on
the plot. Use the number as a starting point, look at welch_07.csv, and
round up -- discarding slightly too much costs a little precision (point 2),
discarding too little biases every number you report (point 1).

If it suggests a value near half your run length, that is not an answer.
It means the run never reached steady state, and you should make it longer.
--------------------------------------------------------------------------
)";
    return 0;
}
