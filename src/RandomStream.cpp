// ============================================================================
// RandomStream.cpp
// ============================================================================

#include "RandomStream.hpp"
#include <cassert>

namespace des {


RandomStream::RandomStream(unsigned seed)
    : m_seed(seed), m_engine(seed) {}

void RandomStream::reset() {
    m_engine.seed(m_seed);
}

SimTime RandomStream::exponential(SimTime mean) {
    assert(mean > 0.0);
    // The distribution wants the RATE, so invert the mean here -- the single
    // place in the program where that conversion is allowed to happen.
    std::exponential_distribution<SimTime> dist(1.0 / mean);
    return dist(m_engine);
}

SimTime RandomStream::uniform(SimTime a, SimTime b) {
    assert(b >= a);
    std::uniform_real_distribution<SimTime> dist(a, b);
    return dist(m_engine);
}

std::size_t RandomStream::uniformIndex(std::size_t n) {
    assert(n > 0);
    std::uniform_int_distribution<std::size_t> dist(0, n - 1);
    return dist(m_engine);
}

}  // namespace des
