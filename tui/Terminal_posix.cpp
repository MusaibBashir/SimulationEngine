#include "Terminal.hpp"

#include <cstdio>
#include <string>
#include <vector>
#include <sys/ioctl.h>
#include <sys/select.h>
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

// The numbers between ESC[ and the final byte.
std::vector<int> numbersIn(const std::string& params) {
    std::vector<int> out;
    int current = 0;
    bool any = false;
    for (const char c : params) {
        if (c >= '0' && c <= '9') { current = current * 10 + (c - '0'); any = true; }
        else if (c == ';')        { out.push_back(any ? current : 0); current = 0; any = false; }
    }
    if (any) out.push_back(current);
    return out;
}

// One CSI sequence as a Key. Split out because it is the only part of this
// file with any logic in it, and because the mouse, the function keys and the
// modified arrows all arrive through the same door.
Key decodeCsi(const std::string& params, char final) {
    const std::vector<int> n = numbersIn(params);

    // SGR mouse: ESC[<button;x;yM  (press) or ...m (release). Only presses are
    // acted on -- a click is one event to everything above here, and reporting
    // both would double every one of them.
    if (!params.empty() && params[0] == '<') {
        if (n.size() < 3 || final != 'M') return Key::special(KeyKind::Unknown);
        const int code = n[0];
        MouseButton button = MouseButton::Left;
        if (code & 64)              button = (code & 1) ? MouseButton::WheelDown
                                                        : MouseButton::WheelUp;
        else if ((code & 3) == 1)   button = MouseButton::Middle;
        else if ((code & 3) == 2)   button = MouseButton::Right;
        else if ((code & 3) == 3)   return Key::special(KeyKind::Unknown);
        // Terminals count from 1; the Screen counts from 0.
        return Key::mouse(button, n[1] - 1, n[2] - 1);
    }

    // A modifier arrives as the second parameter, one more than a bitmask in
    // which 1 is Shift. Only Shift matters here: it is what extends a
    // selection, and the rest would be keys with nothing bound to them.
    const bool shift = n.size() >= 2 && ((n[1] - 1) & 1) != 0;
    const auto arrow = [shift](KeyKind k) {
        return shift ? Key::shifted(k) : Key::special(k);
    };

    switch (final) {
        case 'A': return arrow(KeyKind::Up);
        case 'B': return arrow(KeyKind::Down);
        case 'C': return arrow(KeyKind::Right);
        case 'D': return arrow(KeyKind::Left);
        case 'H': return arrow(KeyKind::Home);
        case 'F': return arrow(KeyKind::End);
        case 'Z': return Key::special(KeyKind::BackTab);
        case '~':
            if (n.empty()) return Key::special(KeyKind::Unknown);
            switch (n[0]) {
                case 1: case 7:  return arrow(KeyKind::Home);
                case 4: case 8:  return arrow(KeyKind::End);
                case 3:  return Key::special(KeyKind::Delete);
                case 5:  return arrow(KeyKind::PageUp);
                case 6:  return arrow(KeyKind::PageDown);
                case 11: return Key::function(1);
                case 12: return Key::function(2);
                case 13: return Key::function(3);
                case 14: return Key::function(4);
                case 15: return Key::function(5);
                default: return Key::special(KeyKind::Unknown);
            }
        default: return Key::special(KeyKind::Unknown);
    }
}

class PosixTerminal : public ITerminal {
    termios m_saved{};
public:
    PosixTerminal() {
        tcgetattr(STDIN_FILENO, &m_saved);
        termios raw = m_saved;
        // ISIG, IXON and IEXTEN all have to go, and v13 left them on.
        //
        //   ISIG   -- ^C raised SIGINT and killed the program, so ^C could
        //             never mean copy.
        //   IXON   -- ^S was swallowed as XOFF by the terminal driver and
        //             froze the output. v13 bound ^S to SAVE and it never
        //             arrived.
        //   IEXTEN -- ^V is VLNEXT, "take the next key literally", so a paste
        //             would have eaten the keystroke after it.
        //
        // Three keys the interface names in its own footer, none of which
        // reached it. Nobody noticed because the only POSIX use of this is
        // WSL, where the tests run and the UI does not.
        raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | ISIG | IEXTEN));
        raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        // 1000 is click reporting; 1006 is the SGR encoding, which is needed
        // because the original packs a coordinate into one byte and therefore
        // cannot say anything past column 223.
        std::fputs("\033[?1049h\033[?25l\033[?1000h\033[?1006h", stdout);
    }
    ~PosixTerminal() override {
        std::fputs("\033[?1006l\033[?1000l\033[?25h\033[?1049l\033[0m", stdout);
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

    bool keyPending() override {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        timeval zero{0, 0};
        return ::select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &zero) > 0;
    }

    Key nextKey() override {
        char c = 0;
        if (::read(STDIN_FILENO, &c, 1) != 1) return Key::special(KeyKind::Unknown);
        if (c == '\r' || c == '\n') return Key::special(KeyKind::Enter);
        if (c == 127 || c == 8)     return Key::special(KeyKind::Backspace);
        if (c == '\t')              return Key::special(KeyKind::Tab);
        if (c == 27) {
            char a = 0;
            if (::read(STDIN_FILENO, &a, 1) != 1) return Key::special(KeyKind::Escape);
            if (a == 'O') {
                // SS3, which is how most terminals send F1..F4.
                char f = 0;
                if (::read(STDIN_FILENO, &f, 1) != 1) return Key::special(KeyKind::Escape);
                if (f >= 'P' && f <= 'S') return Key::function(f - 'P' + 1);
                return Key::special(KeyKind::Unknown);
            }
            if (a != '[') return Key::special(KeyKind::Escape);

            // Read the rest of the sequence: parameters, then one final byte.
            // Written generally rather than case by case because the modified
            // arrows (ESC[1;2A for Shift+Up) and the SGR mouse reports both
            // carry parameters, and v13's two-byte reader could not see them.
            std::string params;
            char final = 0;
            for (int i = 0; i < 32; ++i) {
                char t = 0;
                if (::read(STDIN_FILENO, &t, 1) != 1) return Key::special(KeyKind::Escape);
                if ((t >= '0' && t <= '9') || t == ';' || t == '<' || t == '?') {
                    params.push_back(t);
                    continue;
                }
                final = t;
                break;
            }
            return decodeCsi(params, final);
        }
        if (c > 0 && c < 27) return Key::control(static_cast<char>('a' + c - 1));
        return Key::character(c);
    }

    void setTitle(const std::string& title) override {
        // OSC 0: every terminal emulator worth the name reads it as the title.
        std::fputs(("\033]0;" + title + "\007").c_str(), stdout);
        std::fflush(stdout);
    }
};

}  // namespace

std::unique_ptr<ITerminal> openTerminal(std::string& whyNot) {
    // Checked FIRST. Raw mode on a pipe does nothing, and nextKey() then reads
    // end-of-file, returns Unknown, and is asked again -- forever, at full speed.
    if (!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO)) {
        whyNot = "it has to be run in a terminal window, with its input and "
                 "output going to that window rather than redirected.";
        return nullptr;
    }
    return std::unique_ptr<ITerminal>(new PosixTerminal());
}

std::string defaultModelPath() { return "untitled.des"; }

// A POSIX terminal outlives the program running in it, so there is never a
// window about to vanish with the last message still in it.
bool launchedOnOwnConsole() { return false; }

}  // namespace des
