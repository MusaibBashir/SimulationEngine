// ============================================================================
// 13 — Where the random numbers come from, and how to tell if they are any good
// ============================================================================
// Every number this simulator produces traces back to one function:
//
//     double RandomStream::u01();     // uniform on (0,1)
//
// Everything else -- exponential, normal, Weibull, Poisson -- is that uniform
// pushed through an inverse CDF. This example shows both halves: how a variate
// is built, and how you check the uniforms underneath it are actually random.
//
// The headline is RANDU: a generator IBM shipped for years that passes every
// one-dimensional test in this file and is destroyed by the three-dimensional
// one. A generator that is broken does not crash. It gives you an answer.
// ============================================================================

#include <iostream>
#include <iomanip>
#include <vector>
#include "des.hpp"

using namespace des;

namespace {

// Mean and standard deviation of many draws, for checking a generator against
// what its parameters promise.
void checkVariate(const char* label, IDistribution& d, RandomStream& rng,
                  double expectedMean) {
    const int n = 200000;
    double sum = 0.0, sumSq = 0.0, lo = 1e18, hi = -1e18;
    for (int i = 0; i < n; ++i) {
        const double x = d.draw(rng);
        sum += x; sumSq += x * x;
        lo = std::min(lo, x); hi = std::max(hi, x);
    }
    const double mean = sum / n;
    const double sd   = std::sqrt(sumSq / n - mean * mean);
    std::cout << "  " << std::setw(30) << std::left << label << std::right
              << "  mean " << std::setw(9) << mean
              << " (says " << std::setw(8) << expectedMean << ")"
              << "   sd " << std::setw(8) << sd
              << "   range [" << std::setw(7) << lo << "," << std::setw(9) << hi << "]\n";
}

}  // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // ---------------------------------------------------------------------
    // 1. Testing the uniforms themselves
    // ---------------------------------------------------------------------
    std::cout << "############ 1. Are the uniforms random? ############\n\n";

    struct Engine { EngineKind kind; const char* name; };
    const Engine engines[] = {
        {EngineKind::MersenneTwister,    "mt19937 (the default)"},
        {EngineKind::LinearCongruential, "LCG, decent constants"},
        {EngineKind::Randu,              "RANDU (IBM, 1960s)"},
    };

    for (const Engine& e : engines) {
        RandomStream rng(12345u, e.kind);
        StreamTests::report(StreamTests::runAll(rng), e.name);
        std::cout << "\n";
    }

    std::cout << R"(--- what to notice ------------------------------------------------------
RANDU PASSES EVERY ONE-DIMENSIONAL TEST. Its values are beautifully
uniform, they do not trend, and consecutive pairs are uncorrelated. If you
only checked uniformity -- which is what "test your random numbers"
usually means to people -- you would ship it.

IT FAILS THE 3D SERIAL TEST BY A FACTOR APPROACHING TWENTY, with 257 of
the 4096 cells never visited once. RANDU satisfies

    x[n+2] = 6*x[n+1] - 9*x[n]   (mod 2^31)

exactly, so every consecutive TRIPLE lies on one of just 15 planes in the
unit cube. In one dimension that is invisible. In three it is a lattice.

Simulations use several numbers at once constantly -- an interarrival, a
service time, a branch draw, all in one event -- so three dimensions is not
an exotic stress test, it is Tuesday.

A PASS IS NOT PROOF. It means this test found nothing. A FAIL is much
stronger evidence, because a good generator fails a 95% test only 1 time
in 20 -- which also means that in the six tests above, an occasional
isolated failure from mt19937 is expected and not alarming.
)";

    // ---------------------------------------------------------------------
    // 2. Building variates from uniforms
    // ---------------------------------------------------------------------
    std::cout << "\n############ 2. From uniforms to variates ############\n\n";
    std::cout << "Every one of these is one u01() pushed through an inverse CDF.\n";
    std::cout << "200,000 draws each.\n\n";

    RandomStream rng(999u);
    auto exp1  = exponential(4.0);
    auto uni1  = uniform(2.0, 6.0);
    auto tri1  = triangular(1.0, 3.0, 8.0);
    auto nor1  = normal(10.0, 2.0);
    auto log1  = lognormalFrom(10.0, 4.0);
    auto wei1  = weibull(5.0, 2.0);
    auto erl1  = erlang(10.0, 4);
    auto dis1  = discrete({1.0, 5.0, 10.0}, {0.5, 0.3, 0.2});
    auto emp1  = empirical({2.0, 2.5, 3.0, 3.5, 9.0, 4.0, 4.5, 12.0});
    auto poi1  = poisson(3.0);

    checkVariate("exponential(4)",         *exp1, rng, exp1->mean());
    checkVariate("uniform(2,6)",           *uni1, rng, uni1->mean());
    checkVariate("triangular(1,3,8)",      *tri1, rng, tri1->mean());
    checkVariate("normal(10,2)",           *nor1, rng, nor1->mean());
    checkVariate("lognormalFrom(10,4)",    *log1, rng, log1->mean());
    checkVariate("weibull(scale 5,shape 2)", *wei1, rng, wei1->mean());
    checkVariate("erlang(mean 10, k=4)",   *erl1, rng, erl1->mean());
    checkVariate("discrete(1,5,10)",       *dis1, rng, dis1->mean());
    checkVariate("empirical(8 obs)",       *emp1, rng, emp1->mean());
    checkVariate("poisson(3)",             *poi1, rng, poi1->mean());

    std::cout << R"(
--- choosing one --------------------------------------------------------
EXPONENTIAL for interarrival times of independent customers. Memoryless:
how long you have already waited tells you nothing about the wait left.
That is a strong claim, and it is right for arrivals and usually wrong for
service.

ERLANG for service that is several sequential steps. k=1 is exponential,
k -> infinity is constant, and real service usually sits between.

LOGNORMAL for service times you measured and found right-skewed. Use
lognormalFrom(mean, sd) -- its parameters are the mean and sd of the LOG,
which is a famous way to be wrong by a factor of several.

WEIBULL for time-to-failure. shape < 1 is infant mortality, 1 is constant
hazard, > 1 is wear-out.

NORMAL for a duration only with care -- it has a left tail, and a negative
service time is nonsense rather than a subtlety. This engine truncates at
zero by default and can be told to throw instead; either way you should
know it is happening.

EMPIRICAL when you have data and no idea which textbook curve it came
from. Sampling your own observations beats guessing, and it interpolates
between them so the model is not limited to values you happened to see.

DISCRETE for part types, batch sizes, "70% small 20% medium 10% large".
)";

    // ---------------------------------------------------------------------
    // 3. Antithetic: the same stream, mirrored
    // ---------------------------------------------------------------------
    std::cout << "\n############ 3. Mirrored draws ############\n\n";
    RandomStream normalRun(42u), mirrored(42u);
    mirrored.setAntithetic(true);
    std::cout << "  u:            ";
    for (int i = 0; i < 5; ++i) std::cout << std::setw(9) << normalRun.u01();
    std::cout << "\n  1-u:          ";
    for (int i = 0; i < 5; ++i) std::cout << std::setw(9) << mirrored.u01();
    std::cout << "\n\n  exponential(1) from each stream:\n    ";
    RandomStream ea(42u), eb(42u); eb.setAntithetic(true);
    for (int i = 0; i < 5; ++i) std::cout << std::setw(9) << ea.exponential(1.0);
    std::cout << "\n    ";
    for (int i = 0; i < 5; ++i) std::cout << std::setw(9) << eb.exponential(1.0);
    std::cout << R"(

A small u gives a short interarrival; 1-u gives a long one. One run is
busy exactly where its mirror is quiet, so averaging the pair cancels much
of the luck. That only works because every variate here is ONE uniform
pushed through a MONOTONE transform -- see example 14 for what it buys.
)";
    return 0;
}
