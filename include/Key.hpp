// ============================================================================
// Key.hpp  --  v13: a keypress as a value; v15: a click is one too
// ============================================================================
// The platform layer's whole job on the input side is turning a
// KEY_EVENT_RECORD or an escape sequence into one of these. Everything above
// it takes a Key, which is what lets a test type into the UI without a
// terminal.
//
// v15 folds the mouse in here rather than adding a second event type. A click
// is something the user did, the input layer already dispatches on `kind`, and
// two streams would have to be merged somewhere anyway -- which is a race in
// every implementation that has ever tried it.

#pragma once
#include <cctype>

namespace des {

enum class KeyKind {
    Char, Enter, Escape, Backspace, Delete, Tab, BackTab,
    Up, Down, Left, Right, Home, End, PageUp, PageDown,
    Ctrl,
    Function,   // ch is 1..12
    Mouse,      // x, y and button are set
    Unknown
};

enum class MouseButton { None, Left, Right, Middle, WheelUp, WheelDown };

struct Key {
    KeyKind kind{KeyKind::Unknown};
    char    ch{0};      // Char: the character. Ctrl: the letter, upper case.

    // SHIFT IS SEPARATE from kind, and only for the movement keys. A text
    // editor needs Shift+Left to mean "extend the selection", and folding that
    // into KeyKind would double every arrow in every switch.
    bool    shift{false};

    // Mouse only. Screen coordinates, so a renderer's layout decides what was
    // hit -- the terminal layer does not know what a palette is.
    int         x{0};
    int         y{0};
    MouseButton button{MouseButton::None};

    static Key character(char c) { return Key{KeyKind::Char, c}; }

    // Upper case, so ^s and ^S are the same key rather than two that must both
    // be handled and can therefore disagree.
    static Key control(char c) {
        Key k;
        k.kind = KeyKind::Ctrl;
        k.ch   = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return k;
    }

    static Key special(KeyKind kind) { Key k; k.kind = kind; return k; }

    static Key shifted(KeyKind kind) {
        Key k; k.kind = kind; k.shift = true; return k;
    }

    static Key function(int n) {
        Key k; k.kind = KeyKind::Function; k.ch = static_cast<char>(n); return k;
    }

    static Key mouse(MouseButton button, int x, int y) {
        Key k;
        k.kind   = KeyKind::Mouse;
        k.button = button;
        k.x      = x;
        k.y      = y;
        return k;
    }

    bool isWheel() const {
        return kind == KeyKind::Mouse &&
               (button == MouseButton::WheelUp || button == MouseButton::WheelDown);
    }
};

}  // namespace des
