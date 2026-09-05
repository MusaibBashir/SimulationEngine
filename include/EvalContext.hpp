// ============================================================================
// EvalContext.hpp  --  v10: the only view of the engine an expression gets
// ============================================================================
// DecideNode must hold an expression, so the core depends on this layer. But
// NQ(Teller) needs live model state, which would point the dependency back up.
//
// v6 solved this exact shape for nodes: NodeContext is a narrow facade, and a
// node cannot touch the FEL or the statistics. Same move here. This layer
// DEFINES IModelState -- the four questions an expression may ask -- and
// SimulationSystem IMPLEMENTS it, so the expression layer never includes
// Model.hpp.

#pragma once
#include <string>
#include <vector>
#include "Common.hpp"

namespace des {

class Entity;
class RandomStream;
class VariableStore;

class IModelState {
public:
    virtual ~IModelState() = default;
    virtual double  queueLength(const std::string& blockName) const = 0;
    virtual double  resourceBusy(const std::string& name) const = 0;
    virtual double  resourceCapacity(const std::string& name) const = 0;
    virtual double  numberInSystem() const = 0;
    virtual SimTime now() const = 0;
};

// Every member may be null, and that is meaningful: a Create block's
// interarrival expression is evaluated with NO entity, because there isn't one
// yet. Reading an attribute through a null entity throws -- validate() catches
// almost all of those before the run starts.
class EvalContext {
private:
    const Entity*      m_entity;
    VariableStore*     m_variables;
    const IModelState* m_state;
    RandomStream*      m_rng;

public:
    EvalContext(const Entity* entity, VariableStore* variables,
                const IModelState* state, RandomStream* rng)
        : m_entity(entity), m_variables(variables), m_state(state), m_rng(rng) {}

    const Entity*      entity() const    { return m_entity; }
    VariableStore*     variables() const { return m_variables; }
    const IModelState* state() const     { return m_state; }
    RandomStream*      rng() const       { return m_rng; }

    // Each throws ExpressionError naming what was missing, rather than
    // returning a plausible default.
    double             attribute(const std::string& name) const;
    const std::string& entityType() const;
    RandomStream&      requireRng() const;
    const IModelState& requireState() const;
};

// Whether the field being validated has an entity to read from at all.
enum class FieldContext { HasEntity, NoEntity };

struct ValidationContext {
    FieldContext             field{FieldContext::HasEntity};
    const VariableStore*     variables{nullptr};
    std::vector<std::string> attributeNames;

    bool isVariable(const std::string& name) const;
    bool isAttribute(const std::string& name) const;
};

}  // namespace des
