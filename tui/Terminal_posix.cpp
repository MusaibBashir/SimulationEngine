#include "Terminal.hpp"

#include <cstdio>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

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

class PosixTerminal : public ITerminal {
    termios m_saved{};
public:
    PosixTerminal() {
        tcgetattr(STDIN_FILENO, &m_saved);
        termios raw = m_saved;
        raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        std::fputs("\033[?1049h\033[?25l", stdout);   // alternate screen, no cursor
    }
    ~PosixTerminal() override {
        std::fputs("\033[?25h\033[?1049l\033[0m", stdout);
        std::fflush(stdout);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_saved);
    }

    PosixTerminal(const PosixTerminal&) = delete;
    PosixTerminal& operator=(const PosixTerminal&) = delete;

    TerminalSize size() const override {
        winsize ws{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0)
            return TerminalSize{};
        return TerminalSize{static_cast<int>(ws.ws_col), static_cast<int>(ws.ws_row)};
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

    Key nextKey() override {
        char c = 0;
        if (::read(STDIN_FILENO, &c, 1) != 1) return Key::special(KeyKind::Unknown);
        if (c == '\r' || c == '\n') return Key::special(KeyKind::Enter);
        if (c == 127 || c == 8)     return Key::special(KeyKind::Backspace);
        if (c == '\t')              return Key::special(KeyKind::Tab);
        if (c == 27) {
            char a = 0, b = 0;
            if (::read(STDIN_FILENO, &a, 1) != 1) return Key::special(KeyKind::Escape);
            if (a != '[') return Key::special(KeyKind::Escape);
            if (::read(STDIN_FILENO, &b, 1) != 1) return Key::special(KeyKind::Escape);
            switch (b) {
                case 'A': return Key::special(KeyKind::Up);
                case 'B': return Key::special(KeyKind::Down);
                case 'C': return Key::special(KeyKind::Right);
                case 'D': return Key::special(KeyKind::Left);
                case 'H': return Key::special(KeyKind::Home);
                case 'F': return Key::special(KeyKind::End);
                case 'Z': return Key::special(KeyKind::BackTab);
                // The tilde-terminated ones. The trailing '~' has to be eaten
                // or it arrives next as a printable character.
                case '5': { char t = 0; (void)::read(STDIN_FILENO, &t, 1);
                            return Key::special(KeyKind::PageUp); }
                case '6': { char t = 0; (void)::read(STDIN_FILENO, &t, 1);
                            return Key::special(KeyKind::PageDown); }
                case '3': { char t = 0; (void)::read(STDIN_FILENO, &t, 1);
                            return Key::special(KeyKind::Delete); }
                default:  return Key::special(KeyKind::Unknown);
            }
        }
        if (c > 0 && c < 27) return Key::control(static_cast<char>('a' + c - 1));
        return Key::character(c);
    }
};

}  // namespace

std::unique_ptr<ITerminal> openTerminal() {
    return std::unique_ptr<ITerminal>(new PosixTerminal());
}

}  // namespace des
