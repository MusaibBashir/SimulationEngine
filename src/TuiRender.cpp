#include "TuiRender.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include "ModuleSchema.hpp"

namespace des {
namespace {

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
    int x = 1;
    for (const std::string& t : state.types()) {
        const bool here = (t == state.type());
        const std::string label = here ? "[" + t + "]" : " " + t + " ";
        if (x + static_cast<int>(label.size()) >= screen.width() - 14) break;
        x = screen.text(x, 0, label, here ? Attr::Reverse : Attr::Normal);
    }
    const std::string name = state.path() + (state.dirty() ? "*" : "");
    const int at = screen.width() - static_cast<int>(name.size()) - 1;
    screen.text(at < x + 1 ? x + 1 : at, 0, name, Attr::Bold);
}

void renderGrid(const TuiState& state, Screen& screen, int top, int bottom) {
    const std::vector<std::string> columns = state.columnsHere();
    screen.box(0, top, screen.width(), bottom - top + 1);

    int x = 2;
    std::vector<int> xs;
    for (const std::string& c : columns) {
        xs.push_back(x);
        screen.text(x, top + 1, c, Attr::Bold);
        x += widthFor(state, c);
        if (x >= screen.width() - 1) break;
    }

    const int rowsVisible = bottom - top - 2;
    if (rowsVisible <= 0) return;
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
            screen.text(xs[c], y, state.document().cell(state.type(), r, columns[c]), attr);
    }
}

void renderDetail(const TuiState& state, Screen& screen, int top) {
    const std::vector<std::string> columns = state.columnsHere();
    const ModuleSchema* schema = state.schemaHere();
    screen.box(0, top, screen.width(), screen.height() - top - 2);

    std::string title = " row " + std::to_string(state.row() + 1) + " of " +
                        std::to_string(state.rowCountHere()) + " ";
    if (schema == nullptr) title += "(unknown module type) ";
    screen.text(3, top, title, Attr::Bold);

    int y = top + 1;
    for (std::size_t c = 0; c < columns.size(); ++c) {
        if (y >= screen.height() - 3) break;
        const bool here = (c == state.column());
        screen.text(2, y, columns[c], here ? Attr::Reverse : Attr::Normal);

        const bool editing = here && state.mode() == Mode::Editing;
        const std::string value =
            editing ? "[" + state.editBuffer() + "]"
                    : state.document().cell(state.type(), state.row(), columns[c]);
        int x = screen.text(18, y, value, editing ? Attr::Bold : Attr::Normal);

        if (schema != nullptr) {
            if (const Column* col = schema->column(columns[c])) {
                if (col->type == ColumnType::Enum) {
                    for (const std::string& v : col->enumValues)
                        x = screen.text(x + 1, y, v, Attr::Dim);
                } else if (col->type == ColumnType::Reference) {
                    screen.text(x + 2, y, "-> " + col->referencedType, Attr::Dim);
                }
            }
        }
        ++y;
        if (const Diagnostic* d = state.diagnosticFor(columns[c])) {
            if (y < screen.height() - 3) {
                screen.text(18, y,
                            (d->severity == Severity::Error ? "error: " : "warning: ") +
                                d->message,
                            Attr::Error);
                ++y;
            }
        }
    }
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
    renderTabs(state, screen);
    // The grid gives up half the screen when the detail pane is up: a wide
    // table is for scanning, and the pane is where it becomes readable.
    const bool detail = state.mode() == Mode::Detail || state.mode() == Mode::Editing;
    const int gridBottom = detail ? (screen.height() / 2) : (screen.height() - 4);
    renderGrid(state, screen, 1, gridBottom);
    if (detail) renderDetail(state, screen, gridBottom + 1);
    screen.text(0, screen.height() - 2, state.status());
    if (state.mode() == Mode::Confirm)
        screen.text(0, screen.height() - 1,
                    "(s)ave and quit   (d)iscard and quit   (c)ancel", Attr::Bold);
    else
        screen.text(0, screen.height() - 1,
                    "^S save  ^R run  ^N new  ^D delete  ^Z undo  Tab pane  q quit",
                    Attr::Dim);
}

}  // namespace des
