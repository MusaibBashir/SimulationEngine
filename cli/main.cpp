// ============================================================================
// cli/main.cpp  --  the `des` command
// ============================================================================
// Three verbs, deliberately:
//
//     des check   model.des          compile and report; non-zero on error
//     des run     model.des [until]  compile and run; print the report
//
// This is the only program this project ships that is neither a demo nor a
// test, and command-line tools accrete flags. The TUI owns the interactive
// surface; this exists so a model file can be tried by hand, and so "the
// format is writable by a person" is checkable rather than merely asserted.
//
// `run` drives RunController::advance() rather than SimulationSystem::run().
// v11 gave the document layer a consumer inside this repo for the same reason:
// an interface with no caller is an interface nobody has checked.

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "des.hpp"

using namespace des;

namespace {

void printDiagnostics(const std::vector<Diagnostic>& ds, const std::string& path) {
    for (const Diagnostic& d : ds) {
        std::cout << path;
        if (d.cell) {
            std::cout << ": " << d.cell->moduleType << " row " << (d.cell->row + 1);
            if (!d.cell->column.empty()) std::cout << ", " << d.cell->column;
            // The offset WITHIN the cell, which is what v10 measured and what a
            // front end puts a cursor on. Tested on length alone this printed
            // nothing for `EXPO(0.8`, whose span is zero-length at end of
            // input -- the one place a caret is most wanted. A schema or
            // reference diagnostic has no span at all, and shows as neither.
            if (d.span.offset > 0 || d.span.length > 0)
                std::cout << " (col " << (d.span.offset + 1) << ")";
        } else if (d.span.offset > 0) {
            // A reader diagnostic: its span carries a line number, not a
            // character offset.
            std::cout << ":" << d.span.offset;
        }
        std::cout << ": " << (d.severity == Severity::Error ? "error" : "warning")
                  << ": " << d.message << "\n";
    }
}

int usage() {
    std::cout << "usage: des check   <model.des>\n"
                 "       des run     <model.des> [until]\n"
                 "       des regress [dir] [--capture] [--force]\n";
    return 2;
}

int regress(int argc, char** argv) {
    std::string dir = "tests/regression";
    bool capture = false, force = false;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--capture")    capture = true;
        else if (arg == "--force") force = true;
        else                       dir = arg;
    }
    const RegressionReport r = runRegression(dir, capture, force);
    for (const RegressionOutcome& o : r.outcomes)
        std::cout << (o.matched ? "ok      " : "DIFFERS ")
                  << o.model << "  " << o.detail << "\n";
    std::cout << "\n" << r.cases << " models, " << r.failures << " differ, "
              << r.missing << " without an expectation";
    if (r.refused > 0) std::cout << ", " << r.refused << " capture(s) refused";
    std::cout << "\n";
    if (capture) {
        std::cout << r.captured << " captured\n";
        return r.refused > 0 ? 1 : 0;
    }
    // A gate that measured nothing has not passed; it has not run.
    std::cout << (r.ok() ? "REGRESSION CLEAN\n" : "REGRESSION FAILED\n");
    return r.ok() ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    const std::string verb = argv[1];

    if (verb == "regress") return regress(argc, argv);
    if (verb != "check" && verb != "run") return usage();
    if (verb == "check" && argc != 3) return usage();
    if (verb == "run" && (argc < 3 || argc > 4)) return usage();

    const std::string path = argv[2];

    std::optional<SimTime> override;
    if (argc == 4) {
        char* end = nullptr;
        const double given = std::strtod(argv[3], &end);
        if (end == argv[3] || *end != '\0' || !(given > 0.0)) {
            std::cout << "des: `" << argv[3] << "' is not a run length\n";
            return usage();
        }
        override = given;
    }

    ReadResult read = readDocumentFile(path);
    printDiagnostics(read.diagnostics, path);
    if (hasErrors(read.diagnostics)) return 1;

    std::vector<Diagnostic> problems;
    std::unique_ptr<RunController> run =
        RunController::fromDocument(read.document, problems, override);
    printDiagnostics(problems, path);
    if (run == nullptr) return 1;

    if (verb == "check") {
        std::cout << path << ": ok\n";
        return 0;
    }

    while (run->state() == RunState::Ready || run->state() == RunState::Running) {
        run->advance(4096);
        const RunProgress p = run->progress();
        if (p.replications > 1)
            std::cout << "\rreplication " << p.replication << " of "
                      << p.replications << std::flush;
    }
    if (run->progress().replications > 1) std::cout << "\n";
    if (run->state() == RunState::Failed) {
        std::cout << path << ": error: " << run->failure() << "\n";
        return 1;
    }
    run->report(std::cout);
    return 0;
}
