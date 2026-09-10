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

// Column widths are computed from the DATA, not fixed, because a schema added
// in a later version has column names this file has never seen.
int widthFor(const TuiState& state, const std::string& column) {
    std::size_t widest = column.size();
    for (std::size_t r = 0; r < state.rowCountHere(); ++r)
        widest = std::max(widest, state.document().cell(state.type(), r, column).size());
    const int w = static_cast<int>(widest) + 2;
    return w > 24 ? 24 : w;      // one runaway expression must not eat the row
}

void renderTabs(const TuiState& state, Screen& screen) {
    const std::vector<std::string>& types = state.types();

    // Reserve the right-hand end for the file name FIRST, so the tabs know how
    // much room they actually have. Sixteen module types do not fit across
    // eighty columns, so this scrolls.
    const std::string name = state.path() + (state.dirty() ? "*" : "");
    const int nameRoom = std::min(static_cast<int>(name.size()), screen.width() / 2);
    screen.text(screen.width() - nameRoom - 1, 0, fitTail(name, nameRoom), Attr::Bold);

    // The COUNT is on the tab. Seventeen tabs and no way to tell which hold
    // anything meant tabbing through every one of them to find out what a
    // model contains -- which is the question the tab bar is for. A type with
    // no rows shows no number and is dimmed, so the model's shape reads at a
    // glance.
    const auto labelOf = [&](const std::string& t) {
        const std::size_t n = state.document().rowCount(t);
        const std::string body = n > 0 ? t + " " + std::to_string(n) : t;
        return (t == state.type()) ? "[" + body + "]" : " " + body + " ";
    };

    // Room for the two scroll markers is reserved WHETHER OR NOT they are
    // drawn. The first version measured without them and drew with them, so
    // the two passes disagreed by a column and the selected tab could still
    // fall off the end -- which is the bug this scrolling exists to prevent.
    const int left  = 2;
    const int right = screen.width() - nameRoom - 3;

    std::size_t current = 0;
    for (std::size_t i = 0; i < types.size(); ++i)
        if (types[i] == state.type()) current = i;

    // Walk `first` forward until the selected tab is inside the window.
    std::size_t first = 0;
    std::size_t last  = 0;
    for (;;) {
        int x = left;
        last = first;
        for (std::size_t i = first; i < types.size(); ++i) {
            const int w = static_cast<int>(labelOf(types[i]).size());
            if (x + w > right) break;
            x += w;
            last = i;
        }
        if (current <= last || first >= current) break;
        ++first;
    }

    int x = left;
    if (first > 0) screen.put(1, 0, '<', Attr::Dim);
    for (std::size_t i = first; i <= last && i < types.size(); ++i) {
        const bool empty = state.document().rowCount(types[i]) == 0;
        const Attr attr = types[i] == state.type()
                              ? Attr::Reverse
                              : (empty ? Attr::Dim : Attr::Normal);
        x = screen.text(x, 0, labelOf(types[i]), attr);
    }
    if (last + 1 < types.size()) screen.put(x, 0, '>', Attr::Dim);
}

void renderGrid(const TuiState& state, Screen& screen, int top, int bottom) {
    const std::vector<std::string> columns = state.columnsHere();
    screen.box(0, top, screen.width(), bottom - top + 1);
    const int right = screen.width() - 2;      // inside the box border

    int x = 2;
    std::vector<int> xs;
    for (const std::string& c : columns) {
        if (x >= right) break;
        xs.push_back(x);
        screen.text(x, top + 1, fit(c, right - x), Attr::Bold);
        x += widthFor(state, c);
    }

    const int rowsVisible = bottom - top - 2;
    if (rowsVisible <= 0) return;

    // An empty table is where a person who has never seen this is stuck. Say
    // what the module is for and which key adds a row, rather than drawing an
    // empty box and waiting.
    if (state.rowCountHere() == 0) {
        const ModuleSchema* schema = state.schemaHere();
        int y = top + 2;
        if (schema != nullptr && !schema->help.empty()) {
            const int room = right - 4;
            for (const std::string& ln : wrapText(schema->help, room)) {
                if (y >= bottom - 2) break;
                screen.text(3, y++, fit(ln, room), Attr::Dim);
            }
        }
        // A document with nothing in it ANYWHERE is a different situation from
        // an empty table in a model that exists. The first is a blank page,
        // and the answer to a blank page is a model you can run and take
        // apart -- so that is what the blank page offers.
        bool anywhere = false;
        for (const std::string& t : state.document().types())
            if (state.document().rowCount(t) > 0) anywhere = true;

        if (y < bottom - 2 && !anywhere) {
            screen.text(3, y + 1, "^T fills this in with a working model to "
                                  "take apart", Attr::Bold);
            if (y + 2 < bottom - 1)
                screen.text(3, y + 2, "^N adds one empty row", Attr::Dim);
        } else if (y < bottom - 1) {
            screen.text(3, y + 1, "no rows yet -- ^N adds one", Attr::Bold);
        }
        return;
    }
    // Scroll so the cursor is always on screen, without moving when it does
    // not have to.
    std::size_t first = 0;
    if (state.row() >= static_cast<std::size_t>(rowsVisible))
        first = state.row() - static_cast<std::size_t>(rowsVisible) + 1;

    for (int i = 0; i < rowsVisible; ++i) {
        const std::size_t r = first + static_cast<std::size_t>(i);
        if (r >= state.rowCountHere()) break;
        const int y = top + 2 + i;
        const bool here = (r == state.row());
        if (here) screen.put(1, y, '>', Attr::Bold);
        const Attr attr = state.readOnlyHere() ? Attr::Dim
                                               : (here ? Attr::Reverse : Attr::Normal);
        for (std::size_t c = 0; c < xs.size() && c < columns.size(); ++c)
            screen.text(xs[c], y,
                        fit(state.document().cell(state.type(), r, columns[c]),
                            right - xs[c]),
                        attr);
    }
}

void renderDetail(const TuiState& state, Screen& screen, int top) {
    const std::vector<std::string> columns = state.columnsHere();
    const ModuleSchema* schema = state.schemaHere();
    const int height = screen.height() - top - 2;
    screen.box(0, top, screen.width(), height);
    const int right = screen.width() - 2;

    std::string title = " row " + std::to_string(state.row() + 1) + " of " +
                        std::to_string(state.rowCountHere()) + " ";
    if (schema == nullptr) title += "(unknown module type) ";
    screen.text(3, top, title, Attr::Bold);

    // A Process has eleven columns and each may add a diagnostic line, so the
    // pane runs out of room long before the fields do. It SCROLLS, for the
    // same reason the grid does: the field being edited was otherwise below
    // the fold and invisible.
    const int slots = height - 2;
    if (slots <= 0) return;
    // HEADROOM below the selected field. Its help and its diagnostic are drawn
    // UNDER it, so a cursor resting on the last visible row left no room for
    // either -- the one field that shows its help was exactly the field that
    // could not. Scrolling it up two lines early costs nothing and fixes both.
    const std::size_t headroom = (slots > 4) ? 3 : 0;
    std::size_t first = 0;
    if (state.column() + headroom >= static_cast<std::size_t>(slots))
        first = state.column() + headroom - static_cast<std::size_t>(slots) + 1;
    if (first > state.column()) first = state.column();
    if (first > 0) screen.text(right - 6, top, "more^", Attr::Dim);

    int y = top + 1;
    for (std::size_t c = first; c < columns.size(); ++c) {
        if (y >= top + height - 1) {
            screen.text(right - 6, top + height - 1, "more v", Attr::Dim);
            break;
        }
        const bool here = (c == state.column());
        screen.text(2, y, fit(columns[c], 15), here ? Attr::Reverse : Attr::Normal);

        const bool editing = here && state.mode() == Mode::Editing;
        const std::string value =
            editing ? "[" + state.editBuffer() + "]"
                    : state.document().cell(state.type(), state.row(), columns[c]);
        int x = screen.text(18, y, fit(value, right - 18),
                            editing ? Attr::Bold : Attr::Normal);

        if (schema != nullptr) {
            if (const Column* col = schema->column(columns[c])) {
                if (col->type == ColumnType::Enum) {
                    // Parenthesised. Unbracketed, the allowed spellings ran
                    // straight on from the value and read as "FIFO FIFO LIFO
                    // PRIORITY ...", which looks like the cell holds all of
                    // them.
                    std::string hint = "(";
                    for (std::size_t v = 0; v < col->enumValues.size(); ++v)
                        hint += (v == 0 ? "" : " ") + col->enumValues[v];
                    hint += ")";
                    screen.text(x + 2, y, fit(hint, right - x - 2), Attr::Dim);
                } else if (col->type == ColumnType::Reference) {
                    screen.text(x + 2, y, fit("-> " + col->referencedType, right - x - 2),
                                Attr::Dim);
                }
            }
        }
        ++y;
        // The help for the SELECTED field only. Showing it for all of them
        // would fill the pane with prose and bury the values, and the question
        // "what is this one for" is only ever asked about the one you are on.
        if (here && schema != nullptr && y < top + height - 1) {
            if (const Column* col = schema->column(columns[c])) {
                for (const std::string& ln : wrapText(col->help, right - 18)) {
                    if (y >= top + height - 1) break;
                    screen.text(18, y++, fit(ln, right - 18), Attr::Dim);
                }
            }
        }
        if (const Diagnostic* d = state.diagnosticFor(columns[c])) {
            if (y < top + height - 1) {
                const std::string what =
                    (d->severity == Severity::Error ? "error: " : "warning: ") + d->message;
                screen.text(18, y, fit(what, right - 18), Attr::Error);
                ++y;
            }
        }
    }
}

// Does the selected column offer a list? The footer says so while the pane is
// open, because nothing else on screen tells you Enter does two things.
bool pickableHere(const TuiState& state) {
    const ModuleSchema* schema = state.schemaHere();
    if (schema == nullptr) return false;
    const std::vector<std::string> columns = state.columnsHere();
    if (state.column() >= columns.size()) return false;
    const Column* col = schema->column(columns[state.column()]);
    return col != nullptr && (col->type == ColumnType::Enum ||
                              col->type == ColumnType::Reference);
}

// The pick list, drawn where the detail pane would be. The GRID STAYS ABOVE it
// on purpose: a list of block names means nothing without the row it is about.
void renderPicker(const TuiState& state, Screen& screen, int top) {
    const std::vector<std::string> columns = state.columnsHere();
    const std::vector<std::string>& choices = state.choices();
    const int height = screen.height() - top - 2;
    screen.box(0, top, screen.width(), height);
    const int right = screen.width() - 2;

    const std::string column =
        state.column() < columns.size() ? columns[state.column()] : std::string();
    screen.text(3, top, " " + column + " ", Attr::Bold);

    int listTop = top + 1;
    if (const ModuleSchema* schema = state.schemaHere()) {
        if (const Column* col = schema->column(column)) {
            // At most two lines of it. The list is what this pane is for, and
            // prose that pushes the choices off the bottom has stopped helping.
            const std::vector<std::string> help = wrapText(col->help, right - 2);
            for (std::size_t i = 0; i < help.size() && i < 2; ++i)
                screen.text(2, listTop++, fit(help[i], right - 2), Attr::Dim);
        }
    }

    const int slots = top + height - 1 - listTop;
    if (slots <= 0) return;
    std::size_t first = 0;
    if (state.choice() >= static_cast<std::size_t>(slots))
        first = state.choice() - static_cast<std::size_t>(slots) + 1;
    if (first > 0) screen.text(right - 6, top, "more^", Attr::Dim);

    for (int i = 0; i < slots; ++i) {
        const std::size_t c = first + static_cast<std::size_t>(i);
        if (c >= choices.size()) break;
        const int y = listTop + i;
        const bool here = (c == state.choice());
        if (here) screen.put(1, y, '>', Attr::Bold);
        // The empty choice needs a VISIBLE spelling. Drawn as itself it is a
        // blank line with a cursor on it, which reads as a rendering fault
        // rather than as an answer.
        const std::string label = choices[c].empty() ? "(none)" : choices[c];
        screen.text(3, y, fit(label, right - 3),
                    here ? Attr::Reverse : (choices[c].empty() ? Attr::Dim : Attr::Normal));
    }
    if (first + static_cast<std::size_t>(slots) < choices.size())
        screen.text(right - 6, top + height - 1, "more v", Attr::Dim);
}

// The key map. Two columns of keys, then a short paragraph on the two things
// an Arena user will not guess: that a connection is a name typed in a Next
// cell, and that a module type is a tab rather than a shape on a canvas.
void renderHelp(Screen& screen) {
    screen.box(0, 1, screen.width(), screen.height() - 3);
    screen.text(3, 1, " keys ", Attr::Bold);
    const int right = screen.width() - 2;

    static const char* const KEYS[][2] = {
        {"Tab / Shift-Tab", "the next module type, or the previous one"},
        {"Up / Down",       "move between rows, or between fields"},
        {"Enter",           "open the row; then a list, or the editor"},
        {"Escape",          "back out one step"},
        {"^N  /  ^D",       "add a row  /  delete this row"},
        {"^Z",              "undo -- 64 deep, and it covers row moves"},
        {"^S",              "save"},
        {"^T",              "fill an empty model in with a working one"},
        {"^F",              "the flow: who connects to whom, and what does not"},
        {"^R",              "run it; Escape stops"},
        {"?",               "this"},
        {"q",               "quit, asking first if there is unsaved work"},
    };

    int y = 3;
    for (const auto& row : KEYS) {
        if (y >= screen.height() - 8) break;
        screen.text(4, y, row[0], Attr::Bold);
        screen.text(24, y, fit(row[1], right - 24));
        ++y;
    }

    static const char* const NOTE =
        "There is no canvas. A module type is a tab, its rows are a table, and "
        "a CONNECTION is the name of the next block typed into a Next cell -- "
        "which is the one thing that differs from drawing a line in Arena. "
        "Every field explains itself in the pane below the grid.";
    ++y;
    for (const std::string& ln : wrapText(NOTE, right - 4)) {
        if (y >= screen.height() - 3) break;
        screen.text(4, y++, fit(ln, right - 4), Attr::Dim);
    }
}

// Arena's canvas, as far as a terminal goes. NOT a compiled Model: a document
// with a dangling exit does not compile, and that is exactly when somebody
// needs to see the wiring. So this reads the cells.
void renderFlow(const TuiState& state, Screen& screen) {
    const std::vector<FlowBlock> blocks = flowOf(state.document());
    screen.box(0, 1, screen.width(), screen.height() - 3);
    screen.text(3, 1, " flow ", Attr::Bold);
    const int right = screen.width() - 2;

    const int slots = screen.height() - 6;
    if (slots <= 0) return;
    std::size_t first = 0;
    if (state.flowCursor() >= static_cast<std::size_t>(slots))
        first = state.flowCursor() - static_cast<std::size_t>(slots) + 1;
    if (first > 0) screen.text(right - 6, 1, "more^", Attr::Dim);

    int y = 3;
    for (std::size_t i = first; i < blocks.size(); ++i) {
        if (y >= screen.height() - 4) {
            screen.text(right - 6, screen.height() - 4, "more v", Attr::Dim);
            break;
        }
        const FlowBlock& b = blocks[i];
        const bool here = (i == state.flowCursor());
        if (here) screen.put(1, y, '>', Attr::Bold);

        const std::string name = b.name.empty() ? "(unnamed)" : b.name;
        screen.text(3, y, fit(name, 18), here ? Attr::Reverse : Attr::Bold);
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
    int problems = 0;
    for (const FlowBlock& b : blocks)
        if (b.dangling || b.unreached) ++problems;
    if (problems > 0 && y < screen.height() - 4) {
        ++y;
        for (const FlowBlock& b : blocks) {
            if (y >= screen.height() - 4) break;
            if (b.dangling)
                screen.text(3, y++, fit(b.name + ": an exit names a block that "
                                        "does not exist", right - 3), Attr::Error);
            else if (b.unreached)
                screen.text(3, y++, fit(b.name + ": nothing arrives here",
                                        right - 3), Attr::Dim);
        }
    }
}

void renderRun(const TuiState& state, Screen& screen) {
    screen.box(0, 1, screen.width(), screen.height() - 3);
    screen.text(3, 1, " running ", Attr::Bold);
    const int right = screen.width() - 2;

    const RunController* run = state.running();
    if (run != nullptr) {
        const RunProgress p = run->progress();
        std::string line = "replication " + std::to_string(p.replication) + " of " +
                           std::to_string(p.replications) + "   events " +
                           std::to_string(p.eventsProcessed) + "   ";
        if (run->state() == RunState::Finished ||
            run->state() == RunState::Cancelled) {
            // Distinct from "cannot tell". A finished run has no fraction
            // because the controller has let its system go, and printing the
            // honest-uncertainty message here would make that message mean two
            // different things -- which is how it stops being believed.
            line += describe(run->state());
        } else if (p.fraction) {
            const int filled = static_cast<int>(*p.fraction * 40.0);
            line += "[";
            for (int i = 0; i < 40; ++i) line += (i < filled ? '#' : ' ');
            line += "]";
        } else {
            // NO BAR. The stopping rule cannot say how far through it is, and a
            // bar sitting at zero until it jumps to full is a lie the reader
            // cannot detect. v12's fourth-time rule, reaching the surface it
            // was written for.
            line += "(this rule cannot say how far through it is)";
        }
        screen.text(2, 3, fit(line, right - 2));
    }

    int y = 5;
    const std::string report = state.runReport();
    std::string current;
    for (char c : report) {
        if (c == '\n') {
            if (y < screen.height() - 4) screen.text(2, y++, fit(current, right - 2));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty() && y < screen.height() - 4)
        screen.text(2, y, fit(current, right - 2));
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
    if (state.mode() == Mode::Running) {
        renderTabs(state, screen);
        renderRun(state, screen);
        screen.text(0, screen.height() - 2, fit(state.status(), screen.width()));
        screen.text(0, screen.height() - 1, "Esc  stop and return to the grid",
                    Attr::Dim);
        return;
    }
    if (state.mode() == Mode::Help) {
        renderTabs(state, screen);
        renderHelp(screen);
        screen.text(0, screen.height() - 2, fit(state.status(), screen.width()));
        screen.text(0, screen.height() - 1, "any key returns", Attr::Dim);
        return;
    }
    if (state.mode() == Mode::Flow) {
        renderTabs(state, screen);
        renderFlow(state, screen);
        screen.text(0, screen.height() - 2, fit(state.status(), screen.width()));
        screen.text(0, screen.height() - 1,
                    "Up/Down move   Enter opens that block   any key returns",
                    Attr::Dim);
        return;
    }
    renderTabs(state, screen);
    // The grid gives up half the screen when the detail pane is up: a wide
    // table is for scanning, and the pane is where it becomes readable.
    const bool picking = state.mode() == Mode::Picking;
    const bool detail = picking || state.mode() == Mode::Detail ||
                        state.mode() == Mode::Editing;
    const int gridBottom = detail ? (screen.height() / 2) : (screen.height() - 4);
    renderGrid(state, screen, 1, gridBottom);
    if (picking)      renderPicker(state, screen, gridBottom + 1);
    else if (detail)  renderDetail(state, screen, gridBottom + 1);
    screen.text(0, screen.height() - 2, fit(state.status(), screen.width()));
    if (state.mode() == Mode::Confirm)
        screen.text(0, screen.height() - 1,
                    "(s)ave and quit   (d)iscard and quit   (c)ancel", Attr::Bold);
    else if (picking)
        // "type it instead" rather than "cancel", because that is what it
        // does, and because the person who needs it is the one whose answer is
        // not on the list.
        screen.text(0, screen.height() - 1,
                    "Up/Down choose   Enter accept   Esc type it instead", Attr::Dim);
    else if (state.mode() == Mode::Detail && pickableHere(state))
        screen.text(0, screen.height() - 1,
                    "Enter  choose from a list        ^S save  ^R run  Esc back",
                    Attr::Dim);
    else
        // ? EARNS ITS PLACE at the front: it is the key that makes the other
        // dozen findable, and the only one a person needs to be told.
        screen.text(0, screen.height() - 1,
                    "? keys   ^S save  ^R run  ^F flow  ^N new  ^D delete  q quit",
                    Attr::Dim);
}

}  // namespace des
