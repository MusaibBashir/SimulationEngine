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
    std::cout << "usage: des check   <model.des> [--run <name>]\n"
                 "       des run     <model.des> [until] [--run <name>]\n"
                 "       des regress [dir] [--capture] [--force]\n"
                 "\n"
                 "  --run picks one of several [Run] records by its Name.\n";
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
    if (argc < 3) return usage();

    const std::string path = argv[2];

    // v15 lets a file hold several named [Run] records, so the CLI has to be
    // able to say which. Otherwise a model the editor can run four ways runs
    // only one way from a script -- and the script is the half that gets
    // automated.
    std::optional<SimTime> override;
    std::string wantedRun;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--run") {
            if (i + 1 >= argc) { std::cout << "des: --run needs a name\n"; return usage(); }
            wantedRun = argv[++i];
            continue;
        }
        if (verb != "run") return usage();
        char* end = nullptr;
        const double given = std::strtod(arg.c_str(), &end);
        if (end == arg.c_str() || *end != '\0' || !(given > 0.0)) {
            std::cout << "des: `" << arg << "' is not a run length\n";
            return usage();
        }
        override = given;
    }

    ReadResult read = readDocumentFile(path);
    printDiagnostics(read.diagnostics, path);
    if (hasErrors(read.diagnostics)) return 1;

    const std::vector<std::string> names = runNames(read.document);
    std::size_t which = 0;
    if (!wantedRun.empty()) {
        bool found = false;
        for (std::size_t i = 0; i < names.size(); ++i)
            if (names[i] == wantedRun) { which = i; found = true; break; }
        if (!found) {
            // LIST WHAT IT HAS. "no [Run] named 'Lng'" and nothing else leaves
            // somebody opening the file to find out what they meant to type.
            std::cout << path << ": no [Run] named '" << wantedRun << "'";
            if (names.empty()) {
                std::cout << " -- this model has none\n";
            } else {
                std::cout << " -- it has:";
                for (const std::string& n : names) std::cout << " " << n;
                std::cout << "\n";
            }
            return 1;
        }
    }

    std::vector<Diagnostic> problems;
    std::unique_ptr<RunController> run =
        RunController::fromDocument(read.document, problems, override, which);
    printDiagnostics(problems, path);
    if (run == nullptr) return 1;

    if (verb == "check") {
        std::cout << path << ": ok";
        if (names.size() > 1) {
            std::cout << " (" << names.size() << " runs:";
            for (const std::string& n : names) std::cout << " " << n;
            std::cout << ")";
        }
        std::cout << "\n";
        return 0;
    }

    // WHICH RUN produced this, whenever there is a choice. A report that does
    // not say which settings made it is a report you cannot file.
    if (names.size() > 1) std::cout << "run: " << names[which] << "\n";

    while (run->state() == RunState::Ready || run->state() == RunState::Running) {
        run->advance(4096);
        const RunProgress p = run->progress();
        // STDERR, not stdout. This is a carriage-return ticker that overwrites
        // itself, which is right on a terminal and garbage in a file:
        // `des run m.des > results.txt` collected
        // "replication 1 of 5replication 2 of 5replication 2 of 5..." at the
        // top of the results. Progress is not the output.
        if (p.replications > 1)
            std::cerr << "\rreplication " << p.replication << " of "
                      << p.replications << std::flush;
    }
    if (run->progress().replications > 1) std::cerr << "\n";
    if (run->state() == RunState::Failed) {
        std::cout << path << ": error: " << run->failure() << "\n";
        return 1;
    }
    run->report(std::cout);
    return 0;
}
