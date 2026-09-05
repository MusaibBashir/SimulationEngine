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
}
