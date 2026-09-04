# v11 Document Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the model itself data — a set of schema-described spreadsheets that round-trip through a text file and compile into a runnable `Model`, or into diagnostics that point at individual cells.

**Architecture:** A document layer sits *above* the engine core. `ModuleRegistry` publishes what columns each module type has; `ModelDocument` holds rows of text cells that may be invalid; the `.des` reader and writer round-trip them byte-identically; `compile()` runs four passes and produces either a `Model` or a list of `Diagnostic`s carrying a `CellRef`. The engine never learns what a document is.

**Tech Stack:** C++17, CMake, no external dependencies. The hand-rolled test harness in `tests/`.

## Global Constraints

- **C++17.** No newer features, no external dependencies.
- **All 543 v10 checks must pass, unchanged.** Not adapted, not deleted. If one needs changing, stop and raise it.
- **The 15 gated examples stay byte-identical:** `bash tools/baseline.sh check` must print `BASELINE CLEAN`.
- **`bash tools/verify.sh` must print `VERIFY CLEAN`** — GCC 14.2 and Clang 19.1 warning-clean on engine and tests, MSVC AddressSanitizer clean.
- **UBSan runs under WSL only:** `wsl -d Ubuntu -e bash -lc 'cd "/mnt/c/Users/User A/Documents/Indu/Simulation/sim" && g++ -std=c++17 -g -O1 -fsanitize=address,undefined -Iinclude -Itests tests/*.cpp src/*.cpp -o /tmp/des_san && /tmp/des_san'`. Prefer WSL for running tests: it is faster and cannot open a modal dialog.
- **Build with CMake + Visual Studio** via the PowerShell tool: `cmake --build build --config Debug`. Binaries land in `build/Debug/`, examples in `build/examples/Debug/`. Git Bash mangles MSVC-style `/flags` into paths.
- **Headers declare, sources define.** Every `.cpp` includes its own header first.
- **`unique_ptr` = ownership, raw pointer = observation.** Every polymorphic base gets a virtual destructor.
- **Sources are LISTED in `CMakeLists.txt`**, never globbed (except `examples/`).
- **New public headers go in `include/des.hpp`.**
- **User errors are `Diagnostic` values; programmer errors are thrown `ModelError`.** Never mix these.
- **Comments only where they carry design rationale.** This codebase explains *why*, never *what*. Do not narrate code.
- Namespace `des` throughout.

### Constraints specific to v11

- **Rows are addressed by POSITION, never by a stable id.** The two disagree the moment a row moves.
- **Round-trip must be byte-identical**: read a `.des` file, write it back, get the same bytes — comments, blank lines and spacing included.
- **Row order is semantic.** Reordering `DecideBranch` rows changes which condition wins; reordering `AssignField` rows changes what a later field sees.
- **Unknown module types and unknown columns are preserved verbatim** and reported as `Severity::Warning`, never dropped.
- **`Model::validate()` must still throw exactly as it does today, with the same messages.**

## File Structure

**New headers/sources** (each added to `add_library(des_engine ...)` and to `include/des.hpp`):

| File | Responsibility |
|---|---|
| `include/ModuleSchema.hpp` / `src/ModuleSchema.cpp` | `ColumnType`, `Column`, `ModuleKind`, `ModuleSchema`, `ModuleRegistry`. |
| `include/ModuleSchemas.hpp` / `src/ModuleSchemas.cpp` | The actual schema for every module type. Separate file because it is data, not mechanism, and it is the file that grows in v13. |
| `include/ModelDocument.hpp` / `src/ModelDocument.cpp` | Rows of text cells, in order. |
| `include/DocumentFormat.hpp` / `src/DocumentFormat.cpp` | `readDocument` and `writeDocument` — the `.des` format, both directions. One file because round-trip is a property of the pair. |
| `include/Compiler.hpp` / `src/Compiler.cpp` | `CellRef`, `CompileResult`, `compile()`. |

**New test file:** `tests/document_tests.cpp`, exposing `void runDocumentTests();`.

**New program:** `cli/main.cpp` — the `des` command.

**Modified:** `include/Diagnostic.hpp` (add `CellRef`), `include/Model.hpp` + `src/Model.cpp` (`checkStructure()`), `include/des.hpp`, `CMakeLists.txt`, `tests/tests.cpp`, `README.md`, `CHANGELOG.md`, `ARENA_MAP.md`.

**One rename, deliberate.** `add_executable(des main.cpp)` currently claims the name `des` for the five-scenario demo. The CLI deserves it: `des check model.des` is the product, the demo is a demo. Task 10 renames the demo target to `des_demo` and updates `README.md`. Flagged here because it changes a documented command.

**Ordering rationale.** The schema and the document come first because everything else is expressed in terms of them. The format lands before the compiler so that test documents can be written as files rather than built by hand in C++. `checkStructure()` is refactored just before the pass that needs it, so the v10 behaviour change is isolated in one reviewable task.

---

### Task 1: The schema types and the registry

**Files:**
- Create: `include/ModuleSchema.hpp`, `src/ModuleSchema.cpp`
- Modify: `CMakeLists.txt`, `include/des.hpp`
- Create: `tests/document_tests.cpp`; modify `tests/tests.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `enum class des::ColumnType { Text, Identifier, Integer, Real, Boolean, Enum, Expression, Reference }`
  - `enum class des::ModuleKind { Flowchart, Data, Child }`
  - `struct des::Column { std::string id; ColumnType type; bool required; std::string defaultValue; std::vector<std::string> enumValues; std::string referencedType; }`
  - `struct des::ModuleSchema { std::string typeName; ModuleKind kind; bool readOnly; std::string parentColumn; std::vector<Column> columns; const Column* column(const std::string& id) const; }`
  - `class des::ModuleRegistry` with `static const ModuleRegistry& instance()`, `const ModuleSchema* find(const std::string&) const`, `const std::vector<ModuleSchema>& all() const`

- [ ] **Step 1: Create the test suite and wire it in**

Create `tests/document_tests.cpp`:

```cpp
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
}
```

In `tests/tests.cpp`, beside the existing `void runExpressionTests();` declaration add:

```cpp
void runDocumentTests();     // tests/document_tests.cpp
```

and call `runDocumentTests();` immediately after `runExpressionTests();` in `main`.

In `CMakeLists.txt` add `tests/document_tests.cpp` to the `des_tests` executable.

- [ ] **Step 2: Write the failing test**

Append to `runDocumentTests()`:

```cpp
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
```

- [ ] **Step 3: Run it and confirm it fails**

```bash
cmake --build build --config Debug
```

Expected: `'ModuleSchema': undeclared identifier`.

- [ ] **Step 4: Write `include/ModuleSchema.hpp`**

```cpp
// ============================================================================
// ModuleSchema.hpp  --  v11: what columns a module type has
// ============================================================================
// THIS IS THE WHOLE CONTRACT WITH A FRONT END. A front end that can render one
// table from a schema can render every module type, including ones added in a
// later version, without being changed. That property is the reason schemas are
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
    std::string         typeName;
    ModuleKind          kind{ModuleKind::Data};
    // A derived view rather than an editable table. Queue is the first: this
    // engine has no queue object apart from its Process, so an editable Queue
    // module would hold the same fact as the Process row's Discipline column.
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
```

- [ ] **Step 5: Write `src/ModuleSchema.cpp`**

```cpp
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
```

> `buildSchemas()` is Task 2. To keep this task compiling on its own, create `include/ModuleSchemas.hpp` declaring `std::vector<ModuleSchema> buildSchemas();` and `src/ModuleSchemas.cpp` returning a single placeholder schema. Task 2 replaces the source wholesale. Say so in the commit message so it is not mistaken for finished work.

- [ ] **Step 6: Wire into the build**

Add `src/ModuleSchema.cpp` and `src/ModuleSchemas.cpp` to `add_library(des_engine ...)`, after `src/Parser.cpp`. Add `#include "ModuleSchema.hpp"` to `include/des.hpp` after `Parser.hpp`.

- [ ] **Step 7: Run the tests**

```bash
cmake --build build --config Debug
```
then
```bash
./build/Debug/des_tests.exe
```

Expected: `549 / 549 checks passed` (543 + 6 new). No FAIL lines.

- [ ] **Step 8: Commit**

```bash
git add include/ModuleSchema.hpp src/ModuleSchema.cpp include/ModuleSchemas.hpp src/ModuleSchemas.cpp include/des.hpp CMakeLists.txt tests/document_tests.cpp tests/tests.cpp
git commit -m "feat: module schemas and the registry a front end reads

buildSchemas() is a deliberate placeholder here; Task 2 replaces it."
```

---

### Task 2: The schemas themselves

Data, not mechanism. Every module type the engine has, described.

**Files:**
- Rewrite: `include/ModuleSchemas.hpp`, `src/ModuleSchemas.cpp`
- Test: `tests/document_tests.cpp`

**Interfaces:**
- Consumes: `ModuleSchema`, `Column`, `ColumnType`, `ModuleKind` (Task 1).
- Produces: `std::vector<ModuleSchema> des::buildSchemas()`

- [ ] **Step 1: Write the failing test**

```cpp
    section("The schema set");
    {
        const ModuleRegistry& reg = ModuleRegistry::instance();

        for (const char* t : {"Create", "Process", "Delay", "Assign", "Decide",
                              "Batch", "Separate", "Record", "Dispose",
                              "Variable", "Entity", "Queue", "Resource", "Expression",
                              "DecideBranch", "AssignField"})
            check(reg.find(t) != nullptr, std::string("a schema exists for ") + t);

        // Every Reference column must name a module type that exists, or a
        // front end would offer a picker over nothing and the compiler would
        // resolve against a table that was never declared.
        for (const ModuleSchema& s : reg.all())
            for (const Column& c : s.columns)
                if (c.type == ColumnType::Reference)
                    check(reg.find(c.referencedType) != nullptr || c.referencedType == "Block",
                          s.typeName + "." + c.id + " references a real module type");

        // Every Enum column must offer at least two spellings.
        for (const ModuleSchema& s : reg.all())
            for (const Column& c : s.columns)
                if (c.type == ColumnType::Enum)
                    check(c.enumValues.size() >= 2,
                          s.typeName + "." + c.id + " lists its enum values");

        // Every Child module must name the column pointing at its parent, and
        // that column must exist.
        for (const ModuleSchema& s : reg.all())
            if (s.kind == ModuleKind::Child) {
                check(!s.parentColumn.empty(), s.typeName + " names its parent column");
                check(s.column(s.parentColumn) != nullptr,
                      s.typeName + " has the parent column it names");
            }

        check(reg.find("Queue")->readOnly,
              "Queue is read-only: the Process row is where a discipline is set");
        check(!reg.find("Process")->readOnly, "Process is editable");

        // No duplicate type names, and no duplicate column ids within a type.
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
```

- [ ] **Step 2: Run it and confirm it fails**

Expected: FAIL lines for every module type except the placeholder.

- [ ] **Step 3: Write `include/ModuleSchemas.hpp`**

```cpp
// ============================================================================
// ModuleSchemas.hpp  --  v11: every module type, described
// ============================================================================
// Kept apart from ModuleSchema.hpp because this is DATA and that is mechanism.
// This is the file that grows when a version adds a module, and keeping it
// separate means adding one cannot disturb the registry.

#pragma once
#include <vector>
#include "ModuleSchema.hpp"

namespace des {

std::vector<ModuleSchema> buildSchemas();

}  // namespace des
```

- [ ] **Step 4: Write `src/ModuleSchemas.cpp`**

```cpp
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
Column expr(std::string id, bool required = true, std::string def = "") {
    return Column{std::move(id), ColumnType::Expression, required, std::move(def), {}, ""};
}
Column enumeration(std::string id, std::vector<std::string> values, std::string def) {
    return Column{std::move(id), ColumnType::Enum, false, std::move(def),
                  std::move(values), ""};
}
// "Block" is the pseudo-type meaning "any flowchart block", which is what an
// exit column points at. Every other Reference names one real module type.
Column ref(std::string id, std::string target, bool required = false) {
    return Column{std::move(id), ColumnType::Reference, required, "", {}, std::move(target)};
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

    // --- data modules -----------------------------------------------------
    s.push_back(make("Variable", ModuleKind::Data,
                     {ident("Name"), real("Initial Value", "0")}));
    s.push_back(make("Entity", ModuleKind::Data, {ident("Name")}));
    s.push_back(make("Resource", ModuleKind::Data,
                     {ident("Name"), integer("Capacity", "1", true)}));
    s.push_back(make("Expression", ModuleKind::Data,
                     {ident("Name"), expr("Value")}));
    // Read-only: derived from the Process rows, never edited here.
    s.push_back(make("Queue", ModuleKind::Data,
                     {ident("Name"), ref("Process", "Process"),
                      enumeration("Discipline", DISCIPLINES, "FIFO")},
                     /*readOnly=*/true));

    // --- flowchart modules -------------------------------------------------
    s.push_back(make("Create", ModuleKind::Flowchart,
                     {ident("Name"), ref("Entity Type", "Entity"),
                      expr("Interarrival"), integer("Max Arrivals", "-1"),
                      real("First At", "0"), integer("Per Arrival", "1"),
                      ref("Next", "Block")}));
    s.push_back(make("Process", ModuleKind::Flowchart,
                     {ident("Name"), integer("Capacity", "1"),
                      ref("Resource", "Resource"), integer("Units", "1"),
                      enumeration("Discipline", DISCIPLINES, "FIFO"),
                      expr("Service"),
                      integer("Balk At", ""), ref("Balk To", "Block"),
                      expr("Renege After", false), ref("Renege To", "Block"),
                      ref("Next", "Block")}));
    s.push_back(make("Delay", ModuleKind::Flowchart,
                     {ident("Name"), expr("Duration"), ref("Next", "Block")}));
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

    // --- child tables ------------------------------------------------------
    // Flat, with a column naming the parent. Row ORDER is meaningful: a Decide
    // takes the first matching branch, and an Assign runs its fields in order.
    s.push_back(make("DecideBranch", ModuleKind::Child,
                     {ref("Decide", "Decide", true), real("Probability", ""),
                      expr("Condition", false), ref("To", "Block")},
                     /*readOnly=*/false, /*parent=*/"Decide"));
    s.push_back(make("AssignField", ModuleKind::Child,
                     {ref("Assign", "Assign", true),
                      enumeration("Target", {"Attribute", "Variable", "EntityType"},
                                  "Attribute"),
                      text("Name"), expr("Value")},
                     /*readOnly=*/false, /*parent=*/"Assign"));
    return s;
}

}  // namespace des
```

- [ ] **Step 5: Run the tests**

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```

Expected: all checks pass. The exact total depends on how many schema rows the loops visit; what matters is zero FAIL lines and that the count has grown.

- [ ] **Step 6: Run the fast gate**

```bash
bash tools/verify.sh warnings
```

Expected: `VERIFY CLEAN`.

- [ ] **Step 7: Commit**

```bash
git add include/ModuleSchemas.hpp src/ModuleSchemas.cpp tests/document_tests.cpp
git commit -m "feat: a schema for every module type

Routing is exit columns on the source block, and repeating groups are
flat child tables naming their parent -- so there is exactly one shape a
front end has to render."
```

---

### Task 3: ModelDocument

**Files:**
- Create: `include/ModelDocument.hpp`, `src/ModelDocument.cpp`
- Modify: `CMakeLists.txt`, `include/des.hpp`
- Test: `tests/document_tests.cpp`

**Interfaces:**
- Consumes: nothing (deliberately: a document is text, and knows no schema).
- Produces:
  - `struct des::ModelDocument::Cell { std::string text; std::size_t line; }`
  - `struct des::ModelDocument::Row { std::map<std::string, Cell> cells; }`
  - `class des::ModelDocument` with `rows`, `rowCount`, `addRow`, `removeRow`, `moveRow`, `cell`, `setCell`, `hasCell`, `types`

- [ ] **Step 1: Write the failing test**

```cpp
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

        std::vector<std::string> types = doc.types();
        check(types.size() == 2, "types() lists every module type present");

        // Out-of-range access throws rather than returning a plausible empty.
        bool threw = false;
        try { doc.cell("Process", 9, "Name"); } catch (const ModelError&) { threw = true; }
        check(threw, "an out-of-range row throws rather than reading empty");
    }
```

- [ ] **Step 2: Run it and confirm it fails**

Expected: `'ModelDocument': undeclared identifier`.

- [ ] **Step 3: Write `include/ModelDocument.hpp`**

```cpp
// ============================================================================
// ModelDocument.hpp  --  v11: a model as rows of text
// ============================================================================
// THIS MAY BE INVALID, AND THAT IS THE POINT. A Process naming a Resource that
// does not exist yet, a half-typed expression, a missing required column: all
// legal states here. That is the whole reason this is not a Model, which may
// never hold a broken flowchart. Model's strictness is a feature, and editing
// convenience must not erode it.
//
// Rows are addressed by POSITION. An id and a position disagree the moment a
// row moves, and position is what a person sees in the file and in a
// spreadsheet -- a diagnostic naming row 4 means the fourth one.

#pragma once
#include <map>
#include <string>
#include <vector>

namespace des {

class ModelDocument {
public:
    struct Cell {
        std::string text;
        std::size_t line{0};    // where it came from, for diagnostics
    };
    struct Row {
        std::map<std::string, Cell> cells;
    };

private:
    // Insertion-ordered: the order module types first appear is the order they
    // are written back, which is half of what makes round-trip byte-identical.
    std::vector<std::string>                  m_order;
    std::map<std::string, std::vector<Row>>   m_rows;

    std::vector<Row>& require(const std::string& type, std::size_t index);

public:
    std::vector<std::string> types() const { return m_order; }
    std::size_t              rowCount(const std::string& type) const;
    const std::vector<Row>&  rows(const std::string& type) const;

    Row& addRow(const std::string& type);
    void removeRow(const std::string& type, std::size_t index);
    void moveRow(const std::string& type, std::size_t from, std::size_t to);

    bool        hasCell(const std::string& type, std::size_t index,
                        const std::string& column) const;
    std::string cell(const std::string& type, std::size_t index,
                     const std::string& column) const;
    std::size_t cellLine(const std::string& type, std::size_t index,
                         const std::string& column) const;
    void        setCell(const std::string& type, std::size_t index,
                        const std::string& column, std::string text,
                        std::size_t line = 0);
};

}  // namespace des
```

- [ ] **Step 4: Write `src/ModelDocument.cpp`**

```cpp
#include "ModelDocument.hpp"
#include "ModelError.hpp"

namespace des {
namespace {
const std::vector<ModelDocument::Row> EMPTY;
}

std::vector<ModelDocument::Row>& ModelDocument::require(const std::string& type,
                                                        std::size_t index) {
    auto it = m_rows.find(type);
    if (it == m_rows.end() || index >= it->second.size())
        throw ModelError("no row " + std::to_string(index) + " in '" + type + "'");
    return it->second;
}

std::size_t ModelDocument::rowCount(const std::string& type) const {
    const auto it = m_rows.find(type);
    return it == m_rows.end() ? 0 : it->second.size();
}

const std::vector<ModelDocument::Row>& ModelDocument::rows(const std::string& type) const {
    const auto it = m_rows.find(type);
    return it == m_rows.end() ? EMPTY : it->second;
}

ModelDocument::Row& ModelDocument::addRow(const std::string& type) {
    if (m_rows.find(type) == m_rows.end()) m_order.push_back(type);
    m_rows[type].push_back(Row{});
    return m_rows[type].back();
}

void ModelDocument::removeRow(const std::string& type, std::size_t index) {
    std::vector<Row>& rs = require(type, index);
    rs.erase(rs.begin() + static_cast<std::ptrdiff_t>(index));
}

void ModelDocument::moveRow(const std::string& type, std::size_t from, std::size_t to) {
    std::vector<Row>& rs = require(type, from);
    if (to >= rs.size()) throw ModelError("moveRow: destination out of range");
    Row moved = std::move(rs[from]);
    rs.erase(rs.begin() + static_cast<std::ptrdiff_t>(from));
    rs.insert(rs.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
}

bool ModelDocument::hasCell(const std::string& type, std::size_t index,
                            const std::string& column) const {
    if (index >= rowCount(type)) return false;
    const Row& r = rows(type)[index];
    return r.cells.find(column) != r.cells.end();
}

std::string ModelDocument::cell(const std::string& type, std::size_t index,
                                const std::string& column) const {
    if (index >= rowCount(type))
        throw ModelError("no row " + std::to_string(index) + " in '" + type + "'");
    const Row& r = rows(type)[index];
    const auto it = r.cells.find(column);
    return it == r.cells.end() ? std::string() : it->second.text;
}

std::size_t ModelDocument::cellLine(const std::string& type, std::size_t index,
                                    const std::string& column) const {
    if (index >= rowCount(type)) return 0;
    const Row& r = rows(type)[index];
    const auto it = r.cells.find(column);
    return it == r.cells.end() ? 0 : it->second.line;
}

void ModelDocument::setCell(const std::string& type, std::size_t index,
                            const std::string& column, std::string text,
                            std::size_t line) {
    std::vector<Row>& rs = require(type, index);
    rs[index].cells[column] = Cell{std::move(text), line};
}

}  // namespace des
```

- [ ] **Step 5: Wire into the build**

Add `src/ModelDocument.cpp` to `CMakeLists.txt` after `src/ModuleSchemas.cpp`, and `#include "ModelDocument.hpp"` to `include/des.hpp`.

- [ ] **Step 6: Run the tests and the fast gate**

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```
```bash
bash tools/verify.sh warnings
```

Expected: zero FAIL lines; `VERIFY CLEAN`.

- [ ] **Step 7: Commit**

```bash
git add include/ModelDocument.hpp src/ModelDocument.cpp include/des.hpp CMakeLists.txt tests/document_tests.cpp
git commit -m "feat: ModelDocument -- rows of text that may be invalid

Addressed by position, never by id: the two disagree the moment a row
moves, and position is what a person sees in the file."
```

---

### Task 4: The `.des` format, both directions

Reader and writer land together, because round-trip is a property of the pair and neither is meaningfully testable alone.

**Files:**
- Create: `include/DocumentFormat.hpp`, `src/DocumentFormat.cpp`
- Modify: `include/ModelDocument.hpp`, `src/ModelDocument.cpp` (Step 4 adds the
  verbatim source lines and the edited flag that byte-identical round-trip needs)
- Modify: `CMakeLists.txt`, `include/des.hpp`
- Test: `tests/document_tests.cpp`

**Interfaces:**
- Consumes: `ModelDocument` (Task 3), `Diagnostic` / `Severity` (v10).
- Produces:
  - `struct des::ReadResult { ModelDocument document; std::vector<Diagnostic> diagnostics; int formatVersion; }`
  - `ReadResult des::readDocument(const std::string& text)`
  - `ReadResult des::readDocumentFile(const std::string& path)`
  - `std::string des::writeDocument(const ModelDocument& doc)`
  - `bool des::writeDocumentFile(const ModelDocument& doc, const std::string& path)`

- [ ] **Step 1: Write the failing test**

```cpp
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

        // Line numbers are what make a diagnostic point into the file.
        check(r.document.cellLine("Process", 0, "Service") == 11,
              "each cell remembers the line it came from");

        // ROUND TRIP IS BYTE-IDENTICAL. Anything less means a front end
        // silently rewrites a file somebody opened and saved unchanged.
        check(writeDocument(r.document) == src,
              "read then write returns the identical bytes, comments included");

        // Two rows of the same type, and order is preserved.
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

        // Malformed input is diagnostics, never an exception.
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
            // One call reports every problem, as v10's parser does.
            ReadResult bad = readDocument("version = 1\n[Process]\nOne\nTwo\n");
            check(bad.diagnostics.size() >= 2, "one read reports more than one problem");
        }
    }
```

> The expected line number `11` counts from 1 and includes the blank lines in `src`. If the reader disagrees, count the literal by hand before changing the assertion — a wrong line number here becomes a cursor in the wrong place in v12.

- [ ] **Step 2: Run it and confirm it fails**

Expected: `'readDocument': identifier not found`.

- [ ] **Step 3: Write `include/DocumentFormat.hpp`**

```cpp
// ============================================================================
// DocumentFormat.hpp  --  v11: the .des file, read and written
// ============================================================================
// Line-oriented records, chosen over JSON for four reasons that all point the
// same way: it diffs PER FIELD, so changing a service time is a one-line diff;
// it reuses v10's Diagnostic and SourceSpan for positions; it carries comments,
// which people writing models actually want; and it is a third the code.
//
// ROUND TRIP MUST BE BYTE-IDENTICAL, comments and blank lines included.
// Anything less means opening and saving a file in a front end silently
// rewrites it. That is why the reader keeps the raw lines rather than only the
// values it understood.

#pragma once
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "ModelDocument.hpp"

namespace des {

struct ReadResult {
    ModelDocument           document;
    std::vector<Diagnostic> diagnostics;
    int                     formatVersion{0};
};

// Never throws. Malformed input becomes diagnostics and reading continues, so
// one call reports every problem in the file.
ReadResult  readDocument(const std::string& text);
ReadResult  readDocumentFile(const std::string& path);
std::string writeDocument(const ModelDocument& doc);
bool        writeDocumentFile(const ModelDocument& doc, const std::string& path);

}  // namespace des
```

- [ ] **Step 4: Decide how round-trip is achieved, then write `src/DocumentFormat.cpp`**

Byte-identical round-trip needs the writer to reproduce comments, blank lines and the exact spacing around `=`. Storing only cell text cannot do that. So the document carries the **verbatim source lines** alongside the parsed cells, and the writer emits those lines when nothing has changed.

Add to `ModelDocument` in `include/ModelDocument.hpp`:

```cpp
    // The file this document was read from, line for line. The writer emits
    // these verbatim so that reading and writing an unedited file returns the
    // identical bytes -- comments, blank lines and alignment included. Empty
    // for a document built in code, which is then written in canonical form.
    void                            setSourceLines(std::vector<std::string> lines);
    const std::vector<std::string>& sourceLines() const { return m_sourceLines; }
    bool                            edited() const { return m_edited; }
    void                            markEdited() { m_edited = true; }
```

with members `std::vector<std::string> m_sourceLines;` and `bool m_edited{false};`, and `markEdited()` called from `addRow`, `removeRow`, `moveRow` and `setCell`.

Then `writeDocument` is:

```cpp
std::string writeDocument(const ModelDocument& doc) {
    // Unedited and read from a file: give back exactly what was read. This is
    // the only way comments and spacing survive, and it is what stops a front
    // end rewriting a file somebody merely looked at.
    if (!doc.edited() && !doc.sourceLines().empty()) {
        std::string out;
        for (const std::string& line : doc.sourceLines()) { out += line; out += "\n"; }
        return out;
    }
    return writeCanonical(doc);
}
```

`writeCanonical` emits `version = 1`, then for each module type in `types()` order, each row as a `[Type]` header followed by its cells in schema column order (falling back to alphabetical for unknown types), with a blank line between rows.

The reader: split into lines; track a current type and row; `#` or `;` at the first non-space is a comment; `[Name]` is a header; `key = value` splits at the first `=` with both sides trimmed; anything else is a diagnostic. Record `formatVersion` from a `version = N` line before any header, and report a missing one as an error.

- [ ] **Step 5: Run the tests**

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```

Expected: zero FAIL lines.

- [ ] **Step 6: Add a round-trip property test**

```cpp
    section("Round-trip is a property, not an example");
    {
        // Build a document in code, write it, read it back, write again: the
        // second and third forms must agree. A canonical writer that is not
        // idempotent would churn every file on every save.
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
    }
```

- [ ] **Step 7: Run the tests and the fast gate, then commit**

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```
```bash
bash tools/verify.sh warnings
```
```bash
git add include/DocumentFormat.hpp src/DocumentFormat.cpp include/ModelDocument.hpp src/ModelDocument.cpp include/des.hpp CMakeLists.txt tests/document_tests.cpp
git commit -m "feat: the .des format, byte-identical on round trip

The document keeps the verbatim lines it was read from, because storing
only cell values cannot reproduce comments or spacing -- and a front end
that rewrites a file somebody merely opened is worse than one that cannot
save at all."
```

---

### Task 5: `CellRef`, and the compiler's schema pass

**Files:**
- Modify: `include/Diagnostic.hpp`
- Create: `include/Compiler.hpp`, `src/Compiler.cpp`
- Modify: `CMakeLists.txt`, `include/des.hpp`
- Test: `tests/document_tests.cpp`

**Interfaces:**
- Consumes: `ModelDocument`, `ModuleRegistry`, `Diagnostic`.
- Produces:
  - `struct des::CellRef { std::string moduleType; std::size_t row; std::string column; }`
  - `Diagnostic` gains `std::optional<CellRef> cell`
  - `struct des::CompileResult { std::unique_ptr<Model> model; std::vector<Diagnostic> diagnostics; bool structureChecked; }`
  - `CompileResult des::compile(const ModelDocument& doc)`

- [ ] **Step 1: Write the failing test**

```cpp
    section("Compile: the schema pass");
    {
        ModelDocument d;
        d.addRow("Resource");
        d.setCell("Resource", 0, "Name", "Teller");
        d.setCell("Resource", 0, "Capacity", "notanumber");
        d.setCell("Resource", 0, "Nonsense", "x");

        CompileResult r = compile(d);
        check(hasErrors(r.diagnostics), "bad cells are reported");

        bool sawType = false, sawUnknown = false;
        for (const Diagnostic& g : r.diagnostics) {
            check(g.cell.has_value(), "every schema diagnostic names a cell");
            if (g.cell && g.cell->column == "Capacity") {
                sawType = true;
                check(g.cell->moduleType == "Resource" && g.cell->row == 0,
                      "and names the right module and row");
            }
            if (g.cell && g.cell->column == "Nonsense") {
                sawUnknown = true;
                check(g.severity == Severity::Warning,
                      "an unknown column is a WARNING: it is preserved, not dropped");
            }
        }
        check(sawType, "a non-numeric Integer cell is reported");
        check(sawUnknown, "an unknown column is reported");

        // A missing required cell.
        {
            ModelDocument m;
            m.addRow("Resource");
            m.setCell("Resource", 0, "Capacity", "1");
            CompileResult c = compile(m);
            bool sawMissing = false;
            for (const Diagnostic& g : c.diagnostics)
                if (g.cell && g.cell->column == "Name") sawMissing = true;
            check(sawMissing, "a missing required cell is reported against that column");
        }

        // A bad enum names what was allowed.
        {
            ModelDocument m;
            m.addRow("Process");
            m.setCell("Process", 0, "Name", "P");
            m.setCell("Process", 0, "Service", "1");
            m.setCell("Process", 0, "Discipline", "SIDEWAYS");
            CompileResult c = compile(m);
            bool named = false;
            for (const Diagnostic& g : c.diagnostics)
                if (g.cell && g.cell->column == "Discipline" &&
                    g.message.find("FIFO") != std::string::npos) named = true;
            check(named, "a bad enum lists the spellings that are allowed");
        }

        // An unknown module type is a warning, and does not stop the rest.
        {
            ModelDocument m;
            m.addRow("FromTheFuture");
            m.setCell("FromTheFuture", 0, "X", "1");
            CompileResult c = compile(m);
            bool warned = false;
            for (const Diagnostic& g : c.diagnostics)
                if (g.cell && g.cell->moduleType == "FromTheFuture" &&
                    g.severity == Severity::Warning) warned = true;
            check(warned, "an unknown module type warns rather than erroring");
        }
    }
```

- [ ] **Step 2: Run it and confirm it fails**

Expected: `'compile': identifier not found`.

- [ ] **Step 3: Extend `Diagnostic`**

In `include/Diagnostic.hpp`, add `#include <optional>` and:

```cpp
// v11: which cell a diagnostic came from, when it came from one. Additive, so
// every v10 caller compiles unchanged: the expression layer keeps filling in
// `span` and nothing else, and the compiler wraps `cell` around it. That is the
// join v10 was built toward -- the parser still never learns what a table is.
struct CellRef {
    std::string moduleType;
    std::size_t row{0};        // 0-based POSITION
    std::string column;
};
```

and add `std::optional<CellRef> cell;` as the last member of `Diagnostic`.

- [ ] **Step 4: Write `include/Compiler.hpp`**

```cpp
// ============================================================================
// Compiler.hpp  --  v11: a document becomes a Model, or becomes diagnostics
// ============================================================================
// Four passes: schema, references, expressions, structure. EVERY PASS RUNS even
// when an earlier one found errors, wherever that is meaningful, because the
// point of the whole layer is to report every bad cell rather than the first.
//
// The exception is the structure pass: it cannot run with broken references, so
// it is skipped and `structureChecked` says so. Reporting a model as checked
// when it was not is the failure this project keeps refusing.

#pragma once
#include <memory>
#include <vector>
#include "Diagnostic.hpp"
#include "Model.hpp"
#include "ModelDocument.hpp"

namespace des {

struct CompileResult {
    std::unique_ptr<Model>  model;              // null when there are errors
    std::vector<Diagnostic> diagnostics;
    bool                    structureChecked{false};
};

CompileResult compile(const ModelDocument& doc);

}  // namespace des
```

- [ ] **Step 5: Write the schema pass in `src/Compiler.cpp`**

A `Diags` helper collects into a vector, stamping a `CellRef` on each. The schema pass walks every module type in the document:

- unknown type → one `Warning` against `{type, 0, ""}`, then skip its rows;
- for each row, every cell whose column is not in the schema → `Warning`;
- every required column with no cell, or an empty one → `Error` against that column;
- each present cell checked against its `ColumnType`: `Integer` parses as a whole number, `Real` as a number, `Boolean` is `true`/`false`, `Enum` is one of `enumValues` and the message lists them.

`compile()` runs the pass, then returns early with a null model if `hasErrors`.

- [ ] **Step 6: Wire in, run the tests, commit**

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```
```bash
bash tools/verify.sh warnings
```
```bash
git add include/Diagnostic.hpp include/Compiler.hpp src/Compiler.cpp include/des.hpp CMakeLists.txt tests/document_tests.cpp
git commit -m "feat: CellRef, and the compiler's schema pass

Diagnostic gains an optional cell reference. Additive, so every v10
caller compiles unchanged: the expression layer still fills in only the
span, and the compiler wraps cell identity around it."
```

---

### Task 6: The reference pass

**Files:**
- Modify: `src/Compiler.cpp`
- Test: `tests/document_tests.cpp`

**Interfaces:**
- Consumes: the schema pass (Task 5).
- Produces: no new public names; `compile()` gains a second pass.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Compile: the reference pass");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "1");
        d.setCell("Process", 0, "Resource", "Nurse");     // never declared

        CompileResult r = compile(d);
        bool named = false;
        for (const Diagnostic& g : r.diagnostics)
            if (g.cell && g.cell->column == "Resource" &&
                g.message.find("Nurse") != std::string::npos) named = true;
        check(named, "an unresolved reference is reported AT THE CELL that holds it");

        // A reference that resolves is silent.
        {
            ModelDocument m;
            m.addRow("Resource");
            m.setCell("Resource", 0, "Name", "Nurse");
            m.setCell("Resource", 0, "Capacity", "1");
            m.addRow("Process");
            m.setCell("Process", 0, "Name", "Serve");
            m.setCell("Process", 0, "Service", "1");
            m.setCell("Process", 0, "Resource", "Nurse");
            CompileResult c = compile(m);
            for (const Diagnostic& g : c.diagnostics)
                check(!(g.cell && g.cell->column == "Resource"),
                      "a resolvable reference produces no diagnostic");
        }

        // "Block" resolves against every flowchart module, not one table.
        {
            ModelDocument m;
            m.addRow("Process");
            m.setCell("Process", 0, "Name", "A");
            m.setCell("Process", 0, "Service", "1");
            m.setCell("Process", 0, "Next", "B");
            m.addRow("Dispose");
            m.setCell("Dispose", 0, "Name", "B");
            CompileResult c = compile(m);
            for (const Diagnostic& g : c.diagnostics)
                check(!(g.cell && g.cell->column == "Next"),
                      "an exit pointing at any flowchart block resolves");
        }

        // An empty optional reference means "leaves the system", not an error.
        {
            ModelDocument m;
            m.addRow("Process");
            m.setCell("Process", 0, "Name", "A");
            m.setCell("Process", 0, "Service", "1");
            m.setCell("Process", 0, "Next", "");
            CompileResult c = compile(m);
            for (const Diagnostic& g : c.diagnostics)
                check(!(g.cell && g.cell->column == "Next"),
                      "an empty exit is not an unresolved reference");
        }

        // A child row must name a parent that exists.
        {
            ModelDocument m;
            m.addRow("DecideBranch");
            m.setCell("DecideBranch", 0, "Decide", "Ghost");
            CompileResult c = compile(m);
            bool badParent = false;
            for (const Diagnostic& g : c.diagnostics)
                if (g.cell && g.cell->column == "Decide") badParent = true;
            check(badParent, "a child row naming a missing parent is reported");
        }
    }
```

- [ ] **Step 2: Run it and confirm it fails**

Expected: the "unresolved reference" checks FAIL, because nothing resolves references yet.

- [ ] **Step 3: Implement the pass**

Build an index of declared names: for each module type, the value of its `Name` (or `Identifier`) column per row. `Block` is the pseudo-type meaning "any row of any `ModuleKind::Flowchart` module".

Then for every `Reference` cell that is non-empty, look the value up in the index for its `referencedType` and report an `Error` at that cell when it is absent. Empty cells are skipped: an empty exit means the entity leaves the system, which the engine already expresses as a null `next`.

`compile()` runs this pass **even if the schema pass found errors**, so both sets are reported together, and returns a null model if either produced errors.

- [ ] **Step 4: Run the tests and the fast gate, then commit**

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```
```bash
bash tools/verify.sh warnings
```
```bash
git add src/Compiler.cpp tests/document_tests.cpp
git commit -m "feat: the reference pass reports at the cell that holds the name

An empty exit is not an unresolved reference: the engine already spells
'leaves the system' as a null next."
```

---

### Task 7: The expression pass

**Files:**
- Modify: `src/Compiler.cpp`
- Test: `tests/document_tests.cpp`

**Interfaces:**
- Consumes: `parseExpression` (v10), the passes above.
- Produces: no new public names.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Compile: the expression pass");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.8");   // unclosed

        CompileResult r = compile(d);
        bool found = false;
        for (const Diagnostic& g : r.diagnostics)
            if (g.cell && g.cell->column == "Service") {
                found = true;
                // v10 gives the offset WITHIN the cell; v11 wraps the cell
                // around it. Both survive, which is what puts a cursor on the
                // right character of the right cell.
                check(g.span.offset == 8, "the offset within the cell survives");
                check(g.message.find("expected ')'") != std::string::npos,
                      "and v10's message comes through unchanged");
            }
        check(found, "a malformed expression is reported at its cell");

        // Several bad cells are all reported.
        {
            ModelDocument m;
            m.addRow("Process");
            m.setCell("Process", 0, "Name", "A");
            m.setCell("Process", 0, "Service", "1 +");
            m.addRow("Delay");
            m.setCell("Delay", 0, "Name", "B");
            m.setCell("Delay", 0, "Duration", "EXPOO(1)");
            CompileResult c = compile(m);
            std::size_t bad = 0;
            for (const Diagnostic& g : c.diagnostics)
                if (g.cell && (g.cell->column == "Service" || g.cell->column == "Duration"))
                    ++bad;
            check(bad >= 2, "every bad expression is reported, not just the first");
        }

        // A valid expression is silent.
        {
            ModelDocument m;
            m.addRow("Delay");
            m.setCell("Delay", 0, "Name", "B");
            m.setCell("Delay", 0, "Duration", "TRIA(1, 2, 3)");
            CompileResult c = compile(m);
            for (const Diagnostic& g : c.diagnostics)
                check(!(g.cell && g.cell->column == "Duration"),
                      "a valid expression produces no diagnostic");
        }
    }
```

- [ ] **Step 2: Run it and confirm it fails**

Expected: the "reported at its cell" check FAILs.

- [ ] **Step 3: Implement the pass**

For every cell whose column is `ColumnType::Expression` and is non-empty, call `parseExpression(text)` and copy each returned `Diagnostic` into the result with its `cell` set. The `span` is left exactly as v10 produced it.

Skip cells that are empty and not required — an optional expression column that is blank simply is not set.

- [ ] **Step 4: Run the tests and the fast gate, then commit**

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```
```bash
bash tools/verify.sh warnings
```
```bash
git add src/Compiler.cpp tests/document_tests.cpp
git commit -m "feat: the expression pass wraps cell identity around v10's spans"
```

---

### Task 8: `Model::checkStructure()`

The one v10 behaviour-adjacent change. Isolated in its own task so it can be reviewed on its own.

**Files:**
- Modify: `include/Model.hpp`, `src/Model.cpp`
- Test: `tests/document_tests.cpp`

**Interfaces:**
- Consumes: `Diagnostic`.
- Produces: `std::vector<Diagnostic> Model::checkStructure() const`; `validate()` becomes its wrapper.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Model::checkStructure");
    {
        // The collecting form reports several problems at once.
        {
            SimulationSystem sim(1u);
            sim.model().arrivals(constant(1.0));   // no entry block, source unwired
            const std::vector<Diagnostic> problems = sim.model().checkStructure();
            check(problems.size() >= 1, "checkStructure returns problems rather than throwing");
            check(hasErrors(problems), "and marks them as errors");
        }

        // validate() must still throw EXACTLY as it did, with the same message.
        {
            SimulationSystem sim(2u);
            sim.model().arrivals(constant(1.0));
            std::string what;
            try { sim.model().validate(); }
            catch (const ModelError& e) { what = e.what(); }
            check(!what.empty(), "validate() still throws");
            check(what.find("entry") != std::string::npos ||
                  what.find("source") != std::string::npos,
                  "with the message it always had");
        }

        // A sound model reports nothing.
        {
            SimulationSystem sim(3u);
            sim.model().arrivals(constant(1.0))
                       .station("W", 1, FIFO, constant(0.5))
                       .entryAt("W");
            sim.model().wireSources();
            check(!hasErrors(sim.model().checkStructure()),
                  "a sound model produces no structural problems");
        }
    }
```

- [ ] **Step 2: Run it and confirm it fails**

Expected: `'checkStructure': is not a member of 'des::Model'`.

- [ ] **Step 3: Refactor**

Move the body of `validate()` into `checkStructure()`, replacing each `throw ModelError(msg)` with `problems.push_back(Diagnostic{Severity::Error, SourceSpan{}, msg, std::nullopt})` and continuing rather than returning. Keep the checks in the same order, and keep every message string **byte-identical**.

`validate()` becomes:

```cpp
void Model::validate() const {
    // The throwing form C++ callers have always had. checkStructure() is the
    // same checks collected into a list, because a person editing a file needs
    // every problem at once and an exception can carry one.
    const std::vector<Diagnostic> problems = checkStructure();
    for (const Diagnostic& d : problems)
        if (d.severity == Severity::Error) throw ModelError(d.message);
}
```

> Two checks currently short-circuit: the routing-loop DFS throws from inside a recursive helper, and the expression check throws a combined message. Convert the DFS to record and return, and have the expression check append its diagnostics instead. Preserve the exact strings.

- [ ] **Step 4: Run the FULL gates**

This task touches v10 code, so the byte-identical gate applies.

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```
```bash
bash tools/baseline.sh check
```
```bash
bash tools/verify.sh
```

Expected: all 543 v10 checks still pass, `BASELINE CLEAN`, `VERIFY CLEAN`.

- [ ] **Step 5: Commit**

```bash
git add include/Model.hpp src/Model.cpp tests/document_tests.cpp
git commit -m "refactor: validate() becomes a wrapper over checkStructure()

Same checks, same order, same message strings -- but collected into a
list, because somebody editing a file needs every problem at once and an
exception can only carry one. The throwing form is unchanged for C++
callers, and all 543 checks pass untouched."
```

---

### Task 9: Building the Model

The pass that turns rows into `Model` calls. The largest task, and the one the whole layer exists for.

**Files:**
- Modify: `include/Compiler.hpp`, `src/Compiler.cpp`
- Test: `tests/document_tests.cpp`

**Interfaces:**
- Consumes: every pass above, and `Model`'s building API.
- Produces: `CompileResult::model` is non-null for a sound document, and `structureChecked` is true when the structure pass ran.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Compile: building the Model");
    {
        ModelDocument d;
        d.addRow("Variable");
        d.setCell("Variable", 0, "Name", "Served");
        d.setCell("Variable", 0, "Initial Value", "0");

        d.addRow("Resource");
        d.setCell("Resource", 0, "Name", "Teller");
        d.setCell("Resource", 0, "Capacity", "2");

        d.addRow("Create");
        d.setCell("Create", 0, "Name", "Arrivals");
        d.setCell("Create", 0, "Interarrival", "EXPO(1.0)");
        d.setCell("Create", 0, "Next", "Serve");

        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Resource", "Teller");
        d.setCell("Process", 0, "Units", "1");
        d.setCell("Process", 0, "Discipline", "FIFO");
        d.setCell("Process", 0, "Service", "EXPO(0.8)");
        d.setCell("Process", 0, "Next", "Out");

        d.addRow("Dispose");
        d.setCell("Dispose", 0, "Name", "Out");

        CompileResult r = compile(d);
        check(!hasErrors(r.diagnostics), "a sound document compiles cleanly");
        check(r.model != nullptr, "and produces a Model");
        check(r.structureChecked, "and the structure pass ran");

        check(r.model->node("Serve") != nullptr, "the Process block exists");
        check(r.model->resourceNamed("Teller") != nullptr, "the Resource exists");
        check(r.model->variables().has("Served"), "the Variable was declared");
        check(r.model->station("Serve")->resource().capacity() == 2,
              "the Process seizes the shared Resource, not a private one");

        // It runs.
        {
            SimulationSystem sim(4242u);
            CompileResult c = compile(d);
            check(c.model != nullptr, "compiles again");
            // Model is not copyable, so the compiler builds into the system's
            // own model rather than handing one over. See compileInto below.
        }
    }

    section("Compile: Decide branches and Assign fields keep their order");
    {
        ModelDocument d;
        d.addRow("Create");
        d.setCell("Create", 0, "Name", "In");
        d.setCell("Create", 0, "Interarrival", "1");
        d.setCell("Create", 0, "Next", "Stamp");

        d.addRow("Assign");
        d.setCell("Assign", 0, "Name", "Stamp");
        d.setCell("Assign", 0, "Next", "Sort");
        d.addRow("AssignField");
        d.setCell("AssignField", 0, "Assign", "Stamp");
        d.setCell("AssignField", 0, "Target", "Attribute");
        d.setCell("AssignField", 0, "Name", "size");
        d.setCell("AssignField", 0, "Value", "3");
        d.addRow("AssignField");
        d.setCell("AssignField", 1, "Assign", "Stamp");
        d.setCell("AssignField", 1, "Target", "Attribute");
        d.setCell("AssignField", 1, "Name", "doubled");
        d.setCell("AssignField", 1, "Value", "size * 2");   // reads field 1

        d.addRow("Decide");
        d.setCell("Decide", 0, "Name", "Sort");
        d.setCell("Decide", 0, "Type", "Condition");
        d.setCell("Decide", 0, "Next", "Small");
        d.addRow("DecideBranch");
        d.setCell("DecideBranch", 0, "Decide", "Sort");
        d.setCell("DecideBranch", 0, "Condition", "doubled > 4");
        d.setCell("DecideBranch", 0, "To", "Big");

        d.addRow("Dispose"); d.setCell("Dispose", 0, "Name", "Big");
        d.addRow("Dispose"); d.setCell("Dispose", 1, "Name", "Small");

        CompileResult r = compile(d);
        check(!hasErrors(r.diagnostics), "the document compiles");
        check(r.model != nullptr, "and builds");
        check(r.model->nodeAs<AssignNode>("Stamp").rules().size() == 2,
              "both Assign fields landed");
        check(r.model->nodeAs<AssignNode>("Stamp").rules()[0].name == "size",
              "IN FILE ORDER: the second field reads what the first wrote");
        check(r.model->nodeAs<DecideNode>("Sort").branches().size() == 1,
              "the Decide got its branch");
    }
```

> `Model` is neither copyable nor movable, so `CompileResult` cannot hold one by value and a caller cannot move it into a `SimulationSystem`. Add `bool compileInto(const ModelDocument&, Model&, std::vector<Diagnostic>&)` beside `compile()` and implement `compile()` in terms of it. `compile()` stays for tests and for `des check`; `compileInto` is what `des run` and v12 use.

- [ ] **Step 2: Run it and confirm it fails**

Expected: `r.model` is null because nothing builds yet.

- [ ] **Step 3: Implement the build pass**

Order matters, and it mirrors the order a model must be described in C++:

1. **Variables** — `model.variable(name, initial)`.
2. **Entity types** — declarative only; nothing to call.
3. **Resources** — `model.resource(name, capacity)`.
4. **Blocks**, in document order, each created without its exits:
   - `Create` → `model.source(name, entityType, interarrivalText, max, firstAt, perArrival)`
   - `Process` → `model.stationUsing(...)` when `Resource` is set, else `model.station(...)`
   - `Delay`, `Assign`, `Decide`, `Batch`, `Separate`, `Record`, `Dispose` → their `Model` calls
5. **Child rows**, in document order — `AssignField` via `assignTo` / `assignVariable` / `assignEntityType`; `DecideBranch` via `branch` or `branchWhen`.
6. **Exits**, once every block exists — `Next` via `route`, `To` via `routeTrue` or `branch`, `Duplicate` via `routeDuplicate`, `Balk To` and `Renege To` via `balkAt` / `renegeAfter`.
7. **Entry** — the first `Create`'s `Next`, or the first flowchart block.

Blocks are created before exits are wired for the same reason `Model::wireSources()` exists: the order a model is described in must not matter, and `route()` refuses a target that does not exist yet.

Every `Model` call can throw `ModelError`. Wrap the whole build in a try/catch and convert a throw into a `Diagnostic` against the row being built, so a document never propagates an exception to a caller who asked for diagnostics.

Finally, run `model.checkStructure()`, stamp each returned diagnostic with the `CellRef` of the row that named the block where possible, set `structureChecked = true`, and null the model if anything is an error.

- [ ] **Step 4: Run the tests**

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```

Expected: zero FAIL lines.

- [ ] **Step 5: Run the full gates and commit**

```bash
bash tools/baseline.sh check
```
```bash
bash tools/verify.sh
```
```bash
git add include/Compiler.hpp src/Compiler.cpp tests/document_tests.cpp
git commit -m "feat: a document builds a Model

Blocks first, exits second, for the same reason wireSources() exists: the
order a model is described in must not matter, and route() refuses a
target that does not exist yet. Child rows are applied in file order,
because an Assign field reads what the previous one wrote."
```

---

### Task 10: The `des` command

**Files:**
- Create: `cli/main.cpp`
- Modify: `CMakeLists.txt`, `README.md`
- Test: `tests/document_tests.cpp` (the compile path), and by hand

**Interfaces:**
- Consumes: `readDocumentFile`, `compileInto`, `SimulationSystem`.
- Produces: the `des` executable, and `des_demo` for the old five-scenario program.

- [ ] **Step 1: Rename the demo target**

In `CMakeLists.txt` change `add_executable(des main.cpp)` to `add_executable(des_demo main.cpp)`, and update its `target_compile_options` line to match.

In `README.md` change `./build/des          # five demonstration scenarios` to:

```
./build/des_demo     # five demonstration scenarios
./build/des          # the model-file tool: des check / des run
```

> A documented command changes here. The CLI takes the name because `des check model.des` is the product and the demo is a demo.

- [ ] **Step 2: Write `cli/main.cpp`**

```cpp
// ============================================================================
// cli/main.cpp  --  the `des` command
// ============================================================================
// Two verbs, deliberately:
//
//     des check model.des     compile and report; non-zero on error
//     des run   model.des     compile and run; print the report
//
// This is the first program this project ships that is neither a demo nor a
// test, and command-line tools accrete flags. v12 owns the interactive surface;
// this exists so a model file can be tried by hand, and so "the format is
// writable by a person" is checkable rather than merely asserted.

#include <iostream>
#include <string>
#include "des.hpp"

using namespace des;

namespace {

void printDiagnostics(const std::vector<Diagnostic>& ds, const std::string& path) {
    for (const Diagnostic& d : ds) {
        std::cout << path;
        if (d.cell) {
            std::cout << ": " << d.cell->moduleType << " row " << (d.cell->row + 1);
            if (!d.cell->column.empty()) std::cout << ", " << d.cell->column;
            // The offset WITHIN the cell, which is what v10 measured and what a
            // front end puts a cursor on.
            if (d.span.length > 0) std::cout << " (col " << (d.span.offset + 1) << ")";
        }
        std::cout << ": " << (d.severity == Severity::Error ? "error" : "warning")
                  << ": " << d.message << "\n";
    }
}

int usage() {
    std::cout << "usage: des check <model.des>\n"
                 "       des run   <model.des>\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) return usage();
    const std::string verb = argv[1];
    const std::string path = argv[2];
    if (verb != "check" && verb != "run") return usage();

    ReadResult read = readDocumentFile(path);
    printDiagnostics(read.diagnostics, path);
    if (hasErrors(read.diagnostics)) return 1;

    SimulationSystem sim;
    std::vector<Diagnostic> problems;
    const bool built = compileInto(read.document, sim.model(), problems);
    printDiagnostics(problems, path);
    if (!built) return 1;

    if (verb == "check") {
        std::cout << path << ": ok\n";
        return 0;
    }

    // `run` needs a stopping rule, and a document does not carry one yet.
    // Saying so beats inventing a horizon and reporting numbers for it.
    std::cout << "note: a model file carries no run length yet; stopping when drained\n";
    sim.setTermination(anyOf(whenDrained(), timeLimit(100000.0)));
    sim.initialise();
    sim.run();
    sim.report();
    return 0;
}
```

- [ ] **Step 3: Add the target**

In `CMakeLists.txt`, beside the other executables:

```cmake
add_executable(des cli/main.cpp)
target_link_libraries(des PRIVATE des_engine)
target_compile_options(des PRIVATE ${DES_WARNINGS})
```

- [ ] **Step 4: Try it by hand**

Write `examples/models/teller.des`:

```
version = 1

# A single teller. The smallest model the format can express.
[Create]
Name         = Arrivals
Interarrival = EXPO(1.0)
Next         = Serve

[Process]
Name       = Serve
Capacity   = 1
Discipline = FIFO
Service    = EXPO(0.8)
Next       = Out

[Dispose]
Name = Out
```

```bash
./build/Debug/des.exe check examples/models/teller.des
```

Expected: `examples/models/teller.des: ok`.

Then break it deliberately — change `EXPO(0.8)` to `EXPO(0.8` — and check the message names the module, the row, the column and the column offset.

- [ ] **Step 5: Run the tests and the full gates, then commit**

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```
```bash
bash tools/baseline.sh check
```
```bash
bash tools/verify.sh warnings
```
```bash
git add cli/main.cpp examples/models/teller.des CMakeLists.txt README.md
git commit -m "feat: the des command -- check and run a model file

Two verbs and no more. The demo becomes des_demo: the CLI takes the name
because des check model.des is the product and the demo is a demo.

run says out loud that a model file carries no run length yet, rather
than inventing a horizon and reporting numbers for it."
```

---

### Task 11: The decisive test

**Files:**
- Create: `examples/models/*.des` (three files)
- Test: `tests/document_tests.cpp`

- [ ] **Step 1: Hand-write three model files**

`examples/models/decide.des` — a Decide by condition with two branches.
`examples/models/shared.des` — two Process blocks sharing one Resource.
`examples/models/variables.des` — a Variable written by an Assign, and a service time reading an attribute.

Write them **by hand**, not by exporting. That is the point: an exporter would prove the compiler round-trips its own output, which is a weaker claim, and would quietly hide whether a person can write the format at all.

- [ ] **Step 2: Write the failing test**

```cpp
    section("A document runs identically to the same model in C++");
    {
        // The claim this whole layer rests on, tested the way v10 tested its
        // own: same seed, same model expressed two ways, traces compared event
        // for event. A near-miss average would not be evidence.
        auto traceOf = [](bool fromFile, const char* path) {
            SimulationSystem sim(20260904u);
            if (fromFile) {
                ReadResult read = readDocumentFile(path);
                check(!hasErrors(read.diagnostics), std::string("reads: ") + path);
                std::vector<Diagnostic> problems;
                const bool ok = compileInto(read.document, sim.model(), problems);
                check(ok, std::string("compiles: ") + path);
            } else {
                sim.model().arrivals("EXPO(1.0)")
                           .station("Serve", 1, FIFO, "EXPO(0.8)")
                           .dispose("Out")
                           .route("Serve", "Out")
                           .entryAt("Serve");
            }
            const std::string out = fromFile ? "doc_trace.md" : "cpp_trace.md";
            sim.enableTrace(out, TraceLevel::Events);
            sim.stopAt(200.0).initialise();
            sim.run();
            return out;
        };

        traceOf(true,  "examples/models/teller.des");
        traceOf(false, nullptr);

        std::ifstream a("doc_trace.md", std::ios::binary), b("cpp_trace.md", std::ios::binary);
        check(a.good() && b.good(), "both traces were written");
        const std::string fromDoc((std::istreambuf_iterator<char>(a)),
                                   std::istreambuf_iterator<char>());
        const std::string fromCpp((std::istreambuf_iterator<char>(b)),
                                   std::istreambuf_iterator<char>());
        check(fromDoc.size() > 500, "the trace is substantial");
        check(fromDoc == fromCpp,
              "a model compiled from a FILE runs identically to the same model in C++");
    }
```

Add `#include <fstream>` and `#include <iterator>` at the top of `tests/document_tests.cpp`.

> Whole-file comparison is right here, unlike v10's: both runs are built through the same `Model` API, so the model description in the trace header is identical too. If it differs, that is a real difference and must be understood, not filtered out.

> The test reads a file by relative path, so it depends on the working directory. Run the suite from the repository root, and if that proves fragile, make the path absolute via a constant in the test rather than weakening the assertion.

- [ ] **Step 3: Run it, fix what it finds**

```bash
cmake --build build --config Debug && ./build/Debug/des_tests.exe
```

Expected: PASS. A diff here is a real defect in the compiler — find it rather than adjusting the test.

- [ ] **Step 4: Extend to the other two files**

Repeat for `decide.des` and `variables.des`, each against the equivalent C++ model.

- [ ] **Step 5: Add the trace files to .gitignore**

```
/doc_trace.md
/cpp_trace.md
```

- [ ] **Step 6: Run the full gates and commit**

```bash
bash tools/baseline.sh check
```
```bash
bash tools/verify.sh
```
```bash
git add examples/models tests/document_tests.cpp .gitignore
git commit -m "test: a model compiled from a file runs identically to the same model in C++

Three hand-written .des files, event for event against their C++
equivalents. Hand-written rather than exported: an exporter would only
prove the compiler round-trips its own output, and would hide whether a
person can write the format at all."
```

---

### Task 12: The documents

**Files:**
- Create: `V11_READLOG.md`
- Modify: `CHANGELOG.md`, `README.md`, `ARENA_MAP.md`, `examples/README.md`

- [ ] **Step 1: Write `V11_READLOG.md`**

Follow `V10_READLOG.md`: what the version demanded, numbered sections, then the bugs and what is still open. Cover at minimum:

1. **Why routing became a column.** Arena draws connections; a TUI cannot. Exit columns are what the engine already believes, since every routing call attaches an exit to a block.
2. **Why repeating groups became flat child tables.** The property being protected is *a module added in v13 renders in an unmodified front end*, which holds only if there is one shape to render. And the consequence: row order became semantic.
3. **Byte-identical round-trip, and what it cost.** Storing cell values cannot reproduce comments or spacing, so the document keeps its source lines. Say why a front end that rewrites an unedited file is worse than one that cannot save.
4. **Queue had to become read-only.** The module list agreed during design did not survive contact with the engine: there is no queue object apart from its Process, so an editable Queue module would have held the same fact twice. Worth recording as a design error caught while writing the spec rather than while writing code.
5. **`validate()` refactored.** One implementation, two callers, and why catching the exception or duplicating the checks were both worse.
6. **Errors are data, one level up.** v10 made a bad expression a diagnostic; v11 makes a bad *cell* one, and the two compose — the span v10 measured survives inside the cell v11 names.
7. Anything that went wrong while building it. **The bugs are the most valuable part of these documents.**

- [ ] **Step 2: Update `CHANGELOG.md`**

A `v11` section at the top, in the established style.

- [ ] **Step 3: Update `README.md`**

- "Current state" becomes v11, mentioning model files and the `des` command.
- A "A model as a file" section showing a `.des` file and `des check` / `des run`.
- The check count.
- `V11_READLOG.md` in the documents table.
- Note that `des_demo` is the renamed demo.

- [ ] **Step 4: Update `ARENA_MAP.md`**

- A row per data module: Variable, Entity, Queue, Resource, Expression.
- A note that this engine's spreadsheets are flat: a Decide's branches are their own table rather than a grid inside the Decide, and why.
- Under the differences: **Queue is read-only here**, because a queue is not an object apart from its Process.

- [ ] **Step 5: Update `examples/README.md`**

A section on model files: the format, the two commands, and the traps — row order is semantic, an empty exit means "leaves the system", and unknown columns are kept but warned about.

- [ ] **Step 6: Final verification**

```bash
cmake --build build --config Debug --clean-first
```
```bash
./build/Debug/des_tests.exe
```
```bash
bash tools/baseline.sh check
```
```bash
bash tools/verify.sh
```
```bash
python tools/manual.py
```

Expected: no warnings, all checks pass, `BASELINE CLEAN`, `VERIFY CLEAN`, and the reference manual regenerates with the new types documented. **`tools/manual.py` will report the v11 types as having no prose** — add them to `tools/manual_data.py`, since a manual that silently omits a third of the engine is worse than one that admits the gap.

- [ ] **Step 7: Commit**

```bash
git add V11_READLOG.md CHANGELOG.md README.md ARENA_MAP.md examples/README.md tools/manual_data.py DES_Engine_Reference.pdf
git commit -m "v11: the model becomes a document"
```

---

## Self-review notes

**Spec coverage.** Schema types and registry → Tasks 1–2. `ModelDocument` → Task 3. The `.des` format and byte-identical round-trip → Task 4. `CellRef` → Task 5. The four compiler passes → Tasks 5, 6, 7, 9. `checkStructure()` refactor → Task 8. Data modules → Task 2 (schemas) and Task 9 (building). The `des` CLI → Task 10. The decisive test → Task 11. Docs → Task 12.

**Two things this plan specifies less tightly than v10's, deliberately.** Task 4's writer and Task 9's build pass are described by their required behaviour and ordering rather than by complete code, because both depend on the exact shape of `Model`'s building API and on decisions best made with the file open. The *requirements* are exact — byte-identical round-trip, blocks before exits, child rows in file order — and the tests pin them.

**One interface that had to be added mid-plan.** `Model` is neither copyable nor movable, so `CompileResult` cannot hand one to a `SimulationSystem`. `compileInto(document, model, diagnostics)` is the form `des run` and v12 actually use; `compile()` is the convenience for tests and `des check`. Both are named in Task 9 and used consistently in Tasks 10 and 11.

**Check counts are not predicted.** v10's plan quoted running totals and several drifted as tests were split. What matters per task is zero FAIL lines and a total that grew.
