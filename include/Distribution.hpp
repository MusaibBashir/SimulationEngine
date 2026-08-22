// ============================================================================
// Distribution.hpp  --  v3 step 1: sampling behind an interface
// ============================================================================
// v2 hardcoded m_rng.exponential(mean) into both event handlers. Real models
// need constant service times, uniform ones, triangular ones, and -- most
// useful for learning -- DETERMINISTIC ones, so a run can be checked against a
// table worked by hand.
//
// This is the first abstraction in the project, and it is first because the
// interface is obvious: something that produces a SimTime. Get the virtual
// destructor rule right here, where it is easy, before anything depends on it.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include "Common.hpp"

namespace des {


class RandomStream;

class IDistribution {
public:
    // *** WRITE THE VIRTUAL DESTRUCTOR. ***
    // Without it, `delete` through an IDistribution* runs only the base
    // destructor. The derived part is never destroyed -- no error, no warning,
    // just a leak that grows. unique_ptr<IDistribution> does exactly that
    // delete, so this one line is what makes the whole design safe.
    virtual ~IDistribution() = default;

    // NOT const. A Deterministic distribution walks a cursor through a list, so
    // drawing changes it. Declaring this const would have forced `mutable` on
    // the cursor, which is a lie told to the compiler. Let the signature admit
    // that sampling is a mutating operation.
    virtual SimTime draw(RandomStream& rng) = 0;

    virtual std::string describe() const = 0;

    // Put the distribution back to its starting condition. Only Deterministic
    // has anything to do; the base gives everyone else the correct no-op, which
    // is the difference between a pure virtual and a virtual with a default.
    virtual void reset() {}

    // v5: the theoretical mean. The engine uses it to compute the offered load
    // of each station BEFORE the run and warn you if the system is unstable --
    // which is the single most common modelling mistake, and one that otherwise
    // shows up as confident four-decimal nonsense.
    virtual SimTime mean() const = 0;

    // v8: draw from THIS distribution's own stream if it has been given one,
    // otherwise from the shared stream passed in. Giving each distribution its
    // own stream is what makes two model variants comparable: change the
    // service time and the arrival pattern does not shift underneath you.
    void useStream(RandomStream* s) { m_stream = s; }
    RandomStream* stream() const { return m_stream; }

protected:
    RandomStream* m_stream{nullptr};
    // Every subclass draws through this rather than touching `rng` directly.
    RandomStream& pick(RandomStream& fallback) const {
        return m_stream ? *m_stream : fallback;
    }
};

// ----------------------------------------------------------------------------

class Exponential : public IDistribution {
    SimTime m_mean;
public:
    explicit Exponential(SimTime mean);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

class Constant : public IDistribution {
    SimTime m_value;
public:
    explicit Constant(SimTime value);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

class Uniform : public IDistribution {
    SimTime m_low, m_high;
public:
    Uniform(SimTime low, SimTime high);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

class Triangular : public IDistribution {
    SimTime m_low, m_mode, m_high;
public:
    Triangular(SimTime low, SimTime mode, SimTime high);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

// --- v8 additions -----------------------------------------------------------

// Bell-shaped and symmetric. *** Truncated at zero when used for a duration ***
// -- a normal has a left tail that goes negative, and a negative service time is
// not a modelling subtlety, it is nonsense. Set `truncateAtZero` and it clamps;
// leave it and it throws if a negative is drawn, so you find out.
class Normal : public IDistribution {
    SimTime m_mean, m_sd;
    bool m_truncate;
public:
    Normal(SimTime mean, SimTime stdDev, bool truncateAtZero = true);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

// Right-skewed and never negative, which is what most service times actually
// look like. Parameters are the mean and sd OF THE LOGARITHM -- the usual
// convention and the usual source of confusion, so `fromMeanAndSd` builds one
// from the mean and sd you actually observed.
class Lognormal : public IDistribution {
    SimTime m_logMean, m_logSd;
public:
    Lognormal(SimTime logMean, SimTime logStdDev);
    static std::unique_ptr<Lognormal> fromMeanAndSd(SimTime mean, SimTime sd);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

// The reliability workhorse: shape < 1 means failures get rarer with age
// (infant mortality), shape = 1 is exponential, shape > 1 means wear-out.
class Weibull : public IDistribution {
    SimTime m_scale, m_shape;
public:
    Weibull(SimTime scale, SimTime shape);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

// k sequential exponential phases. Fills the gap between constant (k -> inf)
// and exponential (k = 1), which is where most real service times live.
class Erlang : public IDistribution {
    SimTime m_meanEach;
    int m_phases;
public:
    Erlang(SimTime meanOfEachPhase, int phases);
    static std::unique_ptr<Erlang> fromMean(SimTime totalMean, int phases);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

// A finite set of values with given probabilities -- batch sizes, part types,
// "70% small, 20% medium, 10% large". Inverse transform over the cumulative
// mass, so it stays antithetic-friendly.
class Discrete : public IDistribution {
    std::vector<SimTime> m_values;
    std::vector<double> m_cumulative;
public:
    Discrete(std::vector<SimTime> values, std::vector<double> probabilities);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

// Sample straight from DATA you measured, with linear interpolation between the
// observed order statistics. When you have a hundred real service times and no
// idea which textbook distribution they came from, this beats guessing.
class Empirical : public IDistribution {
    std::vector<SimTime> m_sorted;
public:
    explicit Empirical(std::vector<SimTime> observations);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

// Counts, not durations: arrivals per hour, defects per batch.
class Poisson : public IDistribution {
    double m_mean;
public:
    explicit Poisson(double mean);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
};

// The one that makes hand-checking possible: hand it the exact sequence from a
// worked example in your notes and the simulation becomes deterministic, so you
// can compare event for event instead of comparing averages of random draws.
class Deterministic : public IDistribution {
    std::vector<SimTime> m_values;
    std::size_t m_cursor;
    bool m_repeat;      // wrap around at the end, or assert on running out
public:
    explicit Deterministic(std::vector<SimTime> values, bool repeat = true);
    SimTime draw(RandomStream& rng) override;
    std::string describe() const override;
    SimTime mean() const override;
    void reset() override;
};

}  // namespace des
