// ============================================================================
// Entity.hpp  --  the thing that MOVES
// ============================================================================
// Theory: "Components that move within the system, get service from resources,
// interact with one another, wait in queues."
// Also absorbs the "Attribute" row: "A property attached to an individual
// entity (customer type, priority, arrival time, part number). Attributes are
// how entities differ from one another."
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard (pragma form).
//
// [2] Includes -- exactly three, no more:
//       <map>          : the attribute table
//       <string>       : attribute names
//       "Common.hpp"   : EntityId, SimTime
//     If you catch yourself adding "Resource.hpp" here, stop. An Entity in v1
//     does not know that resources exist. Keeping this list short is the whole
//     discipline.
//
// [3] class Entity  --  open the class, `private:` section FIRST.
//     (Write the data, then flip to public. It forces you to design the state
//     before designing the interface.)
//
//     PRIVATE DATA:
//     [3a] EntityId m_id
//          Unique identifier. MINTED BY SimulationSystem, never chosen by the
//          Entity itself. There is exactly one counter in the program and it
//          does not live here.
//
//     [3b] SimTime m_creationTime
//          Clock value at the instant this entity entered the system. You will
//          need it later for time-in-system = departureTime - m_creationTime.
//
//     [3c] std::map<std::string, double> m_attributes
//          The attribute bag: "priority" -> 2, "partNumber" -> 447.
//          A map (not fixed member variables) because the SET of attributes
//          differs from model to model, and you want lookup by name at runtime.
//          Cost: no compile-time checking of attribute names. Accept it for v1.
//
// [4] PUBLIC INTERFACE:
//
//     [4a] Constructor taking (EntityId, SimTime creationTime).
//          Initialise BOTH members in the member-initialiser list (the `: m_id(id),
//          m_creationTime(t)` part after the parameter list) -- NOT by assignment
//          inside the braces. The map is left default-constructed and empty.
//          Mark it `explicit`? No -- two parameters, so it cannot be an
//          accidental implicit conversion anyway.
//
//     [4b] EntityId id() const
//          Returns m_id. One line, safe to define inline right here in the
//          header. `const` because it modifies nothing.
//
//     [4c] SimTime creationTime() const
//          Returns m_creationTime. Inline.
//
//     [4d] void setAttribute(const std::string& name, double value)
//          Inserts or overwrites in m_attributes. Body goes in Entity.cpp.
//          Note the `const std::string&` -- pass strings by const reference,
//          never by value. Start the habit now.
//
//     [4e] double attribute(const std::string& name) const
//          Looks up and returns. DECIDE NOW what a missing key does; for v1
//          return 0.0. (The grown-up version returns std::optional<double> or
//          throws. Leave a // TODO saying so.)
//
//     [4f] bool hasAttribute(const std::string& name) const
//          Returns whether the map contains the key.
//
// [5] Close the class with a semicolon. (You will forget this once. Everyone does.)
//
// ---------------- DESIGN NOTE ----------------
// Notice there is no setId(). An entity's identity is fixed at birth. Not
// providing the setter is a design statement enforced by the compiler.

#pragma once

#include <map>
#include <string>
#include <vector>
#include "Common.hpp"

namespace des {


class Entity{
    private:
        EntityId m_id;
        SimTime m_creationTime;
        std::map<std::string, double> m_attributes;

        // v6: a BATCH entity carries the entities it was formed from. Non-owning
        // raw pointers -- SimulationSystem still owns every entity, exactly as
        // it owns this one. A temporary batch keeps them so Separate can put
        // them back; a permanent batch destroys them and clears this.
        std::vector<Entity*> m_members;

    public:
        Entity(EntityId id, SimTime creationTime)
        : m_id(id), m_creationTime(creationTime) {}

        EntityId id() const { return m_id; }
        SimTime creationTime() const { return m_creationTime; }

        void setAttribute(const std::string& name, double value);
        double attribute(const std::string& name) const;
        bool hasAttribute(const std::string& name) const;

        // v6. Copy every attribute from another entity -- used by Separate's
        // duplicate mode, where a clone should differ only in identity.
        void copyAttributesFrom(const Entity& other);

        // v6: batching support.
        void addMember(Entity* e);
        const std::vector<Entity*>& members() const { return m_members; }
        bool isBatch() const { return !m_members.empty(); }
        void clearMembers() { m_members.clear(); }

        // v6: a batch representative takes the OLDEST member's creation time, so
        // time-in-system measures how long the first arrival waited for the
        // group. This is the only place an entity's birth time may be rewritten,
        // and it exists for that one reason.
        void setCreationTime(SimTime t) { m_creationTime = t; }

};

}  // namespace des
