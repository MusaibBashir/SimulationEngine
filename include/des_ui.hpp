// ============================================================================
// des_ui.hpp  --  the only header a front end needs
// ============================================================================
// Separate from des.hpp on purpose. The layering rule this version rests on is
// that des_ui links des_engine and NEVER the reverse -- and putting Screen.hpp
// into the engine's umbrella would have every example and every engine test
// pulling in the UI, which is the first step towards an engine that knows one
// exists. Two umbrellas, one direction.

#pragma once

#include "des.hpp"
#include "Key.hpp"
#include "Screen.hpp"
#include "TuiState.hpp"
#include "TuiRender.hpp"
