// ============================================================================
// tests/document_tests.cpp  --  v11: the document layer
// ============================================================================
#include <string>
#include "harness.hpp"
#include "des.hpp"

using namespace des;
using des_test::check;
using des_test::checkClose;
using des_test::section;

void runDocumentTests() {
    section("Module schemas");
    {
        const ModuleRegistry& reg = ModuleRegistry::instance();
        check(!reg.all().empty(), "the registry publishes at least one schema");
    }

    section("Schema mechanics");
    {
        ModuleSchema s;
        s.typeName = "Demo";
        s.kind = ModuleKind::Data;
        s.columns.push_back(Column{"Name", ColumnType::Identifier, true, "", {}, ""});
        s.columns.push_back(Column{"Rule", ColumnType::Enum, false, "FIFO",
                                   {"FIFO", "LIFO"}, ""});

        check(s.column("Name") != nullptr, "a declared column is findable by id");
        check(s.column("Name")->required, "and keeps its required flag");
        check(s.column("Nope") == nullptr, "an undeclared column is not found");
        check(s.column("Rule")->defaultValue == "FIFO", "defaults survive");
        check(!s.readOnly, "schemas are editable unless they say otherwise");
    }

    section("The schema set");
    {
        const ModuleRegistry& reg = ModuleRegistry::instance();

        for (const char* t : {"Create", "Process", "Delay", "Assign", "Decide",
                              "Batch", "Separate", "Record", "Dispose",
                              "Variable", "Entity", "Queue", "Resource", "Expression",
                              "DecideBranch", "AssignField"})
            check(reg.find(t) != nullptr, std::string("a schema exists for ") + t);

        // Every Reference must name a module type that exists, or a front end
        // would offer a picker over nothing and the compiler would resolve
        // against a table nobody declared.
        for (const ModuleSchema& s : reg.all())
            for (const Column& c : s.columns)
                if (c.type == ColumnType::Reference)
                    check(reg.find(c.referencedType) != nullptr || c.referencedType == "Block",
                          s.typeName + "." + c.id + " references a real module type");

        for (const ModuleSchema& s : reg.all())
            for (const Column& c : s.columns)
                if (c.type == ColumnType::Enum)
                    check(c.enumValues.size() >= 2,
                          s.typeName + "." + c.id + " lists its enum values");

        for (const ModuleSchema& s : reg.all())
            if (s.kind == ModuleKind::Child) {
                check(!s.parentColumn.empty(), s.typeName + " names its parent column");
                check(s.column(s.parentColumn) != nullptr,
                      s.typeName + " has the parent column it names");
            }

        check(reg.find("Queue")->readOnly,
              "Queue is read-only: the Process row is where a discipline is set");
        check(!reg.find("Process")->readOnly, "Process is editable");

        for (const ModuleSchema& s : reg.all()) {
            std::size_t seen = 0;
            for (const ModuleSchema& o : reg.all()) if (o.typeName == s.typeName) ++seen;
            check(seen == 1, s.typeName + " is declared once");
            for (const Column& c : s.columns) {
                std::size_t n = 0;
                for (const Column& d : s.columns) if (d.id == c.id) ++n;
                check(n == 1, s.typeName + "." + c.id + " is declared once");
            }
        }
    }

    section("ModelDocument");
    {
        ModelDocument doc;
        check(doc.rowCount("Process") == 0, "an empty document has no rows");

        doc.addRow("Process");
        doc.setCell("Process", 0, "Name", "Teller");
        doc.setCell("Process", 0, "Service", "EXPO(0.8)");
        check(doc.rowCount("Process") == 1, "addRow adds a row");
        check(doc.cell("Process", 0, "Name") == "Teller", "cells round-trip");
        check(doc.cell("Process", 0, "Nope").empty(), "an unset cell reads empty");
        check(!doc.hasCell("Process", 0, "Nope"), "and reports itself absent");

        doc.addRow("Process");
        doc.setCell("Process", 1, "Name", "Inspect");
        check(doc.rowCount("Process") == 2, "a second row");

        // ORDER IS SEMANTIC. moveRow exists because reordering DecideBranch
        // rows changes which condition wins, and faking it with remove-then-add
        // would lose the row's other cells.
        doc.moveRow("Process", 1, 0);
        check(doc.cell("Process", 0, "Name") == "Inspect", "moveRow reorders");
        check(doc.cell("Process", 1, "Name") == "Teller", "and keeps the other row");
        check(doc.cell("Process", 1, "Service") == "EXPO(0.8)",
              "and every cell of the moved row travels with it");

        doc.removeRow("Process", 0);
        check(doc.rowCount("Process") == 1, "removeRow removes one");
        check(doc.cell("Process", 0, "Name") == "Teller", "the survivor shifts down");

        // A document may hold module types nobody declared a schema for.
        doc.addRow("SomethingFromV13");
        doc.setCell("SomethingFromV13", 0, "Whatever", "kept");
        check(doc.rowCount("SomethingFromV13") == 1, "unknown types are storable");
        check(doc.types().size() == 2, "types() lists every module type present");

        bool threw = false;
        try { doc.cell("Process", 9, "Name"); } catch (const ModelError&) { threw = true; }
        check(threw, "an out-of-range row throws rather than reading empty");

        threw = false;
        try { doc.moveRow("Process", 0, 9); } catch (const ModelError&) { threw = true; }
        check(threw, "moving past the end throws");
    }
}
