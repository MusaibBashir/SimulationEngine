// ============================================================================
// Distribution.cpp
// ============================================================================

#include "Distribution.hpp"
#include "RandomStream.hpp"
#include <cassert>
#include <cmath>
#include <sstream>

namespace des {


// ------------------------------------------------------------- Exponential --
Exponential::Exponential(SimTime mean) : m_mean(mean) { assert(mean > 0.0); }

SimTime Exponential::draw(RandomStream& rng) { return rng.exponential(m_mean); }

SimTime Exponential::mean() const { return m_mean; }

std::string Exponential::describe() const {
    std::ostringstream os; os << "Exponential(mean=" << m_mean << ")"; return os.str();
}

// ---------------------------------------------------------------- Constant --
Constant::Constant(SimTime value) : m_value(value) { assert(value >= 0.0); }

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

SimTime Uniform::draw(RandomStream& rng) { return rng.uniform(m_low, m_high); }

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
    // Inverse-transform sampling. The CDF of a triangular distribution is
    // piecewise quadratic, so inverting it splits at the mode.
    const SimTime u = rng.uniform(0.0, 1.0);
    const SimTime c = (m_high > m_low) ? (m_mode - m_low) / (m_high - m_low) : 0.0;
    if (u < c) {
        return m_low + std::sqrt(u * (m_high - m_low) * (m_mode - m_low));
    }
    return m_high - std::sqrt((1.0 - u) * (m_high - m_low) * (m_high - m_mode));
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

}  // namespace des
