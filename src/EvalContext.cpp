#include "EvalContext.hpp"
#include <algorithm>
#include "Entity.hpp"
#include "RandomStream.hpp"
#include "Value.hpp"
#include "VariableStore.hpp"

namespace des {

double EvalContext::attribute(const std::string& name) const {
    if (m_entity == nullptr)
        throw ExpressionError("'" + name + "' is an entity attribute, but this field is "
                              "evaluated with no entity");
    return m_entity->attribute(name);
}

const std::string& EvalContext::entityType() const {
    if (m_entity == nullptr)
        throw ExpressionError("Entity.Type read in a field with no entity");
    return m_entity->type();
}

RandomStream& EvalContext::requireRng() const {
    if (m_rng == nullptr)
        throw ExpressionError("this expression samples, but no random stream was supplied");
    return *m_rng;
}

const IModelState& EvalContext::requireState() const {
    if (m_state == nullptr)
        throw ExpressionError("this expression reads model state, but no model state "
                              "was supplied");
    return *m_state;
}

bool ValidationContext::isVariable(const std::string& name) const {
    return variables != nullptr && variables->has(name);
}

bool ValidationContext::isAttribute(const std::string& name) const {
    return std::find(attributeNames.begin(), attributeNames.end(), name) != attributeNames.end();
}

}  // namespace des
