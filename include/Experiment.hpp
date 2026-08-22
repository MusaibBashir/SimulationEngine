// ============================================================================
// Experiment.hpp  --  v4: how confident are we?
// ============================================================================
// Wq has read 3-5% above closed-form theory since v2, and every version has
// said "that's v4". This is v4.
//
// The honest position is that a SINGLE RUN OF A SIMULATION IS ONE SAMPLE FROM A
// RANDOM VARIABLE. Reporting it as "3.2864" implies a precision that does not
// exist. Two things fix that:
//
//   1. WARM-UP REMOVAL. The run starts empty and idle, which is not the steady
//      state. Those early low-queue observations drag the average down (or, for
//      a heavily loaded system, the opposite). Discard the transient.
//
//   2. REPLICATIONS + A CONFIDENCE INTERVAL. Run N times with different seeds,
//      treat the N results as a sample, and report a range. Only then can you
//      ask "does 3.2000 fall inside?" and get a meaningful answer.
//
// Neither is a code fix. The simulation was never wrong; the REPORTING was.

#pragma once

#include <functional>
#include <string>
#include <vector>
#include "Common.hpp"
#include "SimulationSystem.hpp"

// What one replication produced. Everything here is a sample from a
// distribution, which is the whole reason this struct exists rather than the
// numbers being printed straight out.
struct ReplicationResult {
    unsigned seed{0};
    int      served{0};
    double   averageWait{0.0};
    double   averageTimeInSystem{0.0};
    double   Lq{0.0};             // time-average number in queue
    double   L{0.0};              // time-average number in system
    double   utilisation{0.0};    // of the entry station
    SimTime  measuredTime{0.0};
};

// Sample mean, sample standard deviation, and a Student-t confidence interval.
// Separated from Experiment because it is pure arithmetic on a vector of
// doubles -- which also makes it the easiest thing in the project to unit test.
class Summary {
public:
    static double mean(const std::vector<double>& xs);

    // *** SAMPLE standard deviation: divide by (n-1), not n. ***
    // Dividing by n gives the standard deviation OF THE SAMPLE, which
    // systematically underestimates the population's. With 10 replications that
    // is an 5% error in the wrong direction -- it makes your confidence interval
    // narrower than the truth, which is the one direction you must never err.
    static double stdDev(const std::vector<double>& xs);

    // Standard error of the mean = s / sqrt(n).
    static double standardError(const std::vector<double>& xs);

    // Half-width of a 95% confidence interval: t(n-1, 0.975) * s / sqrt(n).
    // Uses the t distribution, NOT 1.96, because n is small. At n=10 the t value
    // is 2.262 -- using 1.96 would make the interval 13% too narrow.
    static double halfWidth95(const std::vector<double>& xs);

    // The t table itself, exposed so it can be tested directly.
    static double tCritical95(int degreesOfFreedom);
};

class Experiment {
public:
    // The caller supplies a function that builds the model into a fresh system.
    // std::function rather than an abstract ModelBuilder class: there is exactly
    // one implementation shape (a lambda closing over parameters), and a
    // hierarchy for that would be ceremony.
    using Builder = std::function<void(SimulationSystem&)>;

private:
    Builder  m_build;
    int      m_replications{10};
    unsigned m_baseSeed{1000u};
    SimTime  m_warmUp{0.0};
    SimTime  m_observeInterval{0.0};
    std::string m_name{"experiment"};

    std::vector<ReplicationResult> m_results;

    // observations[r][k] = number in system at grid point k in replication r.
    std::vector<std::vector<double>> m_series;

public:
    Experiment(std::string name, Builder build);

    Experiment& replications(int n);
    Experiment& baseSeed(unsigned s);
    Experiment& warmUp(SimTime t);
    Experiment& observeEvery(SimTime dt);

    void run();

    const std::vector<ReplicationResult>& results() const { return m_results; }

    // Pull one field out of every replication, ready for Summary.
    std::vector<double> column(double ReplicationResult::* field) const;

    // --- Welch's method ---------------------------------------------------
    // Average the observation series across replications, index by index, then
    // smooth with a moving average of half-width w. Averaging first is what
    // kills the noise; smoothing second is what makes the trend visible.
    std::vector<double> welchAverages(int window) const;

    // Where the smoothed series first settles within `tolerance` (relative) of
    // its own tail mean, expressed as a TIME.
    //
    // This is a HEURISTIC, and Welch's method is genuinely meant to be finished
    // by eye on a plot. Treat the number as a starting suggestion, then look at
    // the series -- writeWelchSeries() dumps it as CSV for exactly that.
    SimTime suggestWarmUp(int window, double tolerance = 0.05) const;

    bool writeWelchSeries(const std::string& path, int window) const;

    void report() const;
};
