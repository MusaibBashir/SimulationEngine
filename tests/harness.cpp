#include "harness.hpp"
#include <cmath>
#include <iostream>

// On Windows a failed assert() or an abort() opens a MODAL DIALOG and waits
// for a human. In an automated run nothing is there to click it, so the suite
// looks like it hung and the actual message is never seen. Send those reports
// to stderr instead, where the failure can be read.
#ifdef _MSC_VER
#include <crtdbg.h>
#include <cstdlib>
namespace {
struct FailLoudlyNotModally {
    // Touching std::cout from a static constructor is only safe once
    // std::ios_base::Init has run. Declaring one HERE, before the members that
    // use it, is the documented way to guarantee that -- without it the order
    // across translation units is unspecified, and "unspecified" showed up as
    // a test binary that hung before printing a single line.
    std::ios_base::Init m_ioGuard;

    FailLoudlyNotModally() {
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        for (int report : {_CRT_ASSERT, _CRT_ERROR, _CRT_WARN}) {
            _CrtSetReportMode(report, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(report, _CRTDBG_FILE_STDERR);
        }
        // abort() does NOT flush stdio. Redirected to a file the output is
        // fully buffered, so an abort discards every line printed before it and
        // the run looks like it died at the first check. Flushing per insertion
        // is slow and is the only way the last section printed is the section
        // that actually failed.
        std::cout << std::unitbuf;
        std::cerr << std::unitbuf;
    }
};
const FailLoudlyNotModally g_failLoudly;
}  // namespace
#endif

namespace des_test {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "\n";
    }
}

void checkClose(double got, double want, double tol, const std::string& what) {
    ++g_checks;
    if (std::fabs(got - want) > tol) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "  (got " << got << ", want " << want << ")\n";
    }
}

void section(const char* name) { std::cout << "[" << name << "]\n"; }

}  // namespace des_test
