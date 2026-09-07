// ============================================================================
// TuiRender.hpp  --  v13: a state becomes a screen
// ============================================================================
// Pure: it reads a TuiState and fills a Screen. No terminal, no globals, no
// clock. That is what lets a test render a document and assert on the text a
// person would read.

#pragma once
#include "Screen.hpp"
#include "TuiState.hpp"

namespace des {

// The smallest terminal this layout fits in. Below it, render() draws one
// message and nothing else.
constexpr int MIN_WIDTH  = 80;
constexpr int MIN_HEIGHT = 24;

void render(const TuiState& state, Screen& screen);

}  // namespace des
