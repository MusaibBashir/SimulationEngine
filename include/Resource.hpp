// ============================================================================
// Resource.hpp  --  the thing that SERVES
// ============================================================================
// Theory: "A component (usually static) that offers some service to entities.
// Entities COMPETE for resources, and the capacity of a resource is usually
// FINITE."
//
// ---------------- WRITE THESE LINES, IN THIS ORDER ----------------
//
// [1] Include guard.
//
// [2] Includes: <string>, "Common.hpp"  (for ResourceState). That is all.
//
// [3] class Resource, private data first:
//
//     [3a] std::string m_name        : "Teller", "Machine A". Used for lookup.
//     [3b] int m_capacity            : how many units EXIST. A 3-teller bank
//                                      has capacity 3. Set once at construction.
//     [3c] int m_unitsBusy           : how many units are currently seized.
//
//     THE INVARIANT, write it as a comment right next to m_unitsBusy:
//         0 <= m_unitsBusy <= m_capacity
//     Every method you write from here on either preserves this or is a bug.
//
// [4] PUBLIC INTERFACE:
//
//     [4a] Constructor (std::string name, int capacity)
//          Member-init list: name and capacity from the parameters,
//          m_unitsBusy explicitly to 0. Do not rely on it defaulting.
//
//     [4b] const std::string& name() const     -> returns m_name. Note the
//          `const&` return: no copy of the string is made.
//     [4c] int capacity() const                -> returns m_capacity.
//     [4d] int unitsBusy() const               -> returns m_unitsBusy.
//
//     [4e] int unitsAvailable() const
//          ONE LINE: capacity minus units busy.
//          *** DERIVED, NOT STORED. *** Do not add an m_unitsAvailable member.
//          Two variables holding the same fact will disagree eventually; that
//          class of bug is unfixable by inspection. Compute, never duplicate.
//
//     [4f] bool isAvailable() const            -> unitsAvailable() > 0.
//          Note it calls [4e] rather than recomputing. Build methods on methods.
//
//     [4g] ResourceState state() const
//          Returns Busy if m_unitsBusy > 0, else Idle. Single-line conditional.
//
// [5] V1 STUBS -- declare them here, leave the bodies in Resource.cpp empty
//     with a `// TODO v2` inside. Declaring now fixes the interface; that is
//     the point of v1.
//
//     [5a] void seize(int units = 1);
//          v2: assert enough units are available, then add to m_unitsBusy.
//          Note the DEFAULT ARGUMENT -- callers usually seize one unit.
//     [5b] void release(int units = 1);
//          v2: assert m_unitsBusy >= units, then subtract.
//
// [6] Close class with semicolon.

#pragma once
#include <string>
#include <vector>
#include <cstddef>
#include "Common.hpp"

namespace des {


class NodeContext;

// v7: something that seizes this resource and may have entities waiting for it.
// A shared resource must decide WHICH waiting queue gets the freed unit, and it
// cannot do that without asking the candidates.
class IResourceUser {
public:
    virtual ~IResourceUser() = default;
    virtual bool hasWaiting() const = 0;
    // When the longest-waiting entity in this user's queue started waiting.
    virtual SimTime headOfLineSince() const = 0;
    // Take the resource and begin serving. Only called when a unit is free.
    virtual void startFromQueue(NodeContext& ctx) = 0;
};

class Resource{
    private:
        std::string m_name;
        int m_capacity;
        int m_unitsBusy; // INVARIANT: 0 <= m_unitsBusy <= m_capacity

        // v7: the blocks that seize this resource. Non-owning -- the Model owns
        // both. Empty for a resource used by exactly one block, which is every
        // model written before v7.
        std::vector<IResourceUser*> m_users;
    
    public:
        // Sink parameter: taken BY VALUE (no const!) so std::move in the
        // definition can actually steal the buffer. `const std::string` by
        // value would silently fall back to a copy.
        Resource(std::string name, int capacity);
        const std::string& name() const { return m_name; }
        int capacity() const { return m_capacity; }
        int unitsBusy() const { return m_unitsBusy; }
        int unitsAvailable() const { return m_capacity - m_unitsBusy; }
        bool isAvailable() const { return unitsAvailable() > 0; }
        ResourceState state() const { return m_unitsBusy > 0 ? ResourceState::Busy : ResourceState::Idle; }

        // v2.1: put the resource back to its t=0 condition. Without this,
        // calling initialise() a second time leaves units still seized from the
        // previous replication -- and if capacity is 1, the server is busy
        // forever and the second run serves NOBODY.
        void reset();

        // v7: register a block as a user of this resource.
        void addUser(IResourceUser* user);
        std::size_t userCount() const { return m_users.size(); }

        // v7: a unit has just been freed. Offer it to whichever waiting block
        // has the entity that has been waiting LONGEST -- global first-come
        // first-served across every queue that shares this resource.
        //
        // That rule is a MODELLING DECISION, not an implementation detail. The
        // alternative -- fixed block priority, so machine A always beats machine
        // B -- is equally defensible and gives different answers. FCFS is chosen
        // because it is the one people assume when they do not say, and it is
        // documented here so nobody has to read the code to find out.
        void offerFreedUnit(NodeContext& ctx);

        void seize(int units = 1);
        void release(int units = 1);
};

}  // namespace des
