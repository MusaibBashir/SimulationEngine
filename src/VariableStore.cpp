#include "VariableStore.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include "ModelError.hpp"

namespace des {

void VariableStore::noteAttributeNames(std::vector<std::string> names) {
    m_attributeNames = std::move(names);
}

void VariableStore::declare(const std::string& name, double initialValue) {
    if (m_arrays.count(name) != 0)
        throw ModelError("variable '" + name + "' is declared twice");
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

void VariableStore::declareArray(const std::string& name, int rows, int columns,
                                 const std::vector<double>& initial) {
    if (m_variables.count(name) != 0 || m_arrays.count(name) != 0)
        throw ModelError("variable '" + name + "' is declared twice");
    if (std::find(m_attributeNames.begin(), m_attributeNames.end(), name) !=
        m_attributeNames.end())
        throw ModelError("'" + name + "' is already an entity attribute; a variable may not "
                         "share its name");
    if (rows < 1 || columns < 1)
        throw ModelError("array '" + name + "' needs at least one row and one column");
    const std::size_t cells = static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
    if (initial.size() != 1 && initial.size() != cells) {
        std::ostringstream os;
        os << "array '" << name << "' is " << rows << " x " << columns << " = " << cells
           << " cells, but " << initial.size() << " initial values were given "
              "(give one for every cell, or one for all of them)";
        throw ModelError(os.str());
    }
    Array a;
    a.rows = rows;
    a.columns = columns;
    a.initial = initial.size() == 1 ? std::vector<double>(cells, initial[0]) : initial;
    a.current = a.initial;
    m_arrays.emplace(name, std::move(a));
    m_arrayOrder.push_back(name);
}

int VariableStore::rowsOf(const std::string& name) const {
    const auto it = m_arrays.find(name);
    return it == m_arrays.end() ? 0 : it->second.rows;
}

int VariableStore::columnsOf(const std::string& name) const {
    const auto it = m_arrays.find(name);
    return it == m_arrays.end() ? 0 : it->second.columns;
}

double VariableStore::element(const std::string& name, double row, double column) const {
    const auto it = m_arrays.find(name);
    if (it == m_arrays.end()) throw ModelError("no array named '" + name + "'");
    const Array& a = it->second;
    const long long r = std::llround(row);
    const long long c = std::llround(column);
    if (r < 1 || r > a.rows || c < 1 || c > a.columns) {
        std::ostringstream os;
        os << name << "(" << r;
        if (a.columns > 1) os << ", " << c;
        os << ") is outside its " << a.rows << " x " << a.columns << " cells";
        throw ModelError(os.str());
    }
    return a.current[static_cast<std::size_t>((r - 1) * a.columns + (c - 1))];
}

std::vector<std::string> VariableStore::arrayNames() const {
    return m_arrayOrder;
}

bool VariableStore::has(const std::string& name) const {
    return m_variables.count(name) != 0;
}

double VariableStore::get(const std::string& name) const {
    if (isArray(name))
        throw ModelError(name + " is an array -- read it as " + name +
                         (columnsOf(name) > 1 ? "(row, column)" : "(index)"));
    const auto it = m_variables.find(name);
    if (it == m_variables.end()) throw ModelError("no variable named '" + name + "'");
    return it->second.current;
}

void VariableStore::set(const std::string& name, double value, SimTime now) {
    if (isArray(name))
        throw ModelError("assigning to an element of array '" + name +
                         "' is not supported yet");
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
    for (auto& pair : m_arrays) pair.second.current = pair.second.initial;
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
