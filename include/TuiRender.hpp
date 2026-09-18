// ============================================================================
// TuiRender.hpp  --  v13: a state and a screen, and nothing else
// ============================================================================
// render() is PURE with respect to the terminal: it reads a TuiState and fills
// a Screen. That is what lets a test assert on what a person would see.

#pragma once
#include <vector>
#include "Screen.hpp"
#include "TuiState.hpp"

namespace des {

// Below this the layout stops making sense, and saying so beats drawing
// something unreadable.
constexpr int MIN_WIDTH  = 80;
constexpr int MIN_HEIGHT = 24;

// v15: where everything is. ONE function computes the geometry, render() draws
// with it and the input layer hit-tests with it -- because a mouse click has to
// be turned back into "the third palette entry" by exactly the arithmetic that
// put it there. Two copies of that arithmetic is the bug the v13 tab bar
// already taught this project once, when the measuring pass and the drawing
// pass disagreed by the width of a marker.
struct Layout {
    int width{0};
    int height{0};

    int barRow{0};
    int bodyTop{1};        // first row inside the frame
    int bodyBottom{0};     // last row inside the frame, inclusive
    int statusRow{0};
    int hintRow{0};

    // The bar across the top: one entry per View, in order.
    std::vector<int> tabStart;
    std::vector<int> tabEnd;      // one past the last column

    // Model view.
    int paletteX{0};       // first column of a palette label
    int paletteWidth{0};
    int paletteTop{0};
    int paletteRows{0};
    // Which palette entry is drawn on paletteTop. Sixteen module types fit in
    // the shortest terminal this accepts, so today it is always zero -- and a
    // hit-test that assumed that would be wrong the first time a data module
    // was added. One number, read by the renderer and by the click handler.
    std::size_t paletteFirst{0};
    int dividerX{0};
    int gutterX{0};        // first column of the line-number gutter
    int gutterWidth{0};
    int textX{0};          // first column of the text itself
    int textWidth{0};
    int textTop{0};
    int textRows{0};

    // Which file line is drawn on textTop, and which column is drawn at textX.
    std::size_t firstLine{0};
    int runsFormX{0};      // v16: first column of the Runs tab's form
    // v16.1: the Tables tab's three panels.
    int tablesEditorX{0};  // first column of the middle panel
    int tablesListX{0};    // first column of the right-hand list
    std::size_t firstColumn{0};
};

Layout layoutFor(const TuiState& state, int width, int height);

// v16.1: the tabs along the top, in order. The label can carry a count --
// "Tables ^D 1E" -- so both the bar and a click on it ask this one function.
constexpr int TAB_COUNT = 5;
View        tabView(int i);
std::string tabLabel(const TuiState& state, int i);

// Where the current table's grid is on screen, worked out once so a click is
// turned back into a cell by the arithmetic that drew it.
struct TableGrid {
    int fieldTop{0};          // first header field's row
    int gridTop{0};           // first grid row's screen row
    int visibleRows{0};
    int visibleColumns{0};
    std::size_t firstRow{0};
    std::size_t firstColumn{0};
    int labelWidth{6};        // the row-number column
    int cellWidth{11};
};
TableGrid tableGridFor(const TuiState& state, const Layout& L);

void render(const TuiState& state, Screen& screen);

}  // namespace des
