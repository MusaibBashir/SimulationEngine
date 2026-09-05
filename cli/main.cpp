// ============================================================================
// cli/main.cpp  --  the `des` command
// ============================================================================
// Two verbs, deliberately:
//
//     des check model.des          compile and report; non-zero on error
//     des run   model.des [until]  compile and run to `until`; print the report
//
// This is the first program this project ships that is neither a demo nor a
// test, and command-line tools accrete flags. v12 owns the interactive surface;
// this exists so a model file can be tried by hand, and so "the format is
// writable by a person" is checkable rather than merely asserted.

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "des.hpp"

using namespace des;

namespace {

// A document carries no run length: there is no Run module, and inventing one
// in the CLI would put a number in the schema by the back door. So `run` takes
// the horizon as an argument and says which one it used.
constexpr SimTime DEFAULT_HORIZON = 480.0;

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
    std::cout << "usage: des check <model.des>\n"
                 "       des run   <model.des> [until]\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) return usage();
    const std::string verb = argv[1];
    const std::string path = argv[2];
    if (verb != "check" && verb != "run") return usage();
    if (verb == "check" && argc != 3) return usage();

    SimTime horizon = DEFAULT_HORIZON;
    bool horizonGiven = false;
    if (argc == 4) {
        char* end = nullptr;
        horizon = std::strtod(argv[3], &end);
        if (end == argv[3] || *end != '\0' || !(horizon > 0.0)) {
            std::cout << "des: `" << argv[3] << "' is not a run length\n";
            return usage();
        }
        horizonGiven = true;
    }

    ReadResult read = readDocumentFile(path);
    printDiagnostics(read.diagnostics, path);
    if (hasErrors(read.diagnostics)) return 1;

    SimulationSystem sim;
    std::vector<Diagnostic> problems;
    const bool built = compileInto(read.document, sim.model(), problems);
    printDiagnostics(problems, path);
    if (!built) return 1;

    if (verb == "check") {
        std::cout << path << ": ok\n";
        return 0;
    }

    // whenDrained() is deliberately NOT used here. It is met the first time the
    // system happens to be empty, which for any model with random arrivals is
    // usually just after the first entity leaves -- a one-entity report that
    // looks like a real one. A stated horizon is honest; that is not.
    if (!horizonGiven)
        std::cout << "note: a model file carries no run length; running to t = "
                  << horizon << " (pass one to change it)\n";

    sim.setTermination(timeLimit(horizon));
    sim.initialise();
    sim.run();
    sim.report();
    return 0;
}
