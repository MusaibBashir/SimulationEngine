#include "ModuleSchemas.hpp"

namespace des {

// PLACEHOLDER -- Task 2 replaces this file wholesale with the real schema set.
std::vector<ModuleSchema> buildSchemas() {
    std::vector<ModuleSchema> s;
    ModuleSchema placeholder;
    placeholder.typeName = "Placeholder";
    placeholder.kind = ModuleKind::Data;
    placeholder.columns.push_back(Column{"Name", ColumnType::Identifier, true, "", {}, ""});
    s.push_back(placeholder);
    return s;
}

}  // namespace des
