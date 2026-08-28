// ============================================================================
// Distribution.cpp
// ============================================================================

#include "Distribution.hpp"
#include "RandomStream.hpp"
#include <cassert>
#include <cmath>
#include <sstream>
#include <algorithm>
#include "ModelError.hpp"

namespace des {


// ------------------------------------------------------------- Exponential --
Exponential::Exponential(SimTime mean) : m_mean(mean) { assert(mean > 0.0); }

SimTime Exponential::draw(RandomStream& rng) { return pick(rng).exponential(m_mean); }

SimTime Exponential::mean() const { return m_mean; }

std::string Exponential::describe() const {
    std::ostringstream os; os << "Exponential(mean=" << m_mean << ")"; return os.str();
}

// ---------------------------------------------------------------- Constant --
Constant::Constant(SimTime value) : m_value(value) {
    // Throw, not assert: a parsed CONS(-5) is a USER mistake, and NDEBUG
    // would otherwise let a negative duration walk the clock backwards.
    if (value < 0.0) throw ModelError("Constant: value must be >= 0");
}

SimTime Constant::draw(RandomStream& /*rng*/) {
    // Draws nothing from the stream. That matters: swapping Exponential for
    // Constant must not shift every OTHER draw in the program, or two runs stop
    // being comparable. Consuming a random number you do not need is a real and
    // subtle way to break variance-reduction experiments later.
    return m_value;
}

SimTime Constant::mean() const { return m_value; }

std::string Constant::describe() const {
    std::ostringstream os; os << "Constant(" << m_value << ")"; return os.str();
}

// ----------------------------------------------------------------- Uniform --
Uniform::Uniform(SimTime low, SimTime high) : m_low(low), m_high(high) {
    assert(high >= low);
}

SimTime Uniform::draw(RandomStream& rng) { return pick(rng).uniform(m_low, m_high); }

SimTime Uniform::mean() const { return 0.5 * (m_low + m_high); }

std::string Uniform::describe() const {
    std::ostringstream os; os << "Uniform(" << m_low << ", " << m_high << ")"; return os.str();
}

// -------------------------------------------------------------- Triangular --
Triangular::Triangular(SimTime low, SimTime mode, SimTime high)
    : m_low(low), m_mode(mode), m_high(high) {
    assert(low <= mode && mode <= high);
}

SimTime Triangular::draw(RandomStream& rng) {
    return pick(rng).triangular(m_low, m_mode, m_high);
}

SimTime Triangular::mean() const { return (m_low + m_mode + m_high) / 3.0; }

std::string Triangular::describe() const {
    std::ostringstream os;
    os << "Triangular(" << m_low << ", " << m_mode << ", " << m_high << ")";
    return os.str();
}

// ----------------------------------------------------------- Deterministic --
Deterministic::Deterministic(std::vector<SimTime> values, bool repeat)
    : m_values(std::move(values)), m_cursor(0), m_repeat(repeat) {
    assert(!m_values.empty() && "Deterministic needs at least one value");
}

SimTime Deterministic::draw(RandomStream& /*rng*/) {
    if (m_cursor >= m_values.size()) {
        // Running off the end is a modelling mistake, not a runtime condition.
        // Say so loudly rather than silently repeating or returning zero.
        assert(m_repeat && "Deterministic ran out of values (repeat=false)");
        m_cursor = 0;
    }
    return m_values[m_cursor++];
}

SimTime Deterministic::mean() const {
    // The average of the list. For a repeating list that IS the long-run mean;
    // for a one-pass list it is the mean of the values you supplied.
    SimTime total = 0.0;
    for (SimTime v : m_values) total += v;
    return total / static_cast<SimTime>(m_values.size());
}

std::string Deterministic::describe() const {
    std::ostringstream os;
    os << "Deterministic(" << m_values.size() << " values, "
       << (m_repeat ? "repeating" : "one pass") << ")";
    return os.str();
}

void Deterministic::reset() { m_cursor = 0; }



// ============================== v8 additions ==============================

Normal::Normal(SimTime mean, SimTime stdDev, bool truncateAtZero)
    : m_mean(mean), m_sd(stdDev), m_truncate(truncateAtZero) {
    if (stdDev < 0.0) throw ModelError("Normal: standard deviation must be >= 0");
}
SimTime Normal::draw(RandomStream& rng) {
    const SimTime x = pick(rng).normal(m_mean, m_sd);
    if (x >= 0.0) return x;
    // A normal has a left tail. For a DURATION that tail is nonsense, and the
    // two honest options are to clamp (changing the distribution slightly, and
    // the mean with it) or to refuse. Silently returning a negative service
    // time would corrupt every statistic downstream.
    if (m_truncate) return 0.0;
    throw ModelError("Normal drew a negative value where a duration was needed; "
                     "use truncateAtZero, or a Lognormal/Erlang instead");
}
std::string Normal::describe() const {
    std::ostringstream os; os << "Normal(mean=" << m_mean << ", sd=" << m_sd << ")";
    return os.str();
}
SimTime Normal::mean() const { return m_mean; }

Lognormal::Lognormal(SimTime logMean, SimTime logStdDev)
    : m_logMean(logMean), m_logSd(logStdDev) {
    if (logStdDev < 0.0) throw ModelError("Lognormal: log sd must be >= 0");
}
std::unique_ptr<Lognormal> Lognormal::fromMeanAndSd(SimTime mean, SimTime sd) {
    if (mean <= 0.0) throw ModelError("Lognormal::fromMeanAndSd: mean must be positive");
    // The conversion nobody remembers, so it lives here once:
    //   sigma^2 = ln(1 + (sd/mean)^2),  mu = ln(mean) - sigma^2 / 2
    const double cv2 = (sd / mean) * (sd / mean);
    const double sigma2 = std::log(1.0 + cv2);
    return std::make_unique<Lognormal>(std::log(mean) - sigma2 / 2.0, std::sqrt(sigma2));
}
SimTime Lognormal::draw(RandomStream& rng) { return pick(rng).lognormal(m_logMean, m_logSd); }
std::string Lognormal::describe() const {
    std::ostringstream os; os << "Lognormal(logMean=" << m_logMean << ", logSd=" << m_logSd << ")";
    return os.str();
}
SimTime Lognormal::mean() const { return std::exp(m_logMean + m_logSd * m_logSd / 2.0); }

Weibull::Weibull(SimTime scale, SimTime shape) : m_scale(scale), m_shape(shape) {
    if (scale <= 0.0 || shape <= 0.0) throw ModelError("Weibull: scale and shape must be positive");
}
SimTime Weibull::draw(RandomStream& rng) { return pick(rng).weibull(m_scale, m_shape); }
std::string Weibull::describe() const {
    std::ostringstream os; os << "Weibull(scale=" << m_scale << ", shape=" << m_shape << ")";
    return os.str();
}
SimTime Weibull::mean() const { return m_scale * std::tgamma(1.0 + 1.0 / m_shape); }

Erlang::Erlang(SimTime meanOfEachPhase, int phases)
    : m_meanEach(meanOfEachPhase), m_phases(phases) {
    if (meanOfEachPhase <= 0.0) throw ModelError("Erlang: phase mean must be positive");
    if (phases < 1) throw ModelError("Erlang: phases must be >= 1");
}
std::unique_ptr<Erlang> Erlang::fromMean(SimTime totalMean, int phases) {
    if (phases < 1) throw ModelError("Erlang::fromMean: phases must be >= 1");
    return std::make_unique<Erlang>(totalMean / phases, phases);
}
SimTime Erlang::draw(RandomStream& rng) { return pick(rng).erlang(m_meanEach, m_phases); }
std::string Erlang::describe() const {
    std::ostringstream os; os << "Erlang(k=" << m_phases << ", phaseMean=" << m_meanEach
                              << ", mean=" << mean() << ")";
    return os.str();
}
SimTime Erlang::mean() const { return m_meanEach * m_phases; }

Discrete::Discrete(std::vector<SimTime> values, std::vector<double> probabilities)
    : m_values(std::move(values)) {
    if (m_values.empty()) throw ModelError("Discrete: needs at least one value");
    if (m_values.size() != probabilities.size())
        throw ModelError("Discrete: values and probabilities must be the same length");
    double total = 0.0;
    for (double p : probabilities) {
        if (p < 0.0) throw ModelError("Discrete: probabilities must be >= 0");
        total += p;
        m_cumulative.push_back(total);
    }
    if (std::fabs(total - 1.0) > 1e-9)
        throw ModelError("Discrete: probabilities sum to " + std::to_string(total) +
                         ", not 1");
}
SimTime Discrete::draw(RandomStream& rng) {
    const double u = pick(rng).u01();
    for (std::size_t i = 0; i < m_cumulative.size(); ++i)
        if (u < m_cumulative[i]) return m_values[i];
    return m_values.back();
}
std::string Discrete::describe() const {
    std::ostringstream os; os << "Discrete(" << m_values.size() << " values, mean="
                              << mean() << ")";
    return os.str();
}
SimTime Discrete::mean() const {
    SimTime m = 0.0, prev = 0.0;
    for (std::size_t i = 0; i < m_values.size(); ++i) {
        m += m_values[i] * (m_cumulative[i] - prev);
        prev = m_cumulative[i];
    }
    return m;
}

Empirical::Empirical(std::vector<SimTime> observations) : m_sorted(std::move(observations)) {
    if (m_sorted.size() < 2) throw ModelError("Empirical: needs at least two observations");
    std::sort(m_sorted.begin(), m_sorted.end());
}
SimTime Empirical::draw(RandomStream& rng) {
    // Inverse transform on the empirical CDF, interpolating linearly between
    // the order statistics. Without interpolation you could only ever return
    // values you had already seen, which for a hundred observations gives a
    // hundred possible service times and a visibly lumpy model.
    const double u = pick(rng).u01();
    const double pos = u * static_cast<double>(m_sorted.size() - 1);
    const std::size_t i = static_cast<std::size_t>(pos);
    const std::size_t j = std::min(i + 1, m_sorted.size() - 1);
    const double frac = pos - static_cast<double>(i);
    return m_sorted[i] + frac * (m_sorted[j] - m_sorted[i]);
}
std::string Empirical::describe() const {
    std::ostringstream os; os << "Empirical(" << m_sorted.size() << " observations, mean="
                              << mean() << ")";
    return os.str();
}
SimTime Empirical::mean() const {
    // NOT the average of the observations. draw() interpolates linearly between
    // consecutive order statistics, so the distribution it actually samples has
    // mean = the TRAPEZOIDAL integral of the inverse CDF:
    //
    //     mean = (1 / (n-1)) * SUM over segments of (x[i] + x[i+1]) / 2
    //
    // which weights the interior points twice and the two extremes once. For a
    // symmetric sample the two agree; for a skewed one they differ visibly, and
    // since this number feeds the stability check it has to be the mean of what
    // is drawn rather than the mean of what was measured.
    const std::size_t n = m_sorted.size();
    SimTime total = 0.0;
    for (std::size_t i = 0; i + 1 < n; ++i) total += 0.5 * (m_sorted[i] + m_sorted[i + 1]);
    return total / static_cast<SimTime>(n - 1);
}

Poisson::Poisson(double mean) : m_mean(mean) {
    if (mean < 0.0) throw ModelError("Poisson: mean must be >= 0");
}
SimTime Poisson::draw(RandomStream& rng) {
    return static_cast<SimTime>(pick(rng).poisson(m_mean));
}
std::string Poisson::describe() const {
    std::ostringstream os; os << "Poisson(mean=" << m_mean << ")"; return os.str();
}
SimTime Poisson::mean() const { return m_mean; }


// v9: clone(), so a distribution can be described once and used twice.
std::unique_ptr<IDistribution> Exponential::clone() const { return std::make_unique<Exponential>(m_mean); }
std::unique_ptr<IDistribution> Constant::clone() const { return std::make_unique<Constant>(m_value); }
std::unique_ptr<IDistribution> Uniform::clone() const { return std::make_unique<Uniform>(m_low, m_high); }
std::unique_ptr<IDistribution> Triangular::clone() const { return std::make_unique<Triangular>(m_low, m_mode, m_high); }
std::unique_ptr<IDistribution> Deterministic::clone() const { return std::make_unique<Deterministic>(m_values, m_repeat); }
std::unique_ptr<IDistribution> Normal::clone() const { return std::make_unique<Normal>(m_mean, m_sd, m_truncate); }
std::unique_ptr<IDistribution> Lognormal::clone() const { return std::make_unique<Lognormal>(m_logMean, m_logSd); }
std::unique_ptr<IDistribution> Weibull::clone() const { return std::make_unique<Weibull>(m_scale, m_shape); }
std::unique_ptr<IDistribution> Erlang::clone() const { return std::make_unique<Erlang>(m_meanEach, m_phases); }
std::unique_ptr<IDistribution> Poisson::clone() const { return std::make_unique<Poisson>(m_mean); }
std::unique_ptr<IDistribution> Discrete::clone() const {
    // Rebuild the per-value probabilities from the cumulative ones.
    std::vector<double> p;
    double prev = 0.0;
    for (double c : m_cumulative) { p.push_back(c - prev); prev = c; }
    return std::make_unique<Discrete>(m_values, p);
}
std::unique_ptr<IDistribution> Empirical::clone() const {
    return std::make_unique<Empirical>(m_sorted);
}

}  // namespace des
