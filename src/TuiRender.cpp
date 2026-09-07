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

    const auto labelOf = [&](const std::string& t) {
        return (t == state.type()) ? "[" + t + "]" : " " + t + " ";
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
        const std::string label = labelOf(types[i]);
        x = screen.text(x, 0, label,
                        types[i] == state.type() ? Attr::Reverse : Attr::Normal);
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
            // Wrapped, not clipped: a module description is two or three lines
            // and fit() alone would cut the rest off. fit() stays as the
            // backstop, because a single word longer than the box would
            // otherwise draw straight through the border.
            const int room = right - 4;
            std::string lineText, word;
            const std::string& help = schema->help;
            for (std::size_t i = 0; i <= help.size() && y < bottom - 2; ++i) {
                const bool end = (i == help.size());
                if (!end && help[i] != ' ') { word.push_back(help[i]); continue; }
                if (static_cast<int>(lineText.size() + word.size() + 1) > room) {
                    screen.text(3, y++, fit(lineText, room), Attr::Dim);
                    lineText.clear();
                }
                if (!lineText.empty()) lineText += " ";
                lineText += word;
                word.clear();
            }
            if (!lineText.empty() && y < bottom - 2)
                screen.text(3, y++, fit(lineText, room), Attr::Dim);
        }
        if (y < bottom - 1)
            screen.text(3, y + 1, "no rows yet -- ^N adds one", Attr::Bold);
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
    std::size_t first = 0;
    if (state.column() >= static_cast<std::size_t>(slots))
        first = state.column() - static_cast<std::size_t>(slots) + 1;
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
                if (!col->help.empty()) {
                    screen.text(18, y, fit(col->help, right - 18), Attr::Dim);
                    ++y;
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
    renderTabs(state, screen);
    // The grid gives up half the screen when the detail pane is up: a wide
    // table is for scanning, and the pane is where it becomes readable.
    const bool detail = state.mode() == Mode::Detail || state.mode() == Mode::Editing;
    const int gridBottom = detail ? (screen.height() / 2) : (screen.height() - 4);
    renderGrid(state, screen, 1, gridBottom);
    if (detail) renderDetail(state, screen, gridBottom + 1);
    screen.text(0, screen.height() - 2, fit(state.status(), screen.width()));
    if (state.mode() == Mode::Confirm)
        screen.text(0, screen.height() - 1,
                    "(s)ave and quit   (d)iscard and quit   (c)ancel", Attr::Bold);
    else
        screen.text(0, screen.height() - 1,
                    "^S save  ^R run  ^N new  ^D delete  ^Z undo  Tab pane  q quit",
                    Attr::Dim);
}

}  // namespace des
