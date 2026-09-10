// ============================================================================
// TuiInput.hpp  --  v13: a key and a state, and nothing else
// ============================================================================
// handleKey() is the whole input layer. It touches nothing outside the state,
// which is what lets a test type a scripted sequence and assert on the result
// without a terminal anywhere.
//
// Returns false when the program should stop. That is a return value rather
// than a flag on the state because quitting is the one thing the loop above
// has to act on, and a bool it must consume cannot be forgotten.

#pragma once
#include "Key.hpp"
#include "TuiRender.hpp"
#include "TuiState.hpp"

namespace des {

// The size is needed only to turn a CLICK back into what was clicked, using
// the same layout the renderer drew with. Keyboard input ignores it, so the
// defaults keep every keyboard test reading as it did.
bool handleKey(TuiState& state, Key key,
               int width = MIN_WIDTH, int height = MIN_HEIGHT);

}  // namespace des
