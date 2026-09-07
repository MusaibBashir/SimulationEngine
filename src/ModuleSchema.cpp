#include "ModuleSchema.hpp"
#include "ModuleSchemas.hpp"

namespace des {

const Column* ModuleSchema::column(const std::string& id) const {
    for (const Column& c : columns)
        if (c.id == id) return &c;
    return nullptr;
}

const char* describe(ColumnType type) {
    switch (type) {
        case ColumnType::Text:       return "text";
        case ColumnType::Identifier: return "a name";
        case ColumnType::Integer:    return "a whole number";
        case ColumnType::Real:       return "a number";
        case ColumnType::Boolean:    return "true or false";
        case ColumnType::Enum:       return "one of a fixed set";
        case ColumnType::Expression: return "an expression";
        case ColumnType::Reference:  return "the name of another row";
    }
    return "a value";
}

ModuleRegistry::ModuleRegistry() : m_schemas(buildSchemas()) {}

const ModuleRegistry& ModuleRegistry::instance() {
    static const ModuleRegistry registry;
    return registry;
}

const ModuleSchema* ModuleRegistry::find(const std::string& typeName) const {
    for (const ModuleSchema& s : m_schemas)
        if (s.typeName == typeName) return &s;
    return nullptr;
}

}  // namespace des
