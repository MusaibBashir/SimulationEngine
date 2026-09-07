// ============================================================================
// Key.hpp  --  v13: a keypress as a value
// ============================================================================
// The platform layer's whole job on the input side is turning a
// KEY_EVENT_RECORD or an escape sequence into one of these. Everything above
// it takes a Key, which is what lets a test type into the UI without a
// terminal.

#pragma once
#include <cctype>

namespace des {

enum class KeyKind {
    Char, Enter, Escape, Backspace, Delete, Tab, BackTab,
    Up, Down, Left, Right, Home, End, PageUp, PageDown, Ctrl, Unknown
};

struct Key {
    KeyKind kind{KeyKind::Unknown};
    char    ch{0};      // Char: the character. Ctrl: the letter, upper case.

    static Key character(char c) { return Key{KeyKind::Char, c}; }

    // Upper case, so ^s and ^S are the same key rather than two that must both
    // be handled and can therefore disagree.
    static Key control(char c) {
        return Key{KeyKind::Ctrl,
                   static_cast<char>(std::toupper(static_cast<unsigned char>(c)))};
    }

    static Key special(KeyKind k) { return Key{k, 0}; }
};

}  // namespace des
