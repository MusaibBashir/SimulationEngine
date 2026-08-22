// ============================================================================
// Experiment.cpp
// ============================================================================

#include "Experiment.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>

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

void Experiment::run() {
    m_results.clear();
    m_series.clear();
    m_results.reserve(static_cast<std::size_t>(m_replications));

    for (int r = 0; r < m_replications; ++r) {
        // *** A DIFFERENT SEED PER REPLICATION, DERIVED FROM ONE BASE SEED. ***
        // Different, or every run is the same run and the "sample" has no
        // variance at all. Derived, so the whole experiment is reproducible from
        // a single number.
        const unsigned seed = m_baseSeed + static_cast<unsigned>(r);

        SimulationSystem sim(seed);
        m_build(sim);                       // caller builds the model
        if (m_warmUp > 0.0)          sim.setWarmUp(m_warmUp);
        if (m_observeInterval > 0.0) sim.setObservationInterval(m_observeInterval);

        sim.initialise();
        sim.run();

        const Station& entry = sim.model().stationAt(0);
        const SimTime measured = sim.measuredTime();

        ReplicationResult res;
        res.seed                = seed;
        res.served              = sim.statistics().numberServed();
        res.averageWait         = sim.statistics().averageWaitingTime();
        res.averageTimeInSystem = sim.statistics().averageTimeInSystem();
        res.Lq                  = sim.statistics().timeAverageA(measured);
        res.L                   = sim.statistics().timeAverageB(measured);
        res.utilisation         = entry.stats().utilisation(measured, entry.resource().capacity());
        res.measuredTime        = measured;
        m_results.push_back(res);

        if (m_observeInterval > 0.0) m_series.push_back(sim.observations());
    }
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

SimTime Experiment::suggestWarmUp(int window, double tolerance) const {
    const std::vector<double> s = welchAverages(window);
    if (s.size() < 10) return 0.0;

    // Compare against the mean of the last third, taken as "settled".
    const std::size_t tailStart = s.size() * 2 / 3;
    double tail = 0.0;
    for (std::size_t k = tailStart; k < s.size(); ++k) tail += s[k];
    tail /= static_cast<double>(s.size() - tailStart);
    if (tail <= 0.0) return 0.0;

    // Walk forward to the first index from which the series STAYS inside the
    // band. "Stays" matters: the transient crosses the steady-state level on its
    // way up, so the first touch is far too early an answer.
    std::size_t settled = 0;
    for (std::size_t k = 0; k < tailStart; ++k) {
        if (std::fabs(s[k] - tail) / tail > tolerance) settled = k + 1;
    }
    return static_cast<SimTime>(settled) * m_observeInterval;
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
