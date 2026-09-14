// ============================================================================
// tui/main.cpp  --  the des_tui command
// ============================================================================
// The loop, and what a program people are SENT has to do around it. Everything
// the loop drives is a value that the test suite already exercises without a
// terminal, which is why this file has no logic worth testing -- only the
// behaviour of being double-clicked by somebody who has never seen it.

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include "des_ui.hpp"
#include "Terminal.hpp"

using namespace des;

namespace {

// Double-clicked from Explorer, the console window closes the instant the
// program returns -- so a message printed on the way out is on screen for no
// time at all, and the person is left with a window that flashed and vanished.
// Wait for Enter, but only then: in a terminal somebody opened themselves, the
// window stays anyway and a pause would just be in the way.
void holdWindowOpen() {
    if (!launchedOnOwnConsole()) return;
    std::cout << "\nPress Enter to close this window." << std::flush;
    std::string ignored;
    std::getline(std::cin, ignored);
}

std::string fileNameOf(const std::string& path) {
    const std::size_t at = path.find_last_of("/\\");
    return at == std::string::npos ? path : path.substr(at + 1);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cout << "usage: " << fileNameOf(argv[0]) << " [model.des]\n"
                     "  With no file it opens untitled.des in Documents\\DES Models.\n";
        holdWindowOpen();
        return 2;
    }

    // NO ARGUMENT IS THE NORMAL CASE, not an error. It is what double-clicking
    // does, and the old behaviour -- print a usage line and exit -- was a window
    // that flashed and closed, which to somebody who has just been sent this
    // looks like a program that is broken. The same file every time, so the
    // model they were building is where they left it when they come back.
    const bool chosen = (argc == 2);
    const std::string path = chosen ? argv[1] : defaultModelPath();
    TuiState state = TuiState::open(path);
    if (!chosen) {
        const bool empty = state.buffer().lineCount() == 1 && state.buffer().lineAt(0).empty();
        state.setStatus(empty ? "a new model -- ^S saves it to " + path +
                                    "   ^T writes a working one"
                              : "reopened " + path);
    }

    std::string whyNot;
    std::unique_ptr<ITerminal> terminal = openTerminal(whyNot);
    if (!terminal) {
        std::cout << "DES Simulator cannot start: " << whyNot << "\n";
        holdWindowOpen();
        return 1;
    }
    terminal->setTitle("DES Simulator - " + fileNameOf(path));

    // Saves the text somewhere it will not be overwritten, then says where.
    // Shared by both catch clauses below so they cannot disagree.
    const auto rescue = [&state, &terminal](const std::string& what) {
        // The terminal FIRST: its destructor restores the console, and a
        // message printed while it is still in raw mode on the alternate
        // screen is gone the moment that screen is.
        terminal.reset();
        std::cout << "DES Simulator stopped unexpectedly:\n  " << what << "\n";
        if (state.dirty()) {
            const std::string to = state.path() + ".recovered";
            if (state.save(to))
                std::cout << "\nYour unsaved model was written to:\n  " << to << "\n";
            else
                std::cout << "\nYour unsaved model could NOT be written to:\n  " << to << "\n";
        }
        holdWindowOpen();
    };

    try {
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
            // never seen -- and a model with no stopping condition could not be
            // stopped at all.
            const bool busy = state.running() != nullptr &&
                              (state.running()->state() == RunState::Ready ||
                               state.running()->state() == RunState::Running);
            if (busy && !terminal->keyPending()) {
                state.advanceRun();
                continue;
            }

            // The SIZE goes with the key, because a click has to be turned back
            // into what was clicked using the layout that drew it -- and the
            // layout depends on how big the window is right now.
            if (!handleKey(state, terminal->nextKey(), size.width, size.height)) break;
            if (state.wantsQuit()) break;
        }
    } catch (const std::exception& e) {
        // A BACKSTOP, not the plan. A model that fails part way through a run
        // now fails the run with a message, and none of the failures found so
        // far reach here -- but "none found so far" is exactly the claim this
        // project has been wrong about, and the cost of being wrong here is
        // somebody's afternoon of work.
        rescue(e.what());
        return 1;
    } catch (...) {
        rescue("an error with no description");
        return 1;
    }
    // The terminal restores itself in its destructor, which is the only reason
    // it is an object rather than a pair of functions: a return from anywhere
    // in that loop must not leave the console in raw mode with no cursor.
    return 0;
}
