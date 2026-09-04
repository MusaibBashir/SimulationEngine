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

    section("The .des format");
    {
        const std::string src =
            "version = 1\n"
            "\n"
            "# The teller queue. Comments survive a round trip.\n"
            "[Resource]\n"
            "Name     = Teller\n"
            "Capacity = 2\n"
            "\n"
            "[Process]\n"
            "Name       = Serve\n"
            "Resource   = Teller\n"
            "Service    = EXPO(0.8)\n";

        ReadResult r = readDocument(src);
        check(!hasErrors(r.diagnostics), "a clean document reads without errors");
        check(r.formatVersion == 1, "the version line is read");
        check(r.document.rowCount("Resource") == 1, "one Resource row");
        check(r.document.rowCount("Process") == 1, "one Process row");
        check(r.document.cell("Resource", 0, "Name") == "Teller", "cells are read");
        check(r.document.cell("Process", 0, "Service") == "EXPO(0.8)",
              "an expression needs no quoting: the value is raw to end of line");
        check(r.document.cellLine("Process", 0, "Service") == 11,
              "each cell remembers the line it came from");

        // ROUND TRIP IS BYTE-IDENTICAL. Anything less means a front end
        // silently rewrites a file somebody opened and saved unchanged.
        check(writeDocument(r.document) == src,
              "read then write returns the identical bytes, comments included");

        const std::string two =
            "version = 1\n"
            "\n"
            "[DecideBranch]\n"
            "Decide = Sort\n"
            "To = Fast\n"
            "\n"
            "[DecideBranch]\n"
            "Decide = Sort\n"
            "To = Slow\n";
        ReadResult t = readDocument(two);
        check(t.document.rowCount("DecideBranch") == 2, "repeated headers make rows");
        check(t.document.cell("DecideBranch", 0, "To") == "Fast", "first row first");
        check(t.document.cell("DecideBranch", 1, "To") == "Slow", "second row second");
        check(writeDocument(t.document) == two, "and that round-trips too");

        {
            ReadResult bad = readDocument("version = 1\n[Process\nName = X\n");
            check(hasErrors(bad.diagnostics), "an unclosed header is an error");
            check(bad.diagnostics[0].span.offset == 2,
                  "reported against the line it is on");
        }
        {
            ReadResult bad = readDocument("version = 1\n[Process]\nName\n");
            check(hasErrors(bad.diagnostics), "a line with no '=' is an error");
        }
        {
            ReadResult bad = readDocument("version = 1\nName = X\n");
            check(hasErrors(bad.diagnostics), "a cell before any header is an error");
        }
        {
            ReadResult bad = readDocument("[Process]\nName = X\n");
            check(hasErrors(bad.diagnostics), "a missing version line is an error");
        }
        {
            ReadResult bad = readDocument("version = 1\n[Process]\nOne\nTwo\n");
            check(bad.diagnostics.size() >= 2, "one read reports more than one problem");
        }
        {
            ReadResult bad = readDocument("version = 2\n[Process]\nName = X\n");
            check(hasErrors(bad.diagnostics),
                  "a version this engine does not read is an error, not a guess");
        }
    }

    section("Round-trip is a property, not an example");
    {
        // Build in code, write, read back, write again: the second and third
        // forms must agree. A canonical writer that is not idempotent would
        // churn every file on every save.
        ModelDocument d;
        d.addRow("Resource");
        d.setCell("Resource", 0, "Name", "Nurse");
        d.setCell("Resource", 0, "Capacity", "2");
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Triage");
        d.setCell("Process", 0, "Service", "TRIA(1, 2, 3)");

        const std::string once = writeDocument(d);
        ReadResult back = readDocument(once);
        check(!hasErrors(back.diagnostics), "canonical output reads back cleanly");
        check(writeDocument(back.document) == once, "and writing it again is stable");
        check(back.document.cell("Process", 0, "Service") == "TRIA(1, 2, 3)",
              "values survive the trip unchanged");

        // An unknown column is written back, not dropped.
        ModelDocument u;
        u.addRow("Resource");
        u.setCell("Resource", 0, "Name", "X");
        u.setCell("Resource", 0, "FromTheFuture", "keep me");
        const std::string text = writeDocument(u);
        check(text.find("FromTheFuture = keep me") != std::string::npos,
              "an unknown column survives being written");

        // Editing a document that came from a file switches it to canonical
        // form rather than replaying stale source lines.
        ReadResult e = readDocument("version = 1\n\n[Resource]\nName = A\n");
        e.document.setCell("Resource", 0, "Name", "B");
        check(writeDocument(e.document).find("Name = B") != std::string::npos,
              "an edit is written, not the line it replaced");
    }
}
