// ============================================================================
// Screen.hpp  --  v13: the screen is a VALUE
// ============================================================================
// This is the decision the whole UI rests on. A renderer fills one of these;
// the terminal blits it. Nothing here knows what a terminal is, so a test can
// render a document and assert on what a person would read -- headless, with
// no timing and no sleeps, under the same sanitisers as the engine.

#pragma once
#include <string>
#include <vector>

namespace des {

enum class Attr { Normal, Bold, Dim, Reverse, Error };

class Screen {
public:
    // NESTED, and not called Cell. `des::Cell` would sit one lookup away from
    // ModelDocument::Cell, which is a spreadsheet cell -- the thing this whole
    // project means by the word. A screen position holding a character is a
    // glyph.
    struct Glyph {
        char ch{' '};
        Attr attr{Attr::Normal};
    };

    Screen(int width, int height);

    int width()  const { return m_width; }
    int height() const { return m_height; }

    void clear();
    void put(int x, int y, char ch, Attr attr = Attr::Normal);
    // Returns the x it stopped at, so callers can chain runs of text.
    int  text(int x, int y, const std::string& s, Attr attr = Attr::Normal);
    void hline(int x, int y, int length, char ch = '-', Attr attr = Attr::Normal);
    void box(int x, int y, int width, int height, Attr attr = Attr::Normal);

    const Glyph& at(int x, int y) const;

    // One row as plain text, trailing blanks trimmed. THIS IS THE TEST
    // INTERFACE: a test asserts on what a person would read rather than on an
    // attribute grid, so a test says what it means.
    std::string line(int y) const;
    std::string asText() const;

private:
    bool inside(int x, int y) const;

    int                m_width{0};
    int                m_height{0};
    std::vector<Glyph> m_glyphs;   // row-major, m_width * m_height
    Glyph              m_void;     // returned by at() for an out-of-range read
};

}  // namespace des
