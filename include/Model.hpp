// ============================================================================
// Model.hpp  --  WHAT is simulated, separate from the machinery
// ============================================================================
// v3 made this the architecture step: the engine takes a Model and runs it, and
// knows nothing about tellers or restaurants.
//
// v6 makes a Model a FLOWCHART rather than a chain. It owns a set of nodes --
// Process, Delay, Assign, Decide, Batch, Separate, Record, Dispose -- wired
// together by route(). That is the Arena Basic Process idea: you draw the
// system, and the engine walks entities through it.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Common.hpp"
#include "Node.hpp"
#include "Nodes.hpp"
#include "Create.hpp"
#include "Station.hpp"
#include "Distribution.hpp"
#include "ModelError.hpp"
#include "VariableStore.hpp"

namespace des {

class Model {
public:
    // An attribute every arriving entity is given, drawn from its own
    // distribution. This is how a model says "customers have a priority" or
    // "jobs have a due date" -- and it is what makes the Priority, SPT and EDD
    // disciplines do anything at all.
    struct ArrivalAttribute {
        std::string name;
        std::unique_ptr<IDistribution> distribution;
    };

    // The result of walking the flowchart: how many times an average arriving
    // entity passes through each node. Needed for the stability check the
    // moment branching exists -- a station on a 10% branch sees a tenth of the
    // work, and treating every station as seeing all of it would reject
    // perfectly good models.
    struct VisitRatios {
        std::unordered_map<const INode*, double> visits;
        // False when a condition-based Decide was involved: the split then
        // depends on entity state, which is an OUTPUT of the simulation, not
        // something that can be computed beforehand. The engine says so rather
        // than guessing.
        bool exact{true};
    };

private:
    std::vector<std::unique_ptr<INode>> m_nodes;      // the Model OWNS them

    // v7: the Model owns the resources, not the Process blocks. That is what
    // makes "one operator shared across three machines" expressible -- several
    // blocks can point at the same Resource and compete for it.
    std::vector<std::unique_ptr<Resource>> m_resources;
    std::vector<Station*> m_processes;                // non-owning, for reports
    std::vector<CreateNode*> m_sources;               // non-owning, for reports
    bool m_allowOverload{false};
    std::vector<ArrivalAttribute> m_arrivalAttributes;
    VariableStore m_variables;
    std::unique_ptr<IDistribution> m_interarrival;
    INode* m_entry{nullptr};

    INode* add(std::unique_ptr<INode> node);
    AssignNode& assignBlock(const std::string& name);
    void requireUnique(const std::string& name) const;

public:
    Model() = default;
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // --- arrivals ---
    // v9: a named arrival source. Several may coexist -- three ball types
    // arriving on their own schedules, each capped, is three of these.
    Model& source(const std::string& name, const std::string& entityType,
                  std::unique_ptr<IDistribution> interarrival,
                  long long maxArrivals = -1, SimTime firstAt = 0.0,
                  int entitiesPerArrival = 1);

    // Shorthand for the single-source case: creates a source called "Arrivals".
    Model& arrivals(std::unique_ptr<IDistribution> d);
    // v10: the same fields written as text. EXPO(0.8), 5, or size * 0.5 --
    // Arena makes no distinction and neither does this.
    Model& arrivals(const std::string& interarrivalText);
    Model& source(const std::string& name, const std::string& entityType,
                  const std::string& interarrivalText, long long maxArrivals = -1,
                  SimTime firstAt = 0.0, int entitiesPerArrival = 1);

    // *** v9: run a model whose queues grow without bound. ***
    // The stability check has refused rho >= 1 since v5, and for a STEADY-STATE
    // study that is right: the queue never settles, so "average wait" is a
    // function of run length rather than a property of the system.
    //
    // But a TERMINATING simulation -- "run the clinic for 4 hours and tell me
    // what happened" -- is a perfectly good question even when work arrives
    // faster than it can be done. That is what an overloaded shift IS, and
    // refusing to model it was conflating two different kinds of study.
    //
    // Turning this on says you know. The report then says so too, so the numbers
    // are never quietly presented as steady-state results.
    Model& allowOverload(bool on = true);
    bool overloadAllowed() const { return m_allowOverload; }
    void setInterarrival(std::unique_ptr<IDistribution> d) { arrivals(std::move(d)); }
    // v10: Arena's Variable data module. Global, numeric, shared by every
    // block -- the thing an attribute cannot say, because an attribute travels
    // with one entity and this belongs to the system.
    Model& variable(const std::string& name, double initialValue = 0.0);
    VariableStore&       variables()       { return m_variables; }
    const VariableStore& variables() const { return m_variables; }

    Model& attribute(const std::string& name, std::unique_ptr<IDistribution> d);
    void assignOnArrival(const std::string& n, std::unique_ptr<IDistribution> d) { attribute(n, std::move(d)); }
    const std::vector<ArrivalAttribute>& arrivalAttributes() const { return m_arrivalAttributes; }

    // --- blocks ---------------------------------------------------------
    // Each returns *this so a whole flowchart reads as one statement. Where you
    // need the node itself (to configure it further) the typed accessors below
    // fetch it by name.
    // Declare a shared resource. Several Process blocks may then seize it.
    Model& resource(const std::string& name, int capacity);
    Resource* resourceNamed(const std::string& name);
    const Resource* resourceNamed(const std::string& name) const;

    // A Process with its OWN private resource of the given capacity -- the v3
    // through v6 behaviour, and still the right thing for a plain queue.
    Model& station(const std::string& name, int capacity,
                   QueueDiscipline discipline, std::unique_ptr<IDistribution> service);
    Model& station(const std::string& name, int capacity,
                   QueueDiscipline discipline, const std::string& serviceText);
    Model& station(const std::string& name, int capacity,
                   QueueDiscipline discipline, ExpressionPtr service);

    // A Process that seizes `units` of an already-declared SHARED resource.
    Model& stationUsing(const std::string& name, const std::string& resourceName,
                        QueueDiscipline discipline, std::unique_ptr<IDistribution> service,
                        int units = 1);
    Model& stationUsing(const std::string& name, const std::string& resourceName,
                        QueueDiscipline discipline, const std::string& serviceText,
                        int units = 1);

    // v7: balking and reneging, configured on an existing Process block.
    //   balk    -- refuse to join a queue already this long
    //   renege  -- join, then give up after a random patience
    // Passing an empty target name means "leaves the system".
    Model& balkAt(const std::string& processName, std::size_t queueLength,
                  const std::string& balkTo = "");
    Model& renegeAfter(const std::string& processName,
                       std::unique_ptr<IDistribution> patience,
                       const std::string& renegeTo = "");

    // v7: N-way Decide. Start one, then add branches in order.
    Model& decideNWayByChance(const std::string& name);
    Model& decideNWayByCondition(const std::string& name);
    Model& branch(const std::string& decideName, double probability, const std::string& to);
    Model& branch(const std::string& decideName, DecideNode::Condition condition,
                  const std::string& to);
    // v10: the same branch written as text. The parse happens here, so a
    // malformed condition is a ModelError at build time -- on the C++ API it IS
    // a programmer error. v11's compile() uses parseExpression() instead and
    // collects diagnostics against the cell.
    Model& branch(const std::string& decideName, ExpressionPtr condition,
                  const std::string& to);
    Model& branchWhen(const std::string& decideName, const std::string& conditionText,
                      const std::string& to);
    Model& process(const std::string& name, int capacity,
                   QueueDiscipline discipline, std::unique_ptr<IDistribution> service) {
        return station(name, capacity, discipline, std::move(service));
    }
    Model& delay(const std::string& name, std::unique_ptr<IDistribution> duration);
    Model& delay(const std::string& name, const std::string& durationText);
    Model& assign(const std::string& name, const std::string& attributeName,
                  std::unique_ptr<IDistribution> value);
    // v10. assignTo writes an attribute from text; assignVariable writes a
    // global; assignEntityType restamps the entity's type.
    Model& assignTo(const std::string& block, const std::string& attributeName,
                    const std::string& valueText);
    Model& assignVariable(const std::string& block, const std::string& variableName,
                          const std::string& valueText);
    Model& assignEntityType(const std::string& block, const std::string& typeText);
    Model& decideByChance(const std::string& name, double probabilityTrue);
    Model& decideByCondition(const std::string& name, DecideNode::Condition condition);
    Model& decideByCondition(const std::string& name, ExpressionPtr condition);
    Model& decideWhen(const std::string& name, const std::string& conditionText);
    Model& batch(const std::string& name, std::size_t size, bool permanent = false);
    // Group entities that AGREE on an attribute (same lot, same order).
    Model& batchBySameAttribute(const std::string& name, std::size_t size,
                                const std::string& attribute, bool permanent = false);
    // Group entities that DIFFER -- one of each type. This is the rule plain
    // batching cannot fake: with three streams running at different speeds, the
    // first three to arrive are often three of the same thing.
    Model& batchOneOfEach(const std::string& name, std::size_t size,
                          const std::string& attribute, bool permanent = false);
    Model& separate(const std::string& name);                 // split a batch
    Model& duplicate(const std::string& name, int copies);    // clone
    Model& record(const std::string& name);                              // count
    Model& recordAttribute(const std::string& name, const std::string& attributeName);
    Model& recordTimeInSystem(const std::string& name);
    Model& dispose(const std::string& name);

    // --- wiring ---
    Model& route(const std::string& from, const std::string& to);
    // A Decide's TRUE branch. Its false branch is plain route(), so a Decide with
    // only a true branch set falls through to whatever comes next.
    Model& routeTrue(const std::string& decideName, const std::string& to);

    // A Separate's DUPLICATE exit. Its Original exit is plain route(), so a
    // Separate with only route() set sends copies the same way as the original.
    Model& routeDuplicate(const std::string& separateName, const std::string& to);
    Model& entryAt(const std::string& name);
    void connect(const std::string& f, const std::string& t) { route(f, t); }
    void setEntry(const std::string& n) { entryAt(n); }

    // --- access ---
    INode* node(const std::string& name);
    const INode* node(const std::string& name) const;
    INode* entry() const { return m_entry; }

    // Typed lookup. Throws ModelError if the name is absent or the wrong kind,
    // rather than handing back a null the caller will dereference three lines
    // later with no idea which of the two mistakes they made.
    template <typename T> T& nodeAs(const std::string& name);

    Station* station(const std::string& name);
    const Station* station(const std::string& name) const;

    // Process nodes only -- these are the ones with a resource and a queue, and
    // the only ones a utilisation table means anything for.
    std::size_t stationCount() const { return m_processes.size(); }
    Station& stationAt(std::size_t i) { return *m_processes[i]; }
    const Station& stationAt(std::size_t i) const { return *m_processes[i]; }
    std::size_t sourceCount() const { return m_sources.size(); }
    CreateNode& sourceAt(std::size_t i) { return *m_sources[i]; }
    const CreateNode& sourceAt(std::size_t i) const { return *m_sources[i]; }
    std::size_t nodeCount() const { return m_nodes.size(); }
    INode& nodeAt(std::size_t i) { return *m_nodes[i]; }
    const INode& nodeAt(std::size_t i) const { return *m_nodes[i]; }

    // v10: an expression's mean may be unknowable, so the offered-load check
    // cannot always be applied. Reporting that is not the same as passing.
    struct StabilityReport {
        bool                     checked{true};
        std::vector<std::string> unverifiable;   // block names
        double                   maxUtilisation{0.0};
    };
    StabilityReport stability() const;

    // False when ANY source's interarrival mean cannot be computed in
    // advance. Then lambda is unknown, so no station's offered load is
    // knowable either -- however simple its own service time looks.
    bool arrivalRateIsKnown() const;

    // Every expression in the model, checked against declared names before the
    // run. v11 calls the same walk and turns each Diagnostic into a cell
    // reference instead of throwing.
    std::vector<Diagnostic> checkExpressions() const;

    // --- analysis ---
    VisitRatios visitRatios() const;
    double offeredLoad(const Station& s) const;

    void wireSources();
    void reset();
    void validate() const;
    std::string describe() const;
};

template <typename T>
T& Model::nodeAs(const std::string& name) {
    INode* n = node(name);
    if (n == nullptr) throw ModelError("no node named '" + name + "'");
    T* typed = dynamic_cast<T*>(n);
    if (typed == nullptr)
        throw ModelError("node '" + name + "' is not the kind of block you asked for");
    return *typed;
}

}  // namespace des
