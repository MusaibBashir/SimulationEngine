#include "VariableStore.hpp"
#include <algorithm>
#include "ModelError.hpp"

namespace des {

void VariableStore::noteAttributeNames(std::vector<std::string> names) {
    m_attributeNames = std::move(names);
}

void VariableStore::declare(const std::string& name, double initialValue) {
    if (m_variables.count(name) != 0)
        throw ModelError("variable '" + name + "' is declared twice");

    if (std::find(m_attributeNames.begin(), m_attributeNames.end(), name) !=
        m_attributeNames.end())
        throw ModelError("'" + name + "' is already an entity attribute; a variable may not "
                         "share its name, because then one of the two would read wrong with "
                         "no error");

    Variable v;
    v.initial = initialValue;
    v.current = initialValue;
    m_variables.emplace(name, v);
    m_order.push_back(name);
}

bool VariableStore::has(const std::string& name) const {
    return m_variables.count(name) != 0;
}

double VariableStore::get(const std::string& name) const {
    const auto it = m_variables.find(name);
    if (it == m_variables.end()) throw ModelError("no variable named '" + name + "'");
    return it->second.current;
}

void VariableStore::set(const std::string& name, double value, SimTime now) {
    const auto it = m_variables.find(name);
    if (it == m_variables.end())
        throw ModelError("no variable named '" + name + "' -- declare it before assigning to it");

    // CLOSE THE INTERVAL FIRST, then change the value. Reversed, the new value
    // is credited with time it never held and every time average goes quietly
    // wrong -- the same ordering rule as the engine's run loop.
    Variable& v = it->second;
    v.area += v.current * (now - v.lastChange);
    v.lastChange = now;
    v.current = value;
}

void VariableStore::reset() {
    for (auto& pair : m_variables) {
        Variable& v = pair.second;
        v.current = v.initial;
        v.area = 0.0;
        v.lastChange = 0.0;
    }
    m_measuringSince = 0.0;
    m_lastUpdate = 0.0;
}

void VariableStore::resetStatistics(SimTime now) {
    for (auto& pair : m_variables) {
        Variable& v = pair.second;
        v.area = 0.0;
        v.lastChange = now;      // the VALUE is untouched; only measurement restarts
    }
    m_measuringSince = now;
    m_lastUpdate = now;
}

void VariableStore::updateIntegrals(SimTime now) {
    for (auto& pair : m_variables) {
        Variable& v = pair.second;
        v.area += v.current * (now - v.lastChange);
        v.lastChange = now;
    }
    m_lastUpdate = now;
}

double VariableStore::timeAverage(const std::string& name) const {
    const auto it = m_variables.find(name);
    if (it == m_variables.end()) throw ModelError("no variable named '" + name + "'");
    const SimTime measured = m_lastUpdate - m_measuringSince;
    if (measured <= 0.0) return it->second.current;
    return it->second.area / measured;
}

}  // namespace des
