#include "TuiInput.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace des {
namespace {

void stepType(TuiState& state, int delta) {
    const std::vector<std::string>& types = state.types();
    if (types.empty()) return;
    auto it = std::find(types.begin(), types.end(), state.type());
    long long i = (it == types.end()) ? 0 : (it - types.begin());
    i += delta;
    if (i < 0) i = static_cast<long long>(types.size()) - 1;
    if (i >= static_cast<long long>(types.size())) i = 0;
    state.setType(types[static_cast<std::size_t>(i)]);
}

// Clamping rather than wrapping, everywhere. A cursor that wraps from the last
// row to the first looks like the list jumped, and in a document where row
// order is semantic that is exactly the wrong thing to make ambiguous.
void stepRow(TuiState& state, long long delta) {
    const long long rows = static_cast<long long>(state.rowCountHere());
    if (rows == 0) return;
    long long r = static_cast<long long>(state.row()) + delta;
    r = std::max<long long>(0, std::min(r, rows - 1));
    state.setRow(static_cast<std::size_t>(r));
}

void stepColumn(TuiState& state, long long delta) {
    const long long cols = static_cast<long long>(state.columnsHere().size());
    if (cols == 0) return;
    long long c = static_cast<long long>(state.column()) + delta;
    c = std::max<long long>(0, std::min(c, cols - 1));
    state.setColumn(static_cast<std::size_t>(c));
}

bool handleGrid(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Up:       stepRow(state, -1); break;
        case KeyKind::Down:     stepRow(state, 1); break;
        case KeyKind::PageUp:   stepRow(state, -10); break;
        case KeyKind::PageDown: stepRow(state, 10); break;
        case KeyKind::Home:     state.setRow(0); break;
        case KeyKind::End:
            if (state.rowCountHere() > 0) state.setRow(state.rowCountHere() - 1);
            break;
        case KeyKind::Tab:      stepType(state, 1); break;
        case KeyKind::BackTab:  stepType(state, -1); break;
        case KeyKind::Left:     stepType(state, -1); break;
        case KeyKind::Right:    stepType(state, 1); break;
        case KeyKind::Enter:    state.setMode(Mode::Detail); break;
        case KeyKind::Char:
            // Unsaved work is asked about, never discarded on one keystroke.
            // The whole point of a UI over a file format is that a person
            // trusts it with work that exists nowhere else.
            if (key.ch == 'q') {
                if (!state.dirty()) return false;
                state.setMode(Mode::Confirm);
                state.setStatus("unsaved changes -- (s)ave and quit, "
                                "(d)iscard and quit, (c)ancel");
            } else if (key.ch == '?') {
                state.setMode(Mode::Help);
            }
            break;
        default: break;
    }
    return true;
}

bool handleConfirm(TuiState& state, Key key) {
    if (key.kind != KeyKind::Char) {
        if (key.kind == KeyKind::Escape) state.setMode(Mode::Grid);
        return true;
    }
    switch (key.ch) {
        case 's': return !state.save();     // stay open if the save failed
        case 'd': return false;
        case 'c': state.setMode(Mode::Grid); state.setStatus(""); return true;
        default:  return true;
    }
}

bool handleDetail(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Up:     stepColumn(state, -1); break;
        case KeyKind::Down:   stepColumn(state, 1); break;
        case KeyKind::Escape: state.setMode(Mode::Grid); break;
        // ONE key for both. Arena has no separate "open the drop-down": you
        // click the cell and either a list appears or a caret does, and which
        // it is depends on the column rather than on which key you knew.
        case KeyKind::Enter:  if (!state.beginPick()) state.beginEdit(); break;
        default: break;
    }
    return true;
}

bool handlePicking(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Up:       state.stepPick(-1); break;
        case KeyKind::Down:     state.stepPick(1); break;
        case KeyKind::PageUp:   state.stepPick(-10); break;
        case KeyKind::PageDown: state.stepPick(10); break;
        case KeyKind::Enter:    state.commitPick(); break;
        // Escape TYPES, it does not cancel. The one thing that must stay easy
        // is naming a block before that block exists -- Serve -> Out while Out
        // is still an idea -- and a list cannot offer that. The second Escape,
        // out of the editor, is the one that abandons.
        case KeyKind::Escape:   state.typeInstead(); break;
        default: break;
    }
    return true;
}

bool handleEditing(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Char:      state.typeEdit(key.ch); break;
        case KeyKind::Backspace: state.backspaceEdit(); break;
        case KeyKind::Enter:     state.commitEdit(); break;
        case KeyKind::Escape:    state.cancelEdit(); break;
        default: break;
    }
    return true;
}

}  // namespace

bool handleKey(TuiState& state, Key key) {
    // Editing swallows everything, including the control keys. Otherwise ^S
    // typed into a Service field would save a half-finished expression, and a
    // person editing text expects the text to receive their keys.
    if (state.mode() == Mode::Editing) return handleEditing(state, key);
    // Picking swallows them for a different reason: ^N would add a row and move
    // the cursor onto it, and the open list would then write its answer into a
    // cell nobody was looking at.
    if (state.mode() == Mode::Picking) return handlePicking(state, key);

    if (state.mode() == Mode::Flow) {
        switch (key.kind) {
            case KeyKind::Up:     state.stepFlow(-1); return true;
            case KeyKind::Down:   state.stepFlow(1);  return true;
            case KeyKind::PageUp: state.stepFlow(-10); return true;
            case KeyKind::PageDown: state.stepFlow(10); return true;
            // Enter GOES THERE. A picture you cannot navigate from is a
            // second place to look rather than a way of getting around.
            case KeyKind::Enter:  state.gotoFlowBlock(); return true;
            default: break;
        }
        if (key.kind == KeyKind::Char && key.ch == 'q') return false;
        state.setMode(Mode::Grid);
        return true;
    }

    // ANY key closes the key map, control keys included. A screen listing the
    // keys, that you then have to work out how to leave, has undone its own
    // job. q still quits, because a person who opened it by accident on their
    // way out should not be trapped.
    if (state.mode() == Mode::Help) {
        if (key.kind == KeyKind::Char && key.ch == 'q') return false;
        state.setMode(Mode::Grid);
        return true;
    }
    // From the grid or the detail pane alike, because "what are the keys" is
    // not a question that waits until you are back at the top.
    if (key.kind == KeyKind::Char && key.ch == '?' &&
        state.mode() != Mode::Confirm && state.mode() != Mode::Running) {
        state.setMode(Mode::Help);
        return true;
    }

    if (key.kind == KeyKind::Ctrl) {
        switch (key.ch) {
            case 'S': state.save();      return true;
            case 'N': state.addRow();    return true;
            case 'D': state.removeRow(); return true;
            case 'R': state.startRun(); return true;
            case 'T': state.insertStarter(); return true;
            case 'F': state.openFlow();      return true;
            case 'Z': state.undo();      return true;
            default:  return true;
        }
    }
    if (state.mode() == Mode::Running) {
        if (key.kind == KeyKind::Escape) state.stopRun();
        return true;
    }
    if (state.mode() == Mode::Confirm) return handleConfirm(state, key);
    if (state.mode() == Mode::Detail)  return handleDetail(state, key);
    return handleGrid(state, key);
}

}  // namespace des
