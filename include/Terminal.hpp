// ============================================================================
// Terminal.hpp  --  v13: the only platform code in the project
// ============================================================================
// As small as it can be. Everything else about the UI is a value that a test
// can build and inspect; this is the part that cannot be. The implementations
// live in tui/ rather than src/ so that the portable half -- which is all of the
// rest -- goes through verify.sh and both sanitisers unchanged.
//
// v15.1 added what a program people are SENT needs from its platform: to know
// whether it is in a console at all, where a person's documents live, whether
// its window will vanish the moment it exits, and what to call that window.

#pragma once
#include <memory>
#include <string>
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

    // The window's title. Several open at once all said the same thing, and a
    // person with two models open could not tell which window was which.
    virtual void setTitle(const std::string& title) = 0;
};

// NULL, with the reason in whyNot, when this cannot be a terminal UI -- input
// or output redirected, or a Windows console too old to understand the escape
// sequences everything is drawn with. Raw mode on something that is not a
// console used to "succeed", and the loop then read nothing forever at full
// speed, which from outside looks exactly like a hang.
std::unique_ptr<ITerminal> openTerminal(std::string& whyNot);

// Where a model goes when the program is started with no file -- which is how
// everyone who double-clicks it starts it. On Windows that is the user's real
// Documents folder, found the way Windows finds it: on a great many laptops it
// has been moved into OneDrive, and %USERPROFILE%\Documents is then an empty
// folder nobody would think to look in.
std::string defaultModelPath();

// True when this program is the ONLY one attached to its console, which is what
// double-clicking it from Explorer means: the window closes the instant the
// program exits, taking any message printed on the way out with it.
bool launchedOnOwnConsole();

}  // namespace des
