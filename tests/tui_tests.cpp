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
    section("Rendering: the tab bar and the grid");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.8)");
        d.addRow("Process");
        d.setCell("Process", 1, "Name", "Check");
        d.setCell("Process", 1, "Service", "EXPO(0.3)");
        TuiState s = TuiState::fromDocument(d, "teller.des");
        s.setType("Process");

        Screen screen(100, 30);
        render(s, screen);
        const std::string text = screen.asText();

        check(text.find("Process") != std::string::npos, "the tab bar names the type");
        check(text.find("Resource") != std::string::npos,
              "and every other type the registry publishes");
        check(text.find("teller.des") != std::string::npos, "the file is named");
        check(text.find("Serve") != std::string::npos, "the grid shows row 1");
        check(text.find("Check") != std::string::npos, "and row 2");
        check(text.find("Service") != std::string::npos, "with its column headings");

        bool marked = false;
        for (int y = 0; y < screen.height(); ++y) {
            const std::string ln = screen.line(y);
            if (ln.find("Serve") != std::string::npos && ln.find('>') != std::string::npos)
                marked = true;
        }
        check(marked, "the current row is marked");

        s.setRow(1);
        Screen second(100, 30);
        render(s, second);
        check(screen.asText() != second.asText(),
              "and moving the cursor changes what is drawn");

        {
            // A dirty document says so, or a person loses work believing it is
            // saved.
            TuiState t = TuiState::fromDocument(d, "teller.des");
            t.setType("Process");
            t.setCell("Name", "Renamed");
            Screen third(100, 30);
            render(t, third);
            check(third.asText().find("teller.des*") != std::string::npos,
                  "an edited document is marked with a star");
        }
    }

    section("Rendering: a terminal too small says so and draws nothing else");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));
        Screen tiny(40, 10);
        render(s, tiny);
        const std::string text = tiny.asText();
        check(text.find("80") != std::string::npos && text.find("24") != std::string::npos,
              "it names the size it needs");
        check(text.find("Process") == std::string::npos,
              "and draws NO grid -- a layout that does not fit produces garbage "
              "that looks like a bug in the model");
    }
    section("Rendering: the detail pane shows the row and its diagnostics");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.8");   // deliberately broken
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Process");
        s.setMode(Mode::Detail);

        Screen screen(100, 30);
        render(s, screen);
        const std::string text = screen.asText();

        check(text.find("Capacity") != std::string::npos,
              "the detail pane shows every column, including empty ones");
        check(text.find("Renege After") != std::string::npos,
              "which is the whole point: an 11-column table is unreadable across");
        check(text.find("FIFO") != std::string::npos,
              "an enum column shows the spellings it allows");
        check(text.find("-> Resource") != std::string::npos,
              "and a reference column shows what it must name");
        check(text.find("expected") != std::string::npos,
              "a broken cell shows its diagnostic IN the pane");

        check(s.diagnosticFor("Service") != nullptr,
              "and the state can find that diagnostic by column");
        check(s.diagnosticFor("Name") == nullptr, "with none for a healthy cell");
    }

    section("The edit buffer");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.8)");
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Process");
        s.setColumn(0);                       // Name

        s.beginEdit();
        check(s.mode() == Mode::Editing, "beginEdit enters Editing");
        check(s.editBuffer() == "Serve", "seeded with what is there");
        s.backspaceEdit();
        s.typeEdit('r');
        check(s.editBuffer() == "Servr", "typing edits the buffer, not the document");
        check(s.document().cell("Process", 0, "Name") == "Serve",
              "the document is untouched until commit");

        s.cancelEdit();
        check(s.mode() == Mode::Detail, "Escape leaves Editing");
        check(s.document().cell("Process", 0, "Name") == "Serve", "and abandons the edit");
        check(!s.dirty(), "leaving nothing dirty");

        s.beginEdit();
        s.typeEdit('!');
        s.commitEdit();
        check(s.document().cell("Process", 0, "Name") == "Serve!", "commit writes it");
        check(s.dirty(), "and marks the document dirty");
    }
    section("Navigation");
    {
        ModelDocument d;
        for (int i = 0; i < 3; ++i) {
            d.addRow("Process");
            d.setCell("Process", static_cast<std::size_t>(i), "Name",
                      "P" + std::to_string(i));
            d.setCell("Process", static_cast<std::size_t>(i), "Service", "1");
        }
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Process");

        check(s.row() == 0, "starts on the first row");
        handleKey(s, Key::special(KeyKind::Down));
        check(s.row() == 1, "Down moves down");
        handleKey(s, Key::special(KeyKind::Up));
        handleKey(s, Key::special(KeyKind::Up));
        check(s.row() == 0, "Up CLAMPS at the top rather than wrapping");
        handleKey(s, Key::special(KeyKind::End));
        check(s.row() == 2, "End goes to the last row");
        handleKey(s, Key::special(KeyKind::Down));
        check(s.row() == 2, "and Down clamps at the bottom");

        const std::string first = s.type();
        handleKey(s, Key::special(KeyKind::Tab));
        check(s.type() != first, "Tab moves to the next module type");
        handleKey(s, Key::special(KeyKind::BackTab));
        check(s.type() == first, "and BackTab comes back");

        s.setRow(2);
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.mode() == Mode::Detail, "Enter opens the detail pane");
        handleKey(s, Key::special(KeyKind::Down));
        check(s.column() == 1, "where Down moves between FIELDS, not rows");
        check(s.row() == 2, "leaving the row where it was");
        handleKey(s, Key::special(KeyKind::Escape));
        check(s.mode() == Mode::Grid, "Escape returns to the grid");

        check(handleKey(s, Key::character('q')) == false,
              "q on a CLEAN document quits at once");
    }

    section("Quitting with unsaved work asks first");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "1");
        TuiState s = TuiState::fromDocument(d, "tui_confirm.des");
        s.setType("Process");
        s.setCell("Name", "Edited");
        check(s.dirty(), "there is unsaved work");

        check(handleKey(s, Key::character('q')) == true,
              "q does NOT quit while the document is dirty");
        check(s.mode() == Mode::Confirm, "it asks instead");

        check(handleKey(s, Key::character('c')) == true, "c cancels");
        check(s.mode() == Mode::Grid, "and returns to the grid");
        check(s.dirty(), "with the work still unsaved");

        handleKey(s, Key::character('q'));
        check(handleKey(s, Key::character('s')) == false, "s saves and quits");
        check(!s.dirty(), "having actually saved");

        TuiState t = TuiState::fromDocument(d, "tui_confirm.des");
        t.setType("Process");
        t.setCell("Name", "Discarded");
        handleKey(t, Key::character('q'));
        check(handleKey(t, Key::character('d')) == false, "d discards and quits");
    }

    section("Editing swallows every key, including the control ones");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "1");
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Process");
        s.setMode(Mode::Detail);
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.mode() == Mode::Editing, "editing a cell");

        // ^S typed into a Service field must not save a half-finished
        // expression. A person editing text expects the text to receive their
        // keys.
        handleKey(s, Key::control('s'));
        check(s.mode() == Mode::Editing, "^S does not escape the editor");
        check(s.status().find("saved") == std::string::npos, "and does not save");
    }
    section("A scripted session round-trips the file");
    {
        // THE CLAIM THIS VERSION RESTS ON. v11's byte-identical round-trip is
        // the property the document layer was built for, and this is the first
        // time it is exercised through the surface a person actually uses. If
        // the UI marks rows edited that nobody edited, nothing else catches it.
        const std::string src = modelPath("teller.des");
        std::ifstream in(src, std::ios::binary);
        const std::string original((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
        check(original.size() > 100, "the source file is substantial");

        // 1. Open, wander around, save. Nothing was edited, so nothing changes.
        {
            TuiState s = TuiState::open(src);
            handleKey(s, Key::special(KeyKind::Tab));
            handleKey(s, Key::special(KeyKind::Tab));
            handleKey(s, Key::special(KeyKind::Down));
            handleKey(s, Key::special(KeyKind::Enter));
            handleKey(s, Key::special(KeyKind::Down));
            handleKey(s, Key::special(KeyKind::Escape));
            check(!s.dirty(), "moving about does not dirty the document");
            check(s.save("tui_session.des"), "and it saves");
        }
        std::ifstream a("tui_session.des", std::ios::binary);
        const std::string wandered((std::istreambuf_iterator<char>(a)),
                                   std::istreambuf_iterator<char>());
        check(wandered == original,
              "opening, navigating and saving gives back the SAME BYTES");

        // 2. Edit one cell. Every other row must still come back verbatim,
        //    comments and blank lines included.
        {
            TuiState s = TuiState::open(src);
            s.setType("Process");
            s.setColumn(0);
            s.beginEdit();
            for (int i = 0; i < 40; ++i) s.backspaceEdit();
            for (char c : std::string("Teller")) s.typeEdit(c);
            s.commitEdit();
            check(s.dirty(), "the edit dirties the document");
            check(s.save("tui_session.des"), "and it saves");
        }
        std::ifstream b("tui_session.des", std::ios::binary);
        const std::string edited((std::istreambuf_iterator<char>(b)),
                                 std::istreambuf_iterator<char>());
        check(edited != original, "an edited file differs");
        check(edited.find("Teller") != std::string::npos, "and carries the new value");
        check(edited.find("# A single teller.") != std::string::npos,
              "while the COMMENTS survive, which is what the source lines are for");
        check(edited.find("[Dispose]") != std::string::npos,
              "and so does every table nobody touched");

        // 3. The edited file re-reads to a document that says what was typed.
        ReadResult back = readDocumentFile("tui_session.des");
        check(!hasErrors(back.diagnostics), "the saved file reads cleanly");
        bool found = false;
        for (std::size_t r = 0; r < back.document.rowCount("Process"); ++r)
            if (back.document.cell("Process", r, "Name") == "Teller") found = true;
        check(found, "and round-trips the edit back");
    }
}
