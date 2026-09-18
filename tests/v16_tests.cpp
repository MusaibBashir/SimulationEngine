// ============================================================================
// v16_tests.cpp  --  the Runs form, naming new files, and everything the
// Group 9 ride-hailing model needs: Match, schedules, Route/Station, array
// variables, richer Records, and the statistics that report them.
// ============================================================================

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include "harness.hpp"
#include "des.hpp"
#include "des_ui.hpp"

using namespace des;
using des_test::check;
using des_test::checkClose;
using des_test::modelPath;
using des_test::section;

namespace {

void typeText(TuiState& s, const std::string& text) {
    for (const char c : text) handleKey(s, Key::character(c));
}

}  // namespace

void runV16Tests() {
    section("replaceLines is one undo step");
    {
        TextBuffer b("a\nb\nc");
        b.replaceLines(1, 1, {"x", "y"});
        check(b.text() == "a\nx\ny\nc", "the middle line is replaced by two");
        b.undo();
        check(b.text() == "a\nb\nc", "and one undo puts it back");
    }

    section("The Runs form edits the text, and keeps the comments");
    {
        TuiState s = TuiState::fromText(
            "version = 1\n# how long\n[Run]\nName   = Base\nLength = 480\n", "t.des");
        check(s.setRunValue(0, "Replications", "10"), "a new field is accepted");
        check(s.buffer().text().find("Replications = 10") != std::string::npos,
              "and written into the [Run] block");
        check(s.buffer().text().find("# how long") != std::string::npos, "the comment survives");
        check(s.setRunValue(0, "Length", "600"), "an existing field is rewritten");
        check(s.buffer().text().find("Length = 600") != std::string::npos,
              "keeping the key as it was typed");
        check(s.document().cell("Run", 0, "Length") == "600", "the document sees it");
        check(!s.setRunValue(0, "Replications", "ten"), "letters in a whole number are refused");
        check(s.document().cell("Run", 0, "Replications") == "10", "and the text is not changed");
        check(s.setRunValue(0, "Length", ""), "blank removes the line");
        check(s.buffer().text().find("Length") == std::string::npos, "so it is gone");
        s.undo();
        check(s.buffer().text().find("Length = 600") != std::string::npos, "one undo restores it");
    }

    section("Deleting a run removes its block and the comments above it");
    {
        TuiState s = TuiState::fromText(
            "version = 1\n[Run]\nName = A\n\n# the second\n[Run]\nName = B\nLength = 5\n"
            "\n[Create]\nName = C\n", "t.des");
        check(s.deleteRun(1), "run B is deleted");
        check(s.runs().size() == 1 && s.runs()[0] == "A", "only A is left");
        check(s.buffer().text().find("# the second") == std::string::npos, "its comment went too");
        check(s.buffer().text().find("[Create]") != std::string::npos, "the next block did not");
        s.undo();
        check(s.runs().size() == 2, "one undo brings it back");

        TuiState t = TuiState::fromText("version = 1\n[Run]\nName = A\n\n[Run]\nName = B\n", "t.des");
        check(t.deleteRun(1), "the last block in the file can be deleted");
        check(t.buffer().text() == "version = 1\n[Run]\nName = A\n",
              "without leaving blank lines behind");
    }

    section("The Runs tab: A adds, Delete asks, Tab reaches the form");
    {
        TuiState s = TuiState::fromText("version = 1\n[Run]\nName = A\n", "t.des");
        handleKey(s, Key::control('u'));
        handleKey(s, Key::character('a'));
        check(s.runs().size() == 2, "A adds a run");
        check(s.view() == View::Runs, "without leaving the Runs tab");
        check(s.runs()[1] == "Run2", "and gives it a name");
        handleKey(s, Key::special(KeyKind::Escape));
        check(s.runsFocus() == RunsFocus::List, "Esc in the form goes back to the list");
        handleKey(s, Key::special(KeyKind::Delete));
        check(s.overlay() == Overlay::Confirm && s.confirmFor() == ConfirmFor::DeleteRun,
              "Delete asks first");
        handleKey(s, Key::character('n'));
        check(s.runs().size() == 2 && s.overlay() == Overlay::None, "n keeps it");
        handleKey(s, Key::special(KeyKind::Delete));
        handleKey(s, Key::character('y'));
        check(s.runs().size() == 1, "and y deletes");
        handleKey(s, Key::special(KeyKind::Tab));
        check(s.runsFocus() == RunsFocus::Form, "Tab moves into the form");
        // Field 0 is Name, field 1 Length.
        handleKey(s, Key::special(KeyKind::Down));
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.editingRunField(), "Enter edits the field");
        typeText(s, "90");
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.document().cell("Run", 0, "Length") == "90", "and Enter writes it");
        check(!s.editingRunField(), "ending the edit");

        handleKey(s, Key::special(KeyKind::Enter));
        typeText(s, "x");
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.editingRunField(), "a refused value keeps the edit open to fix");
        check(s.document().cell("Run", 0, "Length") == "90", "and the text unchanged");
        handleKey(s, Key::special(KeyKind::Escape));
        check(!s.editingRunField(), "Esc abandons it");

        // A yes/no field opens the list rather than taking typing.
        std::size_t antithetic = 0;
        for (std::size_t i = 0; i < s.runFields().size(); ++i)
            if (s.runFields()[i] == "Antithetic") antithetic = i;
        s.setRunField(antithetic);
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.overlay() == Overlay::Picker, "a yes/no field opens the list");
        handleKey(s, Key::special(KeyKind::Down));
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.document().cell("Run", 0, "Antithetic") == "true", "and the choice is written");

        Screen screen(100, 30);
        render(s, screen);
        check(screen.asText().find("Replications") != std::string::npos,
              "the form lists every field");
        check(screen.asText().find("(default)") != std::string::npos,
              "showing defaults for the ones not set");
    }

    section("A typed model name becomes a path");
    {
        check(resolveModelName("line2", "D") == "D/line2.des", "a bare name goes in the default folder");
        check(resolveModelName("line2.des", "D") == "D/line2.des", ".des is not doubled");
        check(resolveModelName("sub/x", "D") == "sub/x.des", "a path is used as given");
        check(resolveModelName("a.txt", "D") == "D/a.txt", "an extension is kept");
        check(resolveModelName("  ", "D").empty(), "blank is refused");
    }

    section("^N asks for a name; an unnamed model asks on first save");
    {
        const std::string dir = "v16_newfile_dir";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        TuiState s = TuiState::fromText("", dir + "/untitled.des");
        s.setDefaultDirectory(dir);
        s.setUnnamed(true);
        typeText(s, "x");
        handleKey(s, Key::control('s'));
        check(s.overlay() == Overlay::Prompt && s.promptFor() == PromptFor::SaveAs,
              "the first save of an unnamed model asks for a name");
        check(!std::filesystem::exists(dir + "/untitled.des"), "rather than writing untitled.des");
        typeText(s, "fresh");
        handleKey(s, Key::special(KeyKind::Enter));
        check(std::filesystem::exists(dir + "/fresh.des"), "and saves under it");
        check(!s.unnamed() && !s.dirty(), "after which it is named and clean");
        check(s.path() == dir + "/fresh.des", "and that is its path");

        handleKey(s, Key::control('n'));
        check(s.overlay() == Overlay::Prompt && s.promptFor() == PromptFor::NewFile,
              "^N on a clean model goes straight to the name");
        typeText(s, "fresh");
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.overlay() == Overlay::Confirm && s.confirmFor() == ConfirmFor::OpenExisting,
              "an existing name asks before opening it");
        handleKey(s, Key::character('n'));
        check(s.overlay() == Overlay::Prompt && s.promptText() == "fresh",
              "no goes back to the prompt, keeping what was typed");
        handleKey(s, Key::special(KeyKind::Escape));
        check(s.overlay() == Overlay::None, "Esc cancels");

        typeText(s, "y");
        handleKey(s, Key::control('n'));
        check(s.overlay() == Overlay::Confirm && s.confirmFor() == ConfirmFor::NewFile,
              "^N with unsaved changes asks first");
        handleKey(s, Key::character('d'));
        typeText(s, "other");
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.path() == dir + "/other.des", "discard then a name opens the new model there");
        check(s.buffer().text().empty() && !s.dirty(), "empty and clean");

        handleKey(s, Key::control('n'));
        typeText(s, "fresh");
        handleKey(s, Key::special(KeyKind::Enter));
        handleKey(s, Key::character('y'));
        check(s.path() == dir + "/fresh.des" && s.buffer().text().find('x') != std::string::npos,
              "yes opens the existing file");
        std::filesystem::remove_all(dir);
    }

    section("Per Arrival may be a variable, and Max Arrivals counts arrivals");
    {
        const std::string text =
            "version = 1\n[Run]\nName = R\nLength = 10\n"
            "[Variable]\nName = Fleet\nInitial Value = 20\n"
            "[Create]\nName = Cars\nInterarrival = 1\nPer Arrival = Fleet\n"
            "Max Arrivals = 1\nNext = Out\n[Dispose]\nName = Out\n";
        check(compile(readDocument(text).document).model != nullptr, "it compiles");
        SimulationSystem sim(1);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(text).document, sim.model(), d), "into a system too");
        sim.stopAt(10).execute();
        check(sim.results().arrived == 20, "one arrival of Fleet = 20 entities");

        SimulationSystem fixed(1);
        std::string twenty = text;
        twenty.replace(twenty.find("Per Arrival = Fleet"), 19, "Per Arrival = 20");
        compileInto(readDocument(twenty).document, fixed.model(), d);
        fixed.stopAt(10).execute();
        check(fixed.results().arrived == 20, "a plain 20 with Max Arrivals 1 is 20 too");

        SimulationSystem bad(1);
        std::string negative = text;
        negative.replace(negative.find("Initial Value = 20"), 18, "Initial Value = -2");
        compileInto(readDocument(negative).document, bad.model(), d);
        bool threw = false;
        try { bad.stopAt(10).execute(); } catch (const ModelError&) { threw = true; }
        check(threw, "a negative Per Arrival stops the run with a message");
    }

    section("An array variable is read by index");
    {
        const std::string text =
            "version = 1\n[Run]\nName = R\nLength = 5\n"
            "[Variable]\nName = T\nRows = 2\nColumns = 2\nInitial Value = 1, 2, 3, 4\n"
            "[Create]\nName = C\nInterarrival = 1\nMax Arrivals = 1\nNext = A\n"
            "[Assign]\nName = A\nNext = Out\n"
            "[AssignField]\nAssign = A\nTarget = Attribute\nName = got\nValue = T(2, 1)\n"
            "[Record]\nName = Out\nWhat = Attribute\nAttribute = got\n";
        SimulationSystem sim(1);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(text).document, sim.model(), d), "it compiles");
        sim.stopAt(5).execute();
        check(sim.model().nodeAs<RecordNode>("Out").average() == 3.0, "T(2,1) is 3, row by row");
    }
    {
        std::vector<Diagnostic> d;
        Model m;
        compileInto(readDocument("version = 1\n[Variable]\nName = T\nRows = 2\nColumns = 2\n"
                                 "Initial Value = 1, 2, 3\n").document, m, d);
        check(hasErrors(d), "three values for four cells is an error");
        d.clear();
        Model one;
        compileInto(readDocument("version = 1\n[Variable]\nName = T\nRows = 3\nColumns = 3\n"
                                 "Initial Value = 7\n[Create]\nName = C\nInterarrival = T(1,1)\n"
                                 "Next = O\n[Dispose]\nName = O\n").document, one, d);
        check(!hasErrors(d), "one value fills every cell");
        d.clear();
        Model scalar;
        compileInto(readDocument("version = 1\n[Variable]\nName = V\nInitial Value = x\n").document,
                    scalar, d);
        check(hasErrors(d), "a plain variable still needs a number");
        const ParseResult p = parseExpression("Nope(1)");
        check(hasErrors(p.diagnostics), "an unknown function is still an unknown function");
    }
    {
        VariableStore v;
        v.declareArray("T", 3, 3, {0});
        bool threw = false;
        try { v.element("T", 4, 1); } catch (const ModelError& e) {
            threw = std::string(e.what()).find("3 x 3") != std::string::npos;
        }
        check(threw, "an index outside the array stops the run, naming the table's size");
        check(v.element("T", 2.4, 3) == 0.0, "indices are rounded");
        check(v.names().empty(), "arrays are not in the time-averaged variable list");
        Model m;
        m.variableArray("T", 2, 2, {1, 2, 3, 4});
        bool refused = false;
        try { m.assignVariable("A", "T", "1"); } catch (const ModelError&) { refused = true; }
        check(refused, "assigning to an array is refused, for now");
    }

    section("Records share a statistic, and TimeInterval measures from an attribute");
    {
        const std::string text =
            "version = 1\n[Run]\nName = R\nLength = 100\n"
            "[Create]\nName = C\nInterarrival = 10\nMax Arrivals = 3\nNext = S\n"
            "[Assign]\nName = S\nNext = D\n"
            "[AssignField]\nAssign = S\nTarget = Attribute\nName = t0\nValue = TNOW\n"
            "[Delay]\nName = D\nDuration = 2\nNext = W\n"
            "[Record]\nName = W\nWhat = TimeInterval\nAttribute = t0\nStatistic = Wait\nNext = K\n"
            "[Record]\nName = K\nWhat = Count\nValue = 2\nStatistic = Trips\nNext = E\n"
            "[Record]\nName = E\nWhat = Expression\nValue = t0 <= 5\n";
        SimulationSystem sim(1);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(text).document, sim.model(), d), "it compiles");
        sim.stopAt(100).execute();
        check(sim.model().nodeAs<RecordNode>("W").average() == 2.0, "every interval is 2");
        check(sim.model().nodeAs<RecordNode>("W").statistic() == "Wait", "under its statistic's name");
        check(sim.model().nodeAs<RecordNode>("K").total() == 6.0, "3 passes x Value 2 = 6");
        checkClose(sim.model().nodeAs<RecordNode>("E").average(), 1.0 / 3.0, 1e-12,
                   "an Expression Record averages its value: one of three stamped by 5");
        check(sim.model().nodeAs<RecordNode>("E").statistic() == "E",
              "an unnamed statistic is the block's name");
    }
    {
        std::vector<Diagnostic> d;
        Model m;
        compileInto(readDocument(
            "version = 1\n[Create]\nName = C\nInterarrival = 1\nNext = A\n"
            "[Record]\nName = A\nWhat = Count\nStatistic = X\nNext = B\n"
            "[Record]\nName = B\nWhat = TimeInSystem\nStatistic = X\n").document, m, d);
        check(hasErrors(d), "a counter and a tally may not share a name");
        d.clear();
        Model shared;
        compileInto(readDocument(
            "version = 1\n[Create]\nName = C\nInterarrival = 1\nNext = A\n"
            "[Record]\nName = A\nWhat = TimeInSystem\nStatistic = X\nNext = B\n"
            "[Record]\nName = B\nWhat = TimeInSystem\nStatistic = X\n").document, shared, d);
        check(!hasErrors(d), "two tallies may");
    }

    section("A Route delivers to its Station after the route time");
    {
        const std::string text =
            "version = 1\n[Run]\nName = R\nLength = 100\n"
            "[Create]\nName = C\nInterarrival = 10\nMax Arrivals = 2\nNext = Go\n"
            "[Route]\nName = Go\nRoute Time = 3\nDestination = There\n"
            "[Station]\nName = There\nNext = T\n"
            "[Record]\nName = T\nWhat = TimeInSystem\n";
        SimulationSystem sim(1);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(text).document, sim.model(), d), "it compiles");
        check(!hasErrors(d) && d.empty(), "with nothing to warn about");
        sim.stopAt(100).execute();
        check(sim.model().nodeAs<RecordNode>("T").average() == 3.0, "3 on the road");
        check(sim.model().nodeAs<RecordNode>("T").count() == 2, "both arrived");
        check(sim.model().nodeAs<RouteNode>("Go").travelled() == 2, "the Route counts its trips");
        checkClose(sim.model().nodeAs<RouteNode>("Go").averageInTransit(100.0), 0.06, 1e-12,
                   "and 6 entity-units on the road over 100 is 0.06 on average");
    }
    {
        std::vector<Diagnostic> d;
        Model m;
        compileInto(readDocument("version = 1\n[Create]\nName = C\nInterarrival = 1\nNext = Go\n"
                                 "[Route]\nName = Go\nRoute Time = 1\nDestination = C\n").document, m, d);
        check(hasErrors(d), "a Route must go to a Station");
        d.clear();
        Model lonely;
        compileInto(readDocument("version = 1\n[Create]\nName = C\nInterarrival = 1\nNext = O\n"
                                 "[Dispose]\nName = O\n[Station]\nName = Nowhere\n").document,
                    lonely, d);
        bool warned = false;
        for (const Diagnostic& x : d)
            warned |= x.severity == Severity::Warning &&
                      x.message.find("Nowhere") != std::string::npos;
        check(warned && !hasErrors(d), "a Station nothing reaches is a warning, not an error");
    }

    section("Match pairs entities with the same attribute");
    {
        // Riders at 0 and 10, zone 1. One car at 25, zone 1: it takes the
        // rider who has waited longest. A zone-2 car at 30 matches nobody.
        const std::string text =
            "version = 1\n[Run]\nName = R\nLength = 100\n"
            "[Create]\nName = Riders\nInterarrival = 10\nMax Arrivals = 2\nNext = SR\n"
            "[Assign]\nName = SR\nNext = RL\n"
            "[AssignField]\nAssign = SR\nTarget = Attribute\nName = zone\nValue = 1\n"
            "[AssignField]\nAssign = SR\nTarget = Attribute\nName = t0\nValue = TNOW\n"
            "[Create]\nName = Cars\nInterarrival = 5\nFirst At = 25\nMax Arrivals = 2\nNext = SC\n"
            "[Assign]\nName = SC\nNext = CL\n"
            "[AssignField]\nAssign = SC\nTarget = Attribute\nName = zone\nValue = TNOW / 25\n"
            "[Match]\nName = M\nType = Attribute\nAttribute = zone\n"
            "[MatchLane]\nMatch = M\nName = RL\nNext = Picked\n"
            "[MatchLane]\nMatch = M\nName = CL\nNext = Gone\n"
            "[Record]\nName = Picked\nWhat = TimeInterval\nAttribute = t0\n"
            "[Dispose]\nName = Gone\n";
        SimulationSystem sim(1);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(text).document, sim.model(), d), "it compiles");
        sim.stopAt(100).execute();
        const auto& picked = sim.model().nodeAs<RecordNode>("Picked");
        check(picked.count() == 1, "one matching car picks up exactly one rider");
        check(picked.average() == 25.0, "the rider who has waited longest goes first");
        const auto& riders = sim.model().nodeAs<MatchLaneNode>("RL");
        const auto& cars = sim.model().nodeAs<MatchLaneNode>("CL");
        check(riders.waiting() == 1, "the other rider still waits");
        check(cars.waiting() == 1, "and so does the car from another zone");
        check(riders.averageWait() == 25.0 && cars.averageWait() == 0.0,
              "each lane measures its own waits");
        // Rider area: 1 over [0,10), 2 over [10,25), 1 over [25,100) = 10+30+75.
        checkClose(riders.averageWaiting(100.0), 115.0 / 100.0, 1e-12,
                   "and its time-average length");
    }
    {
        const std::string anyText =
            "version = 1\n[Run]\nName = R\nLength = 50\n"
            "[Create]\nName = A\nInterarrival = 10\nNext = LA\n"
            "[Create]\nName = B\nInterarrival = 20\nNext = LB\n"
            "[Match]\nName = M\n"
            "[MatchLane]\nMatch = M\nName = LA\nNext = Out\n"
            "[MatchLane]\nMatch = M\nName = LB\nNext = Out\n"
            "[Dispose]\nName = Out\n";
        SimulationSystem sim(1);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(anyText).document, sim.model(), d), "a Match of Any compiles");
        sim.stopAt(45).execute();
        check(sim.model().nodeAs<MatchNode>("M").matches() == 3,
              "Any matches whoever is at the head: B's arrivals at 0, 20, 40");
        // stopAt ends after the first event past the limit, so A's arrival
        // at 50 happens: A at 30, 40 and 50 are left.
        check(sim.queueLength("LA") == 3.0, "NQ reads a lane");
    }
    {
        std::vector<Diagnostic> d;
        Model m;
        compileInto(readDocument("version = 1\n[Create]\nName = C\nInterarrival = 1\nNext = L\n"
                                 "[Match]\nName = M\n[MatchLane]\nMatch = M\nName = L\n").document, m, d);
        check(hasErrors(d), "a Match needs at least two lanes");
        d.clear();
        Model toMatch;
        compileInto(readDocument("version = 1\n[Create]\nName = C\nInterarrival = 1\nNext = M\n"
                                 "[Match]\nName = M\n[MatchLane]\nMatch = M\nName = A\n"
                                 "[MatchLane]\nMatch = M\nName = B\n").document, toMatch, d);
        bool named = false;
        for (const Diagnostic& x : d) named |= x.message.find("one of its lanes: A, B") != std::string::npos;
        check(named, "routing to the Match itself names its lanes");
    }
    {
        // Cars loop back into their lane for ever; that is a population, not a
        // routing loop, and must compile.
        const std::string loop =
            "version = 1\n[Run]\nName = R\nLength = 30\n"
            "[Create]\nName = Riders\nInterarrival = 1\nNext = RL\n"
            "[Create]\nName = Cars\nInterarrival = 1\nMax Arrivals = 1\nPer Arrival = 2\nNext = CL\n"
            "[Match]\nName = M\n"
            "[MatchLane]\nMatch = M\nName = RL\nNext = Out\n"
            "[MatchLane]\nMatch = M\nName = CL\nNext = Trip\n"
            "[Delay]\nName = Trip\nDuration = 3\nNext = CL\n"
            "[Dispose]\nName = Out\n";
        std::vector<Diagnostic> d;
        Model m;
        check(compileInto(readDocument(loop).document, m, d), "cars circulating through a Match lane compile");
    }

    section("A schedule changes the arrival rate over time");
    {
        ArrivalSchedule s({{0.0, 60.0}, {2.0, 60.0}});
        check(s.rateAt(30) == 0.0 && s.rateAt(90) == 2.0, "the rate follows the rows");
        check(s.rateAt(150) == 0.0, "and repeats after the last one");
        check(s.peak() == 2.0 && s.cycle() == 120.0, "peak and cycle");
        check(unitsPer(TimeUnit::Hours, TimeUnit::Minutes) == 60.0, "an hour is 60 minutes");
        bool threw = false;
        try { ArrivalSchedule none({{0.0, 1.0}}); } catch (const ModelError&) { threw = true; }
        check(threw, "a schedule of nothing but zeros is refused");

        // Nothing in the first hour, 600 an hour in the second: every arrival
        // lands in minutes 60..120, and there are about 600 of them.
        const std::string text =
            "version = 1\n[Run]\nName = R\nLength = 120\nTime Units = Minutes\n"
            "[Schedule]\nName = S\nType = Arrival\nTime Units = Hours\n"
            "[ScheduleRow]\nSchedule = S\nValue = 0\nDuration = 1\n"
            "[ScheduleRow]\nSchedule = S\nValue = 600\nDuration = 1\n"
            "[Create]\nName = C\nSchedule = S\nNext = Stamp\n"
            "[Assign]\nName = Stamp\nNext = Early\n"
            "[AssignField]\nAssign = Stamp\nTarget = Attribute\nName = at\nValue = TNOW\n"
            "[Record]\nName = Early\nWhat = Expression\nValue = at < 60\n";
        std::vector<Diagnostic> d;
        SimulationSystem sim(7);
        check(compileInto(readDocument(text).document, sim.model(), d), "it compiles");
        check(sim.model().nodeAs<CreateNode>("C").scheduled(), "and the Create follows the schedule");
        sim.stopAt(119.999).execute();
        const auto& early = sim.model().nodeAs<RecordNode>("Early");
        check(early.count() > 520 && early.count() < 680,
              "about 600 arrivals (got " + std::to_string(early.count()) + ")");
        check(early.total() == 0.0, "and none of them in the first hour");
    }
    {
        std::vector<Diagnostic> d;
        Model m;
        compileInto(readDocument("version = 1\n[Schedule]\nName = S\n"
                                 "[ScheduleRow]\nSchedule = S\nValue = 1\nDuration = 1\n"
                                 "[Create]\nName = C\nSchedule = S\nInterarrival = 1\n").document, m, d);
        check(hasErrors(d), "a Create cannot have both a schedule and an interarrival");
        d.clear();
        Model neither;
        compileInto(readDocument("version = 1\n[Create]\nName = C\nNext = O\n[Dispose]\nName = O\n").document,
                    neither, d);
        check(hasErrors(d), "nor neither");
        d.clear();
        Model zero;
        compileInto(readDocument("version = 1\n[Schedule]\nName = S\n"
                                 "[ScheduleRow]\nSchedule = S\nValue = 0\nDuration = 1\n"
                                 "[Create]\nName = C\nSchedule = S\nNext = O\n[Dispose]\nName = O\n").document,
                    zero, d);
        check(hasErrors(d), "a schedule where nothing ever arrives is an error");
    }

    section("Named statistics reach both reports");
    {
        const std::string many =
            "version = 1\n[Run]\nName = R\nLength = 50\nReplications = 3\n"
            "[Create]\nName = C\nInterarrival = EXPO(1)\nNext = W\n"
            "[Record]\nName = W\nWhat = Count\nStatistic = Trips\nNext = G\n"
            "[Route]\nName = G\nRoute Time = 2\nDestination = S\n"
            "[Station]\nName = S\n";
        std::vector<Diagnostic> d;
        auto run = RunController::fromDocument(readDocument(many).document, d);
        check(run != nullptr, "a model with no Process can be set up");
        run->runToCompletion();
        check(run->state() == RunState::Finished, "and run to the end of a study");
        std::ostringstream os;
        run->report(os);
        check(os.str().find("Trips (counter)") != std::string::npos,
              "the counter is in the multi-replication report");
        check(os.str().find("G avg in transit") != std::string::npos, "so is the Route");
        check(os.str().find("+/-") != std::string::npos, "with half-widths");
        check(run->namedStatistics().size() == 3 && run->namedStatistics()[0].size() == 2,
              "one list per replication");

        std::string once = many;
        once.replace(once.find("Replications = 3"), 16, "Replications = 1");
        std::vector<Diagnostic> d1;
        auto single = RunController::fromDocument(readDocument(once).document, d1);
        single->runToCompletion();
        std::ostringstream one;
        single->report(one);
        check(one.str().find("Trips (counter)") != std::string::npos,
              "and in the single-run report");

        // Two Records into one tally are pooled over every value recorded.
        SimulationSystem sim(1);
        std::vector<Diagnostic> d2;
        compileInto(readDocument(
            "version = 1\n[Create]\nName = C\nInterarrival = 1\nMax Arrivals = 3\nNext = A\n"
            "[Record]\nName = A\nWhat = Expression\nValue = 1\nStatistic = X\nNext = B\n"
            "[Record]\nName = B\nWhat = Expression\nValue = 4\nStatistic = X\n").document,
            sim.model(), d2);
        sim.stopAt(10).execute();
        const std::vector<NamedStatistic> named = sim.namedStatistics();
        check(named.size() == 1 && named[0].label == "X (tally)" && named[0].value == 2.5,
              "one tally, the mean of all six values");
    }
}
