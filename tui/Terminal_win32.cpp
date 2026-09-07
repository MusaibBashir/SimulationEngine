#include "Terminal.hpp"

#include <cstdio>
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
        SetConsoleMode(m_in, static_cast<DWORD>(
            m_savedIn & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT)));
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

    bool keyPending() override {
        for (;;) {
            INPUT_RECORD records[16];
            DWORD available = 0;
            if (!PeekConsoleInputW(m_in, records, 16, &available) || available == 0)
                return false;
            for (DWORD i = 0; i < available; ++i)
                if (records[i].EventType == KEY_EVENT &&
                    records[i].Event.KeyEvent.bKeyDown)
                    return true;
            // Only mouse, focus or resize records are waiting. Discard them, or
            // the handle stays signalled forever and the run never advances.
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
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                continue;

            const KEY_EVENT_RECORD& k = record.Event.KeyEvent;
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
};

}  // namespace

std::unique_ptr<ITerminal> openTerminal() {
    return std::unique_ptr<ITerminal>(new Win32Terminal());
}

}  // namespace des
