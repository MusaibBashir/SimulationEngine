# v11 — the model as a document

**Status:** design approved, not yet implemented.
**Date:** 2026-09-04
**Branch:** `v11-document-layer`, built on `v10-expression-layer`.

---

## Where this sits

v10 made every field of a model **text**. v11 makes the **model itself** data: a
set of spreadsheets that can be edited, saved, loaded, and compiled into a
runnable `Model`. v12 then adds a terminal front end, in a separate repository,
that renders those spreadsheets.

Decisions carried forward from the three-version design, not reopened here:

- The TUI is a **separate repo**; the engine keeps versioning after it exists.
- Compatibility is **source-level, not ABI**. Schemas are discovered at runtime
  so a data module added in v13 renders in an unmodified front end; the two are
  rebuilt together. **Model files are the durable artifact** and migrate forward.
- Data modules in scope: **Variable, Entity, Queue, Resource, Expression**. Set
  and Schedule are deferred — they change simulation semantics and are far
  easier to build *through* a spreadsheet than alongside one.

## The five decisions this version turns on

| # | Question | Decision |
|---|---|---|
| 1 | How is routing represented? | **Exit columns on the source block.** |
| 2 | How are repeating groups expressed? | **Child tables with a parent column.** |
| 3 | What does a model file look like? | **Line-oriented records.** |
| 4 | One direction or two? | **Document → Model only.** |
| 5 | How are structural problems reported? | **Refactor `validate()` to collect diagnostics.** |

Each is argued where it appears below.

---

## Architecture

v11 sits **above** the engine core. The engine never learns what a document is.

```
  v12  Runtime control                        (later)
  ──────────────────────────────────────────────────────
  v11  ModuleRegistry · ModelDocument
       DocumentReader/Writer · compile() · des CLI
  ──────────────────────────────────────────────────────
  v1–v10  Model · INode · Expression · Diagnostic
```

Five units, each with one job:

| Unit | Responsibility |
|---|---|
| `ModuleSchema` / `ModuleRegistry` | What columns each module type has. The only thing a front end reads in order to render. |
| `ModelDocument` | Rows of text cells, in order. May be invalid, half-finished, contradictory. |
| `DocumentReader` / `DocumentWriter` | The `.des` format, both directions of *text*. |
| `Compiler` | `ModelDocument` → `Model`, or diagnostics that point at cells. |
| `des` CLI | `check` and `run`. The only human-facing surface v11 has. |

---

## 1. The schema

```cpp
enum class ColumnType {
    Text, Identifier, Integer, Real, Boolean,
    Enum,        // fixed set: FIFO, LIFO, PRIORITY, SPT, EDD, RANDOM
    Expression,  // parsed by v10 -- a duration, a condition, or a value
    Reference    // names a row in another module type
};

struct Column {
    std::string              id;              // "Service"
    ColumnType               type;
    bool                     required{false};
    std::string              defaultValue;
    std::vector<std::string> enumValues;      // Enum only
    std::string              referencedType;  // Reference only: "Resource"
};

enum class ModuleKind { Flowchart, Data, Child };

struct ModuleSchema {
    std::string         typeName;      // "Process"
    ModuleKind          kind;
    bool                readOnly{false};  // a derived view, not an editable table
    std::string         parentColumn;     // Child only: "Decide"
    std::vector<Column> columns;
};
```

`ModuleRegistry` holds every schema and can be iterated. **This is the whole
contract with a front end.** A front end that renders a table from a schema
renders every module type, including ones added later, without changing.

### Why routing is a column (decision 1)

Arena draws connections on a canvas; a TUI has none, so routing must exist as
data. It becomes **exit columns on the source block**: a Process has `Next`,
`Balk To` and `Renege To`; a Separate has `Next` and `Duplicate`; a
`DecideBranch` row carries its own `To`.

This is what the engine already believes. Every routing call in `Model` attaches
an exit **to a block** — `route()` sets that block's `next`, `routeTrue()` sets a
branch's target, `balkAt()` sets the balk exit. Columns are a faithful
description of that rather than a new invention.

The alternative, a separate Connections table, reads better for "what feeds
Inspect?" — but a Decide's branch targets have to live in the branch rows
anyway, so it would split routing across two places, and duplicated facts
eventually disagree. That view can be *built* from these columns by v12.

### Why repeating groups are child tables (decision 2)

A Decide has N branches; an Assign has N fields. Both become their own module
types — `DecideBranch`, `AssignField` — with a `Reference` column naming the
parent, and `ModuleKind::Child`.

The deciding argument is the property the whole boundary was designed around: *a
data module added in v13 renders in an unmodified front end.* That holds only if
there is exactly one thing to render — a flat table. Nested sub-tables would
make the front end grow a nested-grid editor, and a differently-shaped nesting
in v13 could break it.

**Row order is therefore semantic.** Reorder two `DecideBranch` rows and you
change which condition wins; reorder two `AssignField` rows and a field may no
longer see what the previous one wrote. The format preserves file order, and
`ModelDocument::moveRow` is a first-class operation rather than something a
caller fakes with delete-and-re-add.

This is a choice about the **data model, not the presentation**. v12 can still
show branches nested under their Decide; it just builds that view from flat rows.

---

## 2. `ModelDocument`

```cpp
class ModelDocument {
public:
    struct Cell { std::string text; std::size_t line{0}; };
    struct Row  { std::map<std::string, Cell> cells; };

    std::vector<Row>&  rows(const std::string& moduleType);
    Row&               addRow(const std::string& moduleType);
    void               removeRow(const std::string& moduleType, std::size_t index);
    void               moveRow(const std::string& moduleType,
                               std::size_t from, std::size_t to);
    std::string        cell(const std::string& type, std::size_t index,
                            const std::string& column) const;
    void               setCell(const std::string& type, std::size_t index,
                               const std::string& column, std::string text);
};
```

**Rows are addressed by position, never by a stable id.** An earlier draft had
both, and the two disagree the moment a row moves — exactly the duplicated-fact
problem this project's rules warn about. Position is also what a person sees in
a spreadsheet and in a file, so a diagnostic naming row 4 means the fourth one.

Three properties it is required to have:

**It may be invalid.** A Process naming a Resource that does not exist yet, a
half-typed expression, a missing required column — all legal states. That is the
entire reason it is not a `Model`, which may never hold a broken flowchart. The
strictness of `Model` is a feature, and editing convenience must not erode it.

**Row order is preserved and meaningful**, per decision 2.

**Every cell remembers its line number**, which is what turns a diagnostic into a
file position without the reader keeping a side table.

---

## 3. The `.des` format

```
version = 1

# The teller queue. Comments survive a round trip.
[Resource]
Name     = Teller
Capacity = 2

[Process]
Name       = Serve
Resource   = Teller
Discipline = FIFO
Service    = EXPO(0.8)
Next       = Inspect
```

Each `[Type]` header starts a **new row**; `key = value` lines fill its cells
until the next header. Values are raw text to end of line, so an expression needs
no quoting and no escaping: `Service = EXPO(0.8)` is exactly the cell's contents.

### Why line-oriented (decision 3)

- **It diffs per field.** Change a service time and `git diff` shows one line —
  the same property that makes this project's trace-diffing work, applied to
  models. JSON diffs acceptably; a CSV row diffs wholly for a one-cell change.
- **It reuses v10.** An error wants a line, a column and a message, which is
  `Diagnostic` and `SourceSpan`, already built and already tested for exact
  positions. A bad cell in a file and a bad cell in a spreadsheet become the same
  thing.
- **It carries comments.** People annotate models; JSON structurally cannot.
- **About 150 lines to parse**, against roughly 300 for correct JSON.

The cost, stated plainly: it is a bespoke format. Nothing else reads it without a
reader being written, and "just open it in Python" stops being free.

### Round-trip is a hard requirement

**Read a file and write it back and the bytes must be identical** — comments,
blank lines and spacing included. Anything less means the front end silently
rewrites a file when somebody opens and saves it without editing. This is the
format's equivalent of the byte-identical trace gate, and it is testable
directly.

### Unknown data is preserved, not dropped

Unknown module types and unknown columns are **kept verbatim** and reported as
**warnings**. It costs almost nothing — store the raw text — and it means an
older engine opening a newer file is non-destructive rather than quietly
deleting what it did not understand. That is what "models are the durable
artifact" has to mean in practice.

---

## 4. `compile()`

```cpp
struct CellRef { std::string moduleType; std::size_t row;   // 0-based POSITION
                 std::string column; };

struct CompileResult {
    std::unique_ptr<Model>  model;        // null when there are errors
    std::vector<Diagnostic> diagnostics;  // each carrying a CellRef
};

CompileResult compile(const ModelDocument& doc);
```

Four passes, in this order:

1. **Schema** — unknown columns, missing required cells, wrong types, bad enums.
2. **References** — every `Reference` column resolves to a row that exists.
3. **Expressions** — parse each `Expression` cell. v10 already returns spans; this
   wraps a `CellRef` around them.
4. **Structure** — build the `Model`, then `checkStructure()`: entry block,
   routing loops, stability.

**Every pass runs even when an earlier one found errors**, wherever that is
meaningful, because the point is to report every bad cell rather than the first.
The exception is pass 4: it cannot run with broken references, so it is skipped
and the result says so rather than pretending the model was checked.

### One change to a v10 type

`Diagnostic` gains an optional cell reference:

```cpp
struct Diagnostic {
    Severity                severity;
    SourceSpan              span;      // offset WITHIN the cell -- unchanged
    std::string             message;
    std::optional<CellRef>  cell;      // v11: which cell, if it came from one
};
```

Additive, so every v10 caller compiles unchanged. The expression layer keeps
filling in `span` and nothing else; the compiler wraps `cell` around it. This is
the join v10 was built toward — *"v11 adds row and column identity around them;
the parser never learns what a spreadsheet is."*

### Why `validate()` is refactored (decision 5)

`Model::validate()` throws on the **first** structural problem. That is right for
a C++ programmer and wrong for someone with a half-finished file, who would fix
one error, recompile, find the next, and repeat — the workflow this whole layer
exists to avoid. It also cannot say *which cell* contains the offending name.

So `validate()` becomes a thin wrapper over `checkStructure()`, which returns a
list. The alternatives were catching the exception, which loses the cell and
reports one error per compile, and re-implementing the checks against the
document, which duplicates rules that would eventually disagree.

**Constraint, unchanged from v10:** `validate()` must still throw exactly as it
does today, with the same messages, and all 543 checks must pass unmodified.

One thing this cannot fully deliver: the stability check produces a number, not a
cell. "ρ = 1.4 at Teller" points at a block, but the fault may be an arrival rate
three modules away. The diagnostic attaches to the Process row and names a
symptom rather than a cause, and says so.

---

## 5. The data modules

| Module | Kind | Columns |
|---|---|---|
| `Variable` | Data | Name, Initial Value |
| `Entity` | Data | Name (the type name arrivals carry) |
| `Queue` | Data (read-only) | Name, Process, Discipline — see below |
| `Resource` | Data | Name, Capacity |
| `Expression` | Data | Name, Value — a named expression referenced from several blocks |
| `DecideBranch` | Child | Decide, Probability \| Condition, To |
| `AssignField` | Child | Assign, Target, Name, Value |

Flowchart modules — Create, Process, Delay, Assign, Decide, Batch, Separate,
Record, Dispose — get schemas describing the columns `Model` already accepts.

### Queue is read-only, and that is a correction

Writing this spec exposed a problem with the module list agreed earlier. **This
engine has no queue object independent of a Process.** An `EntityQueue` is owned
by its `Station`, and its discipline is set on the Process. An editable `Queue`
module would therefore hold the *same fact* as the Process row's `Discipline`
column — two places to edit one thing, which is precisely what this project's
rules refuse.

Three options, and only one is honest:

- **Editable `Queue` module** — duplicates the discipline. Rejected.
- **Drop `Queue`** — loses a genuinely useful view, and breaks a module the
  design agreed to deliver.
- **Read-only `Queue`** — lists every queue with its owning Process, its
  discipline and, after a run, its statistics. Nothing is edited here; the
  Process row remains the one place a discipline is set.

`ModuleSchema` therefore gains `bool readOnly{false}`, and `Queue` is its first
user. A front end greys such a table rather than offering to edit it.

Making queues first-class objects — shared between blocks, as Arena allows — is
an engine change, and it belongs with Set and Schedule in a later version rather
than being smuggled in here.

---

## 6. The `des` CLI

Two verbs, and no more:

```
des check model.des      # compile; print diagnostics; exit non-zero on error
des run   model.des      # compile and run; print the report
```

Without it v11 has no human-facing surface at all: a library layer whose only
consumer does not exist until v12, with no way to try a model file by hand. It
also turns "the format is writable by a person" from an assertion into something
checkable in a minute.

Kept to two verbs deliberately. This is the first executable the project ships
that is neither a demo nor a test, and CLIs accrete flags.

---

## 7. How it is proven

1. **Round-trip byte-identical** — read and write returns the same bytes,
   comments and blank lines included, on hand-written and generated files.
2. **Diagnostics point at cells** — a file with five bad cells reports five
   diagnostics with the right module, row, column, and the right character offset
   *within* the cell. Asserted exactly, as v10 asserts parser positions.
3. **Every pass reports** — a document with a bad enum *and* a broken reference
   *and* a malformed expression yields all three.
4. **Order is preserved** — `moveRow` changes which `DecideBranch` wins, and a
   saved-and-reloaded document routes identically.
5. **Unknown data survives** — a file with an unrecognised module and column
   round-trips unchanged and warns.

### The decisive test

**Hand-written `.des` files for three or four examples compile to models whose
traces are byte-identical to the C++ versions.** One with a Decide, one with a
shared resource, one with variables and expressions.

Hand-written rather than exported, deliberately: an exporter would prove the
compiler round-trips its own output, which is a weaker claim and would quietly
hide whether a person can write the format at all.

### Gates that do not move

All **543** v10 checks pass unchanged. The **15** byte-identical example
baselines are untouched. Clean under GCC 14.2, Clang 19.1, MSVC
AddressSanitizer, and WSL GCC with ASan and UBSan.

---

## Explicitly not in v11

| Deferred | Why |
|---|---|
| `Model → Document` export | Needs a parseable printer on every AST node — `describe()` prints `Exponential(mean=1)`, not `EXPO(1)` — and changing `describe()` would break the byte-identical gate. Its only consumer would be a test. |
| The TUI | Separate repo, after v12. |
| Set and Schedule data modules | Change simulation semantics; easier to build through a spreadsheet than alongside one. |
| Variable arrays (1-D, 2-D) | YAGNI until a model needs one. |
| Anything in the CLI beyond `check` and `run` | v12 owns the interactive surface. |
| Fixing the `12_shared_resources` non-determinism | A v9 bug, still open, still excluded from the baseline gate with its reasoning recorded. |
| Fixing Separate duplicates inheriting the `counted` mark | A v9 bug found during v10 review. Fixing it changes `NumberOut` and WIP for every model using `duplicate()`, so it needs its own version. |
