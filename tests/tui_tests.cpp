// ============================================================================
// tests/tui_tests.cpp  --  v13: the terminal UI, tested without a terminal
// ============================================================================
// Every test here builds a state, feeds keys, and asserts on a Screen AS TEXT.
// That is possible because the screen is a value: nothing in the UI below
// tui/ touches a terminal, so all of it runs under the sanitisers with
// everything else.
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "harness.hpp"
#include "des_ui.hpp"

using namespace des;
using des_test::check;
using des_test::modelPath;
using des_test::section;

void runTuiTests() {
    section("Screen is a value");
    {
        Screen s(10, 3);
        check(s.width() == 10 && s.height() == 3, "a screen has a size");
        check(s.line(0).empty(), "and starts blank");

        s.text(2, 1, "hi");
        check(s.line(1) == "  hi", "text lands where it was put");
        check(s.at(2, 1).ch == 'h', "and the glyph is readable");

        // Out of range is DROPPED, not asserted. A renderer that computes one
        // column past the edge has a layout bug, and a crash in front of the
        // user is a worse way to learn about it than a missing character.
        s.text(8, 1, "abcdef");
        check(s.line(1) == "  hi    ab", "a run that overflows is clipped, not fatal");
        s.put(-1, -1, 'x');
        s.put(999, 999, 'x');
        check(s.line(1) == "  hi    ab", "and writes outside it change nothing");

        s.text(0, 2, "trailing   ");
        check(s.line(2) == "trailing", "line() trims the trailing blanks");

        Screen t(10, 3);
        t.text(2, 1, "hi");
        t.text(8, 1, "ab");
        t.text(0, 2, "trailing");
        check(s.asText() == t.asText(), "two screens with the same glyphs read alike");

        s.clear();
        check(s.asText() == Screen(10, 3).asText(), "clear() empties it");
    }

    section("Key is a value");
    {
        const Key a = Key::character('a');
        check(a.kind == KeyKind::Char && a.ch == 'a', "a printable key carries its char");
        const Key ctrlS = Key::control('s');
        check(ctrlS.kind == KeyKind::Ctrl && ctrlS.ch == 'S',
              "a control key normalises to upper case, so ^s and ^S are one key");
        check(Key::special(KeyKind::Up).kind == KeyKind::Up, "a special key is its kind");
    }
    section("TuiState opens a document");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));
        check(!s.document().types().empty(), "a real file opens with its types");
        check(s.path() == modelPath("teller.des"), "and remembers where it came from");
        check(!s.dirty(), "a freshly opened document is not dirty");
        check(s.mode() == Mode::Grid, "and starts in the grid");

        // The tab bar's source of truth: every type the registry publishes,
        // then any the document holds that it does not. No module type name is
        // written in the UI's source.
        const std::vector<std::string> tabs = s.types();
        check(tabs.size() >= ModuleRegistry::instance().all().size(),
              "every published type gets a tab");
        bool sawProcess = false, sawRun = false;
        for (const std::string& t : tabs) {
            if (t == "Process") sawProcess = true;
            if (t == "Run")     sawRun = true;
        }
        check(sawProcess && sawRun, "including ones this document happens to use");

        s.setType("Process");
        check(s.type() == "Process", "the cursor can move to a type");
        check(!s.columnsHere().empty(), "which knows its columns");
        check(s.schemaHere() != nullptr, "and its schema");

        {
            // A type the registry does NOT know still gets a tab. v11 preserves
            // unknown modules rather than dropping them, and a UI that showed
            // only what it understood would hide the rows a person needs in
            // order to notice their editor is older than the file.
            ModelDocument d;
            d.addRow("FromTheFuture");
            d.setCell("FromTheFuture", 0, "X", "1");
            TuiState u = TuiState::fromDocument(d, "future.des");
            bool sawUnknown = false;
            for (const std::string& t : u.types())
                if (t == "FromTheFuture") sawUnknown = true;
            check(sawUnknown, "an unknown module type is still shown");
            u.setType("FromTheFuture");
            check(u.schemaHere() == nullptr, "with no schema, which the renderer must handle");
            check(u.columnsHere().size() == 1,
                  "its columns come from the rows themselves");
        }

        {
            TuiState missing = TuiState::open("no_such_file_here.des");
            check(missing.document().types().empty(),
                  "a file that does not exist opens as an EMPTY document");
            check(missing.status().find("new") != std::string::npos,
                  "and says so, because empty and unreadable must not look alike");
        }
    }

    section("TuiState saves without disturbing what it did not touch");
    {
        // The guarantee v11 was built on, exercised through the UI for the
        // first time. If the UI marks rows edited that nobody edited, this is
        // what catches it.
        const std::string src = modelPath("teller.des");
        std::ifstream in(src, std::ios::binary);
        const std::string before((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());

        TuiState s = TuiState::open(src);
        check(s.save("tui_roundtrip.des"), "it saves");
        std::ifstream out("tui_roundtrip.des", std::ios::binary);
        const std::string after((std::istreambuf_iterator<char>(out)),
                                std::istreambuf_iterator<char>());
        check(before == after,
              "opening a file and saving it UNEDITED gives byte-identical bytes");
    }
    section("Edits, and undo across all four of them");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.8)");
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Process");

        check(!s.canUndo(), "nothing to undo yet");

        s.setCell("Service", "EXPO(0.5)");
        check(s.document().cell("Process", 0, "Service") == "EXPO(0.5)", "a cell edits");
        check(s.dirty(), "and the document is dirty");
        check(s.canUndo(), "and undoable");
        s.undo();
        check(s.document().cell("Process", 0, "Service") == "EXPO(0.8)", "undo restores it");

        s.addRow();
        check(s.rowCountHere() == 2, "a row is added");
        s.undo();
        check(s.rowCountHere() == 1, "and undone");

        s.addRow();
        s.setRow(1);
        s.setCell("Name", "Check");
        s.removeRow();
        check(s.rowCountHere() == 1, "a row is removed");
        s.undo();
        check(s.rowCountHere() == 2, "and undone");
        check(s.document().cell("Process", 1, "Name") == "Check",
              "with its contents intact, which is what a snapshot buys");

        // Row order is SEMANTIC in this format -- a Decide takes the first
        // branch that matches -- so moving a row is a model change, not a
        // display preference, and has to be undoable like any other.
        s.setRow(1);
        s.moveRow(-1);
        check(s.document().cell("Process", 0, "Name") == "Check", "a row moves");
        s.undo();
        check(s.document().cell("Process", 0, "Name") == "Serve", "and unmoves");
    }

    section("A read-only module refuses an edit and says why");
    {
        ModelDocument d;
        d.addRow("Queue");
        d.setCell("Queue", 0, "Name", "Serve.Queue");
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Queue");
        check(s.readOnlyHere(), "Queue is read-only");
        s.setCell("Name", "Something Else");
        check(s.document().cell("Queue", 0, "Name") == "Serve.Queue",
              "so the edit does not land");
        check(s.status().find("read-only") != std::string::npos, "and it says why");
        check(!s.dirty(), "and nothing became dirty");
    }
}
