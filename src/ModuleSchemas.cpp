#include "ModuleSchemas.hpp"

namespace des {
namespace {

Column text(std::string id, bool required = false, std::string def = "") {
    return Column{std::move(id), ColumnType::Text, required, std::move(def), {}, ""};
}
Column ident(std::string id, bool required = true) {
    return Column{std::move(id), ColumnType::Identifier, required, "", {}, ""};
}
Column integer(std::string id, std::string def, bool required = false) {
    return Column{std::move(id), ColumnType::Integer, required, std::move(def), {}, ""};
}
Column real(std::string id, std::string def, bool required = false) {
    return Column{std::move(id), ColumnType::Real, required, std::move(def), {}, ""};
}
Column boolean(std::string id, std::string def) {
    return Column{std::move(id), ColumnType::Boolean, false, std::move(def), {}, ""};
}
Column expression(std::string id, bool required = true, std::string def = "") {
    return Column{std::move(id), ColumnType::Expression, required, std::move(def), {}, ""};
}
Column enumeration(std::string id, std::vector<std::string> values, std::string def) {
    return Column{std::move(id), ColumnType::Enum, false, std::move(def),
                  std::move(values), ""};
}

// "Block" is the pseudo-type meaning "any flowchart block", which is what an
// exit column points at. Every other Reference names one real module type.
Column ref(std::string id, std::string target, bool required = false) {
    return Column{std::move(id), ColumnType::Reference, required, "", {},
                  std::move(target)};
}

const std::vector<std::string> DISCIPLINES = {"FIFO", "LIFO", "PRIORITY",
                                              "SPT", "EDD", "RANDOM"};

ModuleSchema make(std::string name, ModuleKind kind, std::vector<Column> cols,
                  bool readOnly = false, std::string parent = "") {
    ModuleSchema s;
    s.typeName = std::move(name);
    s.kind = kind;
    s.readOnly = readOnly;
    s.parentColumn = std::move(parent);
    s.columns = std::move(cols);
    return s;
}

}  // namespace

std::vector<ModuleSchema> buildSchemas() {
    std::vector<ModuleSchema> s;

    // --- data modules -------------------------------------------------------
    s.push_back(make("Variable", ModuleKind::Data,
                     {ident("Name"), real("Initial Value", "0")}));
    s.push_back(make("Entity", ModuleKind::Data, {ident("Name")}));
    s.push_back(make("Resource", ModuleKind::Data,
                     {ident("Name"), integer("Capacity", "1", true)}));
    s.push_back(make("Expression", ModuleKind::Data,
                     {ident("Name"), expression("Value")}));

    // Read-only, and the reason is worth stating where somebody will read it:
    // this engine has no queue object apart from its Process. A discipline is
    // set on the Process row, and an editable Queue module would hold that same
    // fact a second time.
    s.push_back(make("Queue", ModuleKind::Data,
                     {ident("Name"), ref("Process", "Process"),
                      enumeration("Discipline", DISCIPLINES, "FIFO")},
                     /*readOnly=*/true));

    // --- flowchart modules --------------------------------------------------
    s.push_back(make("Create", ModuleKind::Flowchart,
                     {ident("Name"), ref("Entity Type", "Entity"),
                      expression("Interarrival"), integer("Max Arrivals", "-1"),
                      real("First At", "0"), integer("Per Arrival", "1"),
                      ref("Next", "Block")}));

    s.push_back(make("Process", ModuleKind::Flowchart,
                     {ident("Name"), integer("Capacity", "1"),
                      ref("Resource", "Resource"), integer("Units", "1"),
                      enumeration("Discipline", DISCIPLINES, "FIFO"),
                      expression("Service"),
                      integer("Balk At", ""), ref("Balk To", "Block"),
                      expression("Renege After", false), ref("Renege To", "Block"),
                      ref("Next", "Block")}));

    s.push_back(make("Delay", ModuleKind::Flowchart,
                     {ident("Name"), expression("Duration"), ref("Next", "Block")}));

    s.push_back(make("Assign", ModuleKind::Flowchart,
                     {ident("Name"), ref("Next", "Block")}));

    s.push_back(make("Decide", ModuleKind::Flowchart,
                     {ident("Name"),
                      enumeration("Type", {"Chance", "Condition"}, "Chance"),
                      ref("Next", "Block")}));

    s.push_back(make("Batch", ModuleKind::Flowchart,
                     {ident("Name"), integer("Size", "2", true),
                      boolean("Permanent", "false"),
                      enumeration("Rule", {"Any", "SameAttribute", "DistinctAttribute"},
                                  "Any"),
                      text("Attribute"), ref("Next", "Block")}));

    s.push_back(make("Separate", ModuleKind::Flowchart,
                     {ident("Name"),
                      enumeration("Mode", {"Split", "Duplicate"}, "Split"),
                      integer("Copies", "1"),
                      ref("Next", "Block"), ref("Duplicate", "Block")}));

    s.push_back(make("Record", ModuleKind::Flowchart,
                     {ident("Name"),
                      enumeration("What", {"Count", "Attribute", "TimeInSystem"},
                                  "Count"),
                      text("Attribute"), ref("Next", "Block")}));

    s.push_back(make("Dispose", ModuleKind::Flowchart, {ident("Name")}));

    // --- child tables -------------------------------------------------------
    // Flat, with a column naming the parent. ROW ORDER IS MEANINGFUL: a Decide
    // takes the first matching branch, and an Assign runs its fields in order,
    // so a later field sees what an earlier one wrote.
    s.push_back(make("DecideBranch", ModuleKind::Child,
                     {ref("Decide", "Decide", true), real("Probability", ""),
                      expression("Condition", false), ref("To", "Block")},
                     /*readOnly=*/false, /*parent=*/"Decide"));

    s.push_back(make("AssignField", ModuleKind::Child,
                     {ref("Assign", "Assign", true),
                      enumeration("Target", {"Attribute", "Variable", "EntityType"},
                                  "Attribute"),
                      text("Name"), expression("Value")},
                     /*readOnly=*/false, /*parent=*/"Assign"));

    return s;
}

}  // namespace des
