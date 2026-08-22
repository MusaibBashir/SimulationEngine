// ============================================================================
// Entity.cpp  --  definitions for Entity
// ============================================================================
// RULE FOR EVERY .cpp IN THIS PROJECT:
//   The header DECLARES (what it can do). The source DEFINES (how it does it).
//   One-line getters may stay inline in the header. Anything longer lives here.
//
// [1] Include "Entity.hpp" FIRST, on its own line, before any system header.
//     Why first: if Entity.hpp forgot an include it needs, this file fails to
//     compile and you find out immediately. Put it last and a system header
//     might accidentally supply the missing piece, hiding the bug until some
//     other file includes Entity.hpp on its own. This ordering is a free
//     self-test on every header you write.
//
// [2] Then any extra includes this FILE needs but the header does not.
//
// [3] Define, in the same order they appear in the header:
//
//     [3a] Entity::setAttribute(name, value)
//          One line: index into m_attributes with the name and assign the value.
//          operator[] on a std::map inserts the key if absent, which is exactly
//          the insert-or-overwrite behaviour we want here.
//
//     [3b] Entity::attribute(name) const
//          Three lines:
//            - find the key (use .find(), NOT operator[] -- operator[] is
//              non-const and would INSERT into the map, which will not even
//              compile in a const method. That compile error is the map teaching
//              you something; read it rather than deleting the const.)
//            - if the iterator equals end(), return 0.0
//            - otherwise return the mapped value
//
//     [3c] Entity::hasAttribute(name) const
//          One line: count(name) > 0. (Or .contains() if you are on C++20 and
//          have set the standard accordingly in CMakeLists.txt.)
//
// The constructor and the trivial getters were defined inline in the header,
// so they do NOT appear again here. Defining them in both places is a
// redefinition error -- a useful one to trigger once, deliberately, to see it.

#include "Entity.hpp"

namespace des {


void Entity::setAttribute(const std::string& name, double value) {
    m_attributes[name] = value;
}   

double Entity::attribute(const std::string& name) const{
    auto it=m_attributes.find(name);
    if(it==m_attributes.end()) return 0.0;
    return it->second;
}

bool Entity::hasAttribute(const std::string& name) const{
    return m_attributes.count(name)>0;
}

}  // namespace des
