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
        case KeyKind::Enter:  state.beginEdit(); break;
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

    if (key.kind == KeyKind::Ctrl) {
        switch (key.ch) {
            case 'S': state.save();      return true;
            case 'N': state.addRow();    return true;
            case 'D': state.removeRow(); return true;
            case 'Z': state.undo();      return true;
            default:  return true;
        }
    }
    if (state.mode() == Mode::Confirm) return handleConfirm(state, key);
    if (state.mode() == Mode::Detail)  return handleDetail(state, key);
    return handleGrid(state, key);
}

}  // namespace des
