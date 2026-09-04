#include "Compiler.hpp"
#include <cstdlib>
#include "ModuleSchema.hpp"
#include "Parser.hpp"

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

}  // namespace

bool compileInto(const ModelDocument& doc, Model& model,
                 std::vector<Diagnostic>& diagnostics, bool* structureChecked) {
    (void)model;
    if (structureChecked != nullptr) *structureChecked = false;

    // Both passes run even when the first found errors: the point of the
    // layer is to report every bad cell, not the first.
    checkSchema(doc, diagnostics);
    checkReferences(doc, diagnostics);
    checkExpressionCells(doc, diagnostics);
    if (hasErrors(diagnostics)) return false;

    // The structure pass arrives with the build, in the next task.
    return false;
}

CompileResult compile(const ModelDocument& doc) {
    CompileResult result;
    auto model = std::make_unique<Model>();
    const bool ok = compileInto(doc, *model, result.diagnostics, &result.structureChecked);
    if (ok) result.model = std::move(model);
    return result;
}

}  // namespace des
