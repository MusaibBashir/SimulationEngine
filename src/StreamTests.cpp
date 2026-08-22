// ============================================================================
// StreamTests.cpp
// ============================================================================

#include "StreamTests.hpp"
#include "RandomStream.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace des {

double StreamTests::chiSquareCritical95(int df) {
    // Upper 5% points of the chi-square distribution. A table for the small
    // values, and the Wilson-Hilferty approximation beyond -- exact enough that
    // a verdict never turns on it.
    static const double table[] = {
        0.0,   3.841,  5.991,  7.815,  9.488, 11.070, 12.592, 14.067, 15.507,
        16.919, 18.307, 19.675, 21.026, 22.362, 23.685, 24.996, 26.296, 27.587,
        28.869, 30.144, 31.410, 32.671, 33.924, 35.172, 36.415, 37.652, 38.885,
        40.113, 41.337, 42.557, 43.773
    };
    if (df <= 0) return 0.0;
    if (df <= 30) return table[df];
    const double d = static_cast<double>(df);
    const double t = 1.0 - 2.0 / (9.0 * d) + 1.6449 * std::sqrt(2.0 / (9.0 * d));
    return d * t * t * t;
}

TestResult StreamTests::chiSquareUniformity(RandomStream& rng, int n, int bins) {
    std::vector<long long> counts(static_cast<std::size_t>(bins), 0);
    for (int i = 0; i < n; ++i) {
        std::size_t b = static_cast<std::size_t>(rng.u01() * bins);
        if (b >= counts.size()) b = counts.size() - 1;
        ++counts[b];
    }
    const double expected = static_cast<double>(n) / bins;
    double chi = 0.0;
    for (long long c : counts) {
        const double d = static_cast<double>(c) - expected;
        chi += d * d / expected;
    }
    TestResult r;
    r.name      = "chi-square uniformity";
    r.statistic = chi;
    r.critical  = chiSquareCritical95(bins - 1);
    r.passed    = chi <= r.critical;
    r.note      = std::to_string(bins) + " bins, " + std::to_string(n) + " values";
    return r;
}

TestResult StreamTests::kolmogorovSmirnov(RandomStream& rng, int n) {
    std::vector<double> u;
    u.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) u.push_back(rng.u01());
    std::sort(u.begin(), u.end());

    double dPlus = 0.0, dMinus = 0.0;
    for (int i = 0; i < n; ++i) {
        const double frac = static_cast<double>(i + 1) / n;
        dPlus  = std::max(dPlus,  frac - u[static_cast<std::size_t>(i)]);
        dMinus = std::max(dMinus, u[static_cast<std::size_t>(i)] - static_cast<double>(i) / n);
    }
    const double d = std::max(dPlus, dMinus);

    TestResult r;
    r.name      = "Kolmogorov-Smirnov";
    r.statistic = d * std::sqrt(static_cast<double>(n));   // scaled, so the
                                                           // critical value does
                                                           // not depend on n
    r.critical  = 1.358;                                   // 95% point of the KS
    r.passed    = r.statistic <= r.critical;               // limiting distribution
    r.note      = "largest gap between empirical and true CDF";
    return r;
}

TestResult StreamTests::runsUpAndDown(RandomStream& rng, int n) {
    double prev = rng.u01();
    long long runs = 1;
    bool rising = false, started = false;
    for (int i = 1; i < n; ++i) {
        const double cur = rng.u01();
        const bool up = cur > prev;
        if (!started) { rising = up; started = true; }
        else if (up != rising) { ++runs; rising = up; }
        prev = cur;
    }
    // For an independent sequence the run count is approximately normal with
    // mean (2n-1)/3 and variance (16n-29)/90.
    const double mean = (2.0 * n - 1.0) / 3.0;
    const double var  = (16.0 * n - 29.0) / 90.0;
    const double z    = (runs - mean) / std::sqrt(var);

    TestResult r;
    r.name      = "runs up and down";
    r.statistic = std::fabs(z);
    r.critical  = 1.96;
    r.passed    = r.statistic <= r.critical;
    r.note      = std::to_string(runs) + " runs, expected about " +
                  std::to_string(static_cast<long long>(mean));
    return r;
}

TestResult StreamTests::autocorrelation(RandomStream& rng, int n, int lag) {
    std::vector<double> u;
    u.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) u.push_back(rng.u01());

    double mean = 0.0;
    for (double v : u) mean += v;
    mean /= n;

    double num = 0.0, den = 0.0;
    for (int i = 0; i + lag < n; ++i)
        num += (u[static_cast<std::size_t>(i)] - mean) * (u[static_cast<std::size_t>(i + lag)] - mean);
    for (double v : u) den += (v - mean) * (v - mean);
    const double rho = (den > 0.0) ? num / den : 0.0;

    // For an independent sequence rho is approximately N(0, 1/n).
    const double z = rho * std::sqrt(static_cast<double>(n));

    TestResult r;
    r.name      = "autocorrelation at lag " + std::to_string(lag);
    r.statistic = std::fabs(z);
    r.critical  = 1.96;
    r.passed    = r.statistic <= r.critical;
    r.note      = "rho = " + std::to_string(rho);
    return r;
}

TestResult StreamTests::serial3D(RandomStream& rng, int n, int binsPerAxis) {
    const std::size_t cells = static_cast<std::size_t>(binsPerAxis) * binsPerAxis * binsPerAxis;
    std::vector<long long> counts(cells, 0);
    const int triples = n / 3;
    for (int i = 0; i < triples; ++i) {
        auto bin = [&](double v) {
            std::size_t b = static_cast<std::size_t>(v * binsPerAxis);
            return b >= static_cast<std::size_t>(binsPerAxis)
                 ? static_cast<std::size_t>(binsPerAxis) - 1 : b;
        };
        const std::size_t x = bin(rng.u01());
        const std::size_t y = bin(rng.u01());
        const std::size_t z = bin(rng.u01());
        ++counts[(x * binsPerAxis + y) * binsPerAxis + z];
    }
    const double expected = static_cast<double>(triples) / static_cast<double>(cells);
    double chi = 0.0;
    long long empty = 0;
    for (long long c : counts) {
        if (c == 0) ++empty;
        const double d = static_cast<double>(c) - expected;
        chi += d * d / expected;
    }

    TestResult r;
    r.name      = "serial test in 3D";
    r.statistic = chi;
    r.critical  = chiSquareCritical95(static_cast<int>(cells) - 1);
    r.passed    = chi <= r.critical;
    r.note      = std::to_string(empty) + " of " + std::to_string(cells) +
                  " cells never visited";
    return r;
}

std::vector<TestResult> StreamTests::runAll(RandomStream& rng, int n) {
    std::vector<TestResult> out;
    // Each test resets first, so they all see the SAME stream rather than
    // continuing where the previous one stopped. Otherwise a verdict would
    // depend on the order the tests happened to run in.
    rng.reset(); out.push_back(chiSquareUniformity(rng, n));
    rng.reset(); out.push_back(kolmogorovSmirnov(rng, n));
    rng.reset(); out.push_back(runsUpAndDown(rng, n));
    rng.reset(); out.push_back(autocorrelation(rng, n, 1));
    rng.reset(); out.push_back(autocorrelation(rng, n, 5));
    rng.reset(); out.push_back(serial3D(rng, n));
    rng.reset();
    return out;
}

void StreamTests::report(const std::vector<TestResult>& results, const std::string& title) {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "--- " << title << " " << std::string(
        title.size() < 48 ? 48 - title.size() : 1, '-') << "\n";
    for (const TestResult& r : results) {
        std::cout << "  " << std::setw(26) << std::left << r.name << std::right
                  << std::setw(12) << r.statistic
                  << "  vs " << std::setw(10) << r.critical
                  << "   " << (r.passed ? "pass" : "FAIL")
                  << "   " << r.note << "\n";
    }
}

}  // namespace des
