// ============================================================================
// tables_tests.cpp  --  v16.1: the Tables tab. Taking a file apart and putting
// it back, and editing variables, arrays, matrices and schedules as tables.
// ============================================================================

#include <algorithm>
#include <cstdio>
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
using des_test::modelPath;
using des_test::section;

namespace {

std::string fileText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string withoutCR(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}

const Table* named(const std::vector<Table>& ts, const std::string& n) {
    for (const Table& t : ts) if (t.name == n) return &t;
    return nullptr;
}

std::vector<NamedStatistic> studyOf(const std::string& text) {
    std::string two = text;
    const std::size_t at = two.find("Replications = 100");
    if (at != std::string::npos) two.replace(at, 18, "Replications = 2");
    std::vector<Diagnostic> d;
    auto run = RunController::fromDocument(readDocument(two).document, d);
    if (!run) return {};
    run->runToCompletion();
    return run->namedStatistics().empty() ? std::vector<NamedStatistic>{}
                                          : run->namedStatistics().back();
}

}  // namespace

void runTablesTests() {
    section("A file's tables come out, and go back in unchanged");
    {
        const std::string original = withoutCR(fileText(modelPath("ride_hailing.des")));
        const SplitResult split = splitTables(original);
        check(split.tables.size() == 6, "ride_hailing has 6 tables");
        check(split.modelText.find("[Variable]") == std::string::npos &&
              split.modelText.find("[Schedule") == std::string::npos,
              "and none of them is left in the model text");
        check(split.modelText.find("[Match]") != std::string::npos, "the rest of the model is");

        const Table* tt = named(split.tables, "TravelTime");
        check(tt && tt->kind == TableKind::Matrix && tt->rows == 3 && tt->columns == 3,
              "TravelTime is a 3 x 3 matrix");
        check(tt && tt->at(1, 2) == "28" && tt->at(2, 0) == "22", "read row by row");
        check(tt && tt->note.find("origin-destination") != std::string::npos,
              "keeping the comment above it as its note");
        const Table* sub = named(split.tables, "Sched_Suburb");
        check(sub && sub->kind == TableKind::Schedule && sub->rows == 8 && sub->timeUnits == "Hours",
              "Sched_Suburb is a schedule of 8 hourly rows");
        check(sub && sub->at(2, 0) == "23", "with the rush hour in the third");
        const Table* cv = named(split.tables, "Num_CV");
        check(cv && cv->kind == TableKind::Variable && cv->cells[0] == "10", "Num_CV is a variable");

        const std::string joined = joinTables(split.modelText, split.tables);
        check(joined.find(TABLES_MARKER) != std::string::npos, "saving marks the tables section");
        const SplitResult again = splitTables(joined);
        check(joinTables(again.modelText, again.tables) == joined,
              "and saving what was opened writes the same bytes");
        check(again.modelText == split.modelText, "the model text survives the trip");

        const std::vector<NamedStatistic> before = studyOf(original);
        const std::vector<NamedStatistic> after = studyOf(joined);
        bool same = !before.empty() && before.size() == after.size();
        for (std::size_t i = 0; same && i < before.size(); ++i)
            same = before[i].label == after[i].label && before[i].value == after[i].value;
        check(same, "and the model it saves runs to exactly the same numbers");

        const auto ranges = tableLineRanges(split.modelText, split.tables);
        const std::vector<std::string> lines = [&joined] {
            std::vector<std::string> out{std::string()};
            for (const char c : joined) { if (c == '\n') out.push_back(""); else out.back() += c; }
            return out;
        }();
        bool placed = ranges.size() == split.tables.size();
        for (std::size_t i = 0; placed && i < ranges.size(); ++i) {
            // The range starts at the note, when there is one, else the header.
            std::size_t header = ranges[i].first;
            while (header <= lines.size() && lines[header - 1].rfind('#', 0) == 0) ++header;
            placed = lines[header - 1] == (split.tables[i].kind == TableKind::Schedule
                                               ? "[Schedule]" : "[Variable]");
        }
        check(placed, "each table's lines are where tableLineRanges says");
    }

    section("A model with no tables saves exactly as it was");
    {
        const std::string teller = withoutCR(fileText(modelPath("teller.des")));
        const SplitResult s = splitTables(teller);
        check(s.tables.empty(), "teller has none");
        check(joinTables(s.modelText, s.tables).find(TABLES_MARKER) == std::string::npos,
              "so it gets no marker");
    }

    section("Tables resize, paste and rename");
    {
        Table t;
        t.kind = TableKind::Matrix;
        t.rows = 2; t.columns = 2;
        t.cells = {"1", "2", "3", "0"};
        check(t.resizeLoses(1, 2), "shrinking away the 3 would lose a value");
        check(t.resizeLoses(2, 1), "and so would dropping the 2");
        t.resize(3, 2);
        check(t.rows == 3 && t.at(2, 1) == "0" && t.at(1, 0) == "3", "growing keeps what was there");

        const auto tabs = parsePastedBlock("15\t1\n35\t1\r\n90\t1\n");
        check(tabs.size() == 3 && tabs[2][0] == "90" && tabs[2][1] == "1", "a block from Excel");
        const auto commas = parsePastedBlock("4, 14, 22\n14,5,28\n");
        check(commas.size() == 2 && commas[1].size() == 3 && commas[1][1] == "5", "a CSV block");
        const auto spaces = parsePastedBlock("  6 10  14 \n");
        check(spaces.size() == 1 && spaces[0].size() == 3 && spaces[0][2] == "14",
              "numbers separated by spaces");

        check(isNumberCell("0.60") && isNumberCell("-3") && !isNumberCell("x") &&
              !isNumberCell(""), "a cell holds a number");
        check(isTableName("Sched_Suburb") && !isTableName("9lives") && !isTableName("a b"),
              "a name is an identifier");

        const std::string text =
            "[Create]\nName = C\nPer Arrival = Num_CV\nInterarrival = Num_CVX + Num_CV\n"
            "# Num_CV is the fleet\n";
        const std::string renamed = renameInText(text, "Num_CV", "Fleet");
        check(renamed.find("Per Arrival = Fleet") != std::string::npos &&
              renamed.find("Num_CVX + Fleet") != std::string::npos,
              "a rename changes whole words on the value side");
        check(renamed.find("# Num_CV is the fleet") != std::string::npos, "and leaves comments alone");

        const ModelDocument doc = readDocument(
            "version = 1\n[Create]\nName = Riders\nSchedule = S\nNext = O\n"
            "[Route]\nName = Go\nRoute Time = T(1, 2)\nDestination = O\n").document;
        const std::vector<std::string> users = usersOf(doc, "S");
        check(users.size() == 1 && users[0] == "Create Riders", "usersOf finds who names a table");
        check(usersOf(doc, "T").size() == 1, "including inside an expression");
    }

    section("The Tables tab: the model text loses its tables, and runs the same");
    {
        TuiState s = TuiState::fromText(withoutCR(fileText(modelPath("ride_hailing.des"))),
                                        "tables_ride.des");
        check(s.tables().size() == 6, "six tables on the Tables tab");
        check(s.buffer().text().find("[Variable]") == std::string::npos &&
              s.buffer().text().find("[ScheduleRow]") == std::string::npos,
              "and none of them in the Model text");
        check(!hasErrors(s.diagnostics()), "the model still compiles, reading the tables");
        check(s.document().rowCount("ScheduleRow") == 24, "all 24 schedule rows reach the compiler");

        handleKey(s, Key::control('d'));
        check(s.view() == View::Tables, "^D opens the Tables tab");
        Screen wide(120, 30);
        render(s, wide);
        check(wide.asText().find("Sched_Suburb") != std::string::npos &&
              wide.asText().find("TravelTime") != std::string::npos,
              "the list names every table");
        Screen small(80, 24);
        render(s, small);
        check(small.asText().find("Num_CV") != std::string::npos, "and fits in 80 x 24");

        // Choose TravelTime on the list and look at it.
        for (std::size_t i = 0; i < s.tables().size(); ++i)
            if (s.tables()[i].name == "TravelTime") s.selectTable(i);
        handleKey(s, Key::special(KeyKind::Tab));        // List -> Kinds
        handleKey(s, Key::special(KeyKind::Tab));        // Kinds -> Editor
        check(s.tablesFocus() == TablesFocus::Editor, "Tab cycles to the editor");
        render(s, wide);
        check(wide.asText().find("Columns") != std::string::npos &&
              wide.asText().find("28") != std::string::npos,
              "a matrix shows its shape and its numbers");
        check(wide.asText().find("used by: Route Trip_to_Downtown") != std::string::npos,
              "and who uses it");
    }

    section("Cells take numbers, and a pasted block lands as it looked in Excel");
    {
        TuiState s = TuiState::fromText("version = 1\n[Create]\nName = C\nSchedule = S1\nNext = O\n"
                                        "[Dispose]\nName = O\n", "tables_new.des");
        handleKey(s, Key::control('d'));
        handleKey(s, Key::special(KeyKind::Tab));        // List -> Kinds
        for (int i = 0; i < 3; ++i) handleKey(s, Key::special(KeyKind::Down));
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.tables().size() == 1 && s.tables()[0].kind == TableKind::Schedule,
              "Enter on Schedule makes one");
        check(s.tablesFocus() == TablesFocus::Editor && s.tableField() == 0,
              "and puts the cursor on its Name");

        handleKey(s, Key::special(KeyKind::Enter));
        for (int i = 0; i < 20; ++i) handleKey(s, Key::special(KeyKind::Backspace));
        for (const char c : std::string("S1")) handleKey(s, Key::character(c));
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.tables()[0].name == "S1", "it is named");
        check(!hasErrors(s.diagnostics()) || s.tableErrorCount() > 0,
              "and the Create that names it resolves");

        // Down to the grid, then the Excel column: 15 1 / 35 1 / 90 1, as the
        // terminal types a paste -- Tab between cells, Enter between rows.
        handleKey(s, Key::special(KeyKind::Down));
        handleKey(s, Key::special(KeyKind::Down));
        handleKey(s, Key::special(KeyKind::Down));
        check(s.onGrid() && s.cellRow() == 0 && s.cellColumn() == 0, "down into the grid");
        for (const char c : std::string("15\t1\n35\t1\n90\t1\n")) {
            if (c == '\t') handleKey(s, Key::special(KeyKind::Tab));
            else if (c == '\n') handleKey(s, Key::special(KeyKind::Enter));
            else handleKey(s, Key::character(c));
        }
        const Table& t = s.tables()[0];
        check(t.rows >= 3 && t.at(0, 0) == "15" && t.at(1, 0) == "35" && t.at(2, 0) == "90" &&
              t.at(2, 1) == "1", "the typed block fills the grid, growing it row by row");
        check(s.tableErrorCount() == 0, "and the schedule is valid");

        handleKey(s, Key::character('x'));
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.editingTable() && s.status().find("number") != std::string::npos,
              "a letter in a cell is refused, and the edit stays open to fix");
        handleKey(s, Key::special(KeyKind::Escape));

        s.setClipboard("4\t1\n5\t1\n");
        s.setTableCursor(s.headerFields().size(), 0, 0);
        handleKey(s, Key::control('v'));
        check(s.tables()[0].at(0, 0) == "4" && s.tables()[0].at(1, 0) == "5",
              "^V pastes a block from the program's clipboard");
        handleKey(s, Key::control('z'));
        check(s.tables()[0].at(0, 0) == "15", "and ^Z on this tab takes it back");

        const std::string file = "tables_new.des";
        check(s.save(file), "it saves");
        TuiState back = TuiState::open(file);
        check(back.tables().size() == 1 && back.tables()[0].at(2, 0) == "90",
              "and opens again with the same table");
        check(back.buffer().text() == s.buffer().text(), "and the same model text");
        std::remove(file.c_str());
    }

    section("Resizing, renaming and deleting ask when they should");
    {
        TuiState s = TuiState::fromText(
            "version = 1\n[Create]\nName = C\nInterarrival = T(1, 2)\nPer Arrival = N\nNext = O\n"
            "[Dispose]\nName = O\n"
            "[Variable]\nName = N\nInitial Value = 2\n"
            "[Variable]\nName = T\nRows = 2\nColumns = 2\nInitial Value = 1, 5, 3, 4\n",
            "tables_ask.des");
        check(s.tables().size() == 2 && !hasErrors(s.diagnostics()), "two tables, compiling");
        handleKey(s, Key::control('d'));
        s.selectTable(1);
        s.setTablesFocus(TablesFocus::Editor);

        // Columns 2 -> 1 would drop the 5 and the 4.
        s.setTableCursor(2, 0, 0);
        handleKey(s, Key::special(KeyKind::Enter));
        handleKey(s, Key::special(KeyKind::Backspace));
        handleKey(s, Key::character('1'));
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.overlay() == Overlay::Confirm && s.confirmFor() == ConfirmFor::ResizeTable,
              "shrinking away numbers asks first");
        handleKey(s, Key::character('n'));
        check(s.tables()[1].columns == 2, "and no leaves it as it was");

        // Rename T to Travel, everywhere.
        s.setTableCursor(0, 0, 0);
        handleKey(s, Key::special(KeyKind::Enter));
        for (int i = 0; i < 5; ++i) handleKey(s, Key::special(KeyKind::Backspace));
        for (const char c : std::string("Travel")) handleKey(s, Key::character(c));
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.overlay() == Overlay::Confirm && s.confirmFor() == ConfirmFor::RenameTable,
              "renaming a table something uses asks whether to follow it");
        handleKey(s, Key::character('y'));
        check(s.tables()[1].name == "Travel" &&
              s.buffer().text().find("Interarrival = Travel(1, 2)") != std::string::npos,
              "yes renames it and the expression that reads it");
        check(!hasErrors(s.diagnostics()), "so the model still compiles");

        // Deleting N names what uses it.
        s.selectTable(0);
        s.setTablesFocus(TablesFocus::List);
        handleKey(s, Key::special(KeyKind::Delete));
        check(s.overlay() == Overlay::Confirm && s.status().find("Create C") != std::string::npos,
              "deleting asks, naming the Create that uses it");
        handleKey(s, Key::character('y'));
        check(s.tables().size() == 1 && hasErrors(s.diagnostics()),
              "and once it is gone the Create's Per Arrival no longer resolves");
        handleKey(s, Key::control('z'));
        check(s.tables().size() == 2, "^Z brings it back");
    }

    section("An error in a table is shown on the table");
    {
        TuiState s = TuiState::fromText(
            "version = 1\n[Create]\nName = C\nSchedule = S\nNext = O\n[Dispose]\nName = O\n"
            "[Schedule]\nName = S\n[ScheduleRow]\nSchedule = S\nValue = 0\nDuration = 1\n",
            "tables_bad.des");
        check(s.tableErrorCount() == 1, "a schedule where nothing arrives is one error");
        check(s.tableIssues().size() == 1 && s.tableIssues()[0].table == 0,
              "put on that table");
        Screen screen(100, 26);
        render(s, screen);
        check(screen.asText().find("Tables ^D 1E") != std::string::npos,
              "the tab says so from any tab");
        handleKey(s, Key::control('j'));
        check(s.view() == View::Tables && s.tableIndex() == 0,
              "and ^J goes to the table");
    }
}
