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

namespace des {


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
    //
    // v12: the same type RunController takes. Experiment IS a RunController
    // with a report on top, and two spellings of one function type would be two
    // things to keep in step. Declared THERE rather than here because this
    // header would otherwise have to include one that includes it back.
    using Builder = std::function<void(SimulationSystem&)>;

private:
    Builder  m_build;
    int      m_replications{10};
    unsigned m_baseSeed{1000u};
    SimTime  m_warmUp{0.0};
    SimTime  m_observeInterval{0.0};
    std::string m_name{"experiment"};
    bool m_separateStreams{false};
    bool m_antithetic{false};

    std::vector<ReplicationResult> m_results;

    // observations[r][k] = number in system at grid point k in replication r.
    std::vector<std::vector<double>> m_series;

public:
    Experiment(std::string name, Builder build);

    Experiment& replications(int n);
    Experiment& baseSeed(unsigned s);
    Experiment& warmUp(SimTime t);
    Experiment& observeEvery(SimTime dt);

    // v8: give each distribution its own stream. Costs nothing, and it is what
    // makes compare() below a PAIRED comparison rather than two unrelated ones.
    Experiment& separateStreams(bool on = true);

    // v8: ANTITHETIC VARIATES. Each replication is run twice -- once normally
    // and once with every uniform replaced by 1-u -- and the pair is averaged
    // into a single result. The two runs are negatively correlated, so the pair
    // average has lower variance than two independent runs would, for the same
    // compute. `replications(n)` then means n PAIRS, i.e. 2n runs.
    Experiment& antitheticPairs(bool on = true);

    void run();

    const std::vector<ReplicationResult>& results() const { return m_results; }

    // Pull one field out of every replication, ready for Summary.
    std::vector<double> column(double ReplicationResult::* field) const;

    // v5: named accessors. `column(&ReplicationResult::averageWait)` is precise
    // and also the sort of thing you have to look up every time. These are the
    // four columns anyone actually wants.
    std::vector<double> waits() const;
    std::vector<double> timesInSystem() const;
    std::vector<double> queueLengths() const;
    std::vector<double> utilisations() const;

    // mean and 95% half-width of one column, since that pair is always wanted
    // together and separating them invites reporting the mean on its own.
    struct Estimate { double mean{0.0}; double halfWidth{0.0};
                      double low() const { return mean - halfWidth; }
                      double high() const { return mean + halfWidth; }
                      bool covers(double v) const { return v >= low() && v <= high(); } };
    static Estimate estimate(const std::vector<double>& xs);

    // --- Welch's method ---------------------------------------------------
    // Average the observation series across replications, index by index, then
    // smooth with a moving average of half-width w. Averaging first is what
    // kills the noise; smoothing second is what makes the trend visible.
    std::vector<double> welchAverages(int window) const;

    // Suggest a truncation point automatically, by MSER (Marginal Standard
    // Error Rule): choose the d that minimises
    //
    //     MSER(d) = (1 / (n-d)^2) * SUM_{i=d..n-1} (Y_i - Ybar_d)^2
    //
    // i.e. the truncation that gives the most precise estimate of the remaining
    // mean. Discarding more data shrinks the numerator but also shrinks (n-d),
    // so the rule trades bias against variance rather than guessing.
    //
    // WHY NOT A TOLERANCE BAND: the obvious heuristic -- "the first point after
    // which the curve stays within 5% of its tail mean" -- fails on a noisy
    // series, because *some* late point always falls outside the band, and the
    // answer collapses to whatever cap you imposed. It returned two thirds of
    // the run length regardless of the run length, which is how it was caught.
    //
    // `maxFraction` refuses to discard more than that share of the data: an
    // answer near the end of the series means the run was too short to have
    // reached steady state, and should be treated as a warning, not an answer.
    //
    // Still a HEURISTIC. Welch's method is meant to be finished by eye on a
    // plot; writeWelchSeries() dumps the CSV for exactly that.
    SimTime suggestWarmUp(int window = 0, double maxFraction = 0.5) const;

    bool writeWelchSeries(const std::string& path, int window) const;

    void report() const;

    // v8: COMMON RANDOM NUMBERS. Run two model variants across the same set of
    // seeds and compare them REPLICATION BY REPLICATION, then form an interval
    // on the DIFFERENCES.
    //
    // Why this beats comparing two independent means: the sampling noise common
    // to both variants -- an unluckily busy arrival stream, say -- cancels in
    // the difference. Two designs whose separate intervals overlap can still
    // have a difference interval that excludes zero, which is a much stronger
    // statement and often the only one your compute budget can afford.
    struct Comparison {
        std::vector<double> differences;
        double meanDifference{0.0};
        double halfWidth{0.0};
        bool differsSignificantly{false};   // does the interval exclude zero?
    };
    static Comparison compare(Experiment& a, Experiment& b,
                              double ReplicationResult::* field);
};

}  // namespace des
