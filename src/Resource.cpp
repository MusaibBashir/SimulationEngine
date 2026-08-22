// ============================================================================
// Resource.cpp
// ============================================================================
// [1] Include "Resource.hpp" first. Then <cassert> for the invariant checks.
//
// [2] Define the constructor here IF you did not inline it (recommended: put
//     multi-member constructors in the .cpp, keep the header readable as an
//     interface summary).
//
// [3] v1 STUB BODIES -- write the signature, an empty body, and the TODO.
//     Resist filling them in early; the discipline of v1 is that nothing moves.
//
//     [3a] void Resource::seize(int units)
//          // TODO v2, three lines:
//          //   assert(units > 0)
//          //   assert(units <= unitsAvailable())   <- the invariant, checked
//          //   m_unitsBusy += units
//          NOTE: the default argument `= 1` is written in the HEADER ONLY.
//          Repeating it here is a compile error. Common first-time trip-up.
//
//     [3b] void Resource::release(int units)
//          // TODO v2:
//          //   assert(units > 0 && units <= m_unitsBusy)
//          //   m_unitsBusy -= units
//
// [4] The const getters stayed inline in the header. Nothing else belongs here.

#include "Resource.hpp"
#include <cassert>
#include <utility>

Resource::Resource(std::string name, int capacity)
    : m_name(std::move(name)), m_capacity(capacity), m_unitsBusy(0) {
    assert(capacity > 0);
}

void Resource::reset() {
    m_unitsBusy = 0;
    // Name and capacity are model configuration, not run state -- they survive.
}

void Resource::seize(int units) {
    assert(units > 0);
    assert(units <= unitsAvailable());   // the class invariant, checked
    m_unitsBusy += units;
}

void Resource::release(int units) {
    assert(units > 0);
    assert(units <= m_unitsBusy);
    m_unitsBusy -= units;
}
