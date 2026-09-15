#include "ModuleSchemas.hpp"

namespace des {
namespace {

// v14: every column carries one line saying what it is for, and every module
// says what it is and what Arena calls it when the names differ. The helpers
// below take that line last, so a column cannot be declared without one -- and
// a test walks the registry and fails on any that is empty.

Column text(std::string id, std::string help, bool required = false,
            std::string def = "") {
    return Column{std::move(id), ColumnType::Text, required, std::move(def), {}, "",
                  std::move(help)};
}
Column ident(std::string id, std::string help, bool required = true) {
    return Column{std::move(id), ColumnType::Identifier, required, "", {}, "",
                  std::move(help)};
}
Column integer(std::string id, std::string def, std::string help,
               bool required = false) {
    return Column{std::move(id), ColumnType::Integer, required, std::move(def), {}, "",
                  std::move(help)};
}
Column real(std::string id, std::string def, std::string help,
            bool required = false) {
    return Column{std::move(id), ColumnType::Real, required, std::move(def), {}, "",
                  std::move(help)};
}
Column boolean(std::string id, std::string def, std::string help) {
    return Column{std::move(id), ColumnType::Boolean, false, std::move(def), {}, "",
                  std::move(help)};
}
Column expression(std::string id, std::string help, bool required = true,
                  std::string def = "") {
    return Column{std::move(id), ColumnType::Expression, required, std::move(def), {}, "",
                  std::move(help)};
}
Column enumeration(std::string id, std::vector<std::string> values, std::string def,
                   std::string help) {
    return Column{std::move(id), ColumnType::Enum, false, std::move(def),
                  std::move(values), "", std::move(help)};
}

// "Block" is the pseudo-type meaning "any flowchart block", which is what an
// exit column points at. Every other Reference names one real module type.
Column ref(std::string id, std::string target, std::string help,
           bool required = false) {
    return Column{std::move(id), ColumnType::Reference, required, "", {},
                  std::move(target), std::move(help)};
}

const std::vector<std::string> DISCIPLINES = {"FIFO", "LIFO", "PRIORITY",
                                              "SPT", "EDD", "RANDOM"};

ModuleSchema make(std::string name, ModuleKind kind, std::string help,
                  std::vector<Column> cols,
                  bool readOnly = false, std::string parent = "") {
    ModuleSchema s;
    s.typeName = std::move(name);
    s.kind = kind;
    s.readOnly = readOnly;
    s.parentColumn = std::move(parent);
    s.help = std::move(help);
    s.columns = std::move(cols);
    return s;
}

}  // namespace

std::vector<ModuleSchema> buildSchemas() {
    std::vector<ModuleSchema> s;

    // --- data modules -------------------------------------------------------
    s.push_back(make("Variable", ModuleKind::Data,
                     "A global number any block can read and write. Arena's "
                     "Variable data module. Use it to count things across the "
                     "whole model -- an attribute belongs to one entity and "
                     "cannot.",
                     {ident("Name", "What expressions call it, for example "
                                    "Rejected in \"Rejected + 1\"."),
                      real("Initial Value", "0",
                           "Its value at the start of every replication.")}));

    s.push_back(make("Entity", ModuleKind::Data,
                     "A named kind of entity, so results can be reported per "
                     "type. Arena's Entity data module, minus the picture -- "
                     "this engine has nothing to draw.",
                     {ident("Name", "The type name a Create gives its "
                                    "entities, for example Customer.")}));

    s.push_back(make("Resource", ModuleKind::Data,
                     "A pool of servers that Processes seize and release. "
                     "Arena's Resource data module. Declare it here once and "
                     "name it from every Process that shares it.",
                     {ident("Name", "What a Process names in its Resource "
                                    "column."),
                      integer("Capacity", "1",
                              "How many entities it can serve at once.", true)}));

    s.push_back(make("Expression", ModuleKind::Data,
                     "A named expression, so one formula can be written once "
                     "and used in several places. Arena's Expression data "
                     "module.",
                     {ident("Name", "What other cells call it."),
                      expression("Value",
                                 "The expression itself, for example "
                                 "TRIA(1, 2, 5).")}));

    // Arena's Run Setup, as a module like any other.
    s.push_back(make("Run", ModuleKind::Data,
                     "How long to run and how many times. Arena keeps this in "
                     "Run Setup; here it is a module, so the file says "
                     "everything needed to reproduce a result. Several may coexist -- "
                     "name them and choose which to run.",
                     {ident("Name", "A label for this row. Any name will do."),
                      real("Length", "",
                           "Stop at this simulated time. Leave blank to run "
                           "until the model runs out of events."),
                      real("Warm-up", "0",
                           "Discard everything measured before this time. The "
                           "run keeps going; only the statistics restart."),
                      integer("Replications", "1",
                              "How many independent runs. More than one gets "
                              "you a confidence interval instead of a single "
                              "number."),
                      integer("Base Seed", "12345",
                              "Replication r uses this seed plus r, so a whole "
                              "study is reproducible from one number."),
                      boolean("Stop When Drained", "false",
                              "Stop once the system is empty. Beware: it is met "
                              "the FIRST time nothing is in the system, which "
                              "is often just after the first entity leaves."),
                      integer("Max Entities", "",
                              "Stop once this many entities have been served."),
                      boolean("Separate Streams", "false",
                              "Give every distribution its own random stream, "
                              "so changing a service time does not shift the "
                              "arrival pattern. Needed for common random "
                              "numbers."),
                      boolean("Antithetic", "false",
                              "Run each replication twice, the second time with "
                              "every random draw mirrored, and average the "
                              "pair. Lower variance for the same compute.")}));

    // Read-only, and the reason is worth stating where somebody will read it:
    // this engine has no queue object apart from its Process. A discipline is
    // set on the Process row, and an editable Queue module would hold that same
    // fact a second time.
    s.push_back(make("Queue", ModuleKind::Data,
                     "READ ONLY. Arena has an editable Queue data module; this "
                     "engine has no queue object apart from its Process, so "
                     "the discipline is set on the Process row and this is only "
                     "a view of it.",
                     {ident("Name", "The queue's name, derived from its "
                                    "Process."),
                      ref("Process", "Process",
                          "The Process this queue belongs to."),
                      enumeration("Discipline", DISCIPLINES, "FIFO",
                                  "The order it serves in. Set it on the "
                                  "Process row, not here.")},
                     /*readOnly=*/true));

    // --- flowchart modules --------------------------------------------------
    s.push_back(make("Create", ModuleKind::Flowchart,
                     "Where entities enter the model. Arena's Create module. A "
                     "model needs at least one, and several may coexist -- "
                     "three arrival streams is three of these.",
                     {ident("Name", "This block's name. Other blocks route to "
                                    "it by this."),
                      ref("Entity Type", "Entity",
                          "What kind of entity to make. Results are reported "
                          "per type."),
                      expression("Interarrival",
                                 "Time between arrivals, for example EXPO(1.0). "
                                 "A mean of 1.0 is one arrival per time unit."),
                      integer("Max Arrivals", "-1",
                              "Stop after this many arrivals. -1 means never "
                              "stop."),
                      real("First At", "0",
                           "When the first arrival happens."),
                      integer("Per Arrival", "1",
                              "How many entities arrive together each time."),
                      ref("Next", "Block",
                          "Where entities go from here. THIS IS THE "
                          "CONNECTION: Arena draws a line, this engine names "
                          "the next block. Blank means they leave the "
                          "system.")}));

    s.push_back(make("Process", ModuleKind::Flowchart,
                     "Seize a server, be delayed, release it. Arena's Process "
                     "module with Action = Seize Delay Release. Leave Resource "
                     "blank for a server used by this block alone.",
                     {ident("Name", "This block's name, and the name of its "
                                    "queue in the report."),
                      integer("Capacity", "1",
                              "How many at once, when this block has its own "
                              "server. Ignored if Resource names a shared one."),
                      ref("Resource", "Resource",
                          "A shared Resource to seize, declared in the Resource "
                          "module. Blank means this block has its own."),
                      integer("Units", "1",
                              "How many units of that Resource each entity "
                              "seizes."),
                      enumeration("Discipline", DISCIPLINES, "FIFO",
                                  "The order the queue is served in. PRIORITY, "
                                  "SPT and EDD read an attribute."),
                      expression("Service",
                                 "How long the work takes, for example "
                                 "EXPO(0.8). May read an attribute, so "
                                 "size * 0.5 is a service time."),
                      integer("Balk At", "",
                              "Refuse to join a queue already this long. Blank "
                              "means always join."),
                      ref("Balk To", "Block",
                          "Where a balking entity goes. Blank means it leaves "
                          "the system."),
                      expression("Renege After",
                                 "How long an entity will wait before giving "
                                 "up, for example 5. Blank means it waits "
                                 "forever.", false),
                      ref("Renege To", "Block",
                          "Where an entity that gave up goes. Blank means it "
                          "leaves the system."),
                      ref("Next", "Block",
                          "Where entities go after service. THIS IS THE "
                          "CONNECTION: Arena draws a line between two modules, "
                          "this engine names the next block here. Blank means "
                          "they leave the system.")}));

    s.push_back(make("Delay", ModuleKind::Flowchart,
                     "Wait, with no server and no queue. Arena's Delay module. "
                     "Use it for travel time or anything that takes time "
                     "without contending for a resource.",
                     {ident("Name", "This block's name."),
                      expression("Duration",
                                 "How long the delay lasts, for example "
                                 "UNIF(1, 3)."),
                      ref("Next", "Block",
                          "Where entities go next. Blank means they leave the "
                          "system.")}));

    s.push_back(make("Assign", ModuleKind::Flowchart,
                     "Set attributes, variables or the entity's type. Arena's "
                     "Assign module. The fields it sets are rows in the "
                     "AssignField table, and they run IN ORDER.",
                     {ident("Name", "This block's name. AssignField rows name "
                                    "it to attach to it."),
                      ref("Next", "Block",
                          "Where entities go next. Blank means they leave the "
                          "system.")}));

    s.push_back(make("Decide", ModuleKind::Flowchart,
                     "Send entities different ways. Arena's Decide module. The "
                     "branches are rows in the DecideBranch table -- Arena puts "
                     "them in a grid inside the dialog; here they are their own "
                     "table.",
                     {ident("Name", "This block's name. DecideBranch rows name "
                                    "it to attach to it."),
                      enumeration("Type", {"Chance", "Condition"}, "Chance",
                                  "Chance uses each branch's Probability; "
                                  "Condition evaluates each branch's Condition "
                                  "in order."),
                      ref("Next", "Block",
                          "The ELSE exit: where an entity goes when no branch "
                          "matches.")}));

    s.push_back(make("Batch", ModuleKind::Flowchart,
                     "Collect entities into a group. Arena's Batch module. A "
                     "temporary batch can be split again later by a Separate; "
                     "a permanent one consumes its members.",
                     {ident("Name", "This block's name."),
                      integer("Size", "2", "How many entities make one batch.",
                              true),
                      boolean("Permanent", "false",
                              "true consumes the members; false keeps them so a "
                              "Separate can split the batch later."),
                      enumeration("Rule", {"Any", "SameAttribute",
                                           "DistinctAttribute"}, "Any",
                                  "Any takes whoever is waiting. SameAttribute "
                                  "groups entities that AGREE on Attribute. "
                                  "DistinctAttribute takes one of each."),
                      text("Attribute",
                           "Which attribute the rule matches on. Only used by "
                           "SameAttribute and DistinctAttribute."),
                      ref("Next", "Block",
                          "Where the batch goes next. Blank means it leaves the "
                          "system.")}));

    s.push_back(make("Separate", ModuleKind::Flowchart,
                     "Split a batch back into its members, or clone an entity. "
                     "Arena's Separate module. Split only works on a batch that "
                     "was made temporary.",
                     {ident("Name", "This block's name."),
                      enumeration("Mode", {"Split", "Duplicate"}, "Split",
                                  "Split breaks a temporary batch apart. "
                                  "Duplicate makes copies of one entity."),
                      integer("Copies", "1",
                              "How many copies Duplicate makes. Ignored by "
                              "Split."),
                      ref("Next", "Block",
                          "Where the original goes. Blank means it leaves the "
                          "system."),
                      ref("Duplicate", "Block",
                          "Where the copies go. Arena calls these the Original "
                          "and Duplicate exits.")}));

    s.push_back(make("Record", ModuleKind::Flowchart,
                     "Count something, or record a number, where entities pass. "
                     "Arena's Record module.",
                     {ident("Name", "This block's name, and the name the "
                                    "statistic is reported under."),
                      enumeration("What", {"Count", "Attribute", "TimeInSystem"},
                                  "Count",
                                  "Count tallies entities passing. Attribute "
                                  "records the value of Attribute. TimeInSystem "
                                  "records how long the entity has been in the "
                                  "model."),
                      text("Attribute",
                           "Which attribute to record. Only used when What is "
                           "Attribute."),
                      ref("Next", "Block",
                          "Where entities go next. Blank means they leave the "
                          "system.")}));

    s.push_back(make("Dispose", ModuleKind::Flowchart,
                     "Where entities leave the model. Arena's Dispose module. "
                     "A block whose Next is blank disposes of its entities too; "
                     "a named Dispose is how you count them separately.",
                     {ident("Name", "This block's name, and the name its exit "
                                    "count is reported under.")}));

    // --- child tables -------------------------------------------------------
    // Flat, with a column naming the parent. ROW ORDER IS MEANINGFUL: a Decide
    // takes the first matching branch, and an Assign runs its fields in order,
    // so a later field sees what an earlier one wrote.
    s.push_back(make("DecideBranch", ModuleKind::Child,
                     "One branch of a Decide. Arena puts these in a grid inside "
                     "the Decide dialog; here they are their own table so every "
                     "module has the same shape. ROW ORDER MATTERS: the first "
                     "branch that matches wins.",
                     {ref("Decide", "Decide",
                          "Which Decide this branch belongs to.", true),
                      real("Probability", "",
                           "The chance of taking this branch, from 0 to 1. Used "
                           "when the Decide's Type is Chance."),
                      expression("Condition",
                                 "A test, for example size > 7. Used when the "
                                 "Decide's Type is Condition.", false),
                      ref("To", "Block",
                          "Where an entity taking this branch goes.")},
                     /*readOnly=*/false, /*parent=*/"Decide"));

    s.push_back(make("AssignField", ModuleKind::Child,
                     "One field an Assign sets. Arena puts these in a grid "
                     "inside the Assign dialog. ROW ORDER MATTERS: a later "
                     "field sees what an earlier one wrote.",
                     {ref("Assign", "Assign",
                          "Which Assign this field belongs to.", true),
                      enumeration("Target", {"Attribute", "Variable",
                                             "EntityType"}, "Attribute",
                                  "Attribute belongs to this entity. Variable "
                                  "is global and must be declared. EntityType "
                                  "changes what the entity IS."),
                      text("Name",
                           "Which attribute or variable to set. Leave blank "
                           "when Target is EntityType."),
                      expression("Value",
                                 "What to set it to, for example UNIF(1, 4) or "
                                 "Rejected + 1.")},
                     /*readOnly=*/false, /*parent=*/"Assign"));

    return s;
}

}  // namespace des
