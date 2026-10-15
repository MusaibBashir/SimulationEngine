// ============================================================================
// tui/main.cpp  --  the des_tui command
// ============================================================================
// The loop, and what a program people are SENT has to do around it. Everything
// the loop drives is a value that the test suite already exercises without a
// terminal, which is why this file has no logic worth testing -- only the
// behaviour of being double-clicked by somebody who has never seen it.

#include <exception>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include "des_ui.hpp"
#include "CurlTransport.hpp"
#include "Recorder.hpp"
#include "Speech.hpp"
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
    std::error_code ignored;
    const bool existed = std::filesystem::exists(path, ignored);
    TuiState state = TuiState::open(path);
    const std::string folder = std::filesystem::path(defaultModelPath()).parent_path().string();
    state.setDefaultDirectory(folder.empty() ? std::string(".") : folder);
    state.setOpener([](const std::string& file) { return openWithDefaultApp(file); });

    // v17: F2. The settings file and the environment say which model and
    // which key; the request itself runs on a thread TuiState starts.
    state.setLlmConfig(LlmConfig::load());
    AskHooks hooks;
    hooks.generate = [](const std::string& text, const JsonValue& previous, const LlmConfig& config) {
        CurlTransport transport;
        return generateModel(text, previous, config, transport);
    };
    hooks.saveKey = [](const std::string& key, std::string& error) { return saveApiKey(key, error); };
    hooks.openMicrophone = [](std::string& why) { return openMicrophone(why); };
    hooks.transcribe = [](const std::string& wav, const LlmConfig& config) {
        CurlTransport transport(120);   // a minute of audio is ~2 MB to upload
        return transcribe(wav, config, transport);
    };
    state.setAskHooks(std::move(hooks));
    if (!chosen) {
        // A scratch model that was never saved has no name yet. Its first ^S
        // asks for one, rather than every new model becoming untitled.des.
        state.setUnnamed(!existed);
        state.setStatus(!existed ? "a new model -- ^S asks for its name   ^T writes a working one"
                                 : "reopened " + path);
    }

    // A window of its own opens maximized: a flowchart wants the room, and
    // a double-clicked model should not open in a postage stamp.
    if (launchedOnOwnConsole()) maximizeOwnWindow();

    std::string whyNot;
    std::unique_ptr<ITerminal> terminal = openTerminal(whyNot);
    if (!terminal) {
        std::cout << "DES Simulator cannot start: " << whyNot << "\n";
        holdWindowOpen();
        return 1;
    }
    std::string titled = state.path();
    terminal->setTitle("DES Simulator - " + fileNameOf(titled));

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
            // A question out to the language model: look for its answer
            // between keys, and redraw the moment it lands. The short sleep is
            // what keeps a 60-second wait from being a 60-second busy loop.
            if (state.askBusy()) {
                state.pollAsk();
                if (!terminal->keyPending()) {
                    if (state.askBusy()) std::this_thread::sleep_for(std::chrono::milliseconds(30));
                    continue;
                }
            }

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
            // ^N and a first save both change which file this is.
            if (state.path() != titled) {
                titled = state.path();
                terminal->setTitle("DES Simulator - " + fileNameOf(titled));
            }
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
