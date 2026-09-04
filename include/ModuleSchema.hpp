// ============================================================================
// ModuleSchema.hpp  --  v11: what columns a module type has
// ============================================================================
// THIS IS THE WHOLE CONTRACT WITH A FRONT END. A front end that can render one
// table from a schema can render every module type, including ones added in a
// later version, without being changed. That property is why schemas are
// discovered at runtime rather than compiled into whatever draws them.

#pragma once
#include <string>
#include <vector>

namespace des {

enum class ColumnType {
    Text, Identifier, Integer, Real, Boolean,
    Enum,        // a fixed set of spellings
    Expression,  // parsed by v10: a duration, a condition, or a value
    Reference    // names a row in another module type
};

// Child is a repeating group flattened into its own table -- a Decide's
// branches, an Assign's fields. Flat everywhere means one thing to render.
enum class ModuleKind { Flowchart, Data, Child };

struct Column {
    std::string              id;
    ColumnType               type{ColumnType::Text};
    bool                     required{false};
    std::string              defaultValue;
    std::vector<std::string> enumValues;      // Enum only
    std::string              referencedType;  // Reference only
};

struct ModuleSchema {
    std::string typeName;
    ModuleKind  kind{ModuleKind::Data};

    // A derived view rather than an editable table. Queue is the first and so
    // far only user: this engine has no queue object apart from its Process, so
    // an editable Queue module would hold the same fact as the Process row's
    // Discipline column.
    bool                readOnly{false};
    std::string         parentColumn;   // Child only
    std::vector<Column> columns;

    const Column* column(const std::string& id) const;
};

class ModuleRegistry {
private:
    std::vector<ModuleSchema> m_schemas;
    ModuleRegistry();

public:
    static const ModuleRegistry& instance();

    const ModuleSchema*              find(const std::string& typeName) const;
    const std::vector<ModuleSchema>& all() const { return m_schemas; }
};

const char* describe(ColumnType type);   // for diagnostics

}  // namespace des
