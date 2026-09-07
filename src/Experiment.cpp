// ============================================================================
// Experiment.cpp
// ============================================================================

#include "Experiment.hpp"
#include "RunController.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace des {


// ------------------------------------------------------------------ Summary --

double Summary::mean(const std::vector<double>& xs) {
    if (xs.empty()) return 0.0;
    return std::accumulate(xs.begin(), xs.end(), 0.0) / static_cast<double>(xs.size());
}

double Summary::stdDev(const std::vector<double>& xs) {
    const std::size_t n = xs.size();
    if (n < 2) return 0.0;             // one sample says nothing about spread
    const double m = mean(xs);
    double acc = 0.0;
    for (double x : xs) { const double d = x - m; acc += d * d; }
    return std::sqrt(acc / static_cast<double>(n - 1));   // n-1: SAMPLE std dev
}

double Summary::standardError(const std::vector<double>& xs) {
    if (xs.size() < 2) return 0.0;
    return stdDev(xs) / std::sqrt(static_cast<double>(xs.size()));
}

double Summary::tCritical95(int df) {
    // Two-sided 95%, i.e. t(df, 0.975). A table rather than a computed inverse
    // CDF: the values are fixed, twenty of them cover every sensible number of
    // replications, and a table is impossible to get subtly wrong.
    static const double t[] = {
        0.0,                                            // df = 0, unused
        12.706, 4.303, 3.182, 2.776, 2.571,             // 1..5
        2.447,  2.365, 2.306, 2.262, 2.228,             // 6..10
        2.201,  2.179, 2.160, 2.145, 2.131,             // 11..15
        2.120,  2.110, 2.101, 2.093, 2.086,             // 16..20
        2.080,  2.074, 2.069, 2.064, 2.060,             // 21..25
        2.056,  2.052, 2.048, 2.045, 2.042              // 26..30
    };
    if (df <= 0) return 0.0;
    if (df <= 30) return t[df];
    return 1.96;   // beyond 30 the t distribution is close enough to normal
}

double Summary::halfWidth95(const std::vector<double>& xs) {
    if (xs.size() < 2) return 0.0;
    return tCritical95(static_cast<int>(xs.size()) - 1) * standardError(xs);
}

// --------------------------------------------------------------- Experiment --

Experiment::Experiment(std::string name, Builder build)
    : m_build(std::move(build)), m_name(std::move(name)) {
    assert(m_build != nullptr);
}

Experiment& Experiment::replications(int n) { assert(n > 0); m_replications = n; return *this; }
Experiment& Experiment::baseSeed(unsigned s) { m_baseSeed = s; return *this; }
Experiment& Experiment::warmUp(SimTime t) { assert(t >= 0.0); m_warmUp = t; return *this; }
Experiment& Experiment::observeEvery(SimTime dt) { assert(dt > 0.0); m_observeInterval = dt; return *this; }
Experiment& Experiment::separateStreams(bool on) { m_separateStreams = on; return *this; }
Experiment& Experiment::antitheticPairs(bool on) { m_antithetic = on; return *this; }

void Experiment::run() {
    // ONE implementation of the replication loop, two callers -- exactly the
    // move v11 made with validate() and checkStructure(). The alternative was
    // two loops that must agree about seeding, antithetic pairing, warm-up and
    // the observation grid, and the one that drifts is always the one nobody
    // runs.
    RunSetup setup;
    setup.replications    = m_replications;
    setup.baseSeed        = m_baseSeed;
    setup.warmUp          = m_warmUp;
    setup.observeInterval = m_observeInterval;
    setup.separateStreams = m_separateStreams;
    setup.antithetic      = m_antithetic;
    // Deliberately NO stopping rule: an Experiment's caller sets it inside the
    // builder, and RunController leaves the builder's rule alone when the setup
    // names none.

    RunController controller(setup, m_build);
    controller.runToCompletion();
    m_results = controller.results();
    m_series  = controller.series();
}

std::vector<double> Experiment::column(double ReplicationResult::* field) const {
    // A pointer-to-member, so one function serves every numeric field instead of
    // six near-identical getters. Worth knowing the syntax exists; worth using
    // it only where the alternative is genuine duplication, as here.
    std::vector<double> out;
    out.reserve(m_results.size());
    for (const auto& r : m_results) out.push_back(r.*field);
    return out;
}

std::vector<double> Experiment::waits() const { return column(&ReplicationResult::averageWait); }
std::vector<double> Experiment::timesInSystem() const { return column(&ReplicationResult::averageTimeInSystem); }
std::vector<double> Experiment::queueLengths() const { return column(&ReplicationResult::Lq); }
std::vector<double> Experiment::utilisations() const { return column(&ReplicationResult::utilisation); }

Experiment::Estimate Experiment::estimate(const std::vector<double>& xs) {
    Estimate e;
    e.mean      = Summary::mean(xs);
    e.halfWidth = Summary::halfWidth95(xs);
    return e;
}

std::vector<double> Experiment::welchAverages(int window) const {
    if (m_series.empty()) return {};

    // Every replication runs to the same time limit but takes a different number
    // of events, so trim to the SHORTEST series -- averaging index k across a
    // different number of replications for different k would make the tail of
    // the plot noisier for a reason that has nothing to do with the system.
    std::size_t len = m_series.front().size();
    for (const auto& s : m_series) len = std::min(len, s.size());
    if (len == 0) return {};

    std::vector<double> avg(len, 0.0);
    for (std::size_t k = 0; k < len; ++k) {
        double acc = 0.0;
        for (const auto& s : m_series) acc += s[k];
        avg[k] = acc / static_cast<double>(m_series.size());
    }

    // Moving average of half-width `window`, shrinking near the left edge where
    // there is not enough history. Average ACROSS replications first, THEN
    // smooth: the cross-replication average is what removes the noise, and the
    // smoothing only makes the remaining trend readable.
    std::vector<double> smooth(len, 0.0);
    for (std::size_t k = 0; k < len; ++k) {
        const std::size_t w = std::min<std::size_t>(static_cast<std::size_t>(window),
                                                    std::min(k, len - 1 - k));
        double acc = 0.0;
        for (std::size_t j = k - w; j <= k + w; ++j) acc += avg[j];
        smooth[k] = acc / static_cast<double>(2 * w + 1);
    }
    return smooth;
}

SimTime Experiment::suggestWarmUp(int window, double maxFraction) const {
    // MSER is applied to the cross-replication average, NOT to a smoothed copy.
    // Smoothing is for the human reading the plot; smoothing before MSER would
    // blur the very transient the rule is trying to locate. window defaults to 0
    // for that reason -- pass a window only if you want to experiment.
    const std::vector<double> y = welchAverages(window);
    const std::size_t n = y.size();
    if (n < 20) return 0.0;

    const std::size_t maxD = static_cast<std::size_t>(n * maxFraction);
    double bestScore = -1.0;
    std::size_t bestD = 0;

    for (std::size_t d = 0; d < maxD; ++d) {
        const std::size_t m = n - d;
        double sum = 0.0;
        for (std::size_t i = d; i < n; ++i) sum += y[i];
        const double mean = sum / static_cast<double>(m);

        double ss = 0.0;
        for (std::size_t i = d; i < n; ++i) { const double e = y[i] - mean; ss += e * e; }

        // Divide by m SQUARED, not m. m gives the variance of the retained data,
        // which is minimised by keeping almost nothing. m^2 gives the squared
        // standard error OF THE MEAN, which is what you actually want small --
        // and it is what stops the rule from discarding the entire run.
        const double score = ss / (static_cast<double>(m) * static_cast<double>(m));
        if (bestScore < 0.0 || score < bestScore) { bestScore = score; bestD = d; }
    }

    return static_cast<SimTime>(bestD) * m_observeInterval;
}

bool Experiment::writeWelchSeries(const std::string& path, int window) const {
    const std::vector<double> smooth = welchAverages(window);
    if (smooth.empty()) return false;

    std::ofstream out(path);
    if (!out) return false;
    out << "time,smoothed_number_in_system\n";
    out << std::fixed << std::setprecision(6);
    for (std::size_t k = 0; k < smooth.size(); ++k) {
        out << (static_cast<double>(k) * m_observeInterval) << "," << smooth[k] << "\n";
    }
    return true;
}

namespace {
void line(const char* label, const std::vector<double>& xs) {
    const double m  = Summary::mean(xs);
    const double hw = Summary::halfWidth95(xs);
    std::cout << "  " << std::setw(22) << std::left << label << std::right
              << std::setw(10) << m
              << "  +/- " << std::setw(8) << hw
              << "   [" << std::setw(9) << (m - hw)
              << ", " << std::setw(9) << (m + hw) << "]\n";
}
}  // namespace

Experiment::Comparison Experiment::compare(Experiment& a, Experiment& b,
                                           double ReplicationResult::* field) {
    Comparison c;
    const std::vector<double> xa = a.column(field);
    const std::vector<double> xb = b.column(field);
    const std::size_t n = std::min(xa.size(), xb.size());
    if (n < 2) return c;

    // PAIR replication i of a with replication i of b. They share a seed, so
    // with separateStreams() they saw the same arrival pattern -- and that
    // common noise cancels in the difference.
    for (std::size_t i = 0; i < n; ++i) c.differences.push_back(xa[i] - xb[i]);

    c.meanDifference = Summary::mean(c.differences);
    c.halfWidth      = Summary::halfWidth95(c.differences);
    // The interval is on the DIFFERENCES. Excluding zero means the two designs
    // really do differ; straddling zero means this much evidence cannot tell
    // them apart -- which is not the same as "they are the same".
    c.differsSignificantly = (c.meanDifference - c.halfWidth > 0.0) ||
                             (c.meanDifference + c.halfWidth < 0.0);
    return c;
}

void Experiment::report() const {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "=== experiment: " << m_name << " ===\n";
    std::cout << "replications             : " << m_results.size()
              << "  (seeds " << m_baseSeed << ".." << (m_baseSeed + m_results.size() - 1) << ")\n";
    std::cout << "warm-up discarded        : " << m_warmUp << "\n";
    if (!m_results.empty())
        std::cout << "measured period per rep  : " << m_results.front().measuredTime << "\n";
    std::cout << "\n  " << std::setw(22) << std::left << "quantity" << std::right
              << std::setw(10) << "mean" << "      95% half-width      interval\n";
    std::cout << "  ---------------------------------------------------------------------\n";
    line("average wait (Wq)",   column(&ReplicationResult::averageWait));
    line("time in system (W)",  column(&ReplicationResult::averageTimeInSystem));
    line("number in queue (Lq)", column(&ReplicationResult::Lq));
    line("number in system (L)", column(&ReplicationResult::L));
    line("utilisation (rho)",   column(&ReplicationResult::utilisation));
    std::cout << "=====================================================================\n";
}

}  // namespace des
