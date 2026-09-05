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
    FailLoudlyNotModally() {
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        for (int report : {_CRT_ASSERT, _CRT_ERROR, _CRT_WARN}) {
            _CrtSetReportMode(report, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(report, _CRTDBG_FILE_STDERR);
        }
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
