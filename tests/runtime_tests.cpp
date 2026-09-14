// ============================================================================
// tests/runtime_tests.cpp  --  v12: the run as something a caller drives
// ============================================================================
#include <stdexcept>
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
using des_test::modelPath;
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
            // MANY [Run] ROWS, and choosing between them. v11 through v14
            // reported the second one as an error, on the reasoning that Arena
            // has one Run Setup. But a file can hold several where a dialog
            // cannot, and editing the same numbers back and forth to compare a
            // long horizon against a short one loses what they were.
            ModelDocument m;
            m.addRow("Run"); m.setCell("Run", 0, "Name", "Short");
            m.setCell("Run", 0, "Length", "10");
            m.addRow("Run"); m.setCell("Run", 1, "Name", "Long");
            m.setCell("Run", 1, "Length", "5000");
            m.setCell("Run", 1, "Replications", "20");

            std::vector<Diagnostic> out;
            const RunSetup first = readRunSetup(m, out, 0);
            const RunSetup second = readRunSetup(m, out, 1);
            check(!hasErrors(out), "a second [Run] is no longer an error");
            check(first.length && *first.length == 10.0, "the first is read by index");
            check(second.length && *second.length == 5000.0, "and so is the second");
            check(second.replications == 20, "with its own replication count");

            const std::vector<std::string> names = runNames(m);
            check(names.size() == 2 && names[0] == "Short" && names[1] == "Long",
                  "and they are listed by name, which is how one is chosen");

            // OUT OF RANGE READS AS THE DEFAULTS, not as an error. A caller
            // holding a stale index after a row was deleted is not a broken
            // model, and refusing would turn an editing mistake into a failure.
            std::vector<Diagnostic> stale;
            const RunSetup gone = readRunSetup(m, stale, 9);
            check(!gone.length.has_value() && !hasErrors(stale),
                  "an index past the end reads as the defaults");
        }
        {
            // A row with no Name is still a row you may want to run, so it gets
            // a stand-in rather than an empty line in the list.
            ModelDocument m;
            m.addRow("Run");
            const std::vector<std::string> names = runNames(m);
            check(names.size() == 1 && !names[0].empty(),
                  "an unnamed [Run] still has something to call it");
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
    section("The regression harness");
    {
        const std::string dir = "tests/regression";
        RegressionReport a = runRegression(dir, false, false);
        check(a.cases > 0, "the manifest names at least one model");
        check(a.failures == 0, "and every model matches its expectation");
        check(a.missing == 0, "with no expectation missing");
        check(a.ok(), "so the harness is clean");

        RegressionReport b = runRegression(dir, false, false);
        check(a.failures == b.failures && a.cases == b.cases,
              "running it twice gives the same verdict");

        // A gate that measured nothing must never report success. baseline.sh
        // shipped without this and reported BASELINE CLEAN over zero examples.
        RegressionReport empty = runRegression("tests/regression_empty", false, false);
        check(empty.cases == 0, "the empty fixture names no models");
        check(!empty.ok(), "and an empty manifest is a FAILURE, not a pass");

        RegressionReport missing = runRegression("tests/regression_nowhere", false, false);
        check(!missing.ok(), "a directory with no manifest is a failure too");

        // --capture must refuse to overwrite. Re-capturing after a change is
        // how a gate silently stops being a gate.
        RegressionReport recapture = runRegression(dir, true, false);
        check(recapture.refused == recapture.cases,
              "capture refuses every expectation that already exists");
        check(recapture.captured == 0, "and writes none of them");
    }
    section("Chunk size does not change the run");
    {
        // The claim this whole version rests on, over every shipped model
        // rather than one hand-made in the test. If a caller can change the
        // answer by choosing a different budget, none of the rest is safe.
        const char* files[] = {"teller.des", "decide.des", "variables.des", "shared.des"};
        for (const char* file : files) {
            auto traceOf = [file](const std::string& out, std::size_t chunk,
                                  bool withWatcher) {
                {
                    SimulationSystem sim(20260905u);
                    ReadResult read = readDocumentFile(modelPath(file));
                    std::vector<Diagnostic> problems;
                    if (!compileInto(read.document, sim.model(), problems))
                        return std::string();
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
                            // A pause is the ABSENCE of a call, so this is what
                            // one looks like from the engine's side: a watcher
                            // reading, and nothing else happening.
                            if (withWatcher) (void)snapshotOf(sim);
                        }
                    }
                }
                std::ifstream in(out, std::ios::binary);
                return std::string((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
            };

            const std::string whole = traceOf("chunk_whole.md", 0, false);
            check(whole.size() > 500, std::string(file) + ": the trace is substantial");
            check(whole == traceOf("chunk_1.md", 1, false),
                  std::string(file) + ": one event at a time is identical to run()");
            check(whole == traceOf("chunk_13.md", 13, false),
                  std::string(file) + ": thirteen at a time is identical to run()");
            check(whole == traceOf("chunk_watched.md", 5, true),
                  std::string(file) + ": and watching it changes nothing");
        }
    }
    section("Budget size does not change a STUDY");
    {
        // The chunk test above compares stepped runs against run(), and run()
        // IS `while (stepOnce())` -- so at that level nothing about call
        // scheduling can change a deterministic event sequence, and those
        // assertions are close to tautological. Proved so: skipping an
        // iteration of the budget loop, and even a watcher that secretly
        // called stepOnce(), left the traces identical. Only a watcher that
        // CONSUMED RANDOMNESS was caught.
        //
        // This is where batch size can genuinely leak state, and therefore
        // where the assertion has content: RunController carries a replication
        // index, an antithetic half-flag and a captured report ACROSS advance()
        // calls, and a budget boundary can fall anywhere relative to the end of
        // a replication.
        auto build = [](SimulationSystem& sim) {
            sim.model().arrivals("EXPO(1.0)")
                       .station("Serve", 1, FIFO, "EXPO(0.8)")
                       .entryAt("Serve");
            sim.stopAt(60.0);
        };
        RunSetup setup;
        setup.replications = 3;
        setup.baseSeed     = 31337u;

        auto resultsAt = [&](std::size_t budget) {
            RunController c(setup, build);
            while (c.state() == RunState::Ready || c.state() == RunState::Running)
                c.advance(budget);
            return c.results();
        };

        const std::vector<ReplicationResult> big = resultsAt(1000000);
        check(big.size() == 3, "the study ran three replications");
        for (std::size_t budget : {std::size_t{1}, std::size_t{7}, std::size_t{97}}) {
            const std::vector<ReplicationResult> small = resultsAt(budget);
            check(small.size() == big.size(),
                  "a smaller budget produces the same number of replications");
            for (std::size_t i = 0; i < small.size() && i < big.size(); ++i) {
                check(small[i].seed == big[i].seed,
                      "each replication keeps its seed whatever the budget");
                checkClose(small[i].averageWait, big[i].averageWait, 1e-12,
                           "and its numbers, exactly");
                checkClose(small[i].Lq, big[i].Lq, 1e-12,
                           "including Lq");
            }
        }

        // Antithetic pairing is the case where one replication is TWO runs, so
        // a budget boundary can land between the halves of a pair.
        RunSetup paired = setup;
        paired.antithetic = true;
        auto pairedAt = [&](std::size_t budget) {
            RunController c(paired, build);
            while (c.state() == RunState::Ready || c.state() == RunState::Running)
                c.advance(budget);
            return c.results();
        };
        const std::vector<ReplicationResult> pb = pairedAt(1000000);
        const std::vector<ReplicationResult> ps = pairedAt(3);
        check(pb.size() == 3 && ps.size() == 3,
              "an antithetic study is still three replications, not six");
        for (std::size_t i = 0; i < pb.size() && i < ps.size(); ++i)
            checkClose(ps[i].averageWait, pb[i].averageWait, 1e-12,
                       "and a budget boundary between the halves of a pair changes nothing");
    }
    section("A multi-replication report shows EVERY replication");
    {
        // Reported from the terminal UI: a twelve-replication run printed five
        // numbers with a bare "+/-" beside each and nothing else, so a reader
        // could not tell whether those were the last replication or an average
        // over all twelve. They were the average, and the evidence for it was
        // not on screen. v4 exists to say a single run is one sample from a
        // random variable; hiding the samples argues against it.
        auto build = [](SimulationSystem& sim) {
            sim.model().arrivals("EXPO(1.0)")
                       .station("Serve", 1, FIFO, "EXPO(0.8)")
                       .entryAt("Serve");
            sim.stopAt(120.0);
        };
        RunSetup setup;
        setup.replications = 5;
        setup.baseSeed     = 900u;

        RunController c(setup, build);
        c.runToCompletion();
        std::ostringstream out;
        c.report(out);
        const std::string text = out.str();

        check(text.find("5 replications") != std::string::npos, "it says how many");
        check(text.find("seeds 900..904") != std::string::npos,
              "and which seeds, so a result can be reproduced");
        check(text.find("rep   seed   served") != std::string::npos,
              "there is a per-replication table");
        for (const ReplicationResult& r : c.results()) {
            std::ostringstream seed;
            seed << r.seed;
            check(text.find(seed.str()) != std::string::npos,
                  "with a row for every replication");
        }
        check(text.find("95% half-width") != std::string::npos,
              "and the summary SAYS the interval is a 95% half-width, rather "
              "than leaving a bare +/- to be guessed at");
        check(text.find("average wait (Wq)") != std::string::npos,
              "naming each quantity the way Experiment::report does");

        // One replication is a single run, and prints that run's own report.
        RunSetup one;
        one.baseSeed = 900u;
        RunController single(one, build);
        single.runToCompletion();
        std::ostringstream solo;
        single.report(solo);
        check(solo.str().find("simulation report") != std::string::npos,
              "a single replication prints the run's own report, not a table of one");
    }

    section("A model that fails DURING a run fails the run, not the program");
    {
        // Both of these compile, and both used to take the whole program down
        // the moment they were run -- which, in a terminal UI somebody has been
        // sent, is the window closing with their unsaved model in it.
        //
        //   SQRT(0 - 1)  throws ExpressionError at the first service. startRun()
        //                caught model errors during setup; advance() caught
        //                nothing, so it went out through the UI loop and main.
        //   EXPO(1) - 2  is negative most of the time, and the scheduler met a
        //                time in the past with an ASSERT. That aborts outright,
        //                and a release build deletes it and schedules the event
        //                in the past instead, silently.
        //
        // NORM(1, 5) looked like the obvious first case and was not one: the
        // builder clamps a Normal's left tail at zero, so it runs to the end.
        // Found by running each expression in its own process rather than by
        // reading the code, which had suggested the opposite.
        const auto runWith = [](const std::string& service) {
            const ModelDocument doc = readDocument(
                "version = 1\n\n[Run]\nName = R\nLength = 480\n\n"
                "[Create]\nName = In\nInterarrival = EXPO(2)\nNext = Work\n\n"
                "[Process]\nName = Work\nService = " + service + "\nNext = Out\n\n"
                "[Dispose]\nName = Out\n").document;
            std::vector<Diagnostic> problems;
            return RunController::fromDocument(doc, problems);
        };

        for (const std::pair<std::string, std::string>& c :
             {std::pair<std::string, std::string>{"SQRT(0 - 1)", "SQRT"},
              std::pair<std::string, std::string>{"EXPO(1) - 2", "negative"}}) {
            std::unique_ptr<RunController> run = runWith(c.first);
            check(run != nullptr, "'" + c.first + "' compiles");
            bool escaped = false;
            try {
                if (run) run->runToCompletion();
            } catch (const std::exception&) {
                escaped = true;
            }
            check(!escaped, "'" + c.first + "': the error does NOT escape advance()");
            check(run && run->state() == RunState::Failed,
                  "'" + c.first + "': the run is marked Failed");
            check(run && run->failure().find(c.second) != std::string::npos,
                  "'" + c.first + "': and the failure says what went wrong");
            check(run && run->advance(100) == 0,
                  "'" + c.first + "': and a failed run stays stopped");
        }

        // And the clamp that made NORM(1, 5) a non-case keeps working.
        std::unique_ptr<RunController> norm = runWith("NORM(1, 5)");
        if (norm) norm->runToCompletion();
        check(norm && norm->state() == RunState::Finished,
              "NORM(1, 5) still runs to the end, its left tail clamped at zero");
    }
}
