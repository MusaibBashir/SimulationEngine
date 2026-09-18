# v16 Ride-Hailing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> This project's standing instruction is to work inline (no subagents), so it is executed with executing-plans.

**Goal:** Build the Group 9 ride-hailing Arena model in this engine (spec:
`docs/superpowers/specs/2026-09-18-v16-ride-hailing-design.md`), plus the Runs-tab
form and new-file naming in the TUI.

**Architecture:** Each Arena construct becomes a module schema (text format), a
compiler step (`src/Compiler.cpp`), a Model builder call (`src/Model.cpp`) and
an `INode` (`include/Nodes.hpp` / new headers). Statistics that reports need are
exposed per replication as a flat list of named values, which the single-run and
multi-replication reports print. TUI work lives in `TuiState` / `TuiInput` /
`TuiRender`.

**Tech Stack:** C++17, CMake (MinGW g++ locally, MSVC + WSL in `tools/verify.sh`),
the project's own test harness (`tests/harness.hpp`: `section`, `check`,
`checkClose`).

## Global Constraints

- Examples (`tools/baseline.sh check`) and regression models (`des regress`)
  stay byte-identical. Never re-capture an existing expectation.
- `des regress` gains exactly one new model, `ride_hailing.des`, captured once.
- Comments only where they carry a reason; match surrounding style.
- Every column has a one-line help string (a test walks the registry).
- Commits end with `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`.
- Never commit `newmodel.des` or `scratch_new.des`.
- Build: `cmake --build build -j 8`; suite: `./build/des_tests`.

## Decisions made while planning (spec refinements)

1. **Max Arrivals counts arrival events** (Arena), not entities. `Per Arrival =
   80, Max Arrivals = 1` makes 80 entities. No existing model or example uses
   Per Arrival > 1, so nothing already captured changes.
2. **A Match lane breaks the routing-loop check.** The check exists for loops
   nobody can leave (a Process loop fed by endless arrivals). Cars circulating
   back to a Match lane are a population waiting for partners, which Arena
   allows. The DFS stops at a `MatchLaneNode`.
3. **The statistics section** lists *named* statistics only: Records built from
   a document (their Statistic defaults to the block name), Match lanes and
   Routes. Records built through the C++ API (`record()`, `recordAttribute()`,
   `recordTimeInSystem()`) stay unnamed, so example 11's baseline is untouched.
4. **RunController summary with no Process**: utilisation is 0 instead of
   `stationAt(0)` on an empty list.
5. **Mouse in the Runs form**: click selects a field; clicking the selected
   field again edits it (the terminal layer has no double-click).
6. **Route destination is not `next()`**: a Route's `next()` stays null, so the
   loop check and visit ratios stop there; the destination is its own member.

---

### Task 1: One-step line replacement, and the Runs tab form

**Files:**
- Modify: `include/TextBuffer.hpp`, `src/TextBuffer.cpp` — `replaceLines`
- Modify: `include/TuiState.hpp`, `src/TuiState.cpp` — run form state + edits
- Modify: `src/TuiInput.cpp` — Runs keys, mouse
- Modify: `src/TuiRender.cpp` — two-pane Runs view, hints, key map
- Create: `tests/v16_tests.cpp` (+ register in `tests/tests.cpp`, `CMakeLists.txt`)

**Interfaces (produced):**
```cpp
// TextBuffer: replace [first, first+count) with `with`, ONE undo step.
void replaceLines(std::size_t first, std::size_t count,
                  const std::vector<std::string>& with);

// TuiState
enum class RunsFocus { List, Form };
RunsFocus runsFocus() const;            void setRunsFocus(RunsFocus);
const std::vector<std::string>& runFields() const;   // Run schema column ids
std::size_t runField() const;           void stepRunField(long long);
void setRunField(std::size_t);
bool editingRunField() const;           const std::string& runFieldDraft() const;
void beginRunFieldEdit();               // Enter; Boolean/Enum open the picker
void typeRunField(char c); void backspaceRunField();
bool commitRunFieldEdit();              // false + status when invalid
void cancelRunFieldEdit();
bool setRunValue(std::size_t run, const std::string& column, const std::string& value);
void requestDeleteRun();                // opens Confirm(DeleteRun)
bool deleteRun(std::size_t run);        // removes block + comments directly above
// Confirm gains a purpose so one overlay serves several questions:
enum class ConfirmFor { Quit, NewFile, DeleteRun, OpenExisting };
ConfirmFor confirmFor() const;
```
`setRunValue` rewrites only the `key = value` line (inserting after the last
line of the block when absent; deleting it when the value is blank). Validation
reuses the compiler's column rules: build a copy of the document with the new
cell, run `checkSchema`-equivalent via `compile()` diagnostics filtered to that
cell; any Error there refuses the edit with its message.

- [ ] **Step 1: failing tests** in `tests/v16_tests.cpp`:
```cpp
void runV16Tests() {
    section("replaceLines is one undo step");
    {
        TextBuffer b("a\nb\nc");
        b.replaceLines(1, 1, {"x", "y"});
        check(b.text() == "a\nx\ny\nc", "the middle line is replaced by two");
        b.undo();
        check(b.text() == "a\nb\nc", "and one undo puts it back");
    }
    section("The Runs form edits the text, and keeps the comments");
    {
        TuiState s = TuiState::fromText(
            "version = 1\n# how long\n[Run]\nName   = Base\nLength = 480\n", "t.des");
        check(s.setRunValue(0, "Replications", "10"), "a new field is accepted");
        check(s.buffer().text().find("Replications = 10") != std::string::npos,
              "and written into the [Run] block");
        check(s.buffer().text().find("# how long") != std::string::npos, "the comment survives");
        check(s.setRunValue(0, "Length", "600"), "an existing field is rewritten");
        check(s.document().cell("Run", 0, "Length") == "600", "the document sees it");
        check(!s.setRunValue(0, "Replications", "ten"), "letters in a whole number are refused");
        check(s.document().cell("Run", 0, "Replications") == "10", "and the text is not changed");
        check(s.setRunValue(0, "Length", ""), "blank removes the line");
        check(s.buffer().text().find("Length") == std::string::npos, "so it is gone");
        s.undo();
        check(s.buffer().text().find("Length = 600") != std::string::npos, "one undo restores it");
    }
    section("Deleting a run removes its block and the comments above it");
    {
        TuiState s = TuiState::fromText(
            "version = 1\n[Run]\nName = A\n\n# the second\n[Run]\nName = B\nLength = 5\n"
            "\n[Create]\nName = C\n", "t.des");
        check(s.deleteRun(1), "run B is deleted");
        check(s.runs().size() == 1 && s.runs()[0] == "A", "only A is left");
        check(s.buffer().text().find("# the second") == std::string::npos, "its comment went too");
        check(s.buffer().text().find("[Create]") != std::string::npos, "the next block did not");
        s.undo();
        check(s.runs().size() == 2, "one undo brings it back");
    }
    section("The Runs tab: A adds, Delete asks, Tab reaches the form");
    {
        TuiState s = TuiState::fromText("version = 1\n[Run]\nName = A\n", "t.des");
        handleKey(s, Key::control('u'));
        handleKey(s, Key::character('a'));
        check(s.runs().size() == 2, "A adds a run");
        check(s.view() == View::Runs, "without leaving the Runs tab");
        handleKey(s, Key::special(KeyKind::Delete));
        check(s.overlay() == Overlay::Confirm && s.confirmFor() == ConfirmFor::DeleteRun,
              "Delete asks first");
        handleKey(s, Key::character('y'));
        check(s.runs().size() == 1, "and y deletes");
        handleKey(s, Key::special(KeyKind::Tab));
        check(s.runsFocus() == RunsFocus::Form, "Tab moves into the form");
        // Field 0 is Name; Down to Length, Enter, type, Enter.
        handleKey(s, Key::special(KeyKind::Down));
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.editingRunField(), "Enter edits the field");
        typeText(s, "90");
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.document().cell("Run", 0, "Length") == "90", "and Enter writes it");
    }
}
```
- [ ] **Step 2:** register `runV16Tests` in `tests/tests.cpp` next to `runTuiTests`, add
  `tests/v16_tests.cpp` to `des_tests` in `CMakeLists.txt`; build; expect compile
  errors for the missing members.
- [ ] **Step 3: implement** `replaceLines` (pushUndo, erase, insert, caret to `first`);
  run form state; `setRunValue` (find the row's header line via
  `document().rows("Run")[i].headerLine`, block end = line before the next `[`
  header or EOF, trailing blank lines excluded; key match with `keyOfLine`);
  `deleteRun` (block range plus contiguous `#` lines directly above, plus one
  preceding blank line if that leaves two blanks in a row); Confirm purpose
  (`requestQuit` sets `ConfirmFor::Quit`; `handleConfirm` dispatches on it; y/n
  for DeleteRun and OpenExisting). Runs keys: `a`/Insert add (stay on Runs),
  Delete asks, Tab/Shift-Tab switch pane, in the form Up/Down/Enter/Esc/`?`;
  ^N no longer adds a run. Render: list on the left (name, reps), form on the
  right (`Field : value` with the default dimmed), the edited field shows the
  draft with a caret; hints and key map updated.
- [ ] **Step 4:** build + `./build/des_tests` → all pass (earlier TUI tests that
  used ^N on the Runs tab are updated to `a`).
- [ ] **Step 5:** commit `v16: the Runs tab edits and deletes runs`.

### Task 2: Naming a new file

**Files:** `include/TuiState.hpp`, `src/TuiState.cpp`, `src/TuiInput.cpp`,
`src/TuiRender.cpp`, `tui/main.cpp`, `tests/v16_tests.cpp`

**Interfaces (produced):**
```cpp
// Pure: bare name -> <defaultDir>/<name>.des; a path is used as given;
// ".des" added when there is no extension; empty/blank -> "" (refused).
std::string resolveModelName(const std::string& typed, const std::string& defaultDir);

enum class PromptFor { None, NewFile, SaveAs };
// Overlay::Prompt added to Overlay.
void openPrompt(PromptFor why);     PromptFor promptFor() const;
const std::string& promptText() const;
void typePrompt(char c); void backspacePrompt();
void commitPrompt();                void cancelPrompt();
void requestNewFile();              // ^N: Confirm(NewFile) when dirty, else prompt
bool unnamed() const;               void setUnnamed(bool);
void setDefaultDirectory(std::string dir);   // main passes dirname(defaultModelPath())
void newBlankModel(const std::string& path); // empty buffer, clean undo, unnamed=false
```
`save()` on an unnamed model opens `Prompt(SaveAs)` and returns false; the
prompt's commit calls `save(resolved)`, sets the path, clears `unnamed`.
`main.cpp`: when no argument was given and the default file did not exist,
`state.setUnnamed(true)`; after each key, if the path changed, re-set the
window title.

- [ ] **Step 1: failing tests:**
```cpp
    section("A typed model name becomes a path");
    {
        check(resolveModelName("line2", "D") == "D/line2.des", "a bare name goes in the default folder");
        check(resolveModelName("line2.des", "D") == "D/line2.des", ".des is not doubled");
        check(resolveModelName("sub/x", "D") == "sub/x.des", "a path is used as given");
        check(resolveModelName("a.txt", "D") == "D/a.txt", "an extension is kept");
        check(resolveModelName("  ", "D").empty(), "blank is refused");
    }
    section("^N asks for a name; an unnamed model asks on first save");
    {
        const std::string dir = "v16_newfile_dir";
        std::filesystem::create_directories(dir);
        std::filesystem::remove(dir + "/fresh.des");
        TuiState s = TuiState::fromText("", dir + "/untitled.des");
        s.setDefaultDirectory(dir);
        s.setUnnamed(true);
        typeText(s, "x");
        handleKey(s, Key::control('s'));
        check(s.overlay() == Overlay::Prompt && s.promptFor() == PromptFor::SaveAs,
              "the first save of an unnamed model asks for a name");
        typeText(s, "fresh");
        handleKey(s, Key::special(KeyKind::Enter));
        check(std::filesystem::exists(dir + "/fresh.des"), "and saves under it");
        check(!s.unnamed() && !s.dirty(), "after which it is named and clean");
        handleKey(s, Key::control('n'));
        check(s.overlay() == Overlay::Prompt && s.promptFor() == PromptFor::NewFile,
              "^N on a clean model goes straight to the name");
        typeText(s, "fresh");
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.overlay() == Overlay::Confirm && s.confirmFor() == ConfirmFor::OpenExisting,
              "an existing name asks before opening it");
        handleKey(s, Key::character('n'));
        check(s.overlay() == Overlay::Prompt, "no goes back to the prompt");
        handleKey(s, Key::special(KeyKind::Escape));
        typeText(s, "y");
        handleKey(s, Key::control('n'));
        check(s.overlay() == Overlay::Confirm && s.confirmFor() == ConfirmFor::NewFile,
              "^N with unsaved changes asks first");
        handleKey(s, Key::character('d'));
        typeText(s, "other");
        handleKey(s, Key::special(KeyKind::Enter));
        check(s.path() == dir + "/other.des" && s.buffer().text().empty(),
              "discard opens a blank model at the new name");
        std::filesystem::remove_all(dir);
    }
```
- [ ] **Step 2:** build → fails. **Step 3:** implement as specified; render the
  prompt as a one-line box over the body with the caret; hint row shows
  `Enter accepts   Esc cancels`; Confirm text per purpose. **Step 4:** pass.
- [ ] **Step 5:** commit `v16: ^N and the first save ask for a name`.

### Task 3: Create — Per Arrival expression, Max Arrivals counts arrivals

**Files:** `include/Create.hpp`, `src/Create.cpp`, `include/Model.hpp`,
`src/Model.cpp`, `src/Compiler.cpp`, `src/ModuleSchemas.cpp`, `tests/v16_tests.cpp`

**Interfaces:**
```cpp
// CreateNode
void setPerArrival(ExpressionPtr e);          // overrides the int
bool perArrivalIsExpression() const;
const IExpression* perArrivalExpression() const;
long long arrivals() const;                   // arrival EVENTS so far
// exhausted() == m_maxArrivals >= 0 && m_arrivals >= m_maxArrivals
// Model
Model& perArrival(const std::string& createName, const std::string& text);
```
Schema: `Per Arrival` becomes `expression("Per Arrival", help, false, "1")`.
Compiler: when the cell is present and is not a plain integer literal, call
`model.perArrival(name, text)`; plain integers keep the int path (so describe()
and offeredLoad are unchanged). Validation: the expression is validated with
`FieldContext::NoEntity`. At run time, a negative result throws
`ModelError("create 'X': Per Arrival came out negative (-2)")`.

- [ ] **Step 1: failing tests:**
```cpp
    section("Per Arrival may be a variable, and Max Arrivals counts arrivals");
    {
        const char* text =
            "version = 1\n[Run]\nName = R\nLength = 10\n"
            "[Variable]\nName = Fleet\nInitial Value = 20\n"
            "[Create]\nName = Cars\nInterarrival = 1\nPer Arrival = Fleet\n"
            "Max Arrivals = 1\nNext = Out\n[Dispose]\nName = Out\n";
        CompileResult c = compile(readDocument(text).document);
        check(c.model != nullptr, "it compiles");
        SimulationSystem sim(1);
        std::vector<Diagnostic> ignored;
        compileInto(readDocument(text).document, sim.model(), ignored);
        sim.stopAt(10).execute();
        check(sim.results().arrived == 20, "one arrival of Fleet = 20 entities");
    }
```
- [ ] **Step 2–4:** fail → implement → pass (whole suite, then
  `tools/baseline.sh check` and `./build/des regress` stay clean).
- [ ] **Step 5:** commit `v16: Per Arrival is an expression; Max Arrivals counts arrivals`.

### Task 4: Array variables

**Files:** `include/VariableStore.hpp`, `src/VariableStore.cpp`,
`include/Parser.hpp`, `src/Parser.cpp`, `include/Functions.hpp`,
`src/Functions.cpp`, `include/Expression.hpp`, `src/Expression.cpp`,
`include/Model.hpp`, `src/Model.cpp`, `src/Compiler.cpp`, `src/ModuleSchemas.cpp`,
`tests/v16_tests.cpp`

**Interfaces:**
```cpp
// VariableStore
void declareArray(const std::string& name, int rows, int columns,
                  const std::vector<double>& initial);   // size 1 fills all
bool isArray(const std::string& name) const;
int  rowsOf(const std::string& name) const;  int columnsOf(const std::string& name) const;
double element(const std::string& name, double row, double column = 1.0) const; // 1-based, rounded
std::vector<std::string> arrayNames() const;
// Parser
ParseResult parseExpression(const std::string& text,
                            const std::vector<std::string>& arrayNames);
// Functions: buildCall gains `const std::vector<std::string>* arrayNames`;
// a name in that list builds ArrayRead(name, args) instead of "unknown function".
// Model
Model& variableArray(const std::string& name, int rows, int columns,
                     const std::vector<double>& initial);
```
`get(name)` on an array throws "TravelTime is an array -- read it as
TravelTime(row, column)". `ArrayRead::validate` checks arity vs dimensions
(1-D takes 1 index, 2-D takes 2). Out of range throws `ModelError("TravelTime(4,
1) is outside its 3 x 3 cells")`. Model::expr and the compiler's pass 3 pass
the array names (compiler collects them from Variable rows with `Rows` set).
Schema: `Rows`, `Columns` integers (optional); `Initial Value` becomes `text`
(a list); compiler checks every item is a number and the count is 1 or
rows×columns, else an error on the cell. Assigning to an array (AssignField
Target Variable naming one) is an error "not yet supported".

- [ ] **Step 1: failing tests:**
```cpp
    section("An array variable is read by index");
    {
        const char* text =
            "version = 1\n[Run]\nName = R\nLength = 5\n"
            "[Variable]\nName = T\nRows = 2\nColumns = 2\nInitial Value = 1, 2, 3, 4\n"
            "[Create]\nName = C\nInterarrival = 1\nMax Arrivals = 1\nNext = A\n"
            "[Assign]\nName = A\nNext = Out\n"
            "[AssignField]\nAssign = A\nTarget = Attribute\nName = got\nValue = T(2, 1)\n"
            "[Record]\nName = Out\nWhat = Attribute\nAttribute = got\n";
        SimulationSystem sim(1);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(text).document, sim.model(), d), "it compiles");
        sim.stopAt(5).execute();
        check(sim.model().nodeAs<RecordNode>("Out").average() == 3.0, "T(2,1) is 3, row-major");
    }
    {
        std::vector<Diagnostic> d;
        Model m;
        compileInto(readDocument("version = 1\n[Variable]\nName = T\nRows = 2\nColumns = 2\n"
                                 "Initial Value = 1, 2, 3\n").document, m, d);
        check(hasErrors(d), "three values for four cells is an error");
    }
    {
        VariableStore v;
        v.declareArray("T", 3, 3, {0});
        bool threw = false;
        try { v.element("T", 4, 1); } catch (const ModelError&) { threw = true; }
        check(threw, "an index outside the array stops the run");
    }
```
- [ ] **Step 2–4:** fail → implement → pass (plus the existing
  "unknown function" test still passes).
- [ ] **Step 5:** commit `v16: array variables`.

### Task 5: Record — Count/TimeInterval/Expression, Value, Statistic

**Files:** `include/Nodes.hpp`, `src/Nodes.cpp`, `include/Model.hpp`,
`src/Model.cpp`, `src/Compiler.cpp`, `src/ModuleSchemas.cpp`, `tests/v16_tests.cpp`

**Interfaces:**
```cpp
enum class What { Count, Attribute, TimeInSystem, TimeInterval, Expression };
// RecordNode additions
void setValue(ExpressionPtr v);               // Count amount / Expression value
void setStatistic(std::string name);          // "" = unnamed (C++ API default)
const std::string& statistic() const;
bool isCounter() const;                        // What::Count
static std::unique_ptr<RecordNode> timeInterval(std::string name, std::string attribute);
// Model
Model& recordNamed(const std::string& name, RecordNode::What what,
                   const std::string& attribute, const std::string& valueText,
                   const std::string& statistic);
```
Count adds `Value` (default 1) to `m_total`, so `total()` is the counter;
TimeInterval records `now - attribute`; Expression records `Value`. Compiler:
every Record row goes through `recordNamed` with statistic =
`Statistic` or the block Name. `Model::checkStructure` errors when one statistic
name is used by both a counter and a tally Record.

- [ ] **Step 1: failing tests:**
```cpp
    section("Records share a statistic, and TimeInterval measures from an attribute");
    {
        const char* text =
            "version = 1\n[Run]\nName = R\nLength = 100\n"
            "[Create]\nName = C\nInterarrival = 10\nMax Arrivals = 3\nNext = S\n"
            "[Assign]\nName = S\nNext = D\n"
            "[AssignField]\nAssign = S\nTarget = Attribute\nName = t0\nValue = TNOW\n"
            "[Delay]\nName = D\nDuration = 2\nNext = W\n"
            "[Record]\nName = W\nWhat = TimeInterval\nAttribute = t0\nStatistic = Wait\nNext = K\n"
            "[Record]\nName = K\nWhat = Count\nValue = 2\nStatistic = Trips\n";
        SimulationSystem sim(1);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(text).document, sim.model(), d), "it compiles");
        sim.stopAt(100).execute();
        check(sim.model().nodeAs<RecordNode>("W").average() == 2.0, "every interval is 2");
        check(sim.model().nodeAs<RecordNode>("K").total() == 6.0, "3 passes x Value 2 = 6");
    }
    {
        std::vector<Diagnostic> d;
        Model m;
        compileInto(readDocument(
            "version = 1\n[Create]\nName = C\nInterarrival = 1\nNext = A\n"
            "[Record]\nName = A\nWhat = Count\nStatistic = X\nNext = B\n"
            "[Record]\nName = B\nWhat = TimeInSystem\nStatistic = X\n").document, m, d);
        check(hasErrors(d), "a counter and a tally may not share a name");
    }
```
- [ ] **Step 2–4:** fail → implement → pass; baseline + regress clean.
- [ ] **Step 5:** commit `v16: Record counts, time intervals and expressions into named statistics`.

### Task 6: Station and Route

**Files:** create `include/Transfer.hpp`, `src/Transfer.cpp` (StationNode,
RouteNode); modify `CMakeLists.txt`, `include/des.hpp`, `include/Model.hpp`,
`src/Model.cpp`, `src/Compiler.cpp`, `src/ModuleSchemas.cpp`, `tests/v16_tests.cpp`

**Interfaces:**
```cpp
class StationNode : public INode {          // zero time, pass-through
public: explicit StationNode(std::string name);
    void enter(NodeContext&, Entity*) override; long long arrivals() const;
};
class RouteNode : public INode {
public: RouteNode(std::string name, ExpressionPtr time);
    void setDestination(StationNode* s);  StationNode* destination() const;
    void setNext(INode*) override;        // throws: a Route has no Next
    void enter(NodeContext&, Entity*) override;          // schedules return
    void onScheduledEvent(NodeContext&, Entity*) override; // delivers
    long long travelled() const; double averageInTransit(SimTime now, SimTime since) const;
    const IExpression& timeExpression() const;
};
Model& station(const std::string& name);          // name overload: no capacity arg
Model& routeBlock(const std::string& name, const std::string& timeText,
                  const std::string& destination);
```
Note `Model::station(name, int, …)` already exists for Process; the new one is
`stationBlock(name)` to avoid the overload confusion. Schemas: `Station {Name,
Next}`, `Route {Name, Route Time (expression), Destination (ref Station)}`.
Compiler: Route destinations wired in the exits pass; a Station nothing routes
to (no Route destination, no Next/branch naming it) is a Warning. Route time is
validated in `checkExpressions`.

- [ ] **Step 1: failing test:**
```cpp
    section("A Route delivers to its Station after the route time");
    {
        const char* text =
            "version = 1\n[Run]\nName = R\nLength = 100\n"
            "[Create]\nName = C\nInterarrival = 10\nMax Arrivals = 2\nNext = Go\n"
            "[Route]\nName = Go\nRoute Time = 3\nDestination = There\n"
            "[Station]\nName = There\nNext = T\n"
            "[Record]\nName = T\nWhat = TimeInSystem\n";
        SimulationSystem sim(1);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(text).document, sim.model(), d), "it compiles");
        sim.stopAt(100).execute();
        check(sim.model().nodeAs<RecordNode>("T").average() == 3.0, "3 minutes on the road");
        check(sim.model().nodeAs<RecordNode>("T").count() == 2, "both arrived");
    }
    {
        std::vector<Diagnostic> d;
        Model m;
        compileInto(readDocument("version = 1\n[Create]\nName = C\nInterarrival = 1\nNext = Go\n"
                                 "[Route]\nName = Go\nRoute Time = 1\nDestination = C\n").document, m, d);
        check(hasErrors(d), "a Route must go to a Station");
    }
```
- [ ] **Step 2–4:** fail → implement → pass. **Step 5:** commit `v16: Route and Station`.

### Task 7: Match

**Files:** create `include/Match.hpp`, `src/Match.cpp`; modify `CMakeLists.txt`,
`include/des.hpp`, `include/Model.hpp`, `src/Model.cpp`, `src/Compiler.cpp`,
`src/ModuleSchemas.cpp`, `src/SimulationSystem.cpp` (NQ of a lane),
`tests/v16_tests.cpp`

**Interfaces:**
```cpp
class MatchNode;
class MatchLaneNode : public INode {
public: MatchLaneNode(std::string name, MatchNode* owner, std::size_t index);
    void enter(NodeContext&, Entity*) override;
    std::size_t waiting() const;  std::size_t maxWaiting() const;
    double averageWaiting(SimTime now, SimTime since) const;
    double averageWait() const;   long long matched() const;
    void reset() override;        void resetStatistics(SimTime now) override;
};
class MatchNode : public INode {
public: MatchNode(std::string name, std::string attribute /* "" = Any */);
    MatchLaneNode& addLane(std::unique_ptr<MatchLaneNode>&); // via Model
    void enter(NodeContext&, Entity*) override;   // throws: route to a lane
    void tryMatch(NodeContext& ctx);
    std::size_t laneCount() const;  const MatchLaneNode& lane(std::size_t) const;
};
Model& match(const std::string& name, const std::string& attribute);
Model& matchLane(const std::string& matchName, const std::string& laneName);
```
Lanes are ordinary nodes in `m_nodes` (so `route()`/`node()` find them).
Each lane keeps a deque of `{Entity*, SimTime since}` and a time-weighted area.
`tryMatch` loops: Any → all lanes non-empty → take fronts; Attribute → for each
distinct value in lane 0, check every lane has it; pick the value whose earliest
waiter across lanes is oldest; take the oldest with that value from each lane.
Remove all first, then route in lane order; a re-entrancy flag makes a nested
arrival only enqueue, and the outer loop re-checks. Compiler: Match rows then
MatchLane rows (child, parent column `Match`), lane Next wired in the exits
pass; errors: fewer than 2 or more than 5 lanes, Type Attribute without
Attribute, a `Next`/branch/Route naming a Match itself ("route to one of its
lanes: A, B"). `referenceCandidates("Block")` includes MatchLane names and
excludes Match names. The routing-loop DFS stops at a MatchLaneNode.
`SimulationSystem::queueLength` answers for lane names.

- [ ] **Step 1: failing tests:**
```cpp
    section("Match pairs entities with the same attribute");
    {
        const char* text =
            "version = 1\n[Run]\nName = R\nLength = 100\n"
            "[Create]\nName = Riders\nInterarrival = 10\nMax Arrivals = 2\nNext = SR\n"
            "[Assign]\nName = SR\nNext = RL\n"
            "[AssignField]\nAssign = SR\nTarget = Attribute\nName = zone\nValue = 1\n"
            "[Create]\nName = Cars\nInterarrival = 1\nFirst At = 25\nMax Arrivals = 1\nNext = SC\n"
            "[Assign]\nName = SC\nNext = CL\n"
            "[AssignField]\nAssign = SC\nTarget = Attribute\nName = zone\nValue = 1\n"
            "[Match]\nName = M\nType = Attribute\nAttribute = zone\n"
            "[MatchLane]\nMatch = M\nName = RL\nNext = Picked\n"
            "[MatchLane]\nMatch = M\nName = CL\nNext = Gone\n"
            "[Record]\nName = Picked\nWhat = TimeInSystem\n"
            "[Dispose]\nName = Gone\n";
        SimulationSystem sim(1);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(text).document, sim.model(), d), "it compiles");
        sim.stopAt(100).execute();
        const auto& picked = sim.model().nodeAs<RecordNode>("Picked");
        check(picked.count() == 1, "one car picks up exactly one rider");
        check(picked.average() == 25.0, "the rider who waited longest (arrived at 0) goes first");
        check(sim.model().nodeAs<MatchLaneNode>("RL").waiting() == 1, "the other still waits");
    }
    {
        std::vector<Diagnostic> d;
        Model m;
        compileInto(readDocument("version = 1\n[Create]\nName = C\nInterarrival = 1\nNext = L\n"
                                 "[Match]\nName = M\n[MatchLane]\nMatch = M\nName = L\n").document, m, d);
        check(hasErrors(d), "a Match needs at least two lanes");
    }
```
- [ ] **Step 2–4:** fail → implement → pass; the existing routing-loop test
  still fails its loop. **Step 5:** commit `v16: Match`.

### Task 8: Arrival schedules and the base time unit

**Files:** create `include/Schedule.hpp`, `src/Schedule.cpp` (ArrivalSchedule);
modify `CMakeLists.txt`, `include/Create.hpp`, `src/Create.cpp`,
`include/Model.hpp`, `src/Model.cpp`, `src/Compiler.cpp`, `src/ModuleSchemas.cpp`,
`include/RunSetup.hpp`, `src/RunSetup.cpp`, `tests/v16_tests.cpp`

**Interfaces:**
```cpp
enum class TimeUnit { Seconds, Minutes, Hours, Days };
double unitsPer(TimeUnit from, TimeUnit to);   // e.g. Hours->Minutes = 60
struct ScheduleSlot { double rate; double duration; };   // in base units
class ArrivalSchedule {
public: explicit ArrivalSchedule(std::vector<ScheduleSlot> slots); // throws on bad
    double rateAt(SimTime t) const;   // repeats after the last slot
    double peak() const;  double cycle() const;
};
// CreateNode
CreateNode(std::string name, std::string entityType, ArrivalSchedule schedule,
           long long maxArrivals, SimTime firstAt, int perArrival);
bool scheduled() const;  const ArrivalSchedule* schedule() const;
// Model
Model& scheduledSource(const std::string& name, const std::string& entityType,
                       ArrivalSchedule schedule, long long maxArrivals,
                       SimTime firstAt, int perArrival);
```
Thinning in `onScheduledEvent`: draw a candidate gap `-ln(U)/peak` from
`ctx.rng()`; at each candidate, accept with probability `rateAt(now)/peak`
(second draw); the first candidate is at `First At + gap`. A scheduled Create has
no interarrival expression: `describe()`, `Model::describe()`,
`arrivalRateIsKnown()` (→ false), `offeredLoad()` and `checkExpressions()` skip it.
Compiler: Run `Time Units` (enum, default Minutes) read only when a Schedule
exists; each ScheduleRow's Value and Duration are converted (`rate / unitsPer(
schedUnit, base)`, `duration * unitsPer(schedUnit, base)`). Errors: Create with
both or neither of Interarrival/Schedule; negative Value/Duration; zero total
duration; all rates zero. Warning: unused Schedule. `Interarrival` stops being
`required` in the schema; the "neither" check replaces it.

- [ ] **Step 1: failing tests:**
```cpp
    section("A schedule changes the arrival rate over time");
    {
        ArrivalSchedule s({{0.0, 60.0}, {2.0, 60.0}});
        check(s.rateAt(30) == 0.0 && s.rateAt(90) == 2.0, "the rate follows the rows");
        check(s.rateAt(150) == 0.0, "and repeats after the last one");
        check(unitsPer(TimeUnit::Hours, TimeUnit::Minutes) == 60.0, "an hour is 60 minutes");
        const char* text =
            "version = 1\n[Run]\nName = R\nLength = 120\nTime Units = Minutes\n"
            "[Schedule]\nName = S\nType = Arrival\nTime Units = Hours\n"
            "[ScheduleRow]\nSchedule = S\nValue = 0\nDuration = 1\n"
            "[ScheduleRow]\nSchedule = S\nValue = 600\nDuration = 1\n"
            "[Create]\nName = C\nSchedule = S\nNext = Out\n"
            "[Record]\nName = Out\nWhat = TimeInSystem\n";
        SimulationSystem sim(7);
        std::vector<Diagnostic> d;
        check(compileInto(readDocument(text).document, sim.model(), d), "it compiles");
        sim.stopAt(120).execute();
        const long long n = sim.results().arrived;
        check(n > 500 && n < 700, "about 600 arrivals, all in the second hour");
    }
    {
        std::vector<Diagnostic> d;
        Model m;
        compileInto(readDocument("version = 1\n[Schedule]\nName = S\n"
                                 "[ScheduleRow]\nSchedule = S\nValue = 1\nDuration = 1\n"
                                 "[Create]\nName = C\nSchedule = S\nInterarrival = 1\n").document, m, d);
        check(hasErrors(d), "a Create cannot have both a schedule and an interarrival");
    }
```
(The "all in the second hour" claim is checked by also asserting a Record placed
after a `Decide` on `TNOW < 60` counts 0 — added if the first form passes
trivially.)
- [ ] **Step 2–4:** fail → implement → pass. **Step 5:** commit `v16: arrival schedules`.

### Task 9: Named statistics in both reports

**Files:** `include/SimulationSystem.hpp`, `src/SimulationSystem.cpp`,
`include/Experiment.hpp`, `src/Experiment.cpp`, `include/RunController.hpp`,
`src/RunController.cpp`, `tests/v16_tests.cpp`

**Interfaces:**
```cpp
struct NamedStatistic { std::string label; double value{0.0}; };
std::vector<NamedStatistic> SimulationSystem::namedStatistics() const;
void reportNamedStatistics(std::ostream& os,
                           const std::vector<std::vector<NamedStatistic>>& perReplication);
```
Labels, in node order: a named statistic from Records → `"<Statistic> (tally)"`
(pooled mean of all Records sharing it) or `"<Statistic> (counter)"` (sum);
each Match lane → `"<lane> avg number"`, `"<lane> avg wait"`, `"<lane> at end"`;
each Route → `"<route> avg in transit"`. `report()` appends a
`statistic / value` table when the list is non-empty. `RunController` keeps
`std::vector<std::vector<NamedStatistic>>`, averages antithetic halves
position-wise, and after the summary prints `mean  +/- 95% half-width` rows
(single replication: the single report already has them). `finishRun` uses
utilisation 0 when the model has no Process.

- [ ] **Step 1: failing tests:**
```cpp
    section("Named statistics reach both reports");
    {
        const char* text =
            "version = 1\n[Run]\nName = R\nLength = 50\nReplications = 3\n"
            "[Create]\nName = C\nInterarrival = EXPO(1)\nNext = W\n"
            "[Record]\nName = W\nWhat = Count\nStatistic = Trips\n";
        std::vector<Diagnostic> d;
        auto run = RunController::fromDocument(readDocument(text).document, d);
        check(run != nullptr, "a model with no Process can be run");
        run->runToCompletion();
        std::ostringstream os;
        run->report(os);
        check(os.str().find("Trips (counter)") != std::string::npos,
              "the counter is in the multi-replication report");
        check(os.str().find("+/-") != std::string::npos, "with a half-width");
    }
```
- [ ] **Step 2–4:** fail → implement → pass; baseline + regress clean.
- [ ] **Step 5:** commit `v16: counters, tallies, Match lanes and Routes in the report`.

### Task 10: The new modules in the TUI

**Files:** `src/TuiState.cpp` (`flowOf`, palette), `src/TuiRender.cpp`,
`tests/v16_tests.cpp`

`flowOf`: MatchLane rows appear as blocks named `M ▸ Lane` (type `MatchLane`,
its Next as the exit); a Route shows `to ⇢ Station`; the `unreached` pass counts
Route destinations and lane names as arrivals. The palette lists the new
flowchart and data modules (Match, Route, Station, Schedule) and the child
tables through the registry already; verify and test.

- [ ] **Step 1: failing test:**
```cpp
    section("The flow view shows Match lanes and Route destinations");
    {
        const std::vector<FlowBlock> f = flowOf(readDocument(
            "version = 1\n[Route]\nName = Go\nRoute Time = 1\nDestination = S\n"
            "[Station]\nName = S\n[Match]\nName = M\n"
            "[MatchLane]\nMatch = M\nName = A\n[MatchLane]\nMatch = M\nName = B\n").document);
        bool route = false, lane = false, stationReached = true;
        for (const FlowBlock& b : f) {
            if (b.name == "Go") for (const auto& e : b.exits) route |= e.find("S") != std::string::npos;
            if (b.type == "MatchLane") lane = true;
            if (b.name == "S") stationReached = !b.unreached;
        }
        check(route && lane && stationReached, "lanes listed, route drawn, station reached");
    }
```
- [ ] **Step 2–4:** fail → implement → pass. **Step 5:** commit `v16: the flow view knows Match and Route`.

### Task 11: The ride-hailing model

**Files:** create `examples/models/ride_hailing.des`; modify
`tests/regression/manifest`, add `tests/regression/ride_hailing.expected`
(captured once), `tests/v16_tests.cpp`

The model: guide §0–§7 with §8.1 (schedules), §8.2 (shifts/breaks), §8.3
(TravelTime), §8.4 (deadhead), §8.6 (SLA); Num_CV = 10, Num_FV = 40 as in the
report; Run: 480 / 60 warm-up / 10 replications (regression uses `--run` of a
second `[Run]` named `Regression`, 2 replications, to keep the gate fast).

- [ ] **Step 1: failing test:**
```cpp
    section("The Group 9 model builds, conserves and shows the imbalance");
    {
        ReadResult r = readDocument(fileText(modelPath("ride_hailing.des")));
        CompileResult c = compile(r.document);
        check(c.model != nullptr, "the ride-hailing model compiles");
        std::vector<Diagnostic> d;
        auto run = RunController::fromDocument(r.document, d);
        run->runToCompletion();
        std::ostringstream os; run->report(os);
        const auto meanOf = [&](const std::string& label) {
            const std::string t = os.str();
            const std::size_t at = t.find(label);
            return at == std::string::npos ? -1.0
                 : std::strtod(t.c_str() + t.find_first_of("0123456789-", at + label.size()), nullptr);
        };
        check(meanOf("Wait_Suburb (tally)") > meanOf("Wait_Downtown (tally)"),
              "Suburb riders wait longer than Downtown riders");
    }
```
Conservation is checked on one replication with `SimulationSystem`:
`arrived == exited + (lanes waiting) + (in Routes) + (in Delays)`.
- [ ] **Step 2–4:** write the model → pass; `./build/des regress --capture` only
  for the new model (verify the other four expectations are byte-identical via
  `git diff --stat tests/regression`).
- [ ] **Step 5:** commit `v16: the Group 9 ride-hailing model`.

### Task 12: Documentation and gates

**Files:** `tools/manual_data.py` → `DES_Engine_Reference.pdf`, `CHANGELOG.md`,
`README.md`, create `V16_READLOG.md`.

- [ ] Manual: version line `v16 — the ride-hailing model`, new types (MatchNode,
  MatchLaneNode, StationNode, RouteNode, ArrivalSchedule, TimeUnit,
  NamedStatistic) and new members; `python tools/manual.py` reports complete.
- [ ] CHANGELOG `[16.0.0]`, README module table + counts, readlog with the
  decisions above and gate results.
- [ ] Gates: `./build/des_tests`, `tools/baseline.sh check`, `./build/des regress`,
  `tools/verify.sh`, `tools/package.sh`.
- [ ] Commit `v16: docs, manual and gates`.
