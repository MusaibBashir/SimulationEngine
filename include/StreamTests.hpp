// ============================================================================
// StreamTests.hpp  --  v8: is your random number generator actually random?
// ============================================================================
// A generator that is wrong does not crash. It produces numbers that look fine,
// runs your simulation, and gives you an answer. The only way to find out is to
// test the stream itself, which is why every simulation course teaches these.
//
// Each test returns a STATISTIC and a verdict at 95%. A verdict of "passed" is
// never proof of randomness -- it means this particular test found nothing. A
// verdict of "failed" is much stronger evidence, because a good generator fails
// a 95% test only 1 time in 20.
//
// Run them on RANDU (EngineKind::Randu) to see the point: it sails through the
// one-dimensional tests and is destroyed by the serial one.

#pragma once

#include <string>
#include <vector>
#include "Common.hpp"

namespace des {

class RandomStream;

struct TestResult {
    std::string name;
    double statistic{0.0};
    double critical{0.0};      // the 95% critical value it is compared against
    bool   passed{true};
    std::string note;
};

class StreamTests {
public:
    // --- uniformity ------------------------------------------------------
    // CHI-SQUARE: chop (0,1) into `bins` equal intervals and compare how many
    // values landed in each against how many should have. Catches a generator
    // whose output clumps or avoids part of the range.
    static TestResult chiSquareUniformity(RandomStream& rng, int n, int bins = 10);

    // KOLMOGOROV-SMIRNOV: the largest gap between the empirical CDF and the
    // straight line it should be. Uses the whole sample rather than binning it,
    // so it needs no arbitrary bin count -- and is more sensitive to a smooth
    // drift than chi-square, less sensitive to a single spike.
    static TestResult kolmogorovSmirnov(RandomStream& rng, int n);

    // --- independence ----------------------------------------------------
    // RUNS TEST (up and down): count monotone runs. Too few means the values
    // trend; too many means they oscillate. Uniformity says nothing about
    // ORDER, and this is the cheapest test that looks at it.
    static TestResult runsUpAndDown(RandomStream& rng, int n);

    // AUTOCORRELATION at lag k: are u[i] and u[i+k] related? A generator can be
    // perfectly uniform and still have every value predictable from the last.
    static TestResult autocorrelation(RandomStream& rng, int n, int lag = 1);

    // --- the one that catches RANDU --------------------------------------
    // SERIAL TEST in 3 dimensions: bin consecutive TRIPLES into a cube of
    // cells and chi-square the counts. RANDU's triples all lie on 15 planes, so
    // most cells are empty and the statistic explodes -- while every test above
    // passes comfortably. That gap is the entire lesson: a generator can be
    // flawless in one dimension and useless in three, and simulations use
    // several numbers at once all the time.
    // 16 bins per axis, not 8. At 8 the cells are coarse enough that RANDU's
    // 15 planes still land in most of them and it scrapes a pass; at 16 the
    // statistic is 15x the critical value and 257 of 4096 cells are never
    // visited at all. Resolution is the whole test.
    static TestResult serial3D(RandomStream& rng, int n, int binsPerAxis = 16);

    // Run them all.
    // 600k values: the 3D test bins them into 4096 cells, and chi-square wants
    // a healthy expected count per cell.
    static std::vector<TestResult> runAll(RandomStream& rng, int n = 600000);
    static void report(const std::vector<TestResult>& results, const std::string& title);

    // Critical values, exposed so the tests can check them directly.
    static double chiSquareCritical95(int degreesOfFreedom);
};

}  // namespace des
