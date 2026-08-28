// ============================================================================
// Node.hpp  --  v6: a model is a FLOWCHART, not a chain
// ============================================================================
// Through v5 a model was a linked list of Stations, each of which did exactly
// one thing: seize a server, hold the entity, release. That covers a queue. It
// does not cover a system.
//
// v6 makes a model a graph of NODES, in the spirit of Arena's Basic Process
// template. Each node does one job to an entity and decides where it goes next:
//
//     Process   seize a resource, delay, release      (the old Station)
//     Delay     hold for a time, no resource          (transport, cooling)
//     Assign    set attributes                        (stamp a type, a due date)
//     Decide    branch, by chance or by condition     (inspection pass/fail)
//     Batch     accumulate N entities into one        (packing, palletising)
//     Separate  split a batch back out, or duplicate
//     Record    tally a value or count something
//     Dispose   leave the system
//
// ---------------------------------------------------------------------------
// THE INTERFACE PROBLEM, AND WHY IT IS ONLY BEING SOLVED NOW
// ---------------------------------------------------------------------------
// Five versions running, this project declined to build an IEventHandler
// hierarchy, and always for the same reason: handler objects living outside
// SimulationSystem would need its internals -- create an entity, seize a
// resource, schedule an event, route onward -- and widening the public
// interface to satisfy an abstraction is a bad trade.
//
// v6 is the version where that reasoning flips, because nodes MUST live outside
// the engine (users will write their own) and they genuinely need those
// operations. The stated trigger was "handlers that carry state"; Batch, which
// accumulates entities across events, is exactly that.
//
// The answer is NOT to make SimulationSystem's members public. It is
// NodeContext: a deliberately narrow facade exposing the six things a node may
// do and nothing else. A node cannot touch the FEL, the statistics, the entity
// table or the clock directly. The engine's invariants stay the engine's.
//
// That is the general shape of the fix whenever "this abstraction needs my
// internals" comes up: publish a role-specific interface, not the whole class.

#pragma once

#include <string>
#include "Common.hpp"
#include "EvalContext.hpp"

namespace des {

class Entity;
class RandomStream;
class SimulationSystem;
class Trace;
class INode;

// The only view of the engine a node ever gets.
class NodeContext {
private:
    SimulationSystem& m_sim;

public:
    explicit NodeContext(SimulationSystem& sim) : m_sim(sim) {}

    SimTime now() const;
    RandomStream& rng();
    Trace& trace();

    // Hand the entity to the next node. `to == nullptr` means it leaves the
    // system: exit statistics are recorded and the entity is destroyed.
    void route(Entity* e, INode* to);

    // Come back to `node` at time `at` with this entity -- the node's
    // onScheduledEvent() will be called. This is how a Process schedules the
    // end of service without ever seeing the future event list.
    void scheduleReturn(SimTime at, Entity* e, INode* node);

    // v7: come back at `at` to check whether this entity is still waiting.
    void scheduleRenegeCheck(SimTime at, Entity* e, INode* node);

    // v9: a source asking to produce again. A separate call rather than reusing
    // scheduleReturn() because the event TYPE differs, and an Arrival carrying
    // no entity would trip the departure handler's assertion -- which is exactly
    // what happened the first time this was wired up.
    void scheduleNextArrival(SimTime at, INode* source);

    // v10: the only way a node evaluates an expression. Pass nullptr where
    // there is no entity -- a Create block's interarrival field.
    // v10: retyping an entity is not a plain setType(). Per-type NumberIn /
    // NumberOut / WIP are keyed by the type an entity HAD when it arrived, so
    // the engine has to move the live count across.
    void           setEntityType(Entity* e, const std::string& type);
    EvalContext    evaluationContext(const Entity* e);
    VariableStore& variables();

    Entity* createEntity();

    // v9: "this entity has just ENTERED the system". Only a Create block calls
    // it. Batch representatives and Separate duplicates are made with
    // createEntity() and are deliberately NOT arrivals -- they are entities the
    // model manufactured, not demand the system received, and counting them
    // would inflate every arrival rate in the report.
    void registerArrival(Entity* e);
    void destroy(Entity* e);
};

// ---------------------------------------------------------------------------

class INode {
protected:
    std::string m_name;
    INode* m_next{nullptr};   // non-owning: the Model owns every node

public:
    explicit INode(std::string name) : m_name(std::move(name)) {}
    virtual ~INode() = default;

    INode(const INode&) = delete;
    INode& operator=(const INode&) = delete;

    const std::string& name() const { return m_name; }
    INode* next() const { return m_next; }
    virtual void setNext(INode* n) { m_next = n; }

    // An entity has arrived here. The node does its job and routes onward --
    // possibly not immediately: a Process may queue the entity for a while, and
    // a Batch may hold it until enough others arrive.
    virtual void enter(NodeContext& ctx, Entity* e) = 0;

    // A previously scheduled return has come due (end of a service or delay).
    // Nodes that never schedule anything never need this.
    virtual void onScheduledEvent(NodeContext& ctx, Entity* e);

    // v7: a patience timer has expired for an entity that was queued here.
    // Default: ignore. See Station::onRenegeTimeout for why "ignore" is the
    // right default rather than an error -- these events are LAZILY CANCELLED.
    virtual void onRenegeTimeout(NodeContext& /*ctx*/, Entity* /*e*/) {}

    // Back to the t=0 condition. Configuration survives, run state does not --
    // the rule every reset() in this project follows.
    virtual void reset() {}

    // v6: discard the statistics collected so far and start measuring from
    // `now`, WITHOUT touching what the block is holding. This is warm-up
    // removal at block level. Without it a block's counters would span the whole
    // run while the Statistics objects respected the warm-up -- two numbers in
    // the same report meaning different periods, which is exactly the kind of
    // quiet inconsistency this project keeps hunting.
    virtual void resetStatistics(SimTime /*now*/) {}

    virtual std::string describe() const = 0;

    // For the stability check. Mean seconds of resource-time this node consumes
    // per entity passing through, divided by capacity; 0 if it holds no
    // resource. Returning 0 means "not checkable", not "definitely fine".
    virtual double loadPerVisit() const { return 0.0; }

    // v10: false when the mean cannot be computed in advance, which is
    // NOT the same as "no load". A block that returns false is reported
    // as unverified rather than passing the stability check silently.
    virtual bool loadIsKnown() const { return true; }
};

}  // namespace des
