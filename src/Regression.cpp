#include "Regression.hpp"

#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include "Compiler.hpp"
#include "DocumentFormat.hpp"
#include "RunController.hpp"

namespace des {
namespace {

std::string stemOf(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    const std::size_t stop = (dot == std::string::npos || dot < start) ? path.size() : dot;
    return path.substr(start, stop - start);
}

// is_open(), NOT good(). On the GCC this project builds with, constructing an
// ifstream on a file that does not exist leaves good() reporting TRUE while
// is_open() reports false -- so `found` was true for every missing
// expectation, and --capture refused to capture anything, ever. A harness that
// can never record an expectation is not a harness.
//
// is_open() is the unambiguous question anyway: "did this file open", not "is
// this stream in a usable state".
std::string readFile(const std::string& path, bool& found) {
    std::ifstream in(path, std::ios::binary);
    found = in.is_open();
    if (!found) return std::string();
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

std::string firstDifference(const std::string& want, const std::string& got) {
    std::istringstream a(want), b(got);
    std::string la, lb;
    for (int line = 1; ; ++line) {
        la.clear();
        lb.clear();
        const bool gotA = static_cast<bool>(std::getline(a, la));
        const bool gotB = static_cast<bool>(std::getline(b, lb));
        if (!gotA && !gotB) return "identical";
        if (la != lb) {
            std::ostringstream os;
            os << "line " << line << ": expected [" << la << "] got [" << lb << "]";
            return os.str();
        }
    }
}

std::vector<std::string> readManifest(const std::string& dir, bool& found) {
    std::vector<std::string> models;
    std::ifstream in(dir + "/manifest");
    found = in.is_open();
    if (!found) return models;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        models.push_back(line);
    }
    return models;
}

}  // namespace

RegressionReport runRegression(const std::string& dir, bool capture, bool force) {
    RegressionReport report;
    bool haveManifest = false;
    const std::vector<std::string> models = readManifest(dir, haveManifest);
    if (!haveManifest) {
        report.outcomes.push_back(RegressionOutcome{dir + "/manifest",
                                                    "no manifest", false, false});
        return report;
    }

    for (const std::string& path : models) {
        RegressionOutcome out;
        out.model = path;
        ++report.cases;

        ReadResult read = readDocumentFile(path);
        if (hasErrors(read.diagnostics)) {
            out.detail = "does not read";
            report.outcomes.push_back(out);
            ++report.failures;
            continue;
        }
        // Every regression model must say how to run it. An expectation whose
        // meaning depends on a horizon stored somewhere else stops being an
        // expectation the moment that horizon changes.
        if (read.document.rowCount("Run") == 0) {
            out.detail = "no [Run] module";
            report.outcomes.push_back(out);
            ++report.failures;
            continue;
        }
        std::vector<Diagnostic> problems;
        std::unique_ptr<RunController> run =
            RunController::fromDocument(read.document, problems);
        if (run == nullptr) {
            out.detail = "does not compile";
            report.outcomes.push_back(out);
            ++report.failures;
            continue;
        }

        run->runToCompletion();
        std::ostringstream got;
        run->report(got);

        const std::string wantPath = dir + "/" + stemOf(path) + ".expected";
        bool exists = false;
        const std::string want = readFile(wantPath, exists);
        out.hadExpectation = exists;

        if (capture) {
            if (exists && !force) {
                out.detail = "expectation exists; --force to replace";
                ++report.refused;
                report.outcomes.push_back(out);
                continue;
            }
            std::ofstream w(wantPath, std::ios::binary);
            w << got.str();
            out.matched = true;
            out.detail  = "captured";
            ++report.captured;
            report.outcomes.push_back(out);
            continue;
        }

        if (!exists) {
            out.detail = "no expectation stored";
            ++report.missing;
            report.outcomes.push_back(out);
            continue;
        }
        out.matched = (want == got.str());
        out.detail  = out.matched ? "identical" : firstDifference(want, got.str());
        if (!out.matched) ++report.failures;
        report.outcomes.push_back(out);
    }
    return report;
}

}  // namespace des
