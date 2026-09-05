#include "Compiler.hpp"
#include <cstdlib>
#include "ModuleSchema.hpp"
#include "Parser.hpp"
#include "Nodes.hpp"

namespace des {
namespace {

void report(std::vector<Diagnostic>& out, Severity severity,
            const std::string& type, std::size_t row, const std::string& column,
            const std::string& message, SourceSpan span = SourceSpan{}) {
    out.push_back(Diagnostic{severity, span, message, CellRef{type, row, column}});
}

bool looksLikeInteger(const std::string& s) {
    if (s.empty()) return false;
    std::size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

bool looksLikeReal(const std::string& s) {
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end != nullptr && *end == '\0';
}

std::string listOf(const std::vector<std::string>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out += ", ";
        out += values[i];
    }
    return out;
}

// --- pass 1: the schema ----------------------------------------------------
void checkSchema(const ModelDocument& doc, std::vector<Diagnostic>& out) {
    const ModuleRegistry& reg = ModuleRegistry::instance();

    for (const std::string& type : doc.types()) {
        const ModuleSchema* schema = reg.find(type);
        if (schema == nullptr) {
            // A WARNING, not an error: the document preserves it, and an older
            // engine opening a newer file should say what it did not recognise
            // rather than refuse the file or silently drop the rows.
            report(out, Severity::Warning, type, 0, "",
                   "unknown module type '" + type + "'; its rows are kept but ignored");
            continue;
        }

        const std::size_t rows = doc.rowCount(type);
        for (std::size_t r = 0; r < rows; ++r) {
            for (const auto& kv : doc.rows(type)[r].cells)
                if (schema->column(kv.first) == nullptr)
                    report(out, Severity::Warning, type, r, kv.first,
                           "unknown column '" + kv.first + "' on " + type +
                           "; it is kept but ignored");

            for (const Column& c : schema->columns) {
                const bool present = doc.hasCell(type, r, c.id);
                const std::string value = present ? doc.cell(type, r, c.id) : std::string();

                if (c.required && value.empty()) {
                    report(out, Severity::Error, type, r, c.id,
                           type + " needs a " + c.id);
                    continue;
                }
                if (value.empty()) continue;   // optional and unset

                switch (c.type) {
                    case ColumnType::Integer:
                        if (!looksLikeInteger(value))
                            report(out, Severity::Error, type, r, c.id,
                                   c.id + " must be a whole number, not '" + value + "'");
                        break;
                    case ColumnType::Real:
                        if (!looksLikeReal(value))
                            report(out, Severity::Error, type, r, c.id,
                                   c.id + " must be a number, not '" + value + "'");
                        break;
                    case ColumnType::Boolean:
                        if (value != "true" && value != "false")
                            report(out, Severity::Error, type, r, c.id,
                                   c.id + " must be true or false, not '" + value + "'");
                        break;
                    case ColumnType::Enum: {
                        bool ok = false;
                        for (const std::string& v : c.enumValues) if (v == value) ok = true;
                        if (!ok)
                            report(out, Severity::Error, type, r, c.id,
                                   "'" + value + "' is not one of: " + listOf(c.enumValues));
                        break;
                    }
                    case ColumnType::Text:
                    case ColumnType::Identifier:
                    case ColumnType::Expression:
                    case ColumnType::Reference:
                        break;   // checked by later passes, or free-form
                }
            }
        }
    }
}

// --- pass 2: references ----------------------------------------------------
// "Block" is the pseudo-type meaning "any flowchart block", which is what an
// exit column points at. Everything else names one real module type.
bool isDeclared(const ModelDocument& doc, const std::string& targetType,
                const std::string& name) {
    const ModuleRegistry& reg = ModuleRegistry::instance();

    auto rowNames = [&doc](const std::string& type, std::vector<std::string>& into) {
        const std::size_t rows = doc.rowCount(type);
        for (std::size_t r = 0; r < rows; ++r) {
            const std::string n = doc.cell(type, r, "Name");
            if (!n.empty()) into.push_back(n);
        }
    };

    std::vector<std::string> names;
    if (targetType == "Block") {
        for (const ModuleSchema& s : reg.all())
            if (s.kind == ModuleKind::Flowchart) rowNames(s.typeName, names);
    } else {
        rowNames(targetType, names);
    }
    for (const std::string& n : names)
        if (n == name) return true;
    return false;
}

void checkReferences(const ModelDocument& doc, std::vector<Diagnostic>& out) {
    const ModuleRegistry& reg = ModuleRegistry::instance();

    for (const std::string& type : doc.types()) {
        const ModuleSchema* schema = reg.find(type);
        if (schema == nullptr) continue;   // already warned about in pass 1

        const std::size_t rows = doc.rowCount(type);
        for (std::size_t r = 0; r < rows; ++r) {
            for (const Column& c : schema->columns) {
                if (c.type != ColumnType::Reference) continue;
                const std::string value = doc.cell(type, r, c.id);

                // An EMPTY exit is not an unresolved reference: the engine
                // already spells "leaves the system" as a null next.
                if (value.empty()) continue;

                if (!isDeclared(doc, c.referencedType, value))
                    out.push_back(Diagnostic{
                        Severity::Error, SourceSpan{},
                        "no " + (c.referencedType == "Block"
                                     ? std::string("block")
                                     : c.referencedType) +
                            " named '" + value + "'",
                        CellRef{type, r, c.id}});
            }
        }
    }
}

// --- pass 3: expressions ---------------------------------------------------
void checkExpressionCells(const ModelDocument& doc, std::vector<Diagnostic>& out) {
    const ModuleRegistry& reg = ModuleRegistry::instance();

    for (const std::string& type : doc.types()) {
        const ModuleSchema* schema = reg.find(type);
        if (schema == nullptr) continue;

        const std::size_t rows = doc.rowCount(type);
        for (std::size_t r = 0; r < rows; ++r) {
            for (const Column& c : schema->columns) {
                if (c.type != ColumnType::Expression) continue;
                const std::string text = doc.cell(type, r, c.id);
                if (text.empty()) continue;    // required-and-empty is pass 1's

                ParseResult parsed = parseExpression(text);
                for (Diagnostic& d : parsed.diagnostics) {
                    // The span is left exactly as v10 measured it -- an offset
                    // WITHIN the cell -- and the cell is wrapped around it. Both
                    // survive, which is what puts a cursor on the right
                    // character of the right cell.
                    d.cell = CellRef{type, r, c.id};
                    out.push_back(std::move(d));
                }
            }
        }
    }
}

// --- pass 4: build the Model -----------------------------------------------
// Blocks first, exits second, for the same reason Model::wireSources() exists:
// the order a model is described in must not matter, and route() refuses a
// target that does not exist yet.

QueueDiscipline disciplineFrom(const std::string& s) {
    if (s == "LIFO")     return QueueDiscipline::LIFO;
    if (s == "PRIORITY") return QueueDiscipline::Priority;
    if (s == "SPT")      return QueueDiscipline::SPT;
    if (s == "EDD")      return QueueDiscipline::EDD;
    if (s == "RANDOM")   return QueueDiscipline::Random;
    return QueueDiscipline::FIFO;
}

long long asInteger(const std::string& s, long long fallback) {
    if (s.empty()) return fallback;
    return std::strtoll(s.c_str(), nullptr, 10);
}

double asReal(const std::string& s, double fallback) {
    if (s.empty()) return fallback;
    return std::strtod(s.c_str(), nullptr);
}

void buildBlocks(const ModelDocument& doc, Model& model) {
    const ModuleRegistry& reg = ModuleRegistry::instance();

    // 1. Variables, before anything can reference them.
    for (std::size_t r = 0; r < doc.rowCount("Variable"); ++r) {
        model.variable(doc.cell("Variable", r, "Name"),
                       asReal(doc.cellOrDefault("Variable", r, "Initial Value"), 0.0));
    }

    // 2. Entity rows are declarative: a type name is carried by a Create, and
    //    the engine has no object to make for one.

    // 3. Resources, before any Process can seize one.
    for (std::size_t r = 0; r < doc.rowCount("Resource"); ++r) {
        model.resource(doc.cell("Resource", r, "Name"),
                       static_cast<int>(asInteger(doc.cellOrDefault("Resource", r, "Capacity"), 1)));
    }

    // 4. Blocks, WITHOUT their exits.
    for (const std::string& type : doc.types()) {
        const ModuleSchema* schema = reg.find(type);
        if (schema == nullptr || schema->kind != ModuleKind::Flowchart) continue;

        for (std::size_t r = 0; r < doc.rowCount(type); ++r) {
            const std::string name = doc.cell(type, r, "Name");
            const auto cell = [&](const char* c) {
                return doc.cellOrDefault(type, r, c);
            };

            if (type == "Create") {
                const std::string entityType = cell("Entity Type");
                model.source(name, entityType.empty() ? "Entity" : entityType,
                             cell("Interarrival"),
                             asInteger(cell("Max Arrivals"), -1),
                             asReal(cell("First At"), 0.0),
                             static_cast<int>(asInteger(cell("Per Arrival"), 1)));
            } else if (type == "Process") {
                const std::string resourceName = cell("Resource");
                const QueueDiscipline rule = disciplineFrom(cell("Discipline"));
                if (resourceName.empty())
                    model.station(name, static_cast<int>(asInteger(cell("Capacity"), 1)),
                                  rule, cell("Service"));
                else
                    model.stationUsing(name, resourceName, rule, cell("Service"),
                                       static_cast<int>(asInteger(cell("Units"), 1)));
            } else if (type == "Delay") {
                model.delay(name, cell("Duration"));
            } else if (type == "Assign") {
                // Created empty; its fields arrive with the AssignField rows.
                model.assign(name);
            } else if (type == "Decide") {
                if (cell("Type") == "Condition") model.decideNWayByCondition(name);
                else                             model.decideNWayByChance(name);
            } else if (type == "Batch") {
                const std::size_t size =
                    static_cast<std::size_t>(asInteger(cell("Size"), 2));
                const bool permanent = cell("Permanent") == "true";
                const std::string rule = cell("Rule");
                if (rule == "SameAttribute")
                    model.batchBySameAttribute(name, size, cell("Attribute"), permanent);
                else if (rule == "DistinctAttribute")
                    model.batchOneOfEach(name, size, cell("Attribute"), permanent);
                else
                    model.batch(name, size, permanent);
            } else if (type == "Separate") {
                if (cell("Mode") == "Duplicate")
                    model.duplicate(name, static_cast<int>(asInteger(cell("Copies"), 1)));
                else
                    model.separate(name);
            } else if (type == "Record") {
                const std::string what = cell("What");
                if (what == "Attribute")           model.recordAttribute(name, cell("Attribute"));
                else if (what == "TimeInSystem")   model.recordTimeInSystem(name);
                else                               model.record(name);
            } else if (type == "Dispose") {
                model.dispose(name);
            }
        }
    }

    // 5. Child rows, IN FILE ORDER: an Assign field reads what the previous one
    //    wrote, and a Decide takes the first branch that matches.
    for (std::size_t r = 0; r < doc.rowCount("AssignField"); ++r) {
        const std::string block  = doc.cell("AssignField", r, "Assign");
        const std::string target = doc.cellOrDefault("AssignField", r, "Target");
        const std::string field  = doc.cell("AssignField", r, "Name");
        const std::string value  = doc.cell("AssignField", r, "Value");
        if (target == "Variable")        model.assignVariable(block, field, value);
        else if (target == "EntityType") model.assignEntityType(block, value);
        else                             model.assignTo(block, field, value);
    }

    for (std::size_t r = 0; r < doc.rowCount("DecideBranch"); ++r) {
        const std::string decide    = doc.cell("DecideBranch", r, "Decide");
        const std::string condition = doc.cell("DecideBranch", r, "Condition");
        const std::string to        = doc.cell("DecideBranch", r, "To");
        if (!condition.empty()) model.branchWhen(decide, condition, to);
        else                    model.branch(decide, asReal(doc.cell("DecideBranch", r,
                                                                     "Probability"), 0.0), to);
    }

    // 6. Exits, now that every block exists.
    for (const std::string& type : doc.types()) {
        const ModuleSchema* schema = reg.find(type);
        if (schema == nullptr || schema->kind != ModuleKind::Flowchart) continue;

        for (std::size_t r = 0; r < doc.rowCount(type); ++r) {
            const std::string name = doc.cell(type, r, "Name");
            const std::string next = doc.cell(type, r, "Next");
            if (!next.empty()) model.route(name, next);

            if (type == "Separate") {
                const std::string dup = doc.cell(type, r, "Duplicate");
                if (!dup.empty()) model.routeDuplicate(name, dup);
            }
            if (type == "Process") {
                const std::string balkAt = doc.cell(type, r, "Balk At");
                if (!balkAt.empty())
                    model.balkAt(name, static_cast<std::size_t>(asInteger(balkAt, 0)),
                                 doc.cell(type, r, "Balk To"));
                const std::string patience = doc.cell(type, r, "Renege After");
                if (!patience.empty())
                    model.renegeAfter(name, patience, doc.cell(type, r, "Renege To"));
            }
        }
    }

    // 7. Entry: where the first source feeds, or the first flowchart block.
    std::string entry;
    if (doc.rowCount("Create") > 0) entry = doc.cell("Create", 0, "Next");
    if (entry.empty()) {
        for (const std::string& type : doc.types()) {
            const ModuleSchema* schema = reg.find(type);
            if (schema == nullptr || schema->kind != ModuleKind::Flowchart) continue;
            if (type == "Create") continue;
            if (doc.rowCount(type) > 0) { entry = doc.cell(type, 0, "Name"); break; }
        }
    }
    if (!entry.empty()) model.entryAt(entry);
    model.wireSources();
}

}  // namespace

bool compileInto(const ModelDocument& doc, Model& model,
                 std::vector<Diagnostic>& diagnostics, bool* structureChecked) {
    if (structureChecked != nullptr) *structureChecked = false;

    // Both passes run even when the first found errors: the point of the
    // layer is to report every bad cell, not the first.
    checkSchema(doc, diagnostics);
    checkReferences(doc, diagnostics);
    checkExpressionCells(doc, diagnostics);
    // The build cannot run with broken references or unparseable expressions,
    // so it is skipped and structureChecked stays false. Reporting a model as
    // checked when it was not is the failure this project keeps refusing.
    if (hasErrors(diagnostics)) return false;

    try {
        buildBlocks(doc, model);
    } catch (const ModelError& bad) {
        // Every Model call can throw. A document must never propagate an
        // exception to a caller who asked for diagnostics.
        diagnostics.push_back(Diagnostic{Severity::Error, SourceSpan{}, bad.what()});
        return false;
    }

    for (const Diagnostic& d : model.checkStructure()) diagnostics.push_back(d);
    if (structureChecked != nullptr) *structureChecked = true;
    return !hasErrors(diagnostics);
}

CompileResult compile(const ModelDocument& doc) {
    CompileResult result;
    auto model = std::make_unique<Model>();
    const bool ok = compileInto(doc, *model, result.diagnostics, &result.structureChecked);
    if (ok) result.model = std::move(model);
    return result;
}

}  // namespace des
