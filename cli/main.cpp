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
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "des.hpp"
#include "CurlTransport.hpp"
#include "ModelGenerator.hpp"
#include "ScenarioBuilder.hpp"

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
                 "       des new      <scenario.json> [-o model.des]\n"
                 "       des generate [--from scenario.json] [-o model.des] <description...>\n"
                 "\n"
                 "  --run picks one of several [Run] records by its Name.\n"
                 "  generate sends the description to a language model: Groq by default,\n"
                 "  with the key in DES_LLM_API_KEY (or GROQ_API_KEY). DES_LLM_BASE_URL and\n"
                 "  DES_LLM_MODEL point it at any OpenAI-compatible server, Ollama included.\n";
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

// --- v17: models from a description ------------------------------------------

void printList(const std::string& heading, const std::vector<std::string>& items) {
    if (items.empty()) return;
    std::cout << heading << "\n";
    for (const std::string& i : items) std::cout << "  - " << i << "\n";
}

void printQuestions(const std::vector<Question>& qs) {
    if (qs.empty()) return;
    std::cout << "Still needed:\n";
    for (const Question& q : qs) std::cout << "  ? " << q.text << "\n";
}

std::string scenarioPathFor(const std::string& out) {
    const std::size_t dot = out.rfind(".des");
    const std::string stem = (dot != std::string::npos && dot + 4 == out.size()) ? out.substr(0, dot) : out;
    return stem + ".scenario.json";
}

bool writeText(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << text;
    return static_cast<bool>(f);
}

bool readText(const std::string& path, std::string& text) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    text = ss.str();
    return true;
}

// A finished model, said once and the same way by both verbs.
int writeModel(const ModelDocument& doc, const std::string& out) {
    if (!writeDocumentFile(doc, out)) { std::cout << "des: could not write " << out << "\n"; return 1; }
    std::cout << "wrote " << out << "   (des run " << out << " to run it, or open it in des_tui)\n";
    return 0;
}

// des new scenario.json: the form, filled in by hand or kept from `generate`,
// becomes a model with no language model involved -- the half of the pipeline
// that has to be right every time, usable on its own.
int newFromScenario(int argc, char** argv) {
    std::string in, out = "model.des";
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) out = argv[++i];
        else if (in.empty()) in = arg;
        else return usage();
    }
    if (in.empty()) return usage();
    std::string text;
    if (!readText(in, text)) { std::cout << "des: cannot read " << in << "\n"; return 1; }
    const JsonParse json = parseJson(text);
    if (!json.ok) { std::cout << in << ": " << json.error << "\n"; return 1; }
    const ScenarioRead read = readScenario(json.value);
    for (const ScenarioIssue& issue : read.invalid)
        std::cout << in << ": " << (issue.path.empty() ? "" : issue.path + ": ") << issue.message << "\n";
    if (!read.invalid.empty()) return 1;
    std::vector<Question> questions = read.questions;
    if (questions.empty()) questions = loadQuestions(read.scenario);
    const BuiltScenario built = buildScenario(read.scenario);
    printList("Assumed:", built.assumptions);
    printQuestions(questions);
    if (!questions.empty()) return 3;
    return writeModel(built.document, out);
}

int generate(int argc, char** argv) {
    std::string from, out = "model.des", description;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--from" && i + 1 < argc) { from = argv[++i]; continue; }
        if (arg == "-o" && i + 1 < argc)     { out = argv[++i];  continue; }
        if (arg == "--show-prompt")          { std::cout << systemPrompt(); return 0; }
        description += (description.empty() ? "" : " ") + arg;
    }
    if (description.empty()) return usage();

    JsonValue previous;
    if (!from.empty()) {
        std::string text;
        if (!readText(from, text)) { std::cout << "des: cannot read " << from << "\n"; return 1; }
        const JsonParse p = parseJson(text);
        if (!p.ok) { std::cout << from << ": " << p.error << "\n"; return 1; }
        previous = p.value;
        out = from.size() > 14 && from.compare(from.size() - 14, 14, ".scenario.json") == 0
                  ? from.substr(0, from.size() - 14) + ".des" : out;
    }

    const LlmConfig config = LlmConfig::load();
    if (config.needsKey()) {
        std::cout << "des: no API key. Set DES_LLM_API_KEY to a Groq key (console.groq.com/keys),\n"
                     "     or save one from des_tui (F2, then ^K),\n"
                     "     or point DES_LLM_BASE_URL at a local server such as\n"
                     "     http://localhost:11434/v1 (Ollama), which needs none.\n";
        return 1;
    }
    // Said every time, on stderr: what leaves the machine, and where to.
    std::cerr << "asking " << config.model << " at " << config.baseUrl
              << (config.apiKey.empty() ? std::string() : " (key " + maskKey(config.apiKey) + ")")
              << " ...\n";

    CurlTransport transport;
    const GenerateResult r = generateModel(description, previous, config, transport);
    if (!r.error.empty()) { std::cout << "des: " << r.error << "\n"; return 1; }

    const std::string scenarioPath = scenarioPathFor(out);
    if (r.scenarioJson.isObject()) writeText(scenarioPath, writeJson(r.scenarioJson, 2) + "\n");

    printList("Understood:", r.notes);
    printList("Assumed:", r.assumptions);
    printQuestions(r.questions);
    if (!r.questions.empty()) {
        std::cout << "\nAnswer with:\n  des generate --from " << scenarioPath << " \"...\"\n";
        return 3;
    }
    return writeModel(r.document, out);
}

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    const std::string verb = argv[1];

    if (verb == "regress")  return regress(argc, argv);
    if (verb == "new")      return newFromScenario(argc, argv);
    if (verb == "generate") return generate(argc, argv);
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
