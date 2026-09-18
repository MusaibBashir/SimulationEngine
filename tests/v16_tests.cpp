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
}
