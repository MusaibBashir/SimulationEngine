// ============================================================================
// RandomStream.hpp  --  where every random number in the program comes from
// ============================================================================
// v8 rebuilds this around ONE primitive:
//
//     double u01();     // uniform on (0,1)
//
// Everything else -- exponential, normal, Weibull, Poisson, the lot -- is built
// on top of it by INVERSE TRANSFORM: take u, and return F^-1(u).
//
// *** WHY THAT MATTERS, AND IT IS NOT PURITY. ***
// Two variance-reduction techniques depend on it and neither works without it:
//
//   COMMON RANDOM NUMBERS  compare two designs driven by the SAME draws, so the
//                          difference between them is not swamped by sampling
//                          noise. Needs each stream of draws to line up.
//   ANTITHETIC VARIATES    pair each run with one using 1-u everywhere. Needs
//                          exactly one uniform per variate, monotonically
//                          transformed, or "the opposite draw" has no meaning.
//
// v7 used std::exponential_distribution and friends. Those are fine generators
// and completely opaque: you cannot know how many uniforms they consume, so you
// cannot pair anything with anything.

#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include "Common.hpp"

namespace des {

// v8: the underlying bit source is now a choice, so a course can compare them.
enum class EngineKind {
    MersenneTwister,      // std::mt19937. The sane default.
    LinearCongruential,   // X = (aX + c) mod m, a decent modern choice of a,c,m
    Randu                 // *** DELIBERATELY BAD. *** IBM's RANDU, a = 65539,
                          // c = 0, m = 2^31. Shipped for years, and every
                          // triple of consecutive values lies on one of 15
                          // planes in 3D. Included so the tests in StreamTests
                          // can catch it -- see example 13.
};

class RandomStream {
private:
    EngineKind m_kind{EngineKind::MersenneTwister};
    unsigned   m_seed{12345u};
    std::mt19937 m_mt;
    std::uint64_t m_lcgState{0};
    bool m_antithetic{false};
    long long m_draws{0};

public:
    explicit RandomStream(unsigned seed = 12345u,
                          EngineKind kind = EngineKind::MersenneTwister);

    unsigned seed() const { return m_seed; }
    EngineKind engine() const { return m_kind; }
    long long draws() const { return m_draws; }
    void reset();

    // *** THE PRIMITIVE. *** Uniform on the OPEN interval (0,1): never exactly
    // 0 or 1, because inverse transforms take log(u) and log(1-u) and either
    // endpoint would produce an infinity.
    double u01();

    // v8: return 1-u instead of u for every draw. Used for antithetic
    // variates -- run a replication normally, then again with this set, and
    // average the pair. The two runs are negatively correlated, so the average
    // has lower variance than two independent runs would.
    void setAntithetic(bool on) { m_antithetic = on; }
    bool isAntithetic() const { return m_antithetic; }

    // v8: an INDEPENDENT stream derived from this one's seed and a name.
    // Give arrivals their own stream and service its own, and changing the
    // service time distribution no longer shifts the arrival pattern -- which is
    // what makes two model variants comparable at all.
    RandomStream substream(const std::string& name) const;

    // --- variates, all built on u01() ------------------------------------
    SimTime exponential(SimTime mean);
    SimTime uniform(SimTime low, SimTime high);
    SimTime normal(SimTime mean, SimTime stdDev);
    SimTime lognormal(SimTime logMean, SimTime logStdDev);
    SimTime weibull(SimTime scale, SimTime shape);
    SimTime erlang(SimTime meanOfEach, int phases);
    SimTime triangular(SimTime low, SimTime mode, SimTime high);
    int     poisson(double mean);
    bool    bernoulli(double p);
    std::size_t uniformIndex(std::size_t n);

    // The standard normal quantile, exposed because it is the interesting part
    // of normal(): there is no closed form for the inverse of the normal CDF,
    // so this is a rational approximation (Acklam's), accurate to about 1e-9.
    static double normalQuantile(double p);
};

}  // namespace des
