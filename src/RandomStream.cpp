// ============================================================================
// RandomStream.cpp
// ============================================================================

#include "RandomStream.hpp"
#include "ModelError.hpp"
#include <cassert>
#include <cmath>
#include <functional>
#include <limits>

namespace des {

namespace {
// LCG constants. Numerical Recipes' choice: a full-period generator mod 2^32
// with decent (not excellent) spectral behaviour.
constexpr std::uint64_t LCG_A = 1664525u;
constexpr std::uint64_t LCG_C = 1013904223u;
constexpr std::uint64_t LCG_M = 4294967296ull;      // 2^32

// RANDU. a = 65539 = 2^16 + 3, c = 0, m = 2^31, odd seed required.
// The flaw: x[n+2] = 6*x[n+1] - 9*x[n] (mod m) exactly, so every consecutive
// triple lies on one of 15 parallel planes in the unit cube. It passes a
// one-dimensional uniformity test comfortably, which is the lesson.
constexpr std::uint64_t RANDU_A = 65539u;
constexpr std::uint64_t RANDU_M = 2147483648ull;    // 2^31
}  // namespace

RandomStream::RandomStream(unsigned seed, EngineKind kind)
    : m_kind(kind), m_seed(seed), m_mt(seed) {
    reset();
}

void RandomStream::reset() {
    m_mt.seed(m_seed);
    // RANDU needs an ODD seed or it degenerates further still.
    m_lcgState = (m_seed == 0u) ? 1u : (m_seed | 1u);
    m_draws = 0;
}

double RandomStream::u01() {
    ++m_draws;
    double u;
    switch (m_kind) {
        case EngineKind::MersenneTwister: {
            // Divide by 2^32 and nudge off the endpoints. std::generate_canonical
            // would do, but this way the number of raw words consumed per
            // uniform is exactly one, which is what makes streams line up.
            const std::uint32_t x = m_mt();
            u = (static_cast<double>(x) + 0.5) / 4294967296.0;
            break;
        }
        case EngineKind::LinearCongruential:
            m_lcgState = (LCG_A * m_lcgState + LCG_C) % LCG_M;
            u = (static_cast<double>(m_lcgState) + 0.5) / static_cast<double>(LCG_M);
            break;
        case EngineKind::Randu:
            m_lcgState = (RANDU_A * m_lcgState) % RANDU_M;
            u = (static_cast<double>(m_lcgState) + 0.5) / static_cast<double>(RANDU_M);
            break;
        default:
            u = 0.5;
            break;
    }
    // The antithetic flag is applied HERE, at the one primitive, so every
    // variate in the program is automatically antithetic without a single
    // distribution knowing about it.
    return m_antithetic ? (1.0 - u) : u;
}

RandomStream RandomStream::substream(const std::string& name) const {
    // Derive a seed from this stream's seed and the name. Deterministic, so the
    // whole experiment still reproduces from one number -- and independent
    // enough that adding a distribution to a model does not perturb the others.
    //
    // std::hash is not required to be stable across runs of DIFFERENT programs,
    // but it is stable within one, which is all reproducibility needs here. A
    // fixed hash (FNV-1a) would be needed to reproduce across compilers.
    std::uint64_t h = 1469598103934665603ull;          // FNV-1a offset basis
    for (char c : name) { h ^= static_cast<unsigned char>(c); h *= 1099511628211ull; }
    h ^= static_cast<std::uint64_t>(m_seed) * 2654435761ull;
    return RandomStream(static_cast<unsigned>(h & 0x7fffffffu) | 1u, m_kind);
}

// --------------------------------------------------------------- variates --

SimTime RandomStream::exponential(SimTime mean) {
    if (mean <= 0.0) throw ModelError("exponential: mean must be positive");
    // INVERSE TRANSFORM. F(x) = 1 - e^(-x/mean), so x = -mean * ln(1-u).
    // ln(1-u) rather than ln(u): both are correct in distribution, but 1-u keeps
    // the transform MONOTONICALLY INCREASING in u, which is what makes
    // antithetic pairing and common random numbers behave as intended.
    return -mean * std::log(1.0 - u01());
}

SimTime RandomStream::uniform(SimTime low, SimTime high) {
    if (high < low) throw ModelError("uniform: high must be >= low");
    return low + (high - low) * u01();
}

double RandomStream::normalQuantile(double p) {
    // Peter Acklam's rational approximation to the inverse normal CDF. There is
    // no closed form, and this is the standard practical answer: ~1e-9 relative
    // accuracy over the whole range, and monotone, which antithetic pairing
    // needs.
    static const double a[] = {-3.969683028665376e+01, 2.209460984245205e+02,
                               -2.759285104469687e+02, 1.383577518672690e+02,
                               -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[] = {-5.447609879822406e+01, 1.615858368580409e+02,
                               -1.556989798598866e+02, 6.680131188771972e+01,
                               -1.328068155288572e+01};
    static const double c[] = {-7.784894002430293e-03, -3.223964580411365e-01,
                               -2.400758277161838e+00, -2.549732539343734e+00,
                                4.374664141464968e+00,  2.938163982698783e+00};
    static const double d[] = {7.784695709041462e-03, 3.224671290700398e-01,
                               2.445134137142996e+00, 3.754408661907416e+00};
    const double pLow = 0.02425, pHigh = 1.0 - pLow;
    if (p <= 0.0) return -std::numeric_limits<double>::infinity();
    if (p >= 1.0) return  std::numeric_limits<double>::infinity();
    if (p < pLow) {
        const double q = std::sqrt(-2.0 * std::log(p));
        return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /
               ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    }
    if (p > pHigh) {
        const double q = std::sqrt(-2.0 * std::log(1.0 - p));
        return -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /
                ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    }
    const double q = p - 0.5, r = q * q;
    return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q /
           (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1.0);
}

SimTime RandomStream::normal(SimTime mean, SimTime stdDev) {
    if (stdDev < 0.0) throw ModelError("normal: standard deviation must be >= 0");
    // Inverse transform, NOT Box-Muller. Box-Muller is faster and consumes TWO
    // uniforms per variate, and the pair it produces is not a monotone function
    // of either -- so antithetic runs would not be antithetic. One uniform in,
    // one variate out, monotone, is worth more here than speed.
    return mean + stdDev * normalQuantile(u01());
}

SimTime RandomStream::lognormal(SimTime logMean, SimTime logStdDev) {
    // Parameters are the mean and sd OF THE LOG, which is the usual convention
    // and the usual source of confusion. exp(N(mu, sigma)).
    return std::exp(normal(logMean, logStdDev));
}

SimTime RandomStream::weibull(SimTime scale, SimTime shape) {
    if (scale <= 0.0 || shape <= 0.0) throw ModelError("weibull: scale and shape must be positive");
    // F(x) = 1 - exp(-(x/scale)^shape)  =>  x = scale * (-ln(1-u))^(1/shape)
    return scale * std::pow(-std::log(1.0 - u01()), 1.0 / shape);
}

SimTime RandomStream::erlang(SimTime meanOfEach, int phases) {
    if (phases < 1) throw ModelError("erlang: phases must be >= 1");
    // CONVOLUTION: an Erlang-k is the sum of k exponentials. That is not a
    // trick, it is what an Erlang IS -- k sequential phases of work -- which is
    // why it fits service times that are neither constant nor memoryless.
    SimTime total = 0.0;
    for (int i = 0; i < phases; ++i) total += exponential(meanOfEach);
    return total;
}

SimTime RandomStream::triangular(SimTime low, SimTime mode, SimTime high) {
    if (!(low <= mode && mode <= high)) throw ModelError("triangular: need low <= mode <= high");
    const double u = u01();
    const double c = (high > low) ? (mode - low) / (high - low) : 0.0;
    if (u < c) return low + std::sqrt(u * (high - low) * (mode - low));
    return high - std::sqrt((1.0 - u) * (high - low) * (high - mode));
}

int RandomStream::poisson(double mean) {
    if (mean < 0.0) throw ModelError("poisson: mean must be >= 0");
    // Inverse transform by walking the cumulative mass. Fine for the small means
    // simulation models use; for a very large mean the loop gets long and a
    // rejection method would be better -- at the cost of the one-uniform-per-
    // variate property that antithetic pairing needs. Noted, not fixed.
    const double u = u01();
    double p = std::exp(-mean), cumulative = p;
    int k = 0;
    while (u > cumulative && k < 1000000) {
        ++k;
        p *= mean / k;
        cumulative += p;
    }
    return k;
}

bool RandomStream::bernoulli(double p) {
    return u01() < p;
}

std::size_t RandomStream::uniformIndex(std::size_t n) {
    if (n == 0) throw ModelError("uniformIndex: n must be > 0");
    const std::size_t i = static_cast<std::size_t>(u01() * static_cast<double>(n));
    return i < n ? i : n - 1;   // guard the 1-in-2^53 rounding case
}

}  // namespace des
