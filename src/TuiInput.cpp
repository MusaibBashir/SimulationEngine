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
    switch (key.ch) {
        case 's': return !state.save();       // stay open if the save failed
        case 'd': return false;
        case 'c': state.setOverlay(Overlay::None); state.setStatus(""); return true;
        default:  return true;
    }
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

bool handleFlow(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Up:       state.stepFlow(-1); break;
        case KeyKind::Down:     state.stepFlow(1); break;
        case KeyKind::PageUp:   state.stepFlow(-10); break;
        case KeyKind::PageDown: state.stepFlow(10); break;
        // Enter GOES THERE. A picture you cannot navigate from is a second
        // place to look rather than a way of getting around.
        case KeyKind::Enter:    state.gotoFlowBlock(); break;
        case KeyKind::Escape:   state.setView(View::Model); break;
        default: break;
    }
    return true;
}

bool handleRuns(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Up:     state.stepRun(-1); break;
        case KeyKind::Down:   state.stepRun(1); break;
        case KeyKind::Enter:  state.startRun(); break;
        case KeyKind::Escape: state.setView(View::Model); break;
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
            case View::Flow:    state.stepFlow(by); break;
            case View::Runs:    state.stepRun(by); break;
            case View::Results: state.scrollResults(by); break;
        }
        return true;
    }

    if (key.y == L.barRow) {
        const View views[4] = {View::Model, View::Flow, View::Runs, View::Results};
        for (int i = 0; i < 4; ++i)
            if (key.x >= L.tabStart[i] && key.x < L.tabEnd[i]) {
                state.setView(views[i]);
                return true;
            }
        return true;
    }

    if (state.view() != View::Model) {
        const int row = key.y - (L.bodyTop + 2);
        if (row < 0) return true;
        if (state.view() == View::Flow)      state.stepFlow(row - static_cast<long long>(state.flowCursor()));
        else if (state.view() == View::Runs) state.setRunIndex(static_cast<std::size_t>(row));
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

    if (key.kind == KeyKind::Function && key.ch == 1) { openKeyMap(state); return true; }

    if (key.kind == KeyKind::Ctrl) {
        switch (key.ch) {
            // The views.
            case 'B': state.setView(View::Model);   return true;
            case 'F': state.setView(View::Flow);    return true;
            case 'U': state.setView(View::Runs);    return true;
            case 'E': state.setView(View::Results); return true;

            case 'S': state.save();          return true;
            case 'Q': state.requestQuit();   return !state.wantsQuit();
            case 'K': openKeyMap(state);     return true;
            case 'J': state.goToFirstError(); return true;
            case 'R': state.startRun();      return true;
            case 'W': state.saveResults();   return true;
            case 'P': state.setView(View::Model); state.setFocus(Pane::Palette); return true;
            case 'N':
                if (state.view() == View::Runs) state.addRunBlock();
                return true;
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
            case 'Z': state.undo(); return true;
            case 'Y': state.redo(); return true;
            case 'X': state.cut();  return true;
            case 'C': state.copy(); return true;
            case 'V': state.paste(); return true;
            case 'A': state.buffer().selectAll(); return true;
            default:  return true;
        }
    }

    switch (state.view()) {
        case View::Flow:    return handleFlow(state, key);
        case View::Runs:    return handleRuns(state, key);
        case View::Results: return handleResults(state, key);
        case View::Model:   break;
    }
    if (state.focus() == Pane::Palette) return handlePalette(state, key);
    return handleEditor(state, key);
}

}  // namespace des
