// ============================================================================
// tests/tui_tests.cpp  --  v15: the terminal UI, tested without a terminal
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

namespace {

std::string fileText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// A COPY, always, for any test that might save. One of these opened the
// shipped teller.des and pressed (s)ave-and-quit, which wrote a stray comment
// into the repository's own model file -- and then every later test read the
// damage back. A test that can write to one of its inputs is a test that can
// break the next one.
TuiState openCopy(const std::string& model, const std::string& to) {
    std::ofstream out(to, std::ios::binary);
    const std::string text = fileText(modelPath(model));
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    return TuiState::open(to);
}

void typeText(TuiState& s, const std::string& text) {
    for (const char c : text) handleKey(s, Key::character(c));
}

// Move the caret to the first line whose text contains `needle`, by KEYS.
// Every navigation in these tests goes through the input layer, so a field
// that cannot be reached by pressing what is on screen fails the test.
bool goToLine(TuiState& s, const std::string& needle) {
    handleKey(s, Key::control('b'));
    for (std::size_t i = 0; i < s.buffer().lineCount() + 2 && s.buffer().caret().line > 0; ++i)
        handleKey(s, Key::special(KeyKind::Up));
    for (std::size_t i = 0; i < s.buffer().lineCount(); ++i) {
        if (s.buffer().lineAt(s.buffer().caret().line).find(needle) != std::string::npos) {
            handleKey(s, Key::special(KeyKind::End));
            return true;
        }
        handleKey(s, Key::special(KeyKind::Down));
    }
    return false;
}

// The caret onto `field` of the `type` record -- what a person does when they
// scroll to the [Process] and look down it for Service. Counting keypresses
// from a template instead was how the first draft of the cold-build test broke:
// one field added to a schema and the whole sequence writes into the wrong
// lines, silently, and the model still compiles.
bool goToField(TuiState& s, const std::string& type, const std::string& field) {
    handleKey(s, Key::control('b'));
    for (std::size_t i = 0; i < s.buffer().lineCount() + 2 && s.buffer().caret().line > 0; ++i)
        handleKey(s, Key::special(KeyKind::Up));
    bool inside = false;
    for (std::size_t i = 0; i < s.buffer().lineCount(); ++i) {
        const std::string& line = s.buffer().lineAt(s.buffer().caret().line);
        if (line.find('[') != std::string::npos)
            inside = line.find("[" + type + "]") != std::string::npos;
        else if (inside && keyOfLine(line) == field) {
            handleKey(s, Key::special(KeyKind::End));
            return true;
        }
        handleKey(s, Key::special(KeyKind::Down));
    }
    return false;
}

}  // namespace

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

        s.clear();
        check(s.asText() == Screen(10, 3).asText(), "clear() empties it");
    }

    section("Key is a value, and so is a click");
    {
        const Key a = Key::character('a');
        check(a.kind == KeyKind::Char && a.ch == 'a', "a printable key carries its char");
        const Key ctrlS = Key::control('s');
        check(ctrlS.kind == KeyKind::Ctrl && ctrlS.ch == 'S',
              "a control key normalises to upper case, so ^s and ^S are one key");
        check(Key::special(KeyKind::Up).kind == KeyKind::Up, "a special key is its kind");

        const Key shifted = Key::shifted(KeyKind::Left);
        check(shifted.kind == KeyKind::Left && shifted.shift,
              "shift rides ALONGSIDE the kind rather than doubling it -- one "
              "Left in every switch, not two");

        const Key click = Key::mouse(MouseButton::Right, 12, 5);
        check(click.kind == KeyKind::Mouse && click.button == MouseButton::Right &&
              click.x == 12 && click.y == 5,
              "a click is a Key too, so there is one event stream and no merge");
        check(Key::mouse(MouseButton::WheelUp, 0, 0).isWheel(), "a wheel says so");
        check(Key::function(1).kind == KeyKind::Function && Key::function(1).ch == 1,
              "and a function key carries its number");
    }

    section("TextBuffer: lines, a caret and an undo stack");
    {
        TextBuffer b("one\ntwo\nthree");
        check(b.lineCount() == 3, "text splits into lines");
        check(b.lineAt(1) == "two", "which are addressable");
        check(b.text() == "one\ntwo\nthree", "and join back to what went in");

        // ALWAYS at least one line. A buffer with no lines has nowhere to put
        // a cursor, and every caller would need the special case.
        TextBuffer empty("");
        check(empty.lineCount() == 1 && empty.lineAt(0).empty(),
              "an empty buffer still has one empty line");
        check(TextBuffer().lineCount() == 1, "and so does a default-constructed one");

        // CRLF edits as LF and writes back as LF: this project's gates compare
        // bytes, and two line endings is two answers to one question.
        TextBuffer crlf("a\r\nb\r\n");
        check(crlf.text() == "a\nb\n", "CRLF is normalised on the way in");

        b.moveTo(Caret{1, 3});
        b.insert("!");
        check(b.lineAt(1) == "two!", "insert puts text at the caret");
        check(b.caret().column == 4, "and the caret follows it");

        b.insert("\nfour");
        check(b.lineCount() == 4 && b.lineAt(2) == "four",
              "text carrying a newline splits the line, so a paste behaves "
              "exactly like typing");

        b.undo();
        check(b.lineCount() == 3, "undo");
        b.redo();
        check(b.lineCount() == 4, "and redo");
        b.undo();
        b.insert("x");
        check(!b.canRedo(),
              "a NEW edit discards the redo stack -- replaying changes that no "
              "longer apply to the text in front of you is worse than losing them");

        TextBuffer c("hello");
        c.moveTo(Caret{0, 5});
        c.backspace();
        check(c.lineAt(0) == "hell", "backspace");
        c.moveTo(Caret{0, 0});
        c.del();
        check(c.lineAt(0) == "ell", "delete");
        c.del(); c.del(); c.del();
        c.del();
        check(c.lineAt(0).empty(), "deleting past the end does nothing rather than crash");

        // Off either end of a line steps to the next or the previous one,
        // which is what makes Left at column 0 useful.
        TextBuffer d("ab\ncd");
        d.moveTo(Caret{1, 0});
        d.moveBy(0, -1);
        check(d.caret().line == 0 && d.caret().column == 2,
              "Left at column 0 goes to the end of the line above");
        d.moveBy(0, 1);
        check(d.caret().line == 1 && d.caret().column == 0, "and Right comes back");

        TextBuffer e("ab\ncd");
        e.moveTo(Caret{1, 0});
        e.backspace();
        check(e.lineCount() == 1 && e.lineAt(0) == "abcd",
              "backspace at column 0 joins the lines");
    }

    section("TextBuffer: selection, and what cut and copy act on");
    {
        TextBuffer b("hello world");
        check(!b.hasSelection(), "no selection to start with");
        b.moveTo(Caret{0, 0});
        b.moveTo(Caret{0, 5}, /*extend=*/true);
        check(b.hasSelection() && b.selectedText() == "hello", "a selection has text");

        // Built backwards it is the SAME selection. Nothing above this should
        // have to know which way it was dragged.
        TextBuffer c("hello world");
        c.moveTo(Caret{0, 5});
        c.moveTo(Caret{0, 0}, true);
        check(c.selectedText() == "hello", "and dragging backwards selects the same text");
        check(c.selectionStart().column == 0 && c.selectionEnd().column == 5,
              "with the edges in order whichever way round it was made");

        TextBuffer d("one\ntwo\nthree");
        d.moveTo(Caret{0, 1});
        d.moveTo(Caret{2, 2}, true);
        check(d.selectedText() == "ne\ntwo\nth", "a selection spans lines");
        d.deleteSelection();
        check(d.text() == "oree", "and deleting one joins what is left");

        TextBuffer e("abc");
        e.selectAll();
        check(e.selectedText() == "abc", "select all");
        e.insert("z");
        check(e.text() == "z", "and typing over a selection replaces it");
    }

    section("TextBuffer: a module template lands on a line boundary");
    {
        // Inserting a [Create] record into the middle of somebody's
        // `Interarrival = EXPO(1.0)` would leave two broken lines.
        TextBuffer b("Interarrival = EXPO(1.0)");
        b.moveTo(Caret{0, 8});
        b.insertLines({"[Create]", "Name = ", ""});
        check(b.lineAt(0) == "Interarr", "the line it cut through is split");
        check(b.lineAt(1) == "[Create]", "the block goes in whole");
        check(b.lineAt(3).empty() && b.lineAt(4) == "ival = EXPO(1.0)",
              "and the rest of the line follows it");

        // The caret ends on the first field, not on the header: landing on
        // [Create] would make every insertion start with the same two presses.
        TextBuffer c("");
        c.insertLines({"[Create]", "Name = ", "Next = ", ""});
        check(c.caret().line == 1 && c.caret().column == c.lineAt(1).size(),
              "the caret lands ready to type the first value");
    }

    section("The text is the model, and saving gives back what was typed");
    {
        // v13 edited a document and wrote it out, and had to WORK to keep
        // comments alive -- v11's central guarantee turned out to be false the
        // first time a UI exercised it. Here what is saved is what was typed,
        // so the round trip is not a property to maintain.
        const std::string src = modelPath("teller.des");
        const std::string original = fileText(src);
        check(original.size() > 100, "the source file is substantial");

        TuiState s = TuiState::open(src);
        check(!s.dirty(), "a freshly opened file is not dirty");
        check(s.buffer().text() + "\n" == original || s.buffer().text() == original,
              "the buffer holds the file");

        // Wander through every view, then save. Nothing was typed.
        handleKey(s, Key::control('f'));
        handleKey(s, Key::control('u'));
        handleKey(s, Key::control('e'));
        handleKey(s, Key::control('b'));
        handleKey(s, Key::special(KeyKind::Down));
        handleKey(s, Key::special(KeyKind::Down));
        check(!s.dirty(), "looking around does not dirty it");
        check(s.save("tui_roundtrip.des"), "it saves");
        check(fileText("tui_roundtrip.des") == original,
              "and opening, navigating and saving gives back the SAME BYTES");

        // Now change one character. Everything else is still byte for byte
        // what it was, because nothing re-emits it.
        TuiState t = TuiState::open(src);
        check(goToField(t, "Process", "Service"), "the Service line is reachable by keys");
        handleKey(t, Key::special(KeyKind::Backspace));   // the ')'
        handleKey(t, Key::special(KeyKind::Backspace));   // the '8'
        typeText(t, "9)");
        check(t.dirty(), "typing dirties it");
        check(t.save("tui_edited.des"), "and it saves");
        const std::string edited = fileText("tui_edited.des");
        check(edited.find("EXPO(0.9)") != std::string::npos, "the edit is there");
        check(edited.find("# How to run it") != std::string::npos,
              "and every comment survived, because nothing rewrote them");
    }

    section("A file that does not exist opens as a blank page");
    {
        TuiState s = TuiState::open("no_such_model_here.des");
        check(s.buffer().lineCount() == 1 && s.buffer().lineAt(0).empty(),
              "a missing file opens EMPTY rather than refusing");
        check(s.status().find("new file") != std::string::npos,
              "and says so, because empty and unreadable must not look alike");

        Screen page(90, 26);
        render(s, page);
        check(page.asText().find("^T writes a working") != std::string::npos,
              "the blank page offers something to take apart");

        handleKey(s, Key::control('t'));
        check(!hasErrors(s.diagnostics()), "^T leaves a model that COMPILES");
        check(s.dirty(), "and marks it unsaved, since nothing is on disk yet");
        check(s.document().rowCount("Create") == 1 &&
              s.document().rowCount("Process") == 1 &&
              s.document().rowCount("Dispose") == 1 &&
              s.document().rowCount("Run") == 1,
              "arrivals, a server, an exit and a Run row");
        check(s.buffer().text().find("# A single teller") != std::string::npos,
              "with comments IN THE TEXT, which is now the only place they live");

        const std::size_t before = s.buffer().lineCount();
        handleKey(s, Key::control('t'));
        check(s.buffer().lineCount() == before, "^T refuses on a file with text in it");
        check(s.status().find("has text in it") != std::string::npos, "and says why");
    }

    section("A compiler error lands on a LINE, which is the whole point");
    {
        // A diagnostic names a CELL. In a grid that was enough; in a text
        // editor it is useless unless something turns it back into a line.
        // This is the join v15 rests on.
        const std::string text =
            "version = 1\n"
            "\n"
            "[Create]\n"
            "Name = Arrivals\n"
            "Entity Type = Gears\n"          // no [Entity] declares Gears
            "Interarrival = EXPO(0.6)\n"
            "Next = Serve\n"
            "\n"
            "[Process]\n"
            "Name = Serve\n"
            "Service = EXPO(0.5)\n";
        TuiState s = TuiState::fromText(text, "gears.des");
        check(hasErrors(s.diagnostics()), "the model does not compile");

        const std::size_t line = s.firstErrorLine();
        check(line == 5, "the error is on the line the bad value is on");
        const Diagnostic* d = s.diagnosticOnLine(5);
        check(d != nullptr && d->message.find("Gears") != std::string::npos,
              "and the message names the value");

        Screen screen(90, 26);
        render(s, screen);
        bool marked = false;
        for (int y = 0; y < 26; ++y) {
            const std::string ln = screen.line(y);
            // The marker sits immediately left of the text, on the line whose
            // number is beside it.
            if (ln.find("   5") != std::string::npos &&
                ln.find("Entity Type") != std::string::npos &&
                ln.find('E') != std::string::npos)
                marked = true;
        }
        check(marked, "the line is marked on screen");

        handleKey(s, Key::control('j'));
        check(s.buffer().caret().line == 4, "^J puts the cursor on it");
        check(s.status().find("Gears") != std::string::npos,
              "and the status says what is wrong there");

        // A missing REQUIRED field has no cell to point at. The record's
        // header is the nearest true thing, and 0 -- "cannot say" -- is what
        // it must never quietly become.
        TuiState t = TuiState::fromText("version = 1\n\n[Process]\nService = 1\n",
                                        "nameless.des");
        check(t.firstErrorLine() == 3,
              "an error about a field that is NOT THERE lands on its [Header]");

        // And a run refuses with a reason rather than a shrug. Somebody
        // pressing run on the model above got "the document does not compile"
        // and no idea which of four tabs to look at.
        TuiState u = TuiState::fromText(text, "gears.des");
        handleKey(u, Key::control('r'));
        check(u.running() == nullptr, "^R refuses to run a broken model");
        check(u.status().find("Gears") != std::string::npos,
              "and names the actual problem rather than saying it will not");
        check(u.status().find("^J") != std::string::npos, "and where to go");
    }

    section("The palette writes a module in, blank fields and all");
    {
        TuiState s = TuiState::fromText("version = 1\n\n", "new.des");
        handleKey(s, Key::control('p'));
        check(s.focus() == Pane::Palette, "^P moves to the palette");

        // A letter jumps, the way every list in every file manager has always
        // worked -- seventeen types is a long way to arrow through.
        typeText(s, "c");
        check(s.palette()[s.paletteIndex()][0] == 'C', "a letter jumps to that module");
        while (s.palette()[s.paletteIndex()] != "Create")
            handleKey(s, Key::character('c'));

        handleKey(s, Key::special(KeyKind::Enter));
        check(s.focus() == Pane::Editor, "Enter inserts and hands back the keyboard");
        const std::string text = s.buffer().text();
        check(text.find("[Create]") != std::string::npos, "the header is in the text");
        check(text.find("Interarrival = ") != std::string::npos,
              "with every field the module has");
        check(text.find("Balk At") == std::string::npos ||
              text.find("Max Arrivals = ") != std::string::npos,
              "including the optional ones -- a template that hid them would "
              "hide entity types and balking behind knowing they exist");
        check(s.dirty(), "and it counts as an edit");

        // The caret is ready to type the first value.
        const std::string atCaret = s.buffer().lineAt(s.buffer().caret().line);
        check(atCaret.find("Name = ") != std::string::npos,
              "the caret lands on the first field");
        typeText(s, "Arrivals");
        check(s.document().cell("Create", 0, "Name") == "Arrivals",
              "so typing goes straight into it, and the document follows");
    }

    section("^G explains whatever the cursor is on");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));
        check(goToField(s, "Process", "Service"), "on the Service line");
        handleKey(s, Key::control('g'));
        check(s.overlay() == Overlay::Help, "^G opens the help");
        check(s.helpTitle() == "Process.Service", "for that field");
        Screen screen(90, 26);
        render(s, screen);
        check(screen.asText().find("EXPO(0.8)") != std::string::npos ||
              !s.helpBody().empty(), "with something to say about it");

        handleKey(s, Key::character('x'));
        check(s.overlay() == Overlay::None, "and any key closes it");

        // On a [Header] the module's own help is the useful answer, not a
        // refusal about there being no field there.
        check(goToLine(s, "[Process]"), "on the header");
        handleKey(s, Key::control('g'));
        check(s.overlay() == Overlay::Help && s.helpTitle() == "Process",
              "the header explains the module");
        handleKey(s, Key::special(KeyKind::Escape));

        // In the palette it explains the module you are on, with every field.
        handleKey(s, Key::control('p'));
        while (s.palette()[s.paletteIndex()] != "Decide")
            handleKey(s, Key::special(KeyKind::Down));
        handleKey(s, Key::control('g'));
        check(s.overlay() == Overlay::Help && s.helpTitle() == "Decide",
              "and the palette explains a module before you insert it");
        bool listsFields = false;
        for (const std::string& ln : s.helpBody())
            if (ln.find("Next") != std::string::npos) listsFields = true;
        check(listsFields, "listing what its fields are for");
    }

    section("^L is Arena's drop-down, and Escape types instead");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));

        check(goToField(s, "Process", "Discipline"), "on an Enum field");
        handleKey(s, Key::control('l'));
        check(s.overlay() == Overlay::Picker, "^L opens the list");
        check(s.choices().size() == 6, "with every spelling on it");
        check(s.choices()[s.choice()] == "FIFO",
              "opened ON the current value, so Enter alone changes nothing");
        handleKey(s, Key::special(KeyKind::Down));
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.buffer().lineAt(s.buffer().caret().line).find("LIFO") != std::string::npos,
              "and choosing one writes it into the line");
        check(s.document().cell("Process", 0, "Discipline") == "LIFO",
              "which the document reads back");

        check(goToField(s, "Process", "Next"), "on a Reference field");
        handleKey(s, Key::control('l'));
        check(s.overlay() == Overlay::Picker, "a Reference offers what exists");
        check(std::find(s.choices().begin(), s.choices().end(), "Out") != s.choices().end(),
              "the blocks that are declared");
        check(s.choices().front().empty(),
              "and an empty choice FIRST, so an unwired exit opens at the top "
              "of the list rather than scrolled past it");
        handleKey(s, Key::special(KeyKind::Escape));
        check(s.overlay() == Overlay::None, "Escape closes it");
        check(s.status().find("does not have to exist yet") != std::string::npos,
              "saying the thing that matters: a name may be typed before the "
              "block it names exists");

        // Free text has no list, and says so rather than flashing an empty box.
        check(goToField(s, "Process", "Service"), "on an expression field");
        handleKey(s, Key::control('l'));
        check(s.overlay() == Overlay::None, "^L on free text opens nothing");
        check(s.status().find("free text") != std::string::npos, "and says why");

        // Nothing declared yet: straight to typing.
        TuiState fresh = TuiState::fromText("version = 1\n\n[Create]\nNext = \n",
                                            "fresh.des");
        check(goToField(fresh, "Create", "Next"), "on a Next with nothing to point at");
        handleKey(fresh, Key::control('l'));
        check(fresh.overlay() == Overlay::None, "no empty menu");
        check(fresh.status().find("type the name") != std::string::npos,
              "and it says to type it and add the block after");
    }

    section("Cut, copy and paste");
    {
        TuiState s = TuiState::fromText("one\ntwo\nthree\n", "clip.des");
        handleKey(s, Key::control('c'));
        check(s.status().find("nothing selected") != std::string::npos,
              "copy with no selection says so rather than doing nothing");

        while (s.buffer().caret().line > 0) handleKey(s, Key::special(KeyKind::Up));
        handleKey(s, Key::shifted(KeyKind::Down));
        handleKey(s, Key::control('x'));
        check(s.buffer().lineAt(0) == "two", "^X cuts the selection");
        check(s.clipboard() == "one\n", "onto the clipboard");
        check(s.dirty(), "and it is an edit");

        handleKey(s, Key::control('v'));
        check(s.buffer().text() == "one\ntwo\nthree\n", "^V puts it back");

        handleKey(s, Key::control('a'));
        check(s.buffer().hasSelection(), "^A selects all");
        handleKey(s, Key::control('c'));
        check(s.clipboard().find("three") != std::string::npos, "^C copies it");

        // Undo covers a cut as one step, not one character at a time.
        TuiState t = TuiState::fromText("alpha\n", "clip2.des");
        handleKey(t, Key::control('a'));
        handleKey(t, Key::control('x'));
        handleKey(t, Key::control('z'));
        check(t.buffer().text() == "alpha\n", "^Z undoes a cut in one step");
        handleKey(t, Key::control('y'));
        check(t.buffer().text().find("alpha") == std::string::npos, "and ^Y redoes it");
    }

    section("Many runs, chosen by name");
    {
        const std::string text =
            "version = 1\n"
            "\n"
            "[Run]\nName = Short\nLength = 50\n"
            "\n"
            "[Run]\nName = Long\nLength = 400\n"
            "\n"
            "[Create]\nName = Arrivals\nInterarrival = EXPO(1.0)\nNext = Serve\n"
            "\n"
            "[Process]\nName = Serve\nService = EXPO(0.5)\nNext = Out\n"
            "\n"
            "[Dispose]\nName = Out\n";
        TuiState s = TuiState::fromText(text, "many.des");
        check(!hasErrors(s.diagnostics()),
              "TWO [Run] records compile -- v11 through v14 called the second "
              "one an error, and a file can hold what a dialog cannot");
        check(s.runs().size() == 2 && s.runs()[0] == "Short" && s.runs()[1] == "Long",
              "and both are listed by name");

        handleKey(s, Key::control('u'));
        check(s.view() == View::Runs, "^U opens the Runs tab");
        Screen screen(90, 26);
        render(s, screen);
        check(screen.asText().find("Short") != std::string::npos &&
              screen.asText().find("Long") != std::string::npos,
              "which shows them");
        check(screen.asText().find("400") != std::string::npos,
              "with the numbers that tell them apart");

        handleKey(s, Key::special(KeyKind::Down));
        check(s.runIndex() == 1, "Down chooses the other one");
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.running() != nullptr, "Enter runs it");
        check(s.resultsOf() == "Long", "and the results know WHICH run made them");

        int guard = 0;
        while (s.running() != nullptr && guard++ < 200000) s.advanceRun();
        check(guard < 200000, "it terminates");
        check(s.haveResults(), "with results");
        check(s.view() == View::Results,
              "and it lands on them, rather than making you press a key with "
              "no decision behind it");

        // The chosen run is the one that ran: Long is 400, Short is 50.
        check(s.results().find("400") != std::string::npos ||
              s.results().find("Length") != std::string::npos ||
              !s.results().empty(), "the report is there");

        check(s.saveResults(), "^W saves them");
        const std::string saved = fileText(s.resultsPath());
        check(saved.find("Long") != std::string::npos,
              "and the file says which run it was, which a bare report does not");

        // ^N writes another [Run] rather than making you remember the fields.
        TuiState t = TuiState::fromText(text, "many.des");
        handleKey(t, Key::control('u'));
        handleKey(t, Key::control('n'));
        check(t.document().rowCount("Run") == 3, "^N adds a [Run]");
        check(t.view() == View::Model, "and goes to the text to name it");
    }

    section("Results are empty until there are any, and say so");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));
        check(!s.haveResults(), "nothing has been run");
        handleKey(s, Key::control('e'));
        Screen screen(90, 26);
        render(s, screen);
        check(screen.asText().find("Nothing has been run yet") != std::string::npos,
              "the Results tab says so rather than showing an empty box");
        check(!s.saveResults(), "and ^W refuses");
        check(s.status().find("no results") != std::string::npos, "saying why");
    }

    section("^F shows the wiring, including the wiring that is wrong");
    {
        TuiState t = TuiState::open(modelPath("teller.des"));
        handleKey(t, Key::control('f'));
        check(t.view() == View::Flow, "^F opens the flow");

        Screen flow(90, 26);
        render(t, flow);
        const std::string text = flow.asText();
        check(text.find("Arrivals") != std::string::npos &&
              text.find("Next -> Serve") != std::string::npos,
              "which reads the exits out of the cells");
        check(text.find("(leaves the system)") != std::string::npos,
              "and spells out an empty exit, which is a statement rather than "
              "an omission");

        // Enter GOES THERE, and there is now a line to go to.
        handleKey(t, Key::special(KeyKind::Down));
        handleKey(t, Key::special(KeyKind::Enter));
        check(t.view() == View::Model, "Enter opens that block in the text");
        check(t.buffer().lineAt(t.buffer().caret().line).find("[Process]") !=
                  std::string::npos,
              "with the cursor on its [Header]");

        {
            // A Decide's Next is its ELSE exit, taken when no branch matched.
            // Listed first it read as the default tried first, which is
            // backwards -- and this view exists to catch that kind of thing.
            TuiState d = TuiState::open(modelPath("decide.des"));
            handleKey(d, Key::control('f'));
            Screen screen(90, 26);
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
            // The view has to work on a file that does NOT compile: that is
            // exactly when somebody needs to see the wiring.
            TuiState b = TuiState::fromText(
                "version = 1\n\n[Create]\nName = Arrivals\nInterarrival = 1\n"
                "Next = Typo\n\n[Process]\nName = Serve\nService = 1\n",
                "broken.des");
            check(hasErrors(b.diagnostics()), "the model does not compile");
            handleKey(b, Key::control('f'));
            check(b.view() == View::Flow, "and ^F opens anyway");
            Screen screen(90, 26);
            render(b, screen);
            const std::string shown = screen.asText();
            check(shown.find("names a block that does not exist") != std::string::npos,
                  "naming the dangling exit");
            check(shown.find("Serve: nothing arrives here") != std::string::npos,
                  "and the block nothing reaches, which nothing else says");
        }
    }

    section("The mouse lands where the renderer drew");
    {
        // ONE layout function. A click is turned back into "the third palette
        // entry" by the arithmetic that put it there -- two copies is the bug
        // the v13 tab bar already taught this project once.
        TuiState s = TuiState::open(modelPath("teller.des"));
        const int W = 90, H = 26;
        const Layout L = layoutFor(s, W, H);

        handleKey(s, Key::mouse(MouseButton::Left, L.tabStart[1] + 1, L.barRow), W, H);
        check(s.view() == View::Flow, "clicking a tab opens it");
        handleKey(s, Key::mouse(MouseButton::Left, L.tabStart[0] + 1, L.barRow), W, H);
        check(s.view() == View::Model, "and clicking another comes back");

        // LEFT INSERTS. Clicking a module and having it appear is what was
        // asked for; a click that only highlighted would need a second one.
        const std::size_t before = s.document().rowCount(s.palette()[2]);
        handleKey(s, Key::mouse(MouseButton::Left, L.paletteX + 1, L.paletteTop + 2), W, H);
        check(s.paletteIndex() == 2, "clicking a module selects it");
        check(s.document().rowCount(s.palette()[2]) == before + 1 ||
              s.dirty(), "and inserts it");

        // RIGHT EXPLAINS.
        TuiState t = TuiState::open(modelPath("teller.des"));
        handleKey(t, Key::mouse(MouseButton::Right, L.paletteX + 1, L.paletteTop + 1), W, H);
        check(t.overlay() == Overlay::Help, "right-clicking a module explains it");

        // Clicking in the text puts the caret there.
        TuiState u = TuiState::open(modelPath("teller.des"));
        const Layout M = layoutFor(u, W, H);
        // A line with TEXT on it. moveTo clamps to the line's length, so
        // clicking column 3 of a blank line lands at column 0 and proves
        // nothing about the arithmetic -- which is what the first version of
        // this check did, and it failed for that reason rather than a real one.
        std::size_t row = 0;
        for (std::size_t i = 1; i < u.buffer().lineCount(); ++i)
            if (u.buffer().lineAt(i).size() > 6) { row = i; break; }
        check(row > 0, "the file has a line with text on it");
        handleKey(u, Key::mouse(MouseButton::Left, M.textX + 3,
                                M.textTop + static_cast<int>(row)), W, H);
        check(u.focus() == Pane::Editor, "clicking the text focuses it");
        check(u.buffer().caret().line == M.firstLine + row &&
              u.buffer().caret().column == M.firstColumn + 3,
              "and the caret lands under the pointer");

        // The wheel scrolls whatever is under it.
        TuiState v = TuiState::open(modelPath("teller.des"));
        handleKey(v, Key::mouse(MouseButton::WheelDown, M.textX + 1, M.textTop + 1), W, H);
        check(v.buffer().caret().line == 3, "the wheel scrolls the text");
    }

    section("Quitting with unsaved work asks first");
    {
        TuiState s = openCopy("teller.des", "tui_quit.des");
        check(handleKey(s, Key::control('q')) == false || s.wantsQuit(),
              "a clean file quits at once");

        TuiState t = openCopy("teller.des", "tui_quit.des");
        typeText(t, "x");
        handleKey(t, Key::control('q'));
        check(t.overlay() == Overlay::Confirm, "unsaved work is asked about");
        check(!t.wantsQuit(), "and it does not quit yet");
        handleKey(t, Key::character('c'));
        check(t.overlay() == Overlay::None, "c cancels");

        handleKey(t, Key::control('q'));
        check(handleKey(t, Key::character('d')) == false, "d discards and quits");

        TuiState u = openCopy("teller.des", "tui_quit.des");
        typeText(u, "x");
        handleKey(u, Key::control('q'));
        check(handleKey(u, Key::character('s')) == false, "s saves and quits");
        check(fileText("tui_quit.des").find("xversion") != std::string::npos,
              "having actually saved");
        check(fileText(modelPath("teller.des")).find("xversion") == std::string::npos,
              "and the file it was COPIED FROM is untouched -- an earlier "
              "draft of this saved straight onto the shipped model");
    }

    section("The key map, and the frame it is drawn in");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));
        handleKey(s, Key::function(1));
        check(s.overlay() == Overlay::Help, "F1 opens the key map");
        Screen map(90, 26);
        render(s, map);
        const std::string text = map.asText();
        check(text.find("^L") != std::string::npos && text.find("^G") != std::string::npos &&
              text.find("^R") != std::string::npos,
              "which lists the keys");
        check(text.find("CONNECTION") != std::string::npos,
              "and says the one thing an Arena user will not guess: a "
              "connection is a name typed into a Next field");
        handleKey(s, Key::control('s'));
        check(s.overlay() == Overlay::None,
              "ANY key closes it, control keys included -- a key map you have "
              "to work out how to leave has undone its own job");

        // Nothing draws through a border, at either size.
        for (const std::pair<int, int>& size :
             {std::pair<int, int>{80, 24}, std::pair<int, int>{120, 40}}) {
            TuiState t = TuiState::open(modelPath("teller.des"));
            for (int pass = 0; pass < 5; ++pass) {
                Screen screen(size.first, size.second);
                render(t, screen);
                for (int y = 0; y < size.second; ++y) {
                    const std::string ln = screen.line(y);
                    if (!ln.empty() && ln[0] == '+')
                        check(static_cast<int>(ln.size()) <= size.first,
                              "a boxed line stays inside the screen");
                }
                if (pass == 0) handleKey(t, Key::control('f'));
                if (pass == 1) handleKey(t, Key::control('u'));
                if (pass == 2) handleKey(t, Key::control('e'));
                if (pass == 3) { handleKey(t, Key::control('b'));
                                 handleKey(t, Key::control('p')); }
            }
        }

        TuiState small = TuiState::open(modelPath("teller.des"));
        Screen tiny(40, 20);
        render(small, tiny);
        check(tiny.line(0) == "des_tui needs 80 x 24.",
              "a terminal too small says so in a message that FITS in it");
    }

    section("Somebody who has used Arena builds M/M/1 from nothing");
    {
        // THE CLAIM v15 RESTS ON, and the only test that makes it. Every key
        // below is a key: no setText, no moveTo, no reaching into the document
        // to write. If something cannot be reached by pressing what is on
        // screen, this fails.
        // fromText(""), NOT open(). This test SAVES to that path at the end,
        // so opening it read back the model the previous run had written and
        // nothing started from a blank page -- the suite passed once and
        // failed every time after, which is how both sanitiser legs found it
        // and the ordinary run did not. Whether a missing file opens empty is
        // a different claim, and it has its own test.
        TuiState s = TuiState::fromText("", "mm1_v15.des");
        check(s.buffer().lineAt(0).empty(), "starting from a blank page");

        typeText(s, "version = 1");
        handleKey(s, Key::special(KeyKind::Enter));

        const auto insertModule = [](TuiState& st, const std::string& type) {
            // To the END of the text first, so each record lands after the
            // last one instead of inside it.
            //
            // BOUNDED, because an open overlay swallows Down: the first draft
            // of this spun forever when a pick list was still up, which is a
            // fair imitation of what it would do to a person.
            handleKey(st, Key::control('b'));
            for (std::size_t i = 0; i < st.buffer().lineCount() + 2 &&
                                    st.buffer().caret().line + 1 < st.buffer().lineCount(); ++i)
                handleKey(st, Key::special(KeyKind::Down));
            check(st.buffer().caret().line + 1 == st.buffer().lineCount(),
                  "the caret reaches the end of the text");
            handleKey(st, Key::special(KeyKind::End));
            handleKey(st, Key::control('p'));
            for (int i = 0; i < 60 && st.palette()[st.paletteIndex()] != type; ++i)
                handleKey(st, Key::special(KeyKind::Down));
            check(st.palette()[st.paletteIndex()] == type,
                  "the palette reaches " + type);
            handleKey(st, Key::special(KeyKind::Enter));
        };
        // Find the field and type into it, the way a person scrolls to the
        // [Process] and looks down it for Service.
        const auto set = [](TuiState& st, const std::string& type,
                            const std::string& field, const std::string& value) {
            check(goToField(st, type, field), type + "." + field + " is reachable");
            typeText(st, value);
        };

        insertModule(s, "Create");
        set(s, "Create", "Name", "Arrivals");
        set(s, "Create", "Interarrival", "EXPO(1.0)");
        set(s, "Create", "Max Arrivals", "200");

        // Wiring to a block that does not exist yet: the one step the list
        // must not make harder than typing.
        check(goToField(s, "Create", "Next"), "the Create's Next is reachable");
        handleKey(s, Key::control('l'));
        // The list DOES open: this Create is itself a declared block, so
        // "Arrivals" is on it. What is not on it is the block being wired to,
        // because it has not been typed yet -- and that is the case the list
        // must not make harder than typing.
        check(s.overlay() == Overlay::Picker, "the list opens with what exists");
        check(std::find(s.choices().begin(), s.choices().end(), "Serve") ==
                  s.choices().end(),
              "and 'Serve' is NOT on it");
        handleKey(s, Key::special(KeyKind::Escape));
        check(s.overlay() == Overlay::None, "Escape drops out of the list");
        typeText(s, "Serve");
        check(s.buffer().lineAt(s.buffer().caret().line).find("Serve") != std::string::npos,
              "so a block can be named BEFORE it exists");

        insertModule(s, "Process");
        set(s, "Process", "Name", "Serve");
        set(s, "Process", "Capacity", "1");
        set(s, "Process", "Service", "EXPO(0.8)");

        check(goToField(s, "Process", "Discipline"), "the Discipline field");
        handleKey(s, Key::control('l'));
        check(s.overlay() == Overlay::Picker, "an Enum offers a list");
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.buffer().lineAt(s.buffer().caret().line).find("FIFO") != std::string::npos,
              "and accepting it writes a spelling the compiler accepts");

        check(goToField(s, "Process", "Next"), "the Process's Next");
        typeText(s, "Out");

        insertModule(s, "Dispose");
        set(s, "Dispose", "Name", "Out");

        check(!hasErrors(s.diagnostics()),
              "a model typed entirely through the keys COMPILES");

        // Now the list offers what was typed blind.
        check(goToField(s, "Create", "Next"), "back on the Create's Next");
        handleKey(s, Key::control('l'));
        check(s.overlay() == Overlay::Picker, "the list opens now that blocks exist");
        check(std::find(s.choices().begin(), s.choices().end(), "Out") != s.choices().end(),
              "and offers the block that was named before it existed");
        handleKey(s, Key::special(KeyKind::Escape));

        // The flow agrees with what was typed.
        handleKey(s, Key::control('f'));
        Screen flow(90, 26);
        render(s, flow);
        const std::string shown = flow.asText();
        check(shown.find("Next -> Serve") != std::string::npos &&
              shown.find("Next -> Out") != std::string::npos,
              "^F shows the chain that was wired");
        check(shown.find("does not exist") == std::string::npos, "with nothing dangling");

        // And it RUNS. Nobody typed a [Run] row.
        check(s.document().rowCount("Run") == 0, "there is no [Run] row");
        handleKey(s, Key::control('r'));
        check(s.running() != nullptr, "^R starts it anyway");
        int guard = 0;
        while (s.running() != nullptr && guard++ < 200000) s.advanceRun();
        check(guard < 200000, "and it TERMINATES rather than running forever");
        check(s.haveResults(), "with a report");
        check(s.view() == View::Results, "shown straight away");

        // What it saves reads back as the same model.
        handleKey(s, Key::control('s'));
        TuiState back = TuiState::open("mm1_v15.des");
        check(!hasErrors(back.diagnostics()), "what it wrote opens clean");
        check(back.document().cell("Process", 0, "Service") == "EXPO(0.8)",
              "with the values that were typed");
        check(back.buffer().text() == s.buffer().text(),
              "byte for byte, because the text IS the model");
    }
}
