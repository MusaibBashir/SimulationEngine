// ============================================================================
// tui/main.cpp  --  the des_tui command
// ============================================================================
// The loop, and nothing else. Everything it drives is a value that the test
// suite already exercises without a terminal, which is why this file is short
// and has no logic worth testing.

#include <iostream>
#include <memory>
#include <string>
#include "des_ui.hpp"
#include "Terminal.hpp"

using namespace des;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "usage: des_tui <model.des>\n";
        return 2;
    }
    TuiState state = TuiState::open(argv[1]);
    std::unique_ptr<ITerminal> terminal = openTerminal();

    for (;;) {
        const TerminalSize size = terminal->size();
        Screen screen(size.width, size.height);
        render(state, screen);
        terminal->present(screen);

        // Advance a running simulation instead of blocking on a key. Exactly
        // the shape v12's advance(budget) was designed for: do some work,
        // redraw, come back.
        //
        // keyPending() is what makes it interruptible. Without it the loop
        // advanced and continued without ever reading a key, so Escape was
        // never seen -- and a model with no stopping condition, which is every
        // model somebody has just built, could not be stopped at all.
        const bool busy = state.mode() == Mode::Running && state.running() != nullptr &&
                          (state.running()->state() == RunState::Ready ||
                           state.running()->state() == RunState::Running);
        if (busy && !terminal->keyPending()) {
            state.advanceRun();
            continue;
        }

        if (!handleKey(state, terminal->nextKey())) break;
    }
    // The terminal restores itself in its destructor, which is the only reason
    // it is an object rather than a pair of functions: a return from anywhere
    // in that loop must not leave the console in raw mode with no cursor.
    return 0;
}
