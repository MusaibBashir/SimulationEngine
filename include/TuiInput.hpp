// ============================================================================
// TuiInput.hpp  --  v13: a keypress becomes the next state
// ============================================================================
// Pure with respect to the terminal: it takes a Key and mutates a TuiState.
// That is what lets a test type a whole session without one.

#pragma once
#include "Key.hpp"
#include "TuiState.hpp"

namespace des {

// False when the application should stop. The caller owns the loop, exactly as
// v12's RunController hands the run loop to its caller.
//
// Quitting with unsaved work does NOT return false: it enters Mode::Confirm
// and asks. A UI over a file format is trusted with work that exists nowhere
// else, and one keystroke must not be able to discard it.
bool handleKey(TuiState& state, Key key);

}  // namespace des
