// ============================================================================
// tests/harness.hpp  --  the twenty-line test harness, shared
// ============================================================================
// Extracted in v10. tests.cpp was 1650 lines and the expression layer adds
// another 600; one file holding both would be the largest file in the project
// by a factor of three. Still no external framework -- see the note at the top
// of tests.cpp for why.

#pragma once
#include <string>

namespace des_test {

extern int g_checks;
extern int g_failures;

void check(bool condition, const std::string& what);
void checkClose(double got, double want, double tol, const std::string& what);
void section(const char* name);

// Where the shipped .des models are. Compiled in rather than assumed
// relative to the working directory: the suite is run from the repository
// root, but not by every caller. Shared because two test files now read
// those models and one copy of the path rule is enough.
std::string modelPath(const std::string& file);

}  // namespace des_test
