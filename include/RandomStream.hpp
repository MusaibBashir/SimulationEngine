// ============================================================================
// RandomStream.hpp  --  the source of all randomness (NEW IN v2)
// ============================================================================
// A discrete-event simulation is a random experiment. Every draw in the whole
// program must come through ONE object, for one reason: REPRODUCIBILITY. Given
// the same seed, the same run must produce byte-identical output, or you cannot
// debug it and you cannot compare two configurations honestly.
//
// That is why there is no std::rand() anywhere, and no engine tucked away
// inside Resource or EntityQueue. One stream, owned by SimulationSystem,
// handed to anyone who needs it.

#pragma once

#include <random>
#include <cstddef>
#include "Common.hpp"

class RandomStream {
private:
    unsigned     m_seed;
    std::mt19937 m_engine;   // Mersenne Twister: the standard workhorse.
                             // NOT std::rand() -- that has a tiny period, poor
                             // spectral properties, and no way to seed two
                             // independent streams.

public:
    explicit RandomStream(unsigned seed = 12345u);

    unsigned seed() const { return m_seed; }

    // Re-seed back to the original value, for v4 replications.
    void reset();

    // *** PARAMETERISED BY MEAN, NOT BY RATE. ***
    // std::exponential_distribution takes lambda (the RATE). Queueing theory
    // and your notes talk in means (1/lambda). Mixing the two up gives results
    // that are wrong by a factor of mean^2 and still look plausible, which is
    // the worst kind of wrong. The conversion happens HERE, once.
    SimTime exponential(SimTime mean);

    SimTime uniform(SimTime a, SimTime b);

    // Uniform index in [0, n). Used by EntityQueue's Random discipline.
    std::size_t uniformIndex(std::size_t n);
};
