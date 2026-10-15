#include "TuiInput.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include "TuiRender.hpp"

namespace des {
namespace {

// The key map is the Help overlay with no title. One overlay slot, because
// only one of them is ever up, and two would need a rule about which wins.
//
// CLEARING THE TITLE IS THE WHOLE THING. Without it, F1 after any ^G showed
// that field's help again -- the title was still set, so "no title" was never
// true and the key map could not be reached a second time.
void openKeyMap(TuiState& state) {
    state.clearHelp();
    state.setOverlay(Overlay::Help);
    state.setStatus("");
}

bool isKeyMap(const TuiState& state) {
    return state.overlay() == Overlay::Help && state.helpTitle().empty();
}

// --- overlays ---------------------------------------------------------------

bool handleConfirm(TuiState& state, Key key) {
    if (key.kind != KeyKind::Char) {
        if (key.kind == KeyKind::Escape) {
            state.setOverlay(Overlay::None);
            state.setStatus("");
        }
        return true;
    }
    const auto close = [&state] { state.setOverlay(Overlay::None); state.setStatus(""); };
    switch (state.confirmFor()) {
        case ConfirmFor::Quit:
            switch (key.ch) {
                case 's': return !state.save();       // stay open if the save failed
                case 'd': return false;
                case 'c': close(); return true;
                default:  return true;
            }
        case ConfirmFor::DeleteRun:
            if (key.ch == 'y') { state.setOverlay(Overlay::None); state.deleteRun(state.runIndex()); }
            else if (key.ch == 'n') close();
            return true;
        case ConfirmFor::NewFile:
        case ConfirmFor::OpenExisting:
        case ConfirmFor::ResizeTable:
        case ConfirmFor::DeleteTable:
        case ConfirmFor::RenameTable:
            state.answerConfirm(key.ch);
            return true;
    }
    return true;
}

bool handlePicker(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Up:       state.stepPicker(-1); break;
        case KeyKind::Down:     state.stepPicker(1); break;
        case KeyKind::PageUp:   state.stepPicker(-10); break;
        case KeyKind::PageDown: state.stepPicker(10); break;
        case KeyKind::Enter:    state.commitPicker(); break;
        // Escape TYPES, it does not cancel. The one thing that must stay easy
        // is naming a block before that block exists -- Serve -> Out while Out
        // is still an idea -- and a list cannot offer that. Closing the list
        // leaves the cursor on the line, ready to be typed into.
        case KeyKind::Escape:
            state.cancelPicker();
            state.setStatus("type it instead -- the name does not have to exist yet");
            break;
        case KeyKind::Mouse:
            if (key.button == MouseButton::WheelUp)   state.stepPicker(-1);
            if (key.button == MouseButton::WheelDown) state.stepPicker(1);
            break;
        default: break;
    }
    return true;
}

bool handlePrompt(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Char:      state.typePrompt(key.ch); break;
        case KeyKind::Backspace: state.backspacePrompt(); break;
        case KeyKind::Enter:     state.commitPrompt(); break;
        case KeyKind::Escape:    state.cancelPrompt(); break;
        default: break;
    }
    return true;
}

// v17: the Ask panel. Ordinary typing goes into the box -- including what the
// operating system's dictation types, which arrives as keys like any other.
bool handleAsk(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Char:      state.typeAsk(key.ch); break;
        case KeyKind::Tab:       state.typeAsk(' '); break;
        case KeyKind::Backspace: state.backspaceAsk(); break;
        case KeyKind::Enter:     state.commitAsk(); break;
        case KeyKind::Escape:    state.escapeAsk(); break;
        case KeyKind::Function:
            if (key.ch == 2) state.escapeAsk();      // F2 again closes it
            break;
        case KeyKind::Ctrl:
            switch (key.ch) {
                case 'K': state.beginKeyEntry(); break;
                case 'V': state.pasteIntoAsk(state.clipboard()); break;
                case 'U': state.clearAsk(); break;
                // Quitting from inside the panel still asks about unsaved work.
                case 'Q': state.closeAsk(); state.requestQuit(); return !state.wantsQuit();
                default: break;
            }
            break;
        default: break;
    }
    return true;
}

bool handleRunning(TuiState& state, Key key) {
    if (key.kind == KeyKind::Escape) state.stopRun();
    return true;
}

// --- the editor -------------------------------------------------------------

bool handleEditor(TuiState& state, Key key) {
    TextBuffer& buffer = state.buffer();
    const bool extend = key.shift;
    switch (key.kind) {
        case KeyKind::Char:      state.typeChar(key.ch); break;
        case KeyKind::Enter:     state.newline(); break;
        case KeyKind::Backspace: state.backspace(); break;
        case KeyKind::Delete:    state.del(); break;
        case KeyKind::Up:        buffer.moveBy(-1, 0, extend); break;
        case KeyKind::Down:      buffer.moveBy(1, 0, extend); break;
        case KeyKind::Left:      buffer.moveBy(0, -1, extend); break;
        case KeyKind::Right:     buffer.moveBy(0, 1, extend); break;
        case KeyKind::PageUp:    buffer.moveBy(-15, 0, extend); break;
        case KeyKind::PageDown:  buffer.moveBy(15, 0, extend); break;
        case KeyKind::Home:      buffer.moveToLineStart(extend); break;
        case KeyKind::End:       buffer.moveToLineEnd(extend); break;
        case KeyKind::Tab:       state.setFocus(Pane::Palette); break;
        case KeyKind::BackTab:   state.setFocus(Pane::Palette); break;
        case KeyKind::Escape:
            if (buffer.hasSelection()) buffer.clearSelection();
            else state.setStatus("");
            break;
        default: break;
    }
    return true;
}

bool handlePalette(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Up:       state.stepPalette(-1); break;
        case KeyKind::Down:     state.stepPalette(1); break;
        case KeyKind::PageUp:   state.stepPalette(-10); break;
        case KeyKind::PageDown: state.stepPalette(10); break;
        case KeyKind::Home:     state.setPaletteIndex(0); break;
        case KeyKind::Enter:    state.insertSelectedModule(); break;
        case KeyKind::Tab:
        case KeyKind::BackTab:
        case KeyKind::Escape:   state.setFocus(Pane::Editor); break;
        case KeyKind::Char:
            // A letter JUMPS to the next module starting with it. Seventeen
            // types is a long way to arrow through, and this is what every
            // list in every file manager has always done.
            if (key.ch == '?' || key.ch == 'h') { state.showModuleHelp(); break; }
            for (std::size_t i = 1; i <= state.palette().size(); ++i) {
                const std::size_t at =
                    (state.paletteIndex() + i) % state.palette().size();
                const char first = state.palette()[at].empty()
                                       ? '\0' : state.palette()[at][0];
                if (std::tolower(static_cast<unsigned char>(first)) ==
                    std::tolower(static_cast<unsigned char>(key.ch))) {
                    state.setPaletteIndex(at);
                    break;
                }
            }
            break;
        default: break;
    }
    return true;
}

// v16.2: move the graph's view by (dx, dy) cells, from wherever it is now.
void panFlow(TuiState& state, int dx, int dy, int width, int height) {
    const FlowViewport v = flowViewportFor(state, layoutFor(state, width, height));
    if (!v.shown) return;
    state.setFlowView(std::max(0, std::min(v.x + dx, std::max(0, v.width - v.cols))),
                      std::max(0, std::min(v.y + dy, std::max(0, v.height - v.rows))));
}

bool handleFlow(TuiState& state, Key key, int width, int height) {
    const int step = std::max(8, (width - 2) / 3);
    switch (key.kind) {
        case KeyKind::Left:     panFlow(state, -step, 0, width, height); return true;
        case KeyKind::Right:    panFlow(state, step, 0, width, height); return true;
        case KeyKind::Home:     state.followFlowCursor(); return true;
        default: break;
    }
    switch (key.kind) {
        case KeyKind::Up:       state.stepFlow(-1); break;
        case KeyKind::Down:     state.stepFlow(1); break;
        case KeyKind::PageUp:   state.stepFlow(-10); break;
        case KeyKind::PageDown: state.stepFlow(10); break;
        // Enter GOES THERE. A picture you cannot navigate from is a second
        // place to look rather than a way of getting around.
        case KeyKind::Enter:    state.gotoFlowBlock(); break;
        case KeyKind::Escape:   state.setView(View::Model); break;
        case KeyKind::Char:
            if (key.ch == 'l' || key.ch == 'L') state.setFlowAsList(!state.flowAsList());
            if (key.ch == 'p' || key.ch == 'P') state.saveFlowPicture();
            break;
        default: break;
    }
    return true;
}

bool handleRunForm(TuiState& state, Key key) {
    if (state.editingRunField()) {
        switch (key.kind) {
            case KeyKind::Char:      state.typeRunField(key.ch); break;
            case KeyKind::Backspace: state.backspaceRunField(); break;
            case KeyKind::Enter:     state.commitRunFieldEdit(); break;
            case KeyKind::Escape:    state.cancelRunFieldEdit(); break;
            default: break;
        }
        return true;
    }
    switch (key.kind) {
        case KeyKind::Up:      state.stepRunField(-1); break;
        case KeyKind::Down:    state.stepRunField(1); break;
        case KeyKind::Home:    state.setRunField(0); break;
        case KeyKind::Enter:   state.beginRunFieldEdit(); break;
        case KeyKind::Tab:
        case KeyKind::BackTab:
        case KeyKind::Left:
        case KeyKind::Escape:  state.setRunsFocus(RunsFocus::List); break;
        case KeyKind::Char:
            if (key.ch == '?' || key.ch == 'h') state.showRunFieldHelp();
            break;
        default: break;
    }
    return true;
}

bool handleRuns(TuiState& state, Key key) {
    if (state.runsFocus() == RunsFocus::Form && !state.runs().empty())
        return handleRunForm(state, key);
    switch (key.kind) {
        case KeyKind::Up:     state.stepRun(-1); break;
        case KeyKind::Down:   state.stepRun(1); break;
        case KeyKind::Enter:  state.startRun(); break;
        case KeyKind::Delete: state.requestDeleteRun(); break;
        case KeyKind::Tab:
        case KeyKind::BackTab:
        case KeyKind::Right:
            if (!state.runs().empty()) state.setRunsFocus(RunsFocus::Form);
            break;
        case KeyKind::Escape: state.setView(View::Model); break;
        case KeyKind::Char:
            if (key.ch == 'a' || key.ch == 'A') state.addRunBlock();
            break;
        default: break;
    }
    return true;
}

bool handleResults(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Up:       state.scrollResults(-1); break;
        case KeyKind::Down:     state.scrollResults(1); break;
        case KeyKind::PageUp:   state.scrollResults(-15); break;
        case KeyKind::PageDown: state.scrollResults(15); break;
        case KeyKind::Home:     state.scrollResults(-1000000); break;
        case KeyKind::Escape:   state.setView(View::Model); break;
        default: break;
    }
    return true;
}

// v16.1: the Tables tab. Tab moves between the three panels whenever no cell
// is being edited; while one is, Tab and Enter keep it and move on, which is
// what a block pasted into the terminal from Excel arrives as.
bool handleTables(TuiState& state, Key key) {
    const auto nextPanel = [&state](int by) {
        const TablesFocus order[3] = {TablesFocus::Kinds, TablesFocus::Editor, TablesFocus::List};
        int at = 0;
        for (int i = 0; i < 3; ++i) if (order[i] == state.tablesFocus()) at = i;
        state.setTablesFocus(order[(at + by + 3) % 3]);
    };
    switch (state.tablesFocus()) {
        case TablesFocus::Kinds:
            switch (key.kind) {
                case KeyKind::Up:     state.stepKind(-1); break;
                case KeyKind::Down:   state.stepKind(1); break;
                case KeyKind::Enter:  state.createTable(); break;
                case KeyKind::Tab:    nextPanel(1); break;
                case KeyKind::BackTab: nextPanel(-1); break;
                case KeyKind::Escape: state.setView(View::Model); break;
                case KeyKind::Char:
                    if (key.ch == '?' || key.ch == 'h') state.showTableHelp();
                    break;
                default: break;
            }
            return true;
        case TablesFocus::List:
            switch (key.kind) {
                case KeyKind::Up:     state.stepTable(-1); break;
                case KeyKind::Down:   state.stepTable(1); break;
                case KeyKind::Enter:
                case KeyKind::Left:   state.setTablesFocus(TablesFocus::Editor); break;
                case KeyKind::Delete: state.requestDeleteTable(); break;
                case KeyKind::Tab:    nextPanel(1); break;
                case KeyKind::BackTab: nextPanel(-1); break;
                case KeyKind::Escape: state.setView(View::Model); break;
                default: break;
            }
            return true;
        case TablesFocus::Editor:
            break;
    }

    if (state.editingTable()) {
        switch (key.kind) {
            case KeyKind::Char:      state.typeTable(key.ch); break;
            case KeyKind::Backspace: state.backspaceTable(); break;
            case KeyKind::Enter:     state.tableEnter(); break;
            case KeyKind::Tab:       state.tableTab(); break;
            case KeyKind::Escape:    state.cancelTableEdit(); break;
            case KeyKind::Up:        state.moveTableCursor(-1, 0); break;
            case KeyKind::Down:      state.moveTableCursor(1, 0); break;
            case KeyKind::Left:      state.moveTableCursor(0, -1); break;
            case KeyKind::Right:     state.moveTableCursor(0, 1); break;
            default: break;
        }
        return true;
    }
    switch (key.kind) {
        case KeyKind::Up:        state.moveTableCursor(-1, 0); break;
        case KeyKind::Down:      state.moveTableCursor(1, 0); break;
        case KeyKind::Left:      state.moveTableCursor(0, -1); break;
        case KeyKind::Right:     state.moveTableCursor(0, 1); break;
        case KeyKind::PageUp:    state.moveTableCursor(-15, 0); break;
        case KeyKind::PageDown:  state.moveTableCursor(15, 0); break;
        case KeyKind::Enter:     state.tableEnter(); break;
        case KeyKind::Backspace: state.backspaceTable(); break;
        case KeyKind::Delete:    state.deleteTableRow(); break;
        case KeyKind::Tab:       nextPanel(1); break;
        case KeyKind::BackTab:   nextPanel(-1); break;
        case KeyKind::Escape:    state.setTablesFocus(TablesFocus::List); break;
        case KeyKind::Char:
            if (key.ch == '?') { state.showTableHelp(); break; }
            state.typeTable(key.ch);
            break;
        default: break;
    }
    return true;
}

// --- the mouse --------------------------------------------------------------

// A click is turned back into "the third palette entry" by the SAME arithmetic
// that drew it there, which is why layoutFor() is shared rather than copied.
bool handleMouse(TuiState& state, Key key, int width, int height) {
    const Layout L = layoutFor(state, width, height);

    if (key.button == MouseButton::WheelUp || key.button == MouseButton::WheelDown) {
        const long long by = key.button == MouseButton::WheelUp ? -3 : 3;
        switch (state.view()) {
            case View::Model:
                if (key.x < L.dividerX) state.stepPalette(by);
                else                    state.buffer().moveBy(by, 0);
                break;
            case View::Flow:
                // Shift+wheel -- and a sideways wheel, which arrives as one --
                // pans; the plain wheel still walks the blocks.
                if (key.shift) panFlow(state, static_cast<int>(by) * 4, 0, width, height);
                else           state.stepFlow(by);
                break;
            case View::Runs:    state.stepRun(by); break;
            case View::Results: state.scrollResults(by); break;
            case View::Tables:
                if (key.x >= L.tablesListX) state.stepTable(by);
                else if (key.x < L.tablesEditorX - 2) state.stepKind(by);
                else state.moveTableCursor(by, 0);
                break;
        }
        return true;
    }

    if (key.y == L.barRow) {
        for (int i = 0; i < TAB_COUNT; ++i)
            if (key.x >= L.tabStart[i] && key.x < L.tabEnd[i]) {
                state.setView(tabView(i));
                return true;
            }
        if (key.button == MouseButton::Left && key.x >= L.askStart && key.x < L.askEnd)
            state.openAsk();
        return true;
    }

    if (state.view() == View::Tables) {
        if (key.x < L.tablesEditorX - 2) {
            const int row = key.y - (L.bodyTop + 2);
            if (row >= 0 && row < 4) {
                const bool again = state.tablesFocus() == TablesFocus::Kinds &&
                                   state.kindIndex() == static_cast<std::size_t>(row);
                state.setTablesFocus(TablesFocus::Kinds);
                state.setKindIndex(static_cast<std::size_t>(row));
                if (again || key.button == MouseButton::Left) state.createTable();
            }
            return true;
        }
        if (key.x >= L.tablesListX) {
            const int row = key.y - (L.bodyTop + 2);
            if (row >= 0) {
                state.setTablesFocus(TablesFocus::List);
                state.selectTable(static_cast<std::size_t>(row));
            }
            return true;
        }
        // The middle: a header field, or a cell. A second click edits.
        const TableGrid g = tableGridFor(state, L);
        const Table* t = state.currentTable();
        if (t == nullptr) return true;
        const std::size_t fields = state.headerFields().size();
        const int f = key.y - g.fieldTop;
        if (f >= 0 && f < static_cast<int>(fields)) {
            const bool again = state.tablesFocus() == TablesFocus::Editor && !state.onGrid() &&
                               state.tableField() == static_cast<std::size_t>(f);
            state.setTablesFocus(TablesFocus::Editor);
            state.setTableCursor(static_cast<std::size_t>(f), 0, 0);
            if (again) state.beginTableEdit(true);
            return true;
        }
        const int r = key.y - g.gridTop;
        const int c = (key.x - L.tablesEditorX - g.labelWidth) / g.cellWidth;
        if (r >= 0 && r < g.visibleRows && key.x >= L.tablesEditorX + g.labelWidth && c >= 0) {
            const std::size_t row = g.firstRow + static_cast<std::size_t>(r);
            const std::size_t col = g.firstColumn + static_cast<std::size_t>(c);
            if (row >= t->rows || col >= t->columns) return true;
            const bool again = state.tablesFocus() == TablesFocus::Editor && state.onGrid() &&
                               state.cellRow() == row && state.cellColumn() == col;
            state.setTablesFocus(TablesFocus::Editor);
            state.setTableCursor(fields, row, col);
            if (again) state.beginTableEdit(true);
        }
        return true;
    }

    if (state.view() != View::Model) {
        const int row = key.y - (L.bodyTop + 2);
        if (row < 0) return true;
        if (state.view() == View::Flow) {
            const FlowViewport v = flowViewportFor(state, L);
            if (!v.shown) {
                state.stepFlow(row - static_cast<long long>(state.flowCursor()));
                return true;
            }
            const int step = std::max(8, v.cols / 3);
            // The horizontal slider: an end steps, the track jumps there.
            if (v.hSlider && key.y == v.hRow && key.x >= v.hLeft && key.x <= v.hRight) {
                if (key.x == v.hLeft)       panFlow(state, -step, 0, width, height);
                else if (key.x == v.hRight) panFlow(state, step, 0, width, height);
                else {
                    const int track = v.hRight - v.hLeft - 1;
                    const int at = (key.x - v.hLeft - 1) * v.width / std::max(1, track) - v.cols / 2;
                    state.setFlowView(std::max(0, std::min(at, v.width - v.cols)), v.y);
                }
                return true;
            }
            if (v.vSlider && key.x == v.vColumn && key.y >= v.vTop && key.y <= v.vBottom) {
                const int vstep = std::max(3, v.rows / 3);
                if (key.y == v.vTop)         panFlow(state, 0, -vstep, width, height);
                else if (key.y == v.vBottom) panFlow(state, 0, vstep, width, height);
                else {
                    const int track = v.vBottom - v.vTop - 1;
                    const int at = (key.y - v.vTop - 1) * v.height / std::max(1, track) - v.rows / 2;
                    state.setFlowView(v.x, std::max(0, std::min(at, v.height - v.rows)));
                }
                return true;
            }
            // A click on a box chooses that block.
            const int gx = v.x + key.x - 1, gy = v.y + key.y - v.top;
            const AsciiGraph* g = state.flowGraph();
            const std::vector<FlowBlock> blocks = flowOf(state.document());
            for (std::size_t i = 0; g != nullptr && i < blocks.size(); ++i) {
                const auto it = g->boxes.find(blocks[i].name);
                if (it == g->boxes.end()) continue;
                const CellRect& b = it->second;
                if (gx >= b.left && gx <= b.right && gy >= b.top && gy <= b.bottom) {
                    const bool again = i == state.flowCursor();
                    const int keepX = v.x, keepY = v.y;
                    state.stepFlow(static_cast<long long>(i) - static_cast<long long>(state.flowCursor()));
                    // Clicking must not make the picture jump under the mouse.
                    state.setFlowView(keepX, keepY);
                    if (again) state.gotoFlowBlock();
                    return true;
                }
            }
            return true;
        }
        else if (state.view() == View::Runs) {
            const std::size_t at = static_cast<std::size_t>(row);
            if (key.x < L.runsFormX - 2) {
                state.setRunsFocus(RunsFocus::List);
                state.setRunIndex(at);
            } else if (at < state.runFields().size() && !state.runs().empty()) {
                // A SECOND click on the selected field edits it. The terminal
                // reports clicks, not double-clicks, so this is the nearest
                // honest equivalent.
                const bool again = state.runsFocus() == RunsFocus::Form &&
                                   state.runField() == at;
                state.setRunsFocus(RunsFocus::Form);
                state.setRunField(at);
                if (again && !state.editingRunField()) state.beginRunFieldEdit();
            }
        }
        return true;
    }

    if (key.y >= L.paletteTop && key.y <= L.bodyBottom && key.x < L.dividerX) {
        const std::size_t at = L.paletteFirst +
                               static_cast<std::size_t>(key.y - L.paletteTop);
        if (at >= state.palette().size()) return true;
        state.setFocus(Pane::Palette);
        state.setPaletteIndex(at);
        // LEFT INSERTS, RIGHT EXPLAINS. Clicking a module and having it appear
        // is what was asked for; a click that only highlights would need a
        // second one for every insertion.
        if (key.button == MouseButton::Left)       state.insertSelectedModule();
        else if (key.button == MouseButton::Right) state.showModuleHelp();
        return true;
    }

    if (key.y >= L.textTop && key.y < L.textTop + L.textRows && key.x >= L.textX) {
        state.setFocus(Pane::Editor);
        const std::size_t line = L.firstLine + static_cast<std::size_t>(key.y - L.textTop);
        const std::size_t col  = L.firstColumn + static_cast<std::size_t>(key.x - L.textX);
        state.buffer().moveTo(Caret{line, col});
        if (key.button == MouseButton::Right) state.showFieldHelp();
        return true;
    }
    return true;
}

}  // namespace

bool handleKey(TuiState& state, Key key, int width, int height) {
    // A MOUSE EVENT FIRST, and before the overlays, because a click on the top
    // bar means the same thing wherever the keyboard happens to be -- except
    // while an overlay is up, which owns the screen it is drawn over.
    if (key.kind == KeyKind::Mouse) {
        if (state.overlay() == Overlay::Picker) return handlePicker(state, key);
        if (state.overlay() != Overlay::None)   return true;
        return handleMouse(state, key, width, height);
    }

    // The key map closes on ANY key, control keys included. A screen listing
    // the keys that you then have to work out how to leave has undone its own
    // job.
    if (isKeyMap(state)) {
        state.setOverlay(Overlay::None);
        return true;
    }
    if (state.overlay() == Overlay::Help) {
        state.setOverlay(Overlay::None);
        return true;
    }
    if (state.overlay() == Overlay::Confirm) return handleConfirm(state, key);
    if (state.overlay() == Overlay::Picker)  return handlePicker(state, key);
    if (state.overlay() == Overlay::Running) return handleRunning(state, key);
    if (state.overlay() == Overlay::Prompt)  return handlePrompt(state, key);
    if (state.overlay() == Overlay::Ask)     return handleAsk(state, key);

    if (key.kind == KeyKind::Function && key.ch == 1) { openKeyMap(state); return true; }
    if (key.kind == KeyKind::Function && key.ch == 2) { state.openAsk(); return true; }

    if (key.kind == KeyKind::Ctrl) {
        switch (key.ch) {
            // The views.
            case 'B': state.setView(View::Model);   return true;
            case 'F': state.setView(View::Flow);    return true;
            case 'U': state.setView(View::Runs);    return true;
            case 'E': state.setView(View::Results); return true;
            case 'D': state.setView(View::Tables);  return true;
            case 'O':
                if (state.view() == View::Tables) state.insertTableRow();
                return true;

            case 'S': state.save();          return true;
            case 'Q': state.requestQuit();   return !state.wantsQuit();
            case 'K': openKeyMap(state);     return true;
            case 'J': state.goToFirstError(); return true;
            case 'R': state.startRun();      return true;
            case 'W': state.saveResults();   return true;
            case 'P': state.setView(View::Model); state.setFocus(Pane::Palette); return true;
            case 'N': state.requestNewFile(); return true;
            case 'T':
                if (state.buffer().lineCount() == 1 && state.buffer().lineAt(0).empty()) {
                    // insertText, NOT setText: setText clears the undo stack,
                    // which made the one key that writes the most text the
                    // only one you could not take back.
                    state.insertText(starterModelText());
                    state.setStatus("a working single-server model -- ^R runs it");
                } else {
                    state.setStatus("^T writes into an EMPTY file, and this one "
                                    "has text in it");
                }
                return true;

            // The editor's own keys. They do nothing anywhere else, rather
            // than something surprising.
            case 'G':
                if (state.view() != View::Model) return true;
                if (state.focus() == Pane::Palette) state.showModuleHelp();
                else                                state.showFieldHelp();
                return true;
            case 'L':
                if (state.view() == View::Model && state.focus() == Pane::Editor)
                    state.openPicker();
                return true;
            // Each tab undoes its own edits: the tables have their own history.
            case 'Z':
                if (state.view() == View::Tables) state.undoTables(); else state.undo();
                return true;
            case 'Y':
                if (state.view() == View::Tables) state.redoTables(); else state.redo();
                return true;
            case 'X': state.cut();  return true;
            case 'C': state.copy(); return true;
            case 'V':
                if (state.view() == View::Tables) state.pasteIntoTable(state.clipboard());
                else state.paste();
                return true;
            case 'A': state.buffer().selectAll(); return true;
            default:  return true;
        }
    }

    switch (state.view()) {
        case View::Flow:    return handleFlow(state, key, width, height);
        case View::Runs:    return handleRuns(state, key);
        case View::Results: return handleResults(state, key);
        case View::Tables:  return handleTables(state, key);
        case View::Model:   break;
    }
    if (state.focus() == Pane::Palette) return handlePalette(state, key);
    return handleEditor(state, key);
}

}  // namespace des
