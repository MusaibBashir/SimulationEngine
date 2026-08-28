// ============================================================================
// VariableStore.hpp  --  v10 STUB. Task 6 replaces this file wholesale.
// ============================================================================
// EvalContext needs to name this type before it is designed. Nothing here is
// finished work.

#pragma once
#include <string>

namespace des {

class VariableStore {
public:
    bool   has(const std::string& name) const;
    double get(const std::string& name) const;
};

}  // namespace des
