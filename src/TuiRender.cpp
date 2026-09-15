#include "TuiRender.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include "ModuleSchema.hpp"

namespace des {
namespace {

// Truncate to fit. Screen clips already, so this is not about safety -- it is
// so a run of text that does not fit ends before the box border instead of
// drawing over it and leaving the frame open.
std::string fit(const std::string& s, int room) {
    if (room <= 0) return std::string();
    if (static_cast<int>(s.size()) <= room) return s;
    return s.substr(0, static_cast<std::size_t>(room));
}

// A path is trimmed from the FRONT, keeping the end. fit() cutting a long
// absolute path at 40 columns left "C:/Users/User A/Documents/Indu/Simulation"
// on screen -- every character of it useless, and the one part a person needs,
// the file name, gone.
std::string fitTail(const std::string& s, int room) {
    if (room <= 0) return std::string();
    if (static_cast<int>(s.size()) <= room) return s;
    if (room <= 3) return s.substr(s.size() - static_cast<std::size_t>(room));
    return "..." + s.substr(s.size() - static_cast<std::size_t>(room - 3));
}

// Break prose at spaces. Written because fit() alone cut a module description
// off mid-word -- "Arena draws a line, thi" -- and a sentence that stops in the
// middle reads as a rendering fault rather than as the help it is. fit() stays
// as the backstop for a single word longer than the room.
std::vector<std::string> wrapText(const std::string& text, int room) {
    std::vector<std::string> lines;
    if (room <= 0) return lines;
    std::string line, word;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const bool end = (i == text.size());
        if (!end && text[i] != ' ') { word.push_back(text[i]); continue; }
        if (!word.empty()) {
            if (static_cast<int>(line.size() + word.size() + 1) > room && !line.empty()) {
                lines.push_back(line);
                line.clear();
            }
            if (!line.empty()) line += " ";
            line += word;
            word.clear();
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

const char* const TAB_LABEL[4] = {"Model ^B", "Flow ^F", "Runs ^U", "Results ^E"};

int digitsIn(std::size_t n) {
    int d = 1;
    while (n >= 10) { n /= 10; ++d; }
    return d;
}

// A box that draws its own frame and returns nothing: every overlay wants the
// same three lines, and a fourth copy of them is where they start to differ.
void frame(Screen& screen, int x, int y, int w, int h, const std::string& title) {
    screen.box(x, y, w, h);
    if (!title.empty()) screen.text(x + 3, y, " " + title + " ", Attr::Bold);
}

// Clear the whole BAND a floating box sits in, edge to edge inside the outer
// frame -- not just the box's own rectangle.
//
// Clearing only the rectangle left the page showing either side of it, and
// what showed were the tail ends of sentences: "Expression+-- Process.Service"
// running together on the left and a stray "a result." on the right. Fragments
// of a line read as text, not as background, and the eye tries to make sense of
// them. The columns at 0 and width-1 stay, because those are the frame the box
// is floating inside.
void clearFor(Screen& screen, int /*x*/, int y, int /*w*/, int h) {
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = 1; xx < screen.width() - 1; ++xx) screen.put(xx, yy, ' ');
}

}  // namespace

Layout layoutFor(const TuiState& state, int width, int height) {
    Layout L;
    L.width  = width;
    L.height = height;

    L.barRow     = 0;
    L.bodyTop    = 1;
    L.hintRow    = height - 1;
    L.statusRow  = height - 2;
    L.bodyBottom = height - 3;

    int x = 1;
    for (int i = 0; i < 4; ++i) {
        const int w = static_cast<int>(std::string(TAB_LABEL[i]).size()) + 2;
        L.tabStart.push_back(x);
        L.tabEnd.push_back(x + w);
        x += w;
    }

    // The palette is fixed at fourteen columns of label. Every module type
    // this project has fits, and a palette that resized with its longest entry
    // would move the editor sideways when a later version added one.
    L.paletteX     = 2;
    L.paletteWidth = 14;
    L.paletteTop   = L.bodyTop + 1;
    L.paletteRows  = std::max(0, L.bodyBottom - L.paletteTop);
    L.dividerX     = L.paletteX + L.paletteWidth + 1;
    if (L.paletteRows > 0 &&
        state.paletteIndex() >= static_cast<std::size_t>(L.paletteRows))
        L.paletteFirst = state.paletteIndex() -
                         static_cast<std::size_t>(L.paletteRows) + 1;

    L.gutterWidth = std::max(3, digitsIn(state.buffer().lineCount())) + 1;
    L.gutterX     = L.dividerX + 1;
    // +2: one column for the error marker and one blank after it. With only
    // one, the marker and the text ran together -- "5 EEntity Type = Gears" --
    // and the E read as part of the line rather than as a note about it.
    L.textX       = L.gutterX + L.gutterWidth + 2;
    L.textWidth   = std::max(0, width - 1 - L.textX);
    L.textTop     = L.bodyTop + 1;
    L.textRows    = std::max(0, L.bodyBottom - L.textTop);

    // Scroll is DERIVED from the caret rather than stored, so there is no
    // second piece of state that can disagree with where the cursor is.
    const Caret at = state.buffer().caret();
    if (L.textRows > 3 && at.line + 2 >= static_cast<std::size_t>(L.textRows))
        L.firstLine = at.line + 3 - static_cast<std::size_t>(L.textRows);
    if (L.firstLine > at.line) L.firstLine = at.line;

    if (L.textWidth > 0 && at.column >= static_cast<std::size_t>(L.textWidth))
        L.firstColumn = at.column - static_cast<std::size_t>(L.textWidth) + 1;

    return L;
}

namespace {

void renderBar(const TuiState& state, Screen& screen, const Layout& L) {
    const std::string name = state.path() + (state.dirty() ? "*" : "");
    const int nameRoom = std::min(static_cast<int>(name.size()), L.width / 3);
    screen.text(L.width - nameRoom - 1, L.barRow, fitTail(name, nameRoom), Attr::Bold);

    const View views[4] = {View::Model, View::Flow, View::Runs, View::Results};
    for (int i = 0; i < 4; ++i) {
        const bool here = state.view() == views[i];
        // Results is DIM until there are any. A tab that promises something
        // the program does not have yet is a tab somebody presses once and
        // then distrusts.
        const bool empty = views[i] == View::Results && !state.haveResults();
        screen.text(L.tabStart[i], L.barRow,
                    " " + std::string(TAB_LABEL[i]) + " ",
                    here ? Attr::Reverse : (empty ? Attr::Dim : Attr::Normal));
    }
}

void renderPalette(const TuiState& state, Screen& screen, const Layout& L) {
    const bool focused = state.focus() == Pane::Palette;
    // The title SAYS which pane has the keyboard. Both branches of this were
    // the same string once, so the only difference was an attribute -- and an
    // attribute is invisible in a screenshot, in a test, and to anyone whose
    // terminal renders reverse video faintly.
    screen.text(L.paletteX, L.bodyTop, focused ? "[Modules]" : " Modules ",
                focused ? Attr::Reverse : Attr::Bold);

    const int rows = L.paletteRows;
    if (rows <= 0) return;

    for (int i = 0; i < rows; ++i) {
        const std::size_t p = L.paletteFirst + static_cast<std::size_t>(i);
        if (p >= state.palette().size()) break;
        const bool here = (p == state.paletteIndex());
        // The count of that type ALREADY IN THE FILE, beside the name. It is
        // the question the palette is next to -- what does this model have --
        // and it costs a column.
        const std::size_t n = state.document().rowCount(state.palette()[p]);
        std::string label = state.palette()[p];
        if (n > 0) label += " " + std::to_string(n);
        screen.text(L.paletteX, L.paletteTop + i, fit(label, L.paletteWidth),
                    here ? (focused ? Attr::Reverse : Attr::Bold)
                         : (n > 0 ? Attr::Normal : Attr::Dim));
    }
}

void renderEditor(const TuiState& state, Screen& screen, const Layout& L) {
    const TextBuffer& buffer = state.buffer();
    const bool focused = state.focus() == Pane::Editor;

    std::string title = " " + std::to_string(buffer.lineCount()) +
                        (buffer.lineCount() == 1 ? " line " : " lines ");
    if (buffer.hasSelection()) title += "(selection) ";
    screen.text(L.gutterX, L.bodyTop, title, focused ? Attr::Bold : Attr::Dim);

    const Caret caret = buffer.caret();
    const Caret selA  = buffer.selectionStart();
    const Caret selB  = buffer.selectionEnd();

    for (int i = 0; i < L.textRows; ++i) {
        const std::size_t l = L.firstLine + static_cast<std::size_t>(i);
        if (l >= buffer.lineCount()) break;
        const int y = L.textTop + i;

        const std::string number = std::to_string(l + 1);
        screen.text(L.gutterX + L.gutterWidth - 1 - static_cast<int>(number.size()),
                    y, number, Attr::Dim);

        // The marker column. An error the compiler found gets a mark beside
        // the line it is about, which is the whole reason lineOf() exists: a
        // diagnostic that names a cell is useless in a text editor unless
        // something turns it back into a line.
        if (const Diagnostic* d = state.diagnosticOnLine(l + 1))
            screen.put(L.textX - 2, y, d->severity == Severity::Error ? 'E' : 'w',
                       d->severity == Severity::Error ? Attr::Error : Attr::Dim);

        const std::string& line = buffer.lineAt(l);
        for (int c = 0; c < L.textWidth; ++c) {
            const std::size_t col = L.firstColumn + static_cast<std::size_t>(c);
            const bool onCaret = focused && l == caret.line && col == caret.column;
            const Caret here{l, col};
            const bool selected = buffer.hasSelection() &&
                                  !(here < selA) && here < selB;
            const char ch = col < line.size() ? line[col] : ' ';
            if (onCaret)       screen.put(L.textX + c, y, ch, Attr::Reverse);
            else if (selected) screen.put(L.textX + c, y, ch, Attr::Reverse);
            else if (col < line.size()) {
                // A comment is DIM. It is not the model, and a file that
                // explains itself should not compete with itself.
                const std::size_t at = line.find_first_not_of(" \t");
                const bool comment = at != std::string::npos &&
                                     (line[at] == '#' || line[at] == ';');
                const bool header = at != std::string::npos && line[at] == '[';
                screen.put(L.textX + c, y, ch,
                           comment ? Attr::Dim : (header ? Attr::Bold : Attr::Normal));
            }
        }
    }

    if (L.firstLine > 0)
        screen.text(L.width - 8, L.bodyTop, "more^", Attr::Dim);
    if (L.firstLine + static_cast<std::size_t>(L.textRows) < buffer.lineCount())
        screen.text(L.width - 8, L.bodyBottom, "more v", Attr::Dim);
}

void renderModel(const TuiState& state, Screen& screen, const Layout& L) {
    screen.box(0, L.bodyTop, L.width, L.bodyBottom - L.bodyTop + 1);
    for (int y = L.bodyTop + 1; y < L.bodyBottom; ++y)
        screen.put(L.dividerX - 1, y, '|', Attr::Dim);

    renderPalette(state, screen, L);
    renderEditor(state, screen, L);

    // An empty file is a blank page, and the answer to a blank page is
    // something to take apart rather than a cursor and silence.
    if (state.buffer().lineCount() == 1 && state.buffer().lineAt(0).empty()) {
        int y = L.textTop + 1;
        const int room = L.textWidth - 2;
        for (const std::string& s :
             {std::string("Nothing here yet."),
              std::string(""),
              std::string("^T writes a working single-server model you can run "
                          "and take apart."),
              std::string("Or pick a module on the left and press Enter to "
                          "insert one."),
              std::string(""),
              std::string("^G explains whatever the cursor is on. F1 lists the "
                          "keys.")}) {
            for (const std::string& ln : wrapText(s, room)) {
                if (y >= L.bodyBottom) break;
                screen.text(L.textX + 1, y++, fit(ln, room), Attr::Dim);
            }
            if (s.empty() && y < L.bodyBottom) ++y;
        }
    }
}

void renderFlow(const TuiState& state, Screen& screen, const Layout& L) {
    const std::vector<FlowBlock> blocks = flowOf(state.document());
    frame(screen, 0, L.bodyTop, L.width, L.bodyBottom - L.bodyTop + 1, "flow");
    const int right = L.width - 2;

    if (blocks.empty()) {
        screen.text(3, L.bodyTop + 2,
                    "No flowchart blocks yet. ^T writes a working model, or "
                    "pick one from", Attr::Dim);
        screen.text(3, L.bodyTop + 3, "the palette in the Model view.", Attr::Dim);
        return;
    }

    const int rows = L.bodyBottom - L.bodyTop - 2;
    std::size_t first = 0;
    if (state.flowCursor() >= static_cast<std::size_t>(rows))
        first = state.flowCursor() - static_cast<std::size_t>(rows) + 1;

    int y = L.bodyTop + 2;
    for (std::size_t i = first; i < blocks.size(); ++i) {
        if (y >= L.bodyBottom - 1) break;
        const FlowBlock& b = blocks[i];
        const bool here = (i == state.flowCursor());
        if (here) screen.put(1, y, '>', Attr::Bold);

        screen.text(3, y, fit(b.name.empty() ? "(unnamed)" : b.name, 18),
                    here ? Attr::Reverse : Attr::Bold);
        screen.text(22, y, fit(b.type, 10), Attr::Dim);

        std::string exits;
        for (const std::string& e : b.exits) exits += (exits.empty() ? "" : "   ") + e;
        // An exit that is genuinely empty is a statement, not an omission --
        // the engine spells "leaves the system" as a null next -- so it is
        // spelled out rather than left blank next to blocks that do wire on.
        if (exits.empty()) exits = "(leaves the system)";
        screen.text(33, y, fit(exits, right - 33),
                    b.dangling ? Attr::Error : Attr::Normal);
        ++y;
    }

    // Neither of these is an error on its own, and neither is said anywhere
    // else. A dangling exit the compiler does report -- but as a cell
    // reference, which is not the same as seeing the shape it leaves behind.
    if (y < L.bodyBottom - 1) ++y;
    for (const FlowBlock& b : blocks) {
        if (y >= L.bodyBottom) break;
        if (b.dangling)
            screen.text(3, y++, fit(b.name + ": an exit names a block that does "
                                    "not exist", right - 3), Attr::Error);
        else if (b.unreached)
            screen.text(3, y++, fit(b.name + ": nothing arrives here", right - 3),
                        Attr::Dim);
    }
}

void renderRuns(const TuiState& state, Screen& screen, const Layout& L) {
    frame(screen, 0, L.bodyTop, L.width, L.bodyBottom - L.bodyTop + 1, "runs");
    const int right = L.width - 2;

    if (state.runs().empty()) {
        screen.text(3, L.bodyTop + 2,
                    "No [Run] record. The model still runs -- it ends when the "
                    "event list", Attr::Dim);
        screen.text(3, L.bodyTop + 3,
                    "empties -- but a [Run] says how long, how many times, and "
                    "with which seed.", Attr::Dim);
        screen.text(3, L.bodyTop + 5, "^N adds one.", Attr::Bold);
        return;
    }

    // Built from the SAME widths as the rows below. Written out by hand it
    // was two columns adrift, which is the kind of thing that makes a reader
    // distrust every number under it.
    const std::size_t COLS[5] = {18, 9, 10, 7, 8};
    const char* const HEADS[5] = {"name", "length", "warm-up", "reps", "seed"};
    std::string header;
    for (int i = 0; i < 5; ++i) {
        std::string h = HEADS[i];
        h.resize(COLS[i], ' ');
        header += h;
    }
    screen.text(3, L.bodyTop + 1, fit(header, right - 3), Attr::Bold);
    int y = L.bodyTop + 2;
    for (std::size_t i = 0; i < state.runs().size(); ++i) {
        if (y >= L.bodyBottom) break;
        const bool here = (i == state.runIndex());
        if (here) screen.put(1, y, '>', Attr::Bold);
        const auto cell = [&](const char* column) {
            return state.document().cellOrDefault("Run", i, column);
        };
        const auto pad = [](std::string s, std::size_t w) {
            if (s.empty()) s = "-";
            s.resize(w, ' ');
            return s;
        };
        std::string line = pad(state.runs()[i], COLS[0]);
        line += pad(cell("Length"), COLS[1]);
        line += pad(cell("Warm-up"), COLS[2]);
        line += pad(cell("Replications"), COLS[3]);
        line += pad(cell("Base Seed"), COLS[4]);
        screen.text(3, y, fit(line, right - 3), here ? Attr::Reverse : Attr::Normal);
        ++y;
    }

    if (y + 1 < L.bodyBottom)
        screen.text(3, y + 1,
                    "Enter runs the one you are on. ^N adds another. Enter on "
                    "its name in", Attr::Dim);
    if (y + 2 < L.bodyBottom)
        screen.text(3, y + 2, "the Model view edits it like any other field.",
                    Attr::Dim);
}

void renderResults(const TuiState& state, Screen& screen, const Layout& L) {
    frame(screen, 0, L.bodyTop, L.width, L.bodyBottom - L.bodyTop + 1,
          state.haveResults() ? "results: " + state.resultsOf() : "results");
    const int right = L.width - 2;

    if (!state.haveResults()) {
        screen.text(3, L.bodyTop + 2,
                    "Nothing has been run yet. ^R runs the model.", Attr::Dim);
        screen.text(3, L.bodyTop + 3,
                    "Pick which [Run] on the Runs tab first, if there is more "
                    "than one.", Attr::Dim);
        return;
    }

    std::vector<std::string> lines{std::string()};
    for (const char c : state.results()) {
        if (c == '\n') lines.push_back(std::string());
        else if (c != '\r') lines.back().push_back(c);
    }

    int y = L.bodyTop + 1;
    for (std::size_t i = state.resultsScroll(); i < lines.size(); ++i) {
        if (y >= L.bodyBottom) {
            screen.text(right - 6, L.bodyBottom, "more v", Attr::Dim);
            break;
        }
        screen.text(2, y++, fit(lines[i], right - 2));
    }
    if (state.resultsScroll() > 0)
        screen.text(right - 6, L.bodyTop, "more^", Attr::Dim);
}

// --- overlays ---------------------------------------------------------------

void renderHelp(const TuiState& state, Screen& screen, const Layout& L) {
    const int w = std::min(L.width - 8, 68);
    const int h = std::min(L.height - 6, 18);
    const int x = (L.width - w) / 2;
    const int y = (L.height - h) / 2;
    clearFor(screen, x, y, w, h);
    frame(screen, x, y, w, h, state.helpTitle());

    int line = y + 1;
    for (const std::string& paragraph : state.helpBody()) {
        if (paragraph.empty()) { if (line < y + h - 1) ++line; continue; }
        for (const std::string& ln : wrapText(paragraph, w - 4)) {
            if (line >= y + h - 1) return;
            const bool indented = paragraph.compare(0, 4, "    ") == 0;
            screen.text(x + 2, line++, fit(ln, w - 4),
                        indented ? Attr::Dim : Attr::Normal);
        }
    }
}

void renderPicker(const TuiState& state, Screen& screen, const Layout& L) {
    const std::vector<std::string>& choices = state.choices();
    const int w = std::min(L.width - 20, 40);
    const int h = std::min(L.height - 6,
                           static_cast<int>(choices.size()) + 3);
    const int x = (L.width - w) / 2;
    const int y = (L.height - h) / 2;
    clearFor(screen, x, y, w, h);

    const std::string key = keyOfLine(state.buffer().lineAt(state.buffer().caret().line));
    frame(screen, x, y, w, h, key);

    const int rows = h - 2;
    std::size_t first = 0;
    if (state.choice() >= static_cast<std::size_t>(rows))
        first = state.choice() - static_cast<std::size_t>(rows) + 1;
    if (first > 0) screen.text(x + w - 8, y, "more^", Attr::Dim);

    for (int i = 0; i < rows; ++i) {
        const std::size_t c = first + static_cast<std::size_t>(i);
        if (c >= choices.size()) break;
        const bool here = (c == state.choice());
        if (here) screen.put(x + 1, y + 1 + i, '>', Attr::Bold);
        // The empty choice needs a VISIBLE spelling. Drawn as itself it is a
        // blank line with a cursor on it, which reads as a rendering fault
        // rather than as an answer.
        const std::string label = choices[c].empty() ? "(none)" : choices[c];
        screen.text(x + 3, y + 1 + i, fit(label, w - 5),
                    here ? Attr::Reverse : (choices[c].empty() ? Attr::Dim : Attr::Normal));
    }
    if (first + static_cast<std::size_t>(rows) < choices.size())
        screen.text(x + w - 8, y + h - 1, "more v", Attr::Dim);
}

void renderRunning(const TuiState& state, Screen& screen, const Layout& L) {
    const int w = std::min(L.width - 10, 60);
    const int h = 7;
    const int x = (L.width - w) / 2;
    const int y = (L.height - h) / 2;
    clearFor(screen, x, y, w, h);
    frame(screen, x, y, w, h, "running " + state.resultsOf());

    const RunController* run = state.running();
    if (run == nullptr) return;
    const RunProgress p = run->progress();
    screen.text(x + 2, y + 2,
                fit("replication " + std::to_string(p.replication) + " of " +
                        std::to_string(p.replications) + "    events " +
                        std::to_string(p.eventsProcessed), w - 4));

    if (p.fraction) {
        const int inner = w - 6;
        const int filled = static_cast<int>(*p.fraction * inner);
        std::string bar = "[";
        for (int i = 0; i < inner; ++i) bar += (i < filled ? '#' : ' ');
        bar += "]";
        screen.text(x + 2, y + 4, fit(bar, w - 4));
    } else {
        // NO BAR. The stopping rule cannot say how far through it is, and a
        // bar sitting at zero until it jumps to full is a lie the reader
        // cannot detect.
        screen.text(x + 2, y + 4,
                    fit("this rule cannot say how far through it is", w - 4),
                    Attr::Dim);
    }
}

void renderKeyMap(Screen& screen, const Layout& L) {
    const int w = std::min(L.width - 6, 72);
    const int h = std::min(L.height - 4, 21);
    const int x = (L.width - w) / 2;
    const int y = (L.height - h) / 2;
    clearFor(screen, x, y, w, h);
    frame(screen, x, y, w, h, "keys");

    static const char* const KEYS[][2] = {
        {"^B ^F ^U ^E",  "Model, Flow, Runs, Results"},
        {"Tab",          "swap between the palette and the text"},
        {"Enter",        "in the palette: insert that module"},
        {"^G",           "explain whatever the cursor is on"},
        {"^L",           "list the values a field allows"},
        {"^J",           "jump to the first error"},
        {"^T",           "write a working model into an empty file"},
        {"^R",           "run; ^N on the Runs tab adds another [Run]"},
        {"^W",           "save the results to a file"},
        {"^S",           "save the model"},
        {"^X ^C ^V",     "cut, copy, paste; ^A selects all"},
        {"^Z ^Y",        "undo, redo"},
        {"F1 or ^K",     "this"},
        {"^Q",           "quit, asking first if there is unsaved work"},
    };
    int line = y + 1;
    for (const auto& row : KEYS) {
        if (line >= y + h - 3) break;
        screen.text(x + 3, line, row[0], Attr::Bold);
        screen.text(x + 17, line, fit(row[1], w - 19));
        ++line;
    }
    if (line < y + h - 1) ++line;
    for (const std::string& ln : wrapText(
             "There is no canvas. A model is this text, and a CONNECTION is "
             "the name of the next block typed into a Next field.", w - 6)) {
        if (line >= y + h - 1) break;
        screen.text(x + 3, line++, fit(ln, w - 6), Attr::Dim);
    }
}

const char* hintFor(const TuiState& state) {
    switch (state.overlay()) {
        case Overlay::Help:    return "any key returns";
        case Overlay::Picker:  return "Up/Down choose   Enter accept   Esc type it instead";
        case Overlay::Running: return "Esc  stop the run";
        case Overlay::Confirm: return "(s)ave and quit   (d)iscard and quit   (c)ancel";
        case Overlay::None:    break;
    }
    switch (state.view()) {
        case View::Flow:    return "Up/Down move   Enter opens that block in the text   F1 keys";
        case View::Runs:    return "Up/Down choose   Enter runs it   ^N adds one   F1 keys";
        case View::Results: return "Up/Down scroll   ^W save   ^R run again   F1 keys";
        case View::Model:   break;
    }
    if (state.focus() == Pane::Palette)
        return "Enter inserts   ^G explains it   Tab back to the text   F1 keys";
    return "^S save  ^R run  ^L list  ^G help  ^J error  ^T starter  F1 keys";
}

}  // namespace

void render(const TuiState& state, Screen& screen) {
    screen.clear();
    if (screen.width() < MIN_WIDTH || screen.height() < MIN_HEIGHT) {
        // SHORT, and split. The first draft was 44 characters wide and got
        // clipped to "...at least 80 x" on the 40-column terminal it was
        // complaining about -- a message about a terminal being too small
        // must fit in one.
        screen.text(0, 0, "des_tui needs 80 x 24.");
        screen.text(0, 1, "This one is smaller.");
        screen.text(0, 2, "Resize and retry.");
        return;
    }
    const Layout L = layoutFor(state, screen.width(), screen.height());

    renderBar(state, screen, L);
    switch (state.view()) {
        case View::Model:   renderModel(state, screen, L);   break;
        case View::Flow:    renderFlow(state, screen, L);    break;
        case View::Runs:    renderRuns(state, screen, L);    break;
        case View::Results: renderResults(state, screen, L); break;
    }
    switch (state.overlay()) {
        case Overlay::Help:
            // An empty title means the key map rather than a field's help --
            // the same overlay slot, because only one of them is ever up.
            if (state.helpTitle().empty()) renderKeyMap(screen, L);
            else                           renderHelp(state, screen, L);
            break;
        case Overlay::Picker:  renderPicker(state, screen, L);  break;
        case Overlay::Running: renderRunning(state, screen, L); break;
        case Overlay::Confirm: break;
        case Overlay::None:    break;
    }
    screen.text(0, L.statusRow, fit(state.status(), screen.width()));
    screen.text(0, L.hintRow, fit(hintFor(state), screen.width()),
                state.overlay() == Overlay::Confirm ? Attr::Bold : Attr::Dim);
}

}  // namespace des
