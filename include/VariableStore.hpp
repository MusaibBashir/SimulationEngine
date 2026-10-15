// ============================================================================
// VariableStore.hpp  --  v10: Arena's Variable data module
// ============================================================================
// An entity ATTRIBUTE travels with one entity; a VARIABLE belongs to the
// system. "How many are in the shop right now", "what shift are we on", "how
// many failed inspection today" are all variables, and none can be said with
// attributes.
//
// ONE NAMESPACE WITH ATTRIBUTES. Declaring a variable named `priority` when an
// attribute of that name exists is a hard error. The alternative is a
// resolution order, and a resolution order means one of the two reads silently
// wrong.
//
// TIME-PERSISTENT, NOT TALLY. A variable's average is weighted by how long it
// held each value, so the integral must be closed BEFORE the value changes --
// the same ordering rule the engine's run loop lives by.

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "Common.hpp"

namespace des {

class VariableStore {
private:
    struct Variable {
        double  initial{0.0};
        double  current{0.0};
        double  area{0.0};
        SimTime lastChange{0.0};
    };

    std::unordered_map<std::string, Variable> m_variables;
    // v16: a table of numbers under one name, read as Name(i) or Name(i, j).
    struct Array {
        int                 rows{1};
        int                 columns{1};
        std::vector<double> initial;
        std::vector<double> current;
    };
    std::unordered_map<std::string, Array> m_arrays;
    std::vector<std::string>               m_arrayOrder;   // NOT in m_order:
                                                           // names() is the time-averaged ones
    std::vector<std::string>                  m_order;          // declaration order
    std::vector<std::string>                  m_attributeNames;
    SimTime                                   m_measuringSince{0.0};
    SimTime                                   m_lastUpdate{0.0};

public:
    // Throws ModelError on a duplicate, or on a collision with a known
    // attribute name.
    void declare(const std::string& name, double initialValue);
    // `initial` holds rows x columns values row by row, or ONE value for all.
    void declareArray(const std::string& name, int rows, int columns,
                      const std::vector<double>& initial);
    bool isArray(const std::string& name) const { return m_arrays.count(name) != 0; }
    int  rowsOf(const std::string& name) const;
    int  columnsOf(const std::string& name) const;
    // 1-based and rounded. Outside the table is a ModelError naming the cell,
    // never a read of whatever happens to be next in memory.
    double element(const std::string& name, double row, double column = 1.0) const;
    std::vector<std::string> arrayNames() const;

    // The attribute names declare() checks against. Set before declaring.
    void noteAttributeNames(std::vector<std::string> names);

    bool   has(const std::string& name) const;
    double get(const std::string& name) const;

    // Closes the integral at `now` before changing the value. Throws if the
    // name was never declared -- auto-declaring on write is how a typo becomes
    // a second variable nobody notices.
    void set(const std::string& name, double value, SimTime now);

    void reset();                             // back to t=0; configuration survives
    void resetStatistics(SimTime now);        // warm-up: discard measurements, keep values
    void updateIntegrals(SimTime now);

    double timeAverage(const std::string& name) const;

    const std::vector<std::string>& names() const { return m_order; }
    std::size_t                     count() const { return m_order.size(); }
};

}  // namespace des
