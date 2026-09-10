// ============================================================================
// tests/tui_tests.cpp  --  v13: the terminal UI, tested without a terminal
// ============================================================================
// Every test here builds a state, feeds keys, and asserts on a Screen AS TEXT.
// That is possible because the screen is a value: nothing in the UI below
// tui/ touches a terminal, so all of it runs under the sanitisers with
// everything else.
#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
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
    section("Rendering: what only looking at it found");
    {
        // Four bugs that every assertion above passed over, because a find()
        // on the screen text cannot tell you the thing you were looking for is
        // off the edge, unreachable, or unreadable. They are pinned here.
        TuiState s = TuiState::open(modelPath("teller.des"));
        s.setType("Process");

        {
            // 1. Sixteen module types do not fit across eighty columns. The
            //    tab bar used to stop at the edge, so the tab you were editing
            //    was the one you could not see.
            Screen screen(80, 24);
            render(s, screen);
            check(screen.line(0).find("[Process ") != std::string::npos,
                  "the SELECTED tab is drawn even when the bar has to scroll");
            check(screen.line(0).find("teller.des") != std::string::npos,
                  "and the file name still fits beside it");
        }
        {
            // 2. A Process has eleven columns and each may add a diagnostic
            //    line. The detail pane has to scroll or the field being edited
            //    sits below the fold.
            s.setMode(Mode::Detail);
            const std::vector<std::string> columns = s.columnsHere();
            s.setColumn(columns.size() - 1);          // the last one: Next
            Screen screen(80, 24);
            render(s, screen);
            check(screen.asText().find(columns.back()) != std::string::npos,
                  "the LAST field is visible when it is the one selected");
            s.setColumn(0);
            Screen top(80, 24);
            render(s, top);
            check(top.asText().find("Name") != std::string::npos,
                  "and the first is visible when that one is");
        }
        {
            // 3. Unbracketed, the allowed spellings ran straight on from the
            //    value: "FIFO FIFO LIFO PRIORITY ...", which reads as though
            //    the cell held all of them.
            s.setMode(Mode::Detail);
            s.setColumn(4);                           // Discipline
            Screen screen(80, 24);
            render(s, screen);
            check(screen.asText().find("(FIFO LIFO") != std::string::npos,
                  "the allowed spellings are parenthesised, not run on");
        }
        {
            // 4. Text used to overflow the frame, leaving the box open.
            s.setMode(Mode::Grid);
            Screen screen(80, 24);
            render(s, screen);
            for (int y = 2; y < 8; ++y) {
                const std::string ln = screen.line(y);
                if (ln.empty()) continue;
                check(static_cast<int>(ln.size()) <= 80,
                      "no line runs past the width of the screen");
                if (ln[0] == '|')
                    check(ln.size() < 80 || ln.back() == '|',
                          "and a boxed line still closes its border");
            }
        }
    }
    section("Running a model from the UI");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));
        s.startRun();
        check(s.mode() == Mode::Running, "^R enters Running");
        check(s.running() != nullptr, "with a controller");

        int guard = 0;
        while (s.running() != nullptr &&
               (s.running()->state() == RunState::Ready ||
                s.running()->state() == RunState::Running) &&
               guard++ < 10000)
            s.advanceRun();
        check(s.running() != nullptr && s.running()->state() == RunState::Finished,
              "and it runs to the end");
        check(!s.runReport().empty(), "producing a report");

        Screen screen(100, 30);
        render(s, screen);
        check(screen.asText().find("simulation report") != std::string::npos,
              "which the UI shows");
        check(screen.asText().find("Finished") != std::string::npos,
              "and says the run FINISHED");
        check(screen.asText().find("cannot say") == std::string::npos,
              "not that the rule could not say -- a TimeLimit always can, and "
              "one message meaning two things stops being believed");

        s.stopRun();
        check(s.mode() == Mode::Grid, "Escape returns to the grid");
        check(s.running() == nullptr, "and lets the run go");

        {
            // A document that will not compile must say so rather than
            // entering a run mode with nothing running.
            ModelDocument d;
            d.addRow("Process");
            d.setCell("Process", 0, "Name", "Lonely");
            d.setCell("Process", 0, "Service", "EXPO(1");
            TuiState bad = TuiState::fromDocument(d, "bad.des");
            bad.startRun();
            check(bad.mode() != Mode::Running, "a broken document does not start a run");
            check(!bad.status().empty(), "and says why");
        }
    }

    section("A run with no knowable end draws no bar");
    {
        // v12's fourth-time rule, reaching the surface it was written for. A
        // bar sitting at zero until it jumps to full is a lie the reader
        // cannot detect, so when the rule cannot say, the UI does not draw one.
        ModelDocument d;
        d.addRow("Run");
        d.setCell("Run", 0, "Name", "Setup");
        d.setCell("Run", 0, "Stop When Drained", "true");
        d.addRow("Create");
        d.setCell("Create", 0, "Name", "In");
        d.setCell("Create", 0, "Interarrival", "EXPO(1.0)");
        d.setCell("Create", 0, "Max Arrivals", "20");
        d.setCell("Create", 0, "Next", "Serve");
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.5)");
        d.setCell("Process", 0, "Next", "Out");
        d.addRow("Dispose");
        d.setCell("Dispose", 0, "Name", "Out");

        TuiState s = TuiState::fromDocument(d, "drained.des");
        s.startRun();
        check(s.mode() == Mode::Running, "it starts");
        check(s.running() != nullptr && !s.running()->progress().fraction.has_value(),
              "and whenDrained cannot say how far through it is");

        Screen screen(100, 30);
        render(s, screen);
        const std::string text = screen.asText();
        check(text.find("cannot say") != std::string::npos,
              "so the UI says so in words");
        check(text.find("[####") == std::string::npos &&
              text.find("[    ") == std::string::npos,
              "and draws NO progress bar, empty or otherwise");
    }
    section("The screen explains itself");
    {
        // An empty table is where a person who has never seen this is stuck.
        TuiState s = TuiState::fromDocument(ModelDocument{}, "new.des");
        s.setType("Create");
        Screen empty(80, 24);
        render(s, empty);
        const std::string text = empty.asText();
        check(text.find("Where entities enter the model") != std::string::npos,
              "an empty table says what the module is for");
        check(text.find("Arena's Create module") != std::string::npos,
              "and what Arena calls it");
        check(text.find("^N adds one") != std::string::npos,
              "and which key adds a row, which is the thing they are stuck on");

        // Wrapped, and inside the frame. A description is two or three lines,
        // and a line drawn through the border is how a box stops looking like
        // a box.
        for (int y = 0; y < empty.height(); ++y) {
            const std::string ln = empty.line(y);
            if (!ln.empty() && ln[0] == '|')
                check(ln.size() < 80 || ln.back() == '|',
                      "every boxed line still closes its border");
        }

        // And the help for the field you are ON, not for all of them.
        TuiState t = TuiState::open(modelPath("teller.des"));
        t.setType("Process");
        t.setMode(Mode::Detail);
        const std::vector<std::string> columns = t.columnsHere();
        std::size_t balkAt = 0;
        for (std::size_t i = 0; i < columns.size(); ++i)
            if (columns[i] == "Balk At") balkAt = i;
        t.setColumn(balkAt);
        Screen field(80, 26);
        render(t, field);
        check(field.asText().find("Refuse to join a queue") != std::string::npos,
              "the selected field shows its help");

        t.setColumn(0);
        Screen other(80, 26);
        render(t, other);
        check(other.asText().find("Refuse to join a queue") == std::string::npos,
              "and only the selected one does -- prose for every field would "
              "bury the values it is meant to explain");

        // The LAST field's help is the one that used to vanish: help is drawn
        // under the field, and a cursor resting on the bottom visible row had
        // nowhere to put it. The pane scrolls early to keep room.
        t.setColumn(columns.size() - 1);
        Screen last(80, 24);
        render(t, last);
        const Column* lastCol = t.schemaHere()->column(columns.back());
        check(lastCol != nullptr && !lastCol->help.empty(), "the last column has help");
        check(last.asText().find(lastCol->help.substr(0, 20)) != std::string::npos,
              "and the LAST field shows it too, rather than being the one "
              "field whose help never fits");

        // Wrapped, not cut off. Before wrapping, the help ended mid-word at
        // the border -- "Arena draws a line, thi" -- which reads as a
        // rendering fault rather than as help. The tail arriving proves the
        // whole sentence did.
        const std::size_t lastSpace = lastCol->help.rfind(' ');
        const std::string tail = lastCol->help.substr(lastSpace + 1);
        check(tail.size() > 2 &&
              last.asText().find(tail) != std::string::npos,
              "and it wraps rather than stopping mid-sentence at the border");
    }
    section("A model built from scratch runs");
    {
        // The whole point of the editor, and it ABORTED. A document built from
        // nothing has no [Run] row, so nothing set a stopping rule, and
        // initialise() asserts one exists -- pressing run on a new model killed
        // the program. In the terminal UI that is worse than a crash, because
        // abort() skips destructors and leaves the console in raw mode with no
        // cursor and an alternate screen buffer.
        TuiState s = TuiState::fromDocument(ModelDocument{}, "scratch.des");

        // Built the way a person builds it: a row at a time, through the keys.
        const auto addRowWith = [&](const std::string& type,
                                    const std::vector<std::pair<std::string,
                                                                std::string>>& cells) {
            s.setType(type);
            handleKey(s, Key::control('n'));
            for (const auto& kv : cells) s.setCell(kv.first, kv.second);
        };
        addRowWith("Create", {{"Name", "Arrivals"},
                              {"Interarrival", "EXPO(1.0)"},
                              {"Max Arrivals", "20"},
                              {"Next", "Serve"}});
        addRowWith("Process", {{"Name", "Serve"},
                               {"Service", "EXPO(0.5)"},
                               {"Next", "Out"}});
        addRowWith("Dispose", {{"Name", "Out"}});

        check(!hasErrors(s.diagnostics()), "the model compiles");
        check(s.document().rowCount("Run") == 0, "and it has NO [Run] row");

        s.startRun();
        check(s.mode() == Mode::Running, "^R starts it anyway");
        int guard = 0;
        while (s.running() != nullptr &&
               (s.running()->state() == RunState::Ready ||
                s.running()->state() == RunState::Running) &&
               guard++ < 10000)
            s.advanceRun();
        check(guard < 10000, "and it TERMINATES rather than running forever");
        check(s.running() != nullptr && s.running()->state() == RunState::Finished,
              "finishing cleanly");
        check(s.runReport().find("entities exited         : 20") != std::string::npos ||
              s.runReport().find("entities arrived") != std::string::npos,
              "with a report");
        check(s.runReport().find("until the model runs out of events") !=
                  std::string::npos,
              "that says WHAT stopped it, rather than printing AnyOf[]");
    }

    section("The blank page hands you a model, and the tabs say where things are");
    {
        TuiState blank = TuiState::fromDocument(ModelDocument{}, "new.des");
        check(blank.type() == "Create",
              "a blank document opens on Create, where a flow starts, rather "
              "than on whatever the registry publishes first");

        Screen page(80, 24);
        render(blank, page);
        check(page.asText().find("^T fills this in") != std::string::npos,
              "and the blank page offers a model to take apart");

        handleKey(blank, Key::control('t'));
        check(!hasErrors(blank.diagnostics()), "^T leaves a model that COMPILES");
        check(blank.dirty(), "and marks it unsaved, since nothing is on disk yet");
        check(blank.document().rowCount("Create") == 1 &&
              blank.document().rowCount("Process") == 1 &&
              blank.document().rowCount("Dispose") == 1 &&
              blank.document().rowCount("Run") == 1,
              "arrivals, a server, an exit and a Run row");
        check(!blank.document().preamble().empty(),
              "with comments in the file, which v11 preserves through an edit");

        // It has to RUN, not merely compile. A starter model that needs a fix
        // before it works teaches the wrong first lesson.
        blank.startRun();
        int guard = 0;
        while (blank.running() != nullptr &&
               (blank.running()->state() == RunState::Ready ||
                blank.running()->state() == RunState::Running) &&
               guard++ < 200000)
            blank.advanceRun();
        check(blank.running() != nullptr &&
              blank.running()->state() == RunState::Finished,
              "and ^R runs it to completion straight away");
        blank.stopRun();

        // Once there are rows, ^T must not be able to discard them.
        const std::size_t before = blank.document().rowCount("Process");
        handleKey(blank, Key::control('t'));
        check(blank.document().rowCount("Process") == before,
              "^T refuses on a model that already has rows");
        check(blank.status().find("already") != std::string::npos,
              "and says why rather than doing nothing silently");

        // Row counts on the tabs, and the empty ones dimmed.
        Screen tabs(80, 24);
        render(blank, tabs);
        check(tabs.line(0).find("Create 1") != std::string::npos,
              "a tab carries its row count");
        check(tabs.line(0).find("Variable 0") == std::string::npos,
              "and a type with no rows carries no number at all");

        TuiState t = TuiState::open(modelPath("teller.des"));
        check(t.type() == "Create",
              "opening a real model lands on its flowchart, not on the first "
              "empty data table the registry happens to publish");
    }

    section("? lists the keys");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));
        Screen footer(80, 24);
        render(s, footer);
        check(footer.line(23).find("? keys") != std::string::npos,
              "the footer advertises it, since it is the key that makes the "
              "other dozen findable");

        handleKey(s, Key::character('?'));
        check(s.mode() == Mode::Help, "? opens the key map");
        Screen map(80, 24);
        render(s, map);
        const std::string text = map.asText();
        check(text.find("^T") != std::string::npos &&
              text.find("^R") != std::string::npos &&
              text.find("^Z") != std::string::npos,
              "which lists the control keys");
        check(text.find("CONNECTION") != std::string::npos,
              "and says the one thing an Arena user will not guess: a "
              "connection is a name typed into a Next cell");

        handleKey(s, Key::character('x'));
        check(s.mode() == Mode::Grid,
              "any key closes it -- a key map you have to work out how to "
              "leave has undone its own job");

        // It opens from the detail pane too. "What are the keys" is not a
        // question that waits until you are back at the top.
        s.setMode(Mode::Detail);
        handleKey(s, Key::character('?'));
        check(s.mode() == Mode::Help, "and it opens from the detail pane");
        handleKey(s, Key::control('s'));
        check(s.mode() == Mode::Grid, "a control key closes it as well");
    }

    section("^F shows the wiring, including the wiring that is wrong");
    {
        TuiState t = TuiState::open(modelPath("teller.des"));
        handleKey(t, Key::control('f'));
        check(t.mode() == Mode::Flow, "^F opens the flow");

        Screen flow(80, 24);
        render(t, flow);
        const std::string text = flow.asText();
        check(text.find("Arrivals") != std::string::npos &&
              text.find("Next -> Serve") != std::string::npos,
              "which reads the exits out of the cells");
        check(text.find("(leaves the system)") != std::string::npos,
              "and spells out an empty exit, which is a statement rather than "
              "an omission");

        // Enter GOES THERE. A picture you cannot navigate from is a second
        // place to look rather than a way of getting around.
        handleKey(t, Key::special(KeyKind::Down));
        handleKey(t, Key::special(KeyKind::Enter));
        check(t.type() == "Process" && t.mode() == Mode::Detail,
              "Enter opens the block under the marker");

        // ^F lands on the block you were already on, so it answers "where am
        // I in this" and not only "what is this".
        handleKey(t, Key::control('f'));
        check(t.flowCursor() == 1, "and reopening lands on where you were");

        {
            // A Decide's Next is its ELSE exit, taken when no branch matched.
            // Listed first it read as the default tried first, which is
            // backwards -- and this view exists to catch that kind of thing.
            TuiState d = TuiState::open(modelPath("decide.des"));
            handleKey(d, Key::control('f'));
            Screen screen(80, 24);
            render(d, screen);
            const std::string shown = screen.asText();
            const std::size_t branch = shown.find("size > 7 -> Big");
            const std::size_t other  = shown.find("else -> Small");
            check(branch != std::string::npos && other != std::string::npos,
                  "a Decide shows both ways out");
            check(branch < other,
                  "with the branch BEFORE the else, which is the order they "
                  "are actually tried in");
        }
        {
            // The view has to work on a document that does NOT compile: that
            // is exactly when somebody needs to see the wiring.
            ModelDocument m;
            m.addRow("Create");
            m.setCell("Create", 0, "Name", "Arrivals");
            m.setCell("Create", 0, "Interarrival", "1");
            m.setCell("Create", 0, "Next", "Typo");
            m.addRow("Process");
            m.setCell("Process", 0, "Name", "Serve");
            m.setCell("Process", 0, "Service", "1");
            TuiState b = TuiState::fromDocument(m, "broken.des");
            check(hasErrors(b.diagnostics()), "the document does not compile");
            handleKey(b, Key::control('f'));
            check(b.mode() == Mode::Flow, "and ^F opens anyway");
            Screen screen(80, 24);
            render(b, screen);
            const std::string shown = screen.asText();
            check(shown.find("names a block that does not exist") != std::string::npos,
                  "naming the dangling exit");
            check(shown.find("Serve: nothing arrives here") != std::string::npos,
                  "and the block nothing reaches, which nothing else says");
        }
        {
            TuiState empty = TuiState::fromDocument(ModelDocument{}, "new.des");
            handleKey(empty, Key::control('f'));
            check(empty.mode() != Mode::Flow,
                  "^F on an empty document opens nothing");
            check(empty.status().find("^T") != std::string::npos,
                  "and points at the key that would give it something to show");
        }
    }

    section("Pick lists offer what the compiler accepts");
    {
        // ONE FUNCTION, both jobs. The list a person chooses from and the list
        // the reference pass checks against are the same call, so a menu
        // cannot offer a name that is then rejected.
        ModelDocument d;
        d.addRow("Create");
        d.setCell("Create", 0, "Name", "Arrivals");
        d.setCell("Create", 0, "Interarrival", "EXPO(1.0)");
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.5)");
        d.addRow("Dispose");
        d.setCell("Dispose", 0, "Name", "Out");
        d.addRow("Resource");
        d.setCell("Resource", 0, "Name", "Teller");
        d.setCell("Resource", 0, "Capacity", "1");

        const std::vector<std::string> blocks = referenceCandidates(d, "Block");
        check(blocks.size() == 3, "every flowchart block is a candidate");
        check(std::find(blocks.begin(), blocks.end(), "Arrivals") != blocks.end() &&
              std::find(blocks.begin(), blocks.end(), "Serve") != blocks.end() &&
              std::find(blocks.begin(), blocks.end(), "Out") != blocks.end(),
              "across all three module types");
        check(std::find(blocks.begin(), blocks.end(), "Teller") == blocks.end(),
              "and a Resource is NOT a block");
        check(referenceCandidates(d, "Resource") == std::vector<std::string>{"Teller"},
              "a typed reference offers only that type");

        // The property, checked rather than asserted: everything the list
        // offers survives compilation, and something it does not offer fails.
        for (const std::string& b : blocks) {
            ModelDocument t = d;
            t.setCell("Create", 0, "Next", b);
            const CompileResult r = compile(t);
            bool complained = false;
            for (const Diagnostic& x : r.diagnostics)
                if (x.cell && x.cell->column == "Next") complained = true;
            check(!complained, "'" + b + "' is accepted, having been offered");
        }
        {
            ModelDocument t = d;
            t.setCell("Create", 0, "Next", "Nowhere");
            bool complained = false;
            for (const Diagnostic& x : compile(t).diagnostics)
                if (x.cell && x.cell->column == "Next") complained = true;
            check(complained, "and a name it did NOT offer is rejected");
        }

        // Duplicates collapse. A list showing 'Serve' twice invites choosing
        // the second one, which is not a different thing.
        {
            ModelDocument t = d;
            t.addRow("Process");
            t.setCell("Process", 1, "Name", "Serve");
            check(referenceCandidates(t, "Block").size() == 3,
                  "a duplicated name appears once");
        }
    }

    section("Enter opens a list, and Escape lets you type instead");
    {
        ModelDocument d;
        d.addRow("Create");
        d.setCell("Create", 0, "Name", "Arrivals");
        d.setCell("Create", 0, "Interarrival", "EXPO(1.0)");
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.5)");
        d.setCell("Process", 0, "Discipline", "LIFO");

        const auto columnOf = [](const TuiState& s, const std::string& id) {
            const std::vector<std::string> cs = s.columnsHere();
            for (std::size_t i = 0; i < cs.size(); ++i)
                if (cs[i] == id) return i;
            return std::size_t{0};
        };

        {   // An Enum cell offers its spellings, opened on the current one.
            TuiState s = TuiState::fromDocument(d, "pick.des");
            s.setType("Process");
            s.setMode(Mode::Detail);
            s.setColumn(columnOf(s, "Discipline"));
            handleKey(s, Key::special(KeyKind::Enter));
            check(s.mode() == Mode::Picking, "Enter on an Enum opens the list");
            check(s.choices().size() == 6, "with every spelling on it");
            check(s.choices()[s.choice()] == "LIFO",
                  "opened ON the current value, so Enter alone changes nothing");
            handleKey(s, Key::special(KeyKind::Up));
            check(s.choices()[s.choice()] == "FIFO", "Up moves");
            handleKey(s, Key::special(KeyKind::Up));
            check(s.choices()[s.choice()] == "FIFO",
                  "and CLAMPS rather than wrapping, like everything else");
            handleKey(s, Key::special(KeyKind::Enter));
            check(s.mode() == Mode::Detail, "Enter accepts and closes");
            check(s.document().cell("Process", 0, "Discipline") == "FIFO",
                  "writing the chosen value");
            check(s.dirty(), "and the document is dirty");
        }
        {   // A Reference cell offers the blocks that exist, plus 'no exit'.
            TuiState s = TuiState::fromDocument(d, "pick.des");
            s.setType("Create");
            s.setMode(Mode::Detail);
            s.setColumn(columnOf(s, "Next"));
            handleKey(s, Key::special(KeyKind::Enter));
            check(s.mode() == Mode::Picking, "Enter on a Reference opens the list");
            check(s.choices().size() == 3,
                  "the two blocks, and an empty one for 'leaves the system'");
            check(s.choices().front().empty(),
                  "the empty choice comes FIRST, so an unwired exit opens at "
                  "the top of the list rather than scrolled past it");
            check(s.choice() == 0,
                  "an unwired exit opens on 'none', which is what it holds");
            handleKey(s, Key::special(KeyKind::Down));
            handleKey(s, Key::special(KeyKind::Down));
            handleKey(s, Key::special(KeyKind::Enter));
            check(s.document().cell("Create", 0, "Next") == "Serve",
                  "and choosing one wires the exit");

            // Clearing it again has to be reachable from the same list that
            // wired it, or the only way back is to delete text you cannot see.
            handleKey(s, Key::special(KeyKind::Enter));
            handleKey(s, Key::special(KeyKind::PageUp));
            handleKey(s, Key::special(KeyKind::Enter));
            check(s.document().cell("Create", 0, "Next").empty(),
                  "and 'none' unwires it");
        }
        {   // THE CASE THAT MUST NOT BREAK. 'Out' does not exist yet, so it is
            // not on the list -- and naming it anyway has to stay easy.
            TuiState s = TuiState::fromDocument(d, "pick.des");
            s.setType("Process");
            s.setMode(Mode::Detail);
            s.setColumn(columnOf(s, "Next"));
            handleKey(s, Key::special(KeyKind::Enter));
            check(s.mode() == Mode::Picking, "the list opens");
            check(std::find(s.choices().begin(), s.choices().end(), "Out") ==
                      s.choices().end(),
                  "and 'Out' is NOT on it, because no such block exists");
            handleKey(s, Key::special(KeyKind::Escape));
            check(s.mode() == Mode::Editing,
                  "Escape drops into the editor rather than cancelling");
            for (char c : std::string("Out")) handleKey(s, Key::character(c));
            handleKey(s, Key::special(KeyKind::Enter));
            check(s.document().cell("Process", 0, "Next") == "Out",
                  "so a block can be named BEFORE it exists");

            s.setType("Dispose");
            handleKey(s, Key::control('n'));
            s.setCell("Name", "Out");
            check(!hasErrors(s.diagnostics()),
                  "and adding it afterwards resolves the reference");
        }
        {   // A second Escape abandons: the cell is left as it was.
            TuiState s = TuiState::fromDocument(d, "pick.des");
            s.setType("Process");
            s.setMode(Mode::Detail);
            s.setColumn(columnOf(s, "Discipline"));
            handleKey(s, Key::special(KeyKind::Enter));
            handleKey(s, Key::special(KeyKind::Escape));
            handleKey(s, Key::special(KeyKind::Escape));
            check(s.mode() == Mode::Detail, "a second Escape leaves the editor");
            check(s.document().cell("Process", 0, "Discipline") == "LIFO",
                  "with the cell untouched");
            check(!s.dirty(), "and nothing marked dirty");
        }
        {   // Nothing to point at yet: no empty menu, straight to typing. A row
            // whose Name is still blank declares no block, which is the state
            // a document is in for as long as it takes to type the first one.
            ModelDocument fresh;
            fresh.addRow("Create");
            TuiState s = TuiState::fromDocument(fresh, "fresh.des");
            s.setType("Create");
            s.setMode(Mode::Detail);
            s.setColumn(columnOf(s, "Next"));
            handleKey(s, Key::special(KeyKind::Enter));
            check(s.mode() == Mode::Editing,
                  "with nothing declared, Enter goes straight to the editor");
            check(s.status().find("type the name") != std::string::npos,
                  "and says why rather than flashing an empty box");
        }
        {   // The row must not move out from under an open list.
            TuiState s = TuiState::fromDocument(d, "pick.des");
            s.setType("Process");
            s.setMode(Mode::Detail);
            s.setColumn(columnOf(s, "Discipline"));
            handleKey(s, Key::special(KeyKind::Enter));
            const std::size_t before = s.rowCountHere();
            handleKey(s, Key::control('n'));
            check(s.rowCountHere() == before, "^N is swallowed while picking");
            check(s.mode() == Mode::Picking, "and the list stays open");
        }
        {   // Read-only refuses, and does not fall through to the editor.
            TuiState s = TuiState::fromDocument(d, "pick.des");
            s.setType("Queue");
            if (s.rowCountHere() > 0) {
                s.setMode(Mode::Detail);
                s.setColumn(columnOf(s, "Discipline"));
                handleKey(s, Key::special(KeyKind::Enter));
                check(s.mode() == Mode::Detail,
                      "a read-only table opens neither list nor editor");
            }
        }
    }

    section("The list is on screen, and says what Escape does");
    {
        ModelDocument d;
        d.addRow("Create");
        d.setCell("Create", 0, "Name", "Arrivals");
        d.setCell("Create", 0, "Interarrival", "EXPO(1.0)");
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.5)");

        TuiState s = TuiState::fromDocument(d, "pick.des");
        s.setType("Create");
        s.setMode(Mode::Detail);
        const std::vector<std::string> cs = s.columnsHere();
        for (std::size_t i = 0; i < cs.size(); ++i)
            if (cs[i] == "Next") s.setColumn(i);

        Screen hint(80, 24);
        render(s, hint);
        check(hint.asText().find("choose from a list") != std::string::npos,
              "the footer says Enter opens a list before you press it");

        handleKey(s, Key::special(KeyKind::Enter));
        Screen screen(80, 24);
        render(s, screen);
        const std::string text = screen.asText();
        check(text.find("Arrivals") != std::string::npos &&
              text.find("Serve") != std::string::npos,
              "the choices are drawn");
        check(text.find("(none)") != std::string::npos,
              "the empty choice has a VISIBLE spelling, not a blank line");
        check(text.find("Esc type it instead") != std::string::npos,
              "and the footer says what Escape does, because that is the way "
              "out for the person the list cannot help");
        check(text.find("[Create 1]") != std::string::npos,
              "the grid stays above it: a list of names means nothing without "
              "the row it is about");
        for (int y = 1; y < 22; ++y) {
            const std::string ln = screen.line(y);
            if (!ln.empty() && ln[0] == '|')
                check(ln.size() < 80 || ln.back() == '|',
                      "every boxed line still closes its border");
        }
    }

}
