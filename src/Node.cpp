// ============================================================================
// Node.cpp
// ============================================================================
// This file exists mainly to give INode a KEY FUNCTION -- the first non-inline
// virtual member. The compiler emits the vtable and typeinfo for a polymorphic
// class alongside its key function, and with every virtual defined inline in the
// header there is no such place, so nothing emits them at all.
//
// The symptom is a link error about "undefined reference to typeinfo for INode"
// from every file that uses dynamic_cast, which reads like a build-system fault
// and is not. Defining one virtual out of line fixes it, and is the reason
// polymorphic base classes conventionally have a .cpp even when they look like
// they need none.

#include "Node.hpp"
#include "ModelError.hpp"

namespace des {

void INode::onScheduledEvent(NodeContext&, Entity*) {
    // Only blocks that schedule a return need this. If one arrives here, a node
    // asked to be called back and then did not say what to do about it.
    throw ModelError("block '" + m_name +
                     "' was scheduled a callback but does not handle one");
}

}  // namespace des
