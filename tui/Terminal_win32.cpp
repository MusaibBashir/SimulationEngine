#include "Terminal.hpp"

#include <cstdio>
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

namespace des {
namespace {

const char* sgr(Attr a) {
    switch (a) {
        case Attr::Bold:    return "\033[0m\033[1m";
        case Attr::Dim:     return "\033[0m\033[2m";
        case Attr::Reverse: return "\033[0m\033[7m";
        case Attr::Error:   return "\033[0m\033[31m";
        case Attr::Normal:  break;
    }
    return "\033[0m";
}

class Win32Terminal : public ITerminal {
    HANDLE m_in{nullptr};
    HANDLE m_out{nullptr};
    DWORD  m_savedIn{0};
    DWORD  m_savedOut{0};
public:
    Win32Terminal() {
        m_in  = GetStdHandle(STD_INPUT_HANDLE);
        m_out = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(m_in, &m_savedIn);
        GetConsoleMode(m_out, &m_savedOut);
        // Without ENABLE_VIRTUAL_TERMINAL_PROCESSING the escape sequences print
        // as literal text. Windows 10 and later support it.
        SetConsoleMode(m_out, m_savedOut | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        // ENABLE_MOUSE_INPUT gives clicks and the wheel. ENABLE_QUICK_EDIT_MODE
        // has to go with it, and it is on by default on Windows Terminal: with
        // it set the console eats every drag for its own selection and the
        // program is never told. ENABLE_EXTENDED_FLAGS must be set in the same
        // call or the quick-edit bit is ignored rather than cleared.
        SetConsoleMode(m_in, static_cast<DWORD>(
            (m_savedIn & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
                           ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE)) |
            ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS));
        std::fputs("\033[?1049h\033[?25l", stdout);
    }
    ~Win32Terminal() override {
        std::fputs("\033[?25h\033[?1049l\033[0m", stdout);
        std::fflush(stdout);
        SetConsoleMode(m_in, m_savedIn);
        SetConsoleMode(m_out, m_savedOut);
    }

    Win32Terminal(const Win32Terminal&) = delete;
    Win32Terminal& operator=(const Win32Terminal&) = delete;

    TerminalSize size() const override {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(m_out, &info)) return TerminalSize{};
        return TerminalSize{info.srWindow.Right - info.srWindow.Left + 1,
                            info.srWindow.Bottom - info.srWindow.Top + 1};
    }

    void present(const Screen& screen) override {
        std::string out = "\033[H";
        Attr current = Attr::Normal;
        for (int y = 0; y < screen.height(); ++y) {
            for (int x = 0; x < screen.width(); ++x) {
                const Screen::Glyph& g = screen.at(x, y);
                if (g.attr != current) { out += sgr(g.attr); current = g.attr; }
                out.push_back(g.ch);
            }
            out += "\033[K";
            if (y + 1 < screen.height()) out += "\r\n";
        }
        out += "\033[0m";
        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
    }

    // Is this a record the layer above would act on? Mouse records count now,
    // but only the ones that carry a press or a wheel turn: a bare move is
    // reported constantly and would make the run advance one event per twitch
    // of the mouse.
    static bool interesting(const INPUT_RECORD& r) {
        if (r.EventType == KEY_EVENT) return r.Event.KeyEvent.bKeyDown != 0;
        if (r.EventType == MOUSE_EVENT) {
            const MOUSE_EVENT_RECORD& m = r.Event.MouseEvent;
            if (m.dwEventFlags == MOUSE_WHEELED) return true;
            return m.dwEventFlags == 0 && m.dwButtonState != 0;
        }
        return false;
    }

    bool keyPending() override {
        for (;;) {
            INPUT_RECORD records[16];
            DWORD available = 0;
            if (!PeekConsoleInputW(m_in, records, 16, &available) || available == 0)
                return false;
            for (DWORD i = 0; i < available; ++i)
                if (interesting(records[i])) return true;
            // Only focus, resize or mouse-move records are waiting. Discard
            // them, or the handle stays signalled forever and the run never
            // advances at all -- the same bug wearing the opposite face.
            DWORD read = 0;
            if (!ReadConsoleInputW(m_in, records, available, &read) || read == 0)
                return false;
        }
    }

    Key nextKey() override {
        for (;;) {
            INPUT_RECORD record{};
            DWORD read = 0;
            if (!ReadConsoleInputW(m_in, &record, 1, &read) || read == 0)
                return Key::special(KeyKind::Unknown);

            if (record.EventType == MOUSE_EVENT) {
                const MOUSE_EVENT_RECORD& m = record.Event.MouseEvent;
                const int x = m.dwMousePosition.X;
                const int y = m.dwMousePosition.Y;
                if (m.dwEventFlags == MOUSE_WHEELED) {
                    // The high word is signed, and positive means away from
                    // the user -- which is scrolling UP the document.
                    const short delta = static_cast<short>(HIWORD(m.dwButtonState));
                    return Key::mouse(delta > 0 ? MouseButton::WheelUp
                                                : MouseButton::WheelDown, x, y);
                }
                if (m.dwEventFlags != 0) continue;      // a move, or a drag
                if (m.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
                    return Key::mouse(MouseButton::Left, x, y);
                if (m.dwButtonState & RIGHTMOST_BUTTON_PRESSED)
                    return Key::mouse(MouseButton::Right, x, y);
                continue;                               // a release
            }

            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                continue;

            const KEY_EVENT_RECORD& k = record.Event.KeyEvent;
            const bool shift = (k.dwControlKeyState & SHIFT_PRESSED) != 0;
            if (k.wVirtualKeyCode >= VK_F1 && k.wVirtualKeyCode <= VK_F12)
                return Key::function(k.wVirtualKeyCode - VK_F1 + 1);
            switch (k.wVirtualKeyCode) {
                case VK_UP:    return shift ? Key::shifted(KeyKind::Up)
                                            : Key::special(KeyKind::Up);
                case VK_DOWN:  return shift ? Key::shifted(KeyKind::Down)
                                            : Key::special(KeyKind::Down);
                case VK_LEFT:  return shift ? Key::shifted(KeyKind::Left)
                                            : Key::special(KeyKind::Left);
                case VK_RIGHT: return shift ? Key::shifted(KeyKind::Right)
                                            : Key::special(KeyKind::Right);
                case VK_HOME:  return shift ? Key::shifted(KeyKind::Home)
                                            : Key::special(KeyKind::Home);
                case VK_END:   return shift ? Key::shifted(KeyKind::End)
                                            : Key::special(KeyKind::End);
                case VK_PRIOR: return shift ? Key::shifted(KeyKind::PageUp)
                                            : Key::special(KeyKind::PageUp);
                case VK_NEXT:  return shift ? Key::shifted(KeyKind::PageDown)
                                            : Key::special(KeyKind::PageDown);
                default: break;
            }
            switch (k.wVirtualKeyCode) {
                case VK_UP:     return Key::special(KeyKind::Up);
                case VK_DOWN:   return Key::special(KeyKind::Down);
                case VK_LEFT:   return Key::special(KeyKind::Left);
                case VK_RIGHT:  return Key::special(KeyKind::Right);
                case VK_HOME:   return Key::special(KeyKind::Home);
                case VK_END:    return Key::special(KeyKind::End);
                case VK_PRIOR:  return Key::special(KeyKind::PageUp);
                case VK_NEXT:   return Key::special(KeyKind::PageDown);
                case VK_DELETE: return Key::special(KeyKind::Delete);
                case VK_RETURN: return Key::special(KeyKind::Enter);
                case VK_ESCAPE: return Key::special(KeyKind::Escape);
                case VK_BACK:   return Key::special(KeyKind::Backspace);
                case VK_TAB:
                    return Key::special((k.dwControlKeyState & SHIFT_PRESSED)
                                            ? KeyKind::BackTab : KeyKind::Tab);
                default: break;
            }
            const wchar_t ch = k.uChar.UnicodeChar;
            if (ch == 0) continue;
            if (ch < 27 &&
                (k.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)))
                return Key::control(static_cast<char>('a' + ch - 1));
            if (ch >= 32 && ch < 127) return Key::character(static_cast<char>(ch));
        }
    }

    void setTitle(const std::string& title) override {
        SetConsoleTitleA(title.c_str());
    }
};

}  // namespace

std::unique_ptr<ITerminal> openTerminal(std::string& whyNot) {
    HANDLE in  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD inMode = 0;
    DWORD outMode = 0;

    // Checked FIRST, and it is what "not a console" means on Windows:
    // GetConsoleMode fails on a pipe or a file. Unchecked, the constructor's
    // SetConsoleMode calls failed quietly, ReadConsoleInputW then failed on
    // every call, nextKey() returned Unknown each time, and the loop spun at
    // full speed forever -- which from outside is indistinguishable from a hang.
    if (in == INVALID_HANDLE_VALUE || !GetConsoleMode(in, &inMode) ||
        out == INVALID_HANDLE_VALUE || !GetConsoleMode(out, &outMode)) {
        whyNot = "it has to be run in a console window, with its input and output "
                 "going to that window rather than redirected. Double-click "
                 "DES-Simulator.exe, or start it from Command Prompt or PowerShell.";
        return nullptr;
    }

    // Every glyph is drawn with VT escape sequences, and a console that cannot
    // interpret them -- Windows before 10 -- prints them as text. Try it, then
    // put the mode back: the constructor sets it for real and the destructor
    // restores it, and neither should inherit a half-done probe.
    if (!SetConsoleMode(out, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        whyNot = "this console does not understand the display codes the program "
                 "draws with. It needs Windows 10 or 11.";
        return nullptr;
    }
    SetConsoleMode(out, outMode);
    return std::unique_ptr<ITerminal>(new Win32Terminal());
}

std::string defaultModelPath() {
    const std::string fallback = "untitled.des";

    // The caller frees the path whether or not the call succeeded -- that is
    // how SHGetKnownFolderPath is documented, and a failure path that skipped
    // it would leak on exactly the machines that are already unusual.
    PWSTR wide = nullptr;
    const HRESULT found = SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &wide);
    if (FAILED(found) || wide == nullptr) {
        if (wide != nullptr) CoTaskMemFree(wide);
        return fallback;
    }

    // To the ANSI code page, because every file this program opens goes through
    // a narrow std::fstream, which on Windows means ANSI. A Documents path with
    // characters that page cannot hold -- a user name in a script the system
    // locale does not cover -- could not be opened that way at all, so it falls
    // back to the current folder rather than to a mangled path that saves the
    // model somewhere nobody would ever find it.
    std::string result = fallback;
    const int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide, -1,
                                      nullptr, 0, nullptr, nullptr);
    if (n > 1) {
        std::string narrow(static_cast<std::size_t>(n), '\0');
        BOOL lossy = FALSE;
        WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide, -1,
                            &narrow[0], n, nullptr, &lossy);
        narrow.resize(static_cast<std::size_t>(n - 1));
        if (!lossy) result = narrow + "\\DES Models\\untitled.des";
    }
    CoTaskMemFree(wide);
    return result;
}

bool launchedOnOwnConsole() {
    // Double-clicked, this process is the only one on the console. Started from
    // a Command Prompt, the shell is attached too and the count is at least two.
    DWORD ids[2];
    return GetConsoleProcessList(ids, 2) == 1u;
}

}  // namespace des
