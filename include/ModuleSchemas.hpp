// ============================================================================
// ModuleSchemas.hpp  --  v11: every module type, described
// ============================================================================
// Kept apart from ModuleSchema.hpp because this is DATA and that is mechanism.
// This is the file a version edits when it adds a module, and keeping it
// separate means adding one cannot disturb the registry.

#pragma once
#include <vector>
#include "ModuleSchema.hpp"

namespace des {

std::vector<ModuleSchema> buildSchemas();

}  // namespace des
