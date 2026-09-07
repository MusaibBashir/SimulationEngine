#include "Screen.hpp"

namespace des {

Screen::Screen(int width, int height)
    : m_width(width < 0 ? 0 : width), m_height(height < 0 ? 0 : height),
      m_glyphs(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height)) {}

bool Screen::inside(int x, int y) const {
    return x >= 0 && y >= 0 && x < m_width && y < m_height;
}

void Screen::clear() {
    for (Glyph& g : m_glyphs) g = Glyph{};
}

void Screen::put(int x, int y, char ch, Attr attr) {
    if (!inside(x, y)) return;
    m_glyphs[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
             static_cast<std::size_t>(x)] = Glyph{ch, attr};
}

int Screen::text(int x, int y, const std::string& s, Attr attr) {
    for (char c : s) put(x++, y, c, attr);
    return x;
}

void Screen::hline(int x, int y, int length, char ch, Attr attr) {
    for (int i = 0; i < length; ++i) put(x + i, y, ch, attr);
}

void Screen::box(int x, int y, int width, int height, Attr attr) {
    if (width < 2 || height < 2) return;
    hline(x, y, width, '-', attr);
    hline(x, y + height - 1, width, '-', attr);
    for (int i = 1; i < height - 1; ++i) {
        put(x, y + i, '|', attr);
        put(x + width - 1, y + i, '|', attr);
    }
    put(x, y, '+', attr);
    put(x + width - 1, y, '+', attr);
    put(x, y + height - 1, '+', attr);
    put(x + width - 1, y + height - 1, '+', attr);
}

const Screen::Glyph& Screen::at(int x, int y) const {
    if (!inside(x, y)) return m_void;
    return m_glyphs[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
                    static_cast<std::size_t>(x)];
}

std::string Screen::line(int y) const {
    if (y < 0 || y >= m_height) return std::string();
    std::string out;
    out.reserve(static_cast<std::size_t>(m_width));
    for (int x = 0; x < m_width; ++x) out.push_back(at(x, y).ch);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string Screen::asText() const {
    std::string out;
    for (int y = 0; y < m_height; ++y) {
        out += line(y);
        out.push_back('\n');
    }
    return out;
}

}  // namespace des
