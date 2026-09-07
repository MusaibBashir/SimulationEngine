// ============================================================================
// tests/tui_tests.cpp  --  v13: the terminal UI, tested without a terminal
// ============================================================================
// Every test here builds a state, feeds keys, and asserts on a Screen AS TEXT.
// That is possible because the screen is a value: nothing in the UI below
// tui/ touches a terminal, so all of it runs under the sanitisers with
// everything else.
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "harness.hpp"
#include "des_ui.hpp"

using namespace des;
using des_test::check;
using des_test::modelPath;
using des_test::section;

void runTuiTests() {
    section("Screen is a value");
    {
        Screen s(10, 3);
        check(s.width() == 10 && s.height() == 3, "a screen has a size");
        check(s.line(0).empty(), "and starts blank");

        s.text(2, 1, "hi");
        check(s.line(1) == "  hi", "text lands where it was put");
        check(s.at(2, 1).ch == 'h', "and the glyph is readable");

        // Out of range is DROPPED, not asserted. A renderer that computes one
        // column past the edge has a layout bug, and a crash in front of the
        // user is a worse way to learn about it than a missing character.
        s.text(8, 1, "abcdef");
        check(s.line(1) == "  hi    ab", "a run that overflows is clipped, not fatal");
        s.put(-1, -1, 'x');
        s.put(999, 999, 'x');
        check(s.line(1) == "  hi    ab", "and writes outside it change nothing");

        s.text(0, 2, "trailing   ");
        check(s.line(2) == "trailing", "line() trims the trailing blanks");

        Screen t(10, 3);
        t.text(2, 1, "hi");
        t.text(8, 1, "ab");
        t.text(0, 2, "trailing");
        check(s.asText() == t.asText(), "two screens with the same glyphs read alike");

        s.clear();
        check(s.asText() == Screen(10, 3).asText(), "clear() empties it");
    }

    section("Key is a value");
    {
        const Key a = Key::character('a');
        check(a.kind == KeyKind::Char && a.ch == 'a', "a printable key carries its char");
        const Key ctrlS = Key::control('s');
        check(ctrlS.kind == KeyKind::Ctrl && ctrlS.ch == 'S',
              "a control key normalises to upper case, so ^s and ^S are one key");
        check(Key::special(KeyKind::Up).kind == KeyKind::Up, "a special key is its kind");
    }
}
