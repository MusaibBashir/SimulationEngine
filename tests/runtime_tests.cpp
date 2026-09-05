// ============================================================================
// tests/runtime_tests.cpp  --  v12: the run as something a caller drives
// ============================================================================
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include "harness.hpp"
#include "des.hpp"

using namespace des;
using des_test::check;
using des_test::checkClose;
using des_test::section;

void runRuntimeTests() {
    section("A report can be captured");
    {
        SimulationSystem sim(4242u);
        sim.model().arrivals("EXPO(1.0)")
                   .station("Serve", 1, FIFO, "EXPO(0.5)")
                   .entryAt("Serve");
        sim.stopAt(50.0).execute();

        std::ostringstream a, b;
        sim.report(a);
        sim.report(b);
        check(a.str().find("simulation report") != std::string::npos,
              "report(ostream) writes the report");
        check(a.str() == b.str(),
              "and reporting twice gives the same text, so it can be diffed");

        std::ostringstream arena;
        sim.reportArenaStyle(arena);
        check(arena.str().find("Replication ended at time") != std::string::npos,
              "reportArenaStyle(ostream) writes the Arena layout");
    }
    section("Stepping is running, in pieces");
    {
        // The claim the whole version stands on: the size of the pieces a
        // caller takes must not change the run. Compared as TRACES, event for
        // event, because an equal summary would not be evidence -- two
        // different event orders can average the same.
        auto traceOf = [](const std::string& out, std::size_t chunk) {
            {
                SimulationSystem sim(20260905u);
                sim.model().arrivals("EXPO(1.0)")
                           .station("Serve", 1, FIFO, "EXPO(0.8)")
                           .dispose("Out")
                           .route("Serve", "Out")
                           .entryAt("Serve");
                sim.enableTrace(out, TraceLevel::Events);
                sim.stopAt(200.0).initialise();
                if (chunk == 0) {
                    sim.run();
                } else {
                    bool more = true;
                    while (more) {
                        more = false;
                        for (std::size_t i = 0; i < chunk; ++i) {
                            if (!sim.stepOnce()) break;
                            more = true;
                        }
                    }
                }
            }
            std::ifstream in(out, std::ios::binary);
            return std::string((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        };

        const std::string whole = traceOf("step_whole.md", 0);
        check(whole.size() > 500, "the trace is substantial");
        check(whole == traceOf("step_1.md", 1),
              "one event at a time traces identically to run()");
        check(whole == traceOf("step_7.md", 7),
              "seven at a time traces identically to run()");
        check(whole == traceOf("step_1000.md", 1000),
              "a thousand at a time traces identically to run()");
    }
    section("Progress says when it cannot tell");
    {
        SimulationSystem sim(7u);
        sim.model().arrivals("EXPO(1.0)")
                   .station("Serve", 1, FIFO, "EXPO(0.5)")
                   .entryAt("Serve");
        sim.setTermination(timeLimit(100.0));
        sim.initialise();
        for (int i = 0; i < 200 && sim.canStep(); ++i) sim.stepOnce();

        const TimeLimit t(100.0);
        const std::optional<double> f = t.progress(sim);
        check(f.has_value(), "a time limit knows how far through it is");
        if (f) check(*f > 0.0 && *f <= 1.0, "and it is a fraction");

        const DrainedRule drained;
        check(!drained.progress(sim).has_value(),
              "whenDrained() CANNOT tell, and says so rather than reporting 0");

        const EntityLimit e(50);
        check(e.progress(sim).has_value(), "an entity limit knows");

        // AnyOf reports the largest fraction any child knows: the run ends when
        // the FIRST rule is met, so the most advanced one is the honest answer.
        auto composite = std::make_unique<AnyOf>();
        composite->add(whenDrained());
        composite->add(timeLimit(100.0));
        check(composite->progress(sim).has_value(),
              "anyOf knows if ANY child knows");

        auto blind = std::make_unique<AnyOf>();
        blind->add(whenDrained());
        check(!blind->progress(sim).has_value(),
              "and refuses when NONE of them do");
    }
    section("A snapshot of a run in progress");
    {
        SimulationSystem sim(99u);
        sim.model().variable("Served", 0.0)
                   .resource("Clerk", 2)
                   .arrivals("EXPO(1.0)")
                   .stationUsing("Desk", "Clerk", FIFO, "EXPO(1.4)")
                   .entryAt("Desk");
        sim.stopAt(100.0).initialise();
        for (int i = 0; i < 400 && sim.canStep(); ++i) sim.stepOnce();

        const RunSnapshot s = snapshotOf(sim);
        check(s.now > 0.0, "the snapshot carries the clock");
        check(s.arrived > 0, "and what has arrived");
        check(s.blocks.size() == 1, "one block");
        check(s.blocks[0].name == "Desk", "named");
        check(s.resources.size() == 1 && s.resources[0].name == "Clerk",
              "and the shared resource, which Model could not enumerate before");
        check(s.resources[0].capacity == 2.0, "with its capacity");
        check(s.variables.size() == 1 && s.variables[0].name == "Served",
              "and every declared variable");

        // Taking a snapshot must not perturb the run: it reads, it does not
        // draw, and a caller that watches must get the same numbers as one
        // that does not.
        const double before = s.now;
        for (int i = 0; i < 5; ++i) (void)snapshotOf(sim);
        check(snapshotOf(sim).now == before, "and taking one does not advance anything");

        // A snapshot and the report are two views of one run. If they can
        // disagree, one of them is inventing a number, and a watcher would see
        // figures that never appear in the result.
        const RunResults r = sim.results();
        checkClose(s.blocks[0].utilisation, r.station("Desk").utilisation, 1e-12,
                   "a snapshot agrees with the report at the same instant");
        check(s.exited == r.exited, "on the count too");
    }
    section("The controller drives one replication");
    {
        auto build = [](SimulationSystem& sim) {
            sim.model().arrivals("EXPO(1.0)")
                       .station("Serve", 1, FIFO, "EXPO(0.8)")
                       .entryAt("Serve");
            sim.stopAt(200.0);
        };

        RunSetup setup;                       // one replication, seed 12345
        RunController c(setup, build);
        check(c.state() == RunState::Ready, "a controller starts Ready");
        check(c.progress().replication == 0, "and on no replication yet");

        const std::size_t first = c.advance(10);
        check(first == 10, "advance(10) does ten events");
        check(c.state() == RunState::Running, "and it is now Running");
        check(c.progress().replication == 1, "on replication 1");
        check(c.snapshot().now > 0.0, "with a clock that has moved");

        c.pause();
        check(c.state() == RunState::Paused, "pause() pauses");
        check(c.advance(1000) == 0, "and a paused controller does NO events");
        const SimTime held = c.progress().now;
        check(c.advance(1000) == 0, "however many times it is asked");
        check(c.progress().now == held, "with the clock held exactly where it was");

        c.resume();
        check(c.state() == RunState::Running, "resume() resumes");
        c.runToCompletion();
        check(c.state() == RunState::Finished, "and it finishes");
        check(c.results().size() == 1, "with one replication result");
        check(c.results()[0].served > 0, "that served somebody");

        check(c.advance(10) == 0, "advance() after the end is 0, not an error");
    }

    section("Cancel is terminal, and a failure keeps its reason");
    {
        auto build = [](SimulationSystem& sim) {
            sim.model().arrivals("EXPO(1.0)")
                       .station("Serve", 1, FIFO, "EXPO(0.8)")
                       .entryAt("Serve");
            sim.stopAt(1000.0);
        };
        RunController c(RunSetup{}, build);
        c.advance(50);
        c.cancel();
        check(c.state() == RunState::Cancelled, "cancel() cancels");
        check(c.advance(50) == 0, "and nothing runs afterwards");
        c.resume();
        check(c.state() == RunState::Cancelled, "resume() cannot undo it");
        check(c.snapshot().now > 0.0, "the run so far is still readable");

        RunController bad(RunSetup{}, [](SimulationSystem& sim) {
            sim.model().arrivals("EXPO(1.0)").entryAt("NoSuchBlock");
            sim.stopAt(10.0);
        });
        bad.runToCompletion();
        check(bad.state() == RunState::Failed, "a model that will not build Fails");
        check(!bad.failure().empty(), "and says why");
    }
    section("The controller drives a whole study");
    {
        auto build = [](SimulationSystem& sim) {
            sim.model().arrivals("EXPO(1.0)")
                       .station("Serve", 1, FIFO, "EXPO(0.8)")
                       .entryAt("Serve");
            sim.stopAt(100.0);
        };
        RunSetup setup;
        setup.replications = 4;
        setup.baseSeed     = 5000u;

        RunController c(setup, build);
        c.advance(20);
        check(c.progress().replications == 4, "it knows how many replications");
        c.runToCompletion();
        check(c.state() == RunState::Finished, "and runs them all");
        check(c.results().size() == 4, "producing four results");
        check(c.results()[0].seed != c.results()[1].seed,
              "each with its own seed, or the sample has no variance");

        check(c.progress().replication <= c.progress().replications,
              "and never reports a replication number past the last one");

        // The decisive claim of this task: a study driven in pieces gives the
        // SAME numbers as Experiment's own loop, which is the loop this
        // replaces.
        Experiment e("same", build);
        e.replications(4).baseSeed(5000u);
        e.run();
        check(e.results().size() == 4, "Experiment ran four too");
        for (std::size_t i = 0; i < 4; ++i) {
            checkClose(c.results()[i].averageWait, e.results()[i].averageWait, 1e-12,
                       "replication matches Experiment's, exactly");
            checkClose(c.results()[i].Lq, e.results()[i].Lq, 1e-12,
                       "Lq matches Experiment's, exactly");
        }
    }
    section("A model file carries its own run length");
    {
        ModelDocument d;
        d.addRow("Run");
        d.setCell("Run", 0, "Name", "Setup");
        d.setCell("Run", 0, "Length", "480");
        d.setCell("Run", 0, "Warm-up", "50");
        d.setCell("Run", 0, "Replications", "5");
        d.setCell("Run", 0, "Base Seed", "777");
        d.setCell("Run", 0, "Stop When Drained", "true");

        std::vector<Diagnostic> problems;
        const RunSetup s = readRunSetup(d, problems);
        check(!hasErrors(problems), "a well-formed [Run] row reads cleanly");
        check(s.length.has_value() && *s.length == 480.0, "Length");
        checkClose(s.warmUp, 50.0, 1e-12, "Warm-up");
        check(s.replications == 5, "Replications");
        check(s.baseSeed == 777u, "Base Seed");
        check(s.stopWhenDrained, "Stop When Drained");
        check(!s.maxEntities.has_value(), "an unset Max Entities stays UNSET, not zero");

        {
            ModelDocument m;
            std::vector<Diagnostic> out;
            const RunSetup def = readRunSetup(m, out);
            check(!hasErrors(out), "a document with no [Run] is not an error");
            check(!def.length.has_value() && def.replications == 1,
                  "it just means the defaults");
        }
        {
            ModelDocument m;
            m.addRow("Run"); m.setCell("Run", 0, "Name", "A");
            m.setCell("Run", 0, "Length", "10");
            m.addRow("Run"); m.setCell("Run", 1, "Name", "B");
            std::vector<Diagnostic> out;
            (void)readRunSetup(m, out);
            bool second = false;
            for (const Diagnostic& g : out)
                if (g.cell && g.cell->moduleType == "Run" && g.cell->row == 1) second = true;
            check(second, "a SECOND [Run] row is reported at that row");
        }
        {
            // Legitimate: a Create with Max Arrivals is finite and the future
            // event list empties on its own. A warning, not a refusal.
            ModelDocument m;
            m.addRow("Run"); m.setCell("Run", 0, "Name", "A");
            std::vector<Diagnostic> out;
            (void)readRunSetup(m, out);
            check(!hasErrors(out), "a [Run] with no stopping condition is NOT an error");
            bool warned = false;
            for (const Diagnostic& g : out)
                if (g.severity == Severity::Warning) warned = true;
            check(warned, "but it warns that the run ends only when events run out");
        }
        {
            ModelDocument m;
            m.addRow("Run"); m.setCell("Run", 0, "Name", "A");
            m.setCell("Run", 0, "Length", "soon");
            std::vector<Diagnostic> out;
            (void)readRunSetup(m, out);
            bool badCell = false;
            for (const Diagnostic& g : out)
                if (g.cell && g.cell->column == "Length") badCell = true;
            check(badCell, "a non-numeric Length is reported AT the cell");
        }
    }
    section("A controller built straight from a document");
    {
        ModelDocument d;
        d.addRow("Run");
        d.setCell("Run", 0, "Name", "Setup");
        d.setCell("Run", 0, "Length", "120");
        d.setCell("Run", 0, "Replications", "3");
        d.addRow("Create");
        d.setCell("Create", 0, "Name", "In");
        d.setCell("Create", 0, "Interarrival", "EXPO(1.0)");
        d.setCell("Create", 0, "Next", "Serve");
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.6)");
        d.setCell("Process", 0, "Next", "Out");
        d.addRow("Dispose");
        d.setCell("Dispose", 0, "Name", "Out");

        std::vector<Diagnostic> problems;
        std::unique_ptr<RunController> c = RunController::fromDocument(d, problems);
        check(c != nullptr, "a valid document produces a controller");
        check(!hasErrors(problems), "with no diagnostics");
        if (c) {
            c->runToCompletion();
            check(c->state() == RunState::Finished, "and it runs to the end");
            check(c->results().size() == 3,
                  "THREE replications, because the document said so");
            check(c->results()[0].seed != c->results()[2].seed,
                  "each with its own seed");
        }

        // The override the CLI needs: a length on the command line beats the
        // one in the file, and the file is not edited to say so.
        std::vector<Diagnostic> ignored;
        std::unique_ptr<RunController> shorter =
            RunController::fromDocument(d, ignored, SimTime{40.0});
        check(shorter != nullptr, "an override still builds");
        if (shorter) {
            shorter->runToCompletion();
            check(shorter->results()[0].measuredTime < 60.0,
                  "and the override WINS over the file's Length");
        }

        ModelDocument bad;
        bad.addRow("Process");
        bad.setCell("Process", 0, "Name", "Lonely");
        bad.setCell("Process", 0, "Service", "EXPO(1");
        std::vector<Diagnostic> out;
        check(RunController::fromDocument(bad, out) == nullptr,
              "a document that will not compile produces NO controller");
        check(hasErrors(out), "and says why, at the cell");
    }
}
