// ============================================================================
// Terminal.hpp  --  v13: the only platform code in the project
// ============================================================================
// Three methods, deliberately. Everything else about the UI is a value that a
// test can build and inspect; this is the part that cannot be, so it is kept
// as small as it can possibly be. The implementations live in tui/ rather than
// src/ so that the portable half -- which is all of the rest -- goes through
// verify.sh and both sanitisers unchanged.

#pragma once
#include <memory>
#include "Key.hpp"
#include "Screen.hpp"

namespace des {

struct TerminalSize {
    int width{80};
    int height{24};
};

class ITerminal {
public:
    virtual ~ITerminal() = default;
    virtual TerminalSize size() const = 0;
    virtual void present(const Screen& screen) = 0;
    // Blocks. Returns KeyKind::Unknown for anything it does not recognise,
    // which the input layer ignores -- an unrecognised key must never be an
    // error, or a terminal sending an escape sequence nobody anticipated would
    // stop the program.
    virtual Key nextKey() = 0;

    // Is a keypress waiting? The run loop needs this: while a simulation is
    // advancing it must not block on nextKey(), or the run would only move
    // when somebody typed -- and a model with no stopping condition could
    // not be interrupted at all, because Escape would never be read.
    virtual bool keyPending() = 0;
};

std::unique_ptr<ITerminal> openTerminal();

}  // namespace des
