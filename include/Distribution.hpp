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
