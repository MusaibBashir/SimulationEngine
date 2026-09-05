# v12 Runtime Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a run something a front end can start, watch, pause and cancel — and something a harness can drive over many models and diff byte for byte.

**Architecture:** `SimulationSystem::run()` is split into `canStep()` + `stepOnce()`, leaving `run()` as `while (stepOnce()) {}`. A `RunController` above it owns the replication loop and the state machine, and `Experiment::run()` is refactored into a wrapper over it. A `[Run]` data module puts the run length in the model file. `des regress` runs a manifest of models and diffs their reports.

**Tech Stack:** C++17, CMake, no external dependencies. The hand-rolled test harness in `tests/`.

## Global Constraints

- **C++17.** No newer features, no external dependencies.
- **All 779 v11 checks must pass, unchanged.** Not adapted, not deleted. If one needs changing, stop and raise it.
- **The 15 gated examples stay byte-identical:** `bash tools/baseline.sh check` must print `BASELINE CLEAN`.
- **`bash tools/verify.sh` must print `VERIFY CLEAN`** — GCC 14.2 and Clang 19.1 warning-clean on engine, CLI and tests; MSVC AddressSanitizer clean; WSL Linux GCC ASan + UBSan clean.
- **Build:** `cmake --build build` (MinGW Makefiles). Binaries land in `build/`, examples in `build/examples/`. Run the test suite as `./build/des_tests.exe` **from the repository root**.
- **Headers declare, sources define.** Every `.cpp` includes its own header first.
- **`unique_ptr` = ownership, raw pointer = observation.** Every polymorphic base gets a virtual destructor.
- **Sources are LISTED in `CMakeLists.txt`**, never globbed (except `examples/`).
- **New public headers go in `include/des.hpp`.**
- **User errors are `Diagnostic` values; programmer errors are thrown `ModelError`.** Never mix these.
- **Comments only where they carry design rationale.** This codebase explains *why*, never *what*. Do not narrate code.
- Namespace `des` throughout.

### Constraints specific to v12

- **The order inside the run loop does not move.** Close the integrals for the interval that just ended (old state), then advance the clock, then let the handler change state. Swapping any two corrupts every time-weighted average with no error anywhere.
- **`std::nullopt` from `progress()` means "cannot tell", never "zero".** Fourth occurrence of this rule in this project.
- **Nothing is added to the inner loop.** A snapshot is built on demand. A run must be bit-for-bit identical whether or not anyone is watching.
- **v11's `compileInto` signature does not change.** `readRunSetup` is a separate free function.
- **A gate that measured nothing must never report success.**

## File Structure

**New headers/sources** (each added to `add_library(des_engine ...)` and to `include/des.hpp`):

| File | Responsibility |
|---|---|
| `include/RunSetup.hpp` / `src/RunSetup.cpp` | The `RunSetup` struct and `readRunSetup()` — the `[Run]` module in both directions. Separate from the compiler so `Compiler.hpp` is untouched. |
| `include/RunController.hpp` / `src/RunController.cpp` | `RunState`, `RunProgress`, `RunSnapshot`, `RunController`. The replication loop and the state machine. |
| `include/Regression.hpp` / `src/Regression.cpp` | The manifest, running it, and diffing. Kept out of `cli/` so it is testable. |

**New test file:** `tests/runtime_tests.cpp`, exposing `void runRuntimeTests();`.

**Modified:** `include/SimulationSystem.hpp` + `src/SimulationSystem.cpp` (`canStep`, `stepOnce`, `m_stopped`, `report(std::ostream&)`), `include/TerminationRule.hpp` + `src/TerminationRule.cpp` (`progress()`), `include/Model.hpp` (resource enumeration), `include/ModelDocument.hpp` + `src/ModelDocument.cpp` (`cellOrDefault`), `src/ModuleSchemas.cpp` (the `[Run]` schema), `include/Experiment.hpp` + `src/Experiment.cpp` (refactored onto `RunController`), `cli/main.cpp`, `CMakeLists.txt`, `tests/tests.cpp`, `V11_READLOG.md`, `README.md`, `CHANGELOG.md`, `ARENA_MAP.md`, `examples/README.md`, `examples/models/*.des`.

**Ordering rationale.** The step extraction comes first because everything else stands on it and it is the riskiest change in the version — if it is wrong, the byte-identical gate says so before any new interface has been built on top. Progress and snapshots land before the controller that reports them. `Experiment` is refactored in the same task that adds the replication loop, so the one behaviour that must not change is proved in the task that could change it. The `[Run]` module comes after the controller because it is what the controller reads, not what it is.

---

### Task 1: Capturable reports, and a correction to the v11 readlog

**Files:**
- Modify: `include/SimulationSystem.hpp`, `src/SimulationSystem.cpp`, `V11_READLOG.md`
- Test: `tests/runtime_tests.cpp` (created here), `tests/tests.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces: `void SimulationSystem::report(std::ostream&) const`, `void SimulationSystem::reportArenaStyle(std::ostream&) const`, and the test entry point `void runRuntimeTests()`.

- [ ] **Step 1: Correct `V11_READLOG.md`**

Its closing section is headed `## v12 — the order to do it in` and lists a terminal front end. That contradicts the arc agreed at v10: v12 is runtime control, and the TUI is a separate repository built afterwards. Replace the whole section, from that heading to the end of the file, with:

```markdown
## v12 — the order to do it in

Runtime control. The TUI is a **separate repository, after v12** — that was
settled in the three-version design at v10, and an earlier draft of this
section said otherwise.

1. **A steppable run.** `SimulationSystem::run()` becomes `while (stepOnce())`,
   so a caller can own the loop and therefore pause, cancel and watch.
2. **`RunController`** — the replication loop and the state machine, with
   `Experiment::run()` refactored onto it.
3. **Progress that admits when it cannot tell.** A `whenDrained()` run has no
   knowable fraction, and a progress bar that reads 0% and then jumps to 100%
   is worse than one that says it does not know.
4. **A `[Run]` module**, so a model file carries its own run length and the
   open item above closes.
5. **A regression harness** over model files, byte-identical, that fails when
   it checked nothing.
```

> This is a documentation bug, not a code one, and it is fixed first because every later task's commit message would otherwise be written against a plan the readlog contradicts.

- [ ] **Step 2: Add the ostream overloads**

In `include/SimulationSystem.hpp`, replace the two declarations:

```cpp
    void report() const;
```
with
```cpp
    // v12: the regression harness compares report TEXT, so it needs the report
    // as a string. Redirecting std::cout would also work and is worse -- a
    // global side effect, inside a harness whose entire value is that its
    // result can be trusted.
    void report(std::ostream& os) const;
    void report() const;
```

and

```cpp
    void reportArenaStyle() const;
```
with
```cpp
    void reportArenaStyle(std::ostream& os) const;
    void reportArenaStyle() const;
```

Add `#include <ostream>` to the header's include list.

- [ ] **Step 3: Convert the two definitions**

In `src/SimulationSystem.cpp`, change `void SimulationSystem::report() const {` to `void SimulationSystem::report(std::ostream& os) const {`, replace every `std::cout` in that function body with `os`, replace `reportStability(std::cout)` with `reportStability(os)`, and add immediately after it:

```cpp
void SimulationSystem::report() const { report(std::cout); }
```

Do exactly the same for `reportArenaStyle`. There is no logic change in either: only the stream the text goes to.

- [ ] **Step 4: Create the test file**

Create `tests/runtime_tests.cpp`:

```cpp
// ============================================================================
// tests/runtime_tests.cpp  --  v12: the run as something a caller drives
// ============================================================================
#include <sstream>
#include <string>
#include "harness.hpp"
#include "des.hpp"

using namespace des;
using des_test::check;
using des_test::checkClose;
using des_test::section;

void runRuntimeTests() {
    section("A report can be captured");
    {
        SimulationSystem sim(4242u);
        sim.model().arrivals("EXPO(1.0)")
                   .station("Serve", 1, FIFO, "EXPO(0.5)")
                   .entryAt("Serve");
        sim.stopAt(50.0).execute();

        std::ostringstream a, b;
        sim.report(a);
        sim.report(b);
        check(a.str().find("simulation report") != std::string::npos,
              "report(ostream) writes the report");
        check(a.str() == b.str(),
              "and reporting twice gives the same text, so it can be diffed");

        std::ostringstream arena;
        sim.reportArenaStyle(arena);
        check(arena.str().find("Replication ended at time") != std::string::npos,
              "reportArenaStyle(ostream) writes the Arena layout");
    }
}
```

- [ ] **Step 5: Wire it into the suite**

In `tests/tests.cpp`, beside the two existing forward declarations near line 27, add:

```cpp
void runRuntimeTests();       // tests/runtime_tests.cpp
```

and after the `runDocumentTests();` call near line 1634, add:

```cpp
    runRuntimeTests();
```

In `CMakeLists.txt`, change the `des_tests` source list to include the new file:

```cmake
add_executable(des_tests tests/tests.cpp tests/harness.cpp tests/expression_tests.cpp tests/document_tests.cpp tests/runtime_tests.cpp)
```

> `tools/verify.sh` globs `tests/*.cpp`, so it picks the new file up with no edit. That glob exists because v11 shipped with `document_tests.cpp` missing from a hand-written list and the warnings gate silently skipped every v11 test.

- [ ] **Step 6: Build and run**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: the new `[A report can be captured]` section appears and the total is 782 or more, with no FAIL lines.

- [ ] **Step 7: Commit**

```bash
git add include/SimulationSystem.hpp src/SimulationSystem.cpp tests/runtime_tests.cpp tests/tests.cpp CMakeLists.txt V11_READLOG.md
git commit -m "feat: reports can be written to any stream

The regression harness compares report TEXT, so it needs one as a string.
Redirecting std::cout would also work and is worse: a global side effect
inside a harness whose entire value is that its result can be trusted.

Also corrects V11_READLOG's closing section, which named v12 as a terminal
front end. The TUI is a separate repository AFTER v12; v12 is runtime
control. That was settled in the three-version design at v10."
```

---

### Task 2: The step

**Files:**
- Modify: `include/SimulationSystem.hpp`, `src/SimulationSystem.cpp:428-480`
- Test: `tests/runtime_tests.cpp`

**Interfaces:**
- Produces: `bool SimulationSystem::canStep() const`, `bool SimulationSystem::stepOnce()`. `run()` keeps its signature and its behaviour.

- [ ] **Step 1: Write the failing test**

Add to `runRuntimeTests()`, after the section from Task 1:

```cpp
    section("Stepping is running, in pieces");
    {
        // The claim the whole version stands on: the size of the pieces a
        // caller takes must not change the run. Compared as TRACES, event for
        // event, because an equal summary would not be evidence -- two
        // different event orders can average the same.
        auto traceOf = [](const std::string& out, std::size_t chunk) {
            {
                SimulationSystem sim(20260905u);
                sim.model().arrivals("EXPO(1.0)")
                           .station("Serve", 1, FIFO, "EXPO(0.8)")
                           .dispose("Out")
                           .route("Serve", "Out")
                           .entryAt("Serve");
                sim.enableTrace(out, TraceLevel::Events);
                sim.stopAt(200.0).initialise();
                if (chunk == 0) {
                    sim.run();
                } else {
                    bool more = true;
                    while (more) {
                        more = false;
                        for (std::size_t i = 0; i < chunk; ++i) {
                            if (!sim.stepOnce()) break;
                            more = true;
                        }
                    }
                }
            }
            std::ifstream in(out, std::ios::binary);
            return std::string((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        };

        const std::string whole = traceOf("step_whole.md", 0);
        check(whole.size() > 500, "the trace is substantial");
        check(whole == traceOf("step_1.md", 1),
              "one event at a time traces identically to run()");
        check(whole == traceOf("step_7.md", 7),
              "seven at a time traces identically to run()");
        check(whole == traceOf("step_1000.md", 1000),
              "a thousand at a time traces identically to run()");
    }
```

Add `#include <fstream>` and `#include <iterator>` to the top of `tests/runtime_tests.cpp`.

- [ ] **Step 2: Run it and watch it fail to compile**

```bash
cmake --build build
```

Expected: `error: 'class des::SimulationSystem' has no member named 'stepOnce'`.

- [ ] **Step 3: Declare the step**

In `include/SimulationSystem.hpp`, in the private data near line 109 beside `bool m_initialised{false};`, add:

```cpp
    // v12: an EndSimulation event has fired. run() used to `return` out of its
    // own loop for this, which a caller-owned loop cannot see. Reset by
    // initialise(), like every other piece of run state.
    bool m_stopped{false};
```

In the `--- running ---` public block, beside `void run();`, add:

```cpp
    // v12: one event, so a caller can own the loop and therefore pause, cancel
    // and watch between events. run() is `while (stepOnce()) {}` -- the same
    // loop, with the caller holding the handle.
    bool canStep() const;
    bool stepOnce();
```

- [ ] **Step 4: Extract the loop body**

In `src/SimulationSystem.cpp`, replace the whole of `SimulationSystem::run()` (lines 428 to the closing brace, keeping every comment inside it) with:

```cpp
bool SimulationSystem::canStep() const {
    return !m_stopped && !m_fel.isEmpty()
           && !(m_termination && m_termination->isMet(*this));
}

bool SimulationSystem::stepOnce() {
    assert(m_initialised && "call initialise() before stepOnce()");
    if (!canStep()) return false;

    EventNotice notice = m_fel.popImminent();

    // ORDER IS EVERYTHING: close the integrals for the interval that just
    // ended -- using the OLD state -- then move the clock, then let the
    // handler change state. Swap any two and every time average is wrong.
    updateAllIntegrals(notice.time());
    m_clock.advanceTo(notice.time());

    switch (notice.type()) {
        case EventType::Arrival: {
            // A source's turn to produce. Same shape as any other callback.
            NodeContext ctx(*this);
            if (notice.node()) notice.node()->onScheduledEvent(ctx, nullptr);
            refreshState();
            break;
        }
        case EventType::Departure:     handleDeparture(notice); break;
        case EventType::EndSimulation: m_stopped = true; return false;
        case EventType::WarmUpEnd:     handleWarmUpEnd();       break;
        case EventType::Observe:       handleObservation();     break;
        case EventType::Renege: {
            // A patience timer. It fires whether or not the entity is still
            // waiting -- the block checks and ignores it if stale. See
            // Station::onRenegeTimeout for why nothing is ever cancelled.
            NodeContext ctx(*this);
            if (notice.node() && notice.entity())
                notice.node()->onRenegeTimeout(ctx, notice.entity());
            refreshState();
            break;
        }
        case EventType::StartService:  break;   // reserved
    }
    return true;
}

void SimulationSystem::run() {
    assert(m_initialised && "call initialise() before run()");
    while (stepOnce()) {}
    // The clock JUMPS event to event. Nothing is simulated in between because
    // nothing happens in between -- that is why DES is fast, and why the FEL
    // had to be sorted.
}
```

> **Two things that look cosmetic and are not.** `EndSimulation` used to `return` out of `run()`, which a caller-owned loop has no way to observe — hence `m_stopped`, and hence `stepOnce()` returning `false` for it: that event ends the run, it does not advance it. And `canStep()` deliberately does **not** test `m_initialised`: being initialised is a precondition, asserted by the two callers, and folding it into `canStep()` would turn "you forgot to initialise" into a run that silently does nothing.

> The long commentary that used to sit at the bottom of `run()` — about `IEventHandler`, `NodeContext` and publishing an interface for a role — is v6 history about the switch, so move it above `stepOnce()`, which is where the switch now lives. Do not delete it.

- [ ] **Step 5: Reset the flag**

In `SimulationSystem::initialise()`, in the block of resets beginning `m_clock.reset();` near line 382, add:

```cpp
    m_stopped = false;
```

> Miss this and a second replication on a reused system does nothing at all, silently — the same shape as the v2.1 bug where an unreset resource left the only server busy forever and the second run served nobody.

- [ ] **Step 6: Run the test**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS. A diff here is a real defect in the extraction — find it rather than adjusting the test.

- [ ] **Step 7: Run the byte-identical gate**

```bash
bash tools/baseline.sh check
```

Expected: `BASELINE CLEAN`, 15 examples, 0 differ. **This is the check that matters most in the whole version.** If any example differs, the extraction changed the run and the diff says where.

- [ ] **Step 8: Ignore the trace artefacts**

Add to `.gitignore`, beside the existing `/doc_trace.md` line:

```
/step_whole.md
/step_1.md
/step_7.md
/step_1000.md
```

- [ ] **Step 9: Full gates, then commit**

```bash
bash tools/verify.sh
```

```bash
git add include/SimulationSystem.hpp src/SimulationSystem.cpp tests/runtime_tests.cpp .gitignore
git commit -m "feat: run() becomes while (stepOnce())

The riskiest edit in v12: the loop this splits is the one where closing the
integrals, moving the clock and running the handler must happen in that
order, and swapping any two corrupts every time-weighted average with no
error anywhere. Nothing inside the body moved.

EndSimulation used to return out of run(), which a caller-owned loop cannot
observe, so it sets m_stopped and stepOnce() reports false: that event ends
the run rather than advancing it. canStep() deliberately does not test
m_initialised -- that is a precondition the callers assert, and folding it in
would turn a forgotten initialise() into a run that silently does nothing.

Evidence: the same model traced with run(), and stepped 1, 7 and 1000 events
at a time, is byte-identical four ways; 15 examples still byte-identical."
```

---

### Task 3: Progress a rule can refuse to guess

**Files:**
- Modify: `include/TerminationRule.hpp`, `src/TerminationRule.cpp`
- Test: `tests/runtime_tests.cpp`

**Interfaces:**
- Produces: `virtual std::optional<double> ITerminationRule::progress(const SimulationSystem&) const`, overridden by `TimeLimit`, `EntityLimit` and `AnyOf`. `DrainedRule` deliberately does not override it.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Progress says when it cannot tell");
    {
        SimulationSystem sim(7u);
        sim.model().arrivals("EXPO(1.0)")
                   .station("Serve", 1, FIFO, "EXPO(0.5)")
                   .entryAt("Serve");
        sim.setTermination(timeLimit(100.0));
        sim.initialise();
        for (int i = 0; i < 200 && sim.canStep(); ++i) sim.stepOnce();

        const TimeLimit t(100.0);
        const std::optional<double> f = t.progress(sim);
        check(f.has_value(), "a time limit knows how far through it is");
        if (f) check(*f > 0.0 && *f <= 1.0, "and it is a fraction");

        const DrainedRule drained;
        check(!drained.progress(sim).has_value(),
              "whenDrained() CANNOT tell, and says so rather than reporting 0");

        const EntityLimit e(50);
        check(e.progress(sim).has_value(), "an entity limit knows");

        // AnyOf reports the largest fraction any child knows: the run ends when
        // the FIRST rule is met, so the most advanced one is the honest answer.
        auto composite = std::make_unique<AnyOf>();
        composite->add(whenDrained());
        composite->add(timeLimit(100.0));
        check(composite->progress(sim).has_value(),
              "anyOf knows if ANY child knows");

        auto blind = std::make_unique<AnyOf>();
        blind->add(whenDrained());
        check(!blind->progress(sim).has_value(),
              "and refuses when NONE of them do");
    }
```

Add `#include <memory>` and `#include <optional>` to the top of `tests/runtime_tests.cpp`.

- [ ] **Step 2: Run it and watch it fail to compile**

```bash
cmake --build build
```

Expected: `error: 'const class des::TimeLimit' has no member named 'progress'`.

- [ ] **Step 3: Add the interface method**

In `include/TerminationRule.hpp`, add `#include <optional>` to the includes, and inside `class ITerminationRule` after `describe()`:

```cpp
    // v12: how far through this rule the run is, as a fraction, or NOTHING.
    //
    // Nothing means CANNOT TELL. It does not mean zero, and the difference is
    // the whole reason this returns an optional: a whenDrained() run has no
    // knowable fraction, and a progress bar that reads 0% for its whole
    // duration and then jumps to 100% is not an approximation, it is a lie the
    // caller has no way to detect. This is the fourth time this project has
    // had to write that rule down.
    //
    // Defaulted, so a rule that cannot answer says so by saying nothing.
    virtual std::optional<double> progress(const SimulationSystem&) const {
        return std::nullopt;
    }
```

Declare the override in `TimeLimit`, `EntityLimit` and `AnyOf` — and **not** in `DrainedRule`:

```cpp
    std::optional<double> progress(const SimulationSystem& sim) const override;
```

- [ ] **Step 4: Implement them**

In `src/TerminationRule.cpp`, add `#include <algorithm>` and after each rule's `describe()`:

```cpp
std::optional<double> TimeLimit::progress(const SimulationSystem& sim) const {
    if (!(m_maxTime > 0.0)) return std::nullopt;
    return std::min(1.0, sim.now() / m_maxTime);
}
```

```cpp
std::optional<double> EntityLimit::progress(const SimulationSystem& sim) const {
    if (m_maxEntities <= 0) return std::nullopt;
    const double done = static_cast<double>(sim.statistics().numberServed());
    return std::min(1.0, done / static_cast<double>(m_maxEntities));
}
```

```cpp
std::optional<double> AnyOf::progress(const SimulationSystem& sim) const {
    // The LARGEST fraction any child knows. The run ends when the FIRST rule is
    // met, so the most advanced child is the honest estimate; averaging with a
    // child that has barely started would report less progress than is real.
    std::optional<double> best;
    for (const auto& r : m_rules) {
        const std::optional<double> f = r->progress(sim);
        if (f && (!best || *f > *best)) best = f;
    }
    return best;
}
```

`DrainedRule` gets no override. It inherits the default and therefore says nothing, which is the correct answer: whether a system will next be empty is not knowable in advance.

- [ ] **Step 5: Run the tests**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS, no FAIL lines.

- [ ] **Step 6: Commit**

```bash
git add include/TerminationRule.hpp src/TerminationRule.cpp tests/runtime_tests.cpp
git commit -m "feat: a termination rule reports its progress, or admits it cannot

Returns optional<double>, and nothing means CANNOT TELL rather than zero.
DrainedRule deliberately has no override: whether a system will next be empty
is not knowable in advance, and a progress bar that reads 0% for a whole run
and then jumps to 100% is a lie the caller cannot detect.

Fourth occurrence of this rule, after VisitRatios::exact, v9's silently-zero
WIP and v10's unknowable arrival mean that made rho = 2 look stable."
```

---

### Task 4: What a watcher sees

**Files:**
- Create: `include/RunController.hpp` (structs only in this task), `src/RunController.cpp`
- Modify: `include/Model.hpp`, `include/des.hpp`, `CMakeLists.txt`
- Test: `tests/runtime_tests.cpp`

**Interfaces:**
- Consumes: `SimulationSystem`'s existing read-only views.
- Produces: `RunState`, `RunProgress`, `BlockSnapshot`, `ResourceSnapshot`, `VariableSnapshot`, `RunSnapshot`, `RunSnapshot snapshotOf(const SimulationSystem&)`, and `Model::resourceCount()` / `Model::resourceAt(i)`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("A snapshot of a run in progress");
    {
        SimulationSystem sim(99u);
        sim.model().variable("Served", 0.0)
                   .resource("Clerk", 2)
                   .arrivals("EXPO(1.0)")
                   .stationUsing("Desk", "Clerk", FIFO, "EXPO(1.4)")
                   .entryAt("Desk");
        sim.stopAt(100.0).initialise();
        for (int i = 0; i < 400 && sim.canStep(); ++i) sim.stepOnce();

        const RunSnapshot s = snapshotOf(sim);
        check(s.now > 0.0, "the snapshot carries the clock");
        check(s.arrived > 0, "and what has arrived");
        check(s.blocks.size() == 1, "one block");
        check(s.blocks[0].name == "Desk", "named");
        check(s.resources.size() == 1 && s.resources[0].name == "Clerk",
              "and the shared resource, which Model could not enumerate before");
        check(s.resources[0].capacity == 2.0, "with its capacity");
        check(s.variables.size() == 1 && s.variables[0].name == "Served",
              "and every declared variable");

        // Taking a snapshot must not perturb the run: it reads, it does not
        // draw, and a caller that watches must get the same numbers as one
        // that does not.
        const double before = s.now;
        for (int i = 0; i < 5; ++i) (void)snapshotOf(sim);
        check(snapshotOf(sim).now == before, "and taking one does not advance anything");

        // A snapshot and the report are two views of one run. If they can
        // disagree, one of them is inventing a number, and a watcher would see
        // figures that never appear in the result.
        const RunResults r = sim.results();
        checkClose(s.blocks[0].utilisation, r.station("Desk").utilisation, 1e-12,
                   "a snapshot agrees with the report at the same instant");
        check(s.exited == r.exited, "on the count too");
    }
```

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build
```

Expected: `error: 'RunSnapshot' was not declared in this scope`.

- [ ] **Step 3: Give `Model` a resource enumeration**

`Model` can find a resource by name but cannot list them, and a snapshot has to list them. In `include/Model.hpp`, beside `resourceNamed`, add:

```cpp
    // v12: a snapshot lists every resource, which name lookup cannot do.
    std::size_t     resourceCount() const { return m_resources.size(); }
    const Resource& resourceAt(std::size_t i) const { return *m_resources[i]; }
```

- [ ] **Step 4: Write the header**

Create `include/RunController.hpp`:

```cpp
// ============================================================================
// RunController.hpp  --  v12: a run a caller drives
// ============================================================================
// SimulationSystem::run() blocks until the stopping rule is met, which is the
// right shape for a program and the wrong one for a front end: there is no
// moment at which it can redraw, and no way to stop.
//
// v12 turns that inside out. stepOnce() does one event; this owns the loop
// around it, plus the replication loop above that. A caller advances a budget
// of events, redraws, and advances again -- so pausing is not calling, and
// cancelling is not calling ever again.
//
// NOTHING HERE IS ON THE INNER LOOP. A snapshot is built when it is asked for,
// so a run is bit-for-bit identical whether or not anyone was watching.

#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common.hpp"
#include "Experiment.hpp"
#include "RunSetup.hpp"

namespace des {

class ModelDocument;
struct Diagnostic;
class SimulationSystem;

enum class RunState { Ready, Running, Paused, Finished, Cancelled, Failed };

std::string describe(RunState state);

struct RunProgress {
    RunState  state{RunState::Ready};
    int       replication{0};        // 1-based; 0 before the first advance()
    int       replications{1};
    SimTime   now{0.0};
    long long eventsProcessed{0};    // across the whole study

    // Through the CURRENT RUN, or nothing when no rule can tell. With
    // antithetic pairing a replication is two runs, and the second half
    // restarts this at zero -- so a front end showing a per-replication bar
    // must say which half it is on, or the bar appears to go backwards.
    std::optional<double> fraction;
};

struct BlockSnapshot {
    std::string name;
    double      queueLength{0.0};
    long long   served{0};
    double      utilisation{0.0};
};

struct ResourceSnapshot {
    std::string name;
    double      busy{0.0};
    double      capacity{0.0};
};

struct VariableSnapshot {
    std::string name;
    double      value{0.0};
};

struct RunSnapshot {
    SimTime   now{0.0};
    SimTime   measuredTime{0.0};
    long long arrived{0};
    long long exited{0};
    double    numberInSystem{0.0};
    std::vector<BlockSnapshot>    blocks;
    std::vector<ResourceSnapshot> resources;
    std::vector<VariableSnapshot> variables;
};

// Reads a running system. Const, and it draws nothing: watching a run must not
// change it.
RunSnapshot snapshotOf(const SimulationSystem& sim);

}  // namespace des
```

- [ ] **Step 4b: Write `include/RunSetup.hpp` — the struct only**

`RunController.hpp` includes it and Task 5 reads every one of its fields, so
the struct lands here. Only `readRunSetup()` and the `[Run]` schema wait for
Task 7 — a struct is not a schema.

```cpp
// ============================================================================
// RunSetup.hpp  --  v12: how to run a model, as a struct
// ============================================================================
// v11 left a model file carrying no run length, so `des run` had to take one on
// the command line and a regression harness would have had to store one beside
// each model. An expectation whose meaning depends on a number kept somewhere
// else stops being an expectation the moment that number changes.
//
// Task 7 adds the [Run] module that fills this in from a document. The struct
// comes first because RunController is written against it.

#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Common.hpp"

namespace des {

class ModelDocument;
struct Diagnostic;

struct RunSetup {
    std::optional<SimTime> length;          // nothing = no time limit
    SimTime                warmUp{0.0};
    int                    replications{1};
    unsigned               baseSeed{12345u};
    bool                   stopWhenDrained{false};
    std::optional<int>     maxEntities;
    bool                   separateStreams{false};
    bool                   antithetic{false};

    // NOT a column on [Run]. Welch's warm-up grid is an experiment-level
    // instrument rather than a property of the model, and Experiment sets it
    // directly. Exposing it in the schema would put a diagnostic tool in the
    // model file next to the model.
    SimTime observeInterval{0.0};
};

}  // namespace des
```

- [ ] **Step 5: Write the source**

Create `src/RunController.cpp`:

```cpp
#include "RunController.hpp"

#include "Model.hpp"
#include "Resource.hpp"
#include "SimulationSystem.hpp"
#include "Station.hpp"
#include "VariableStore.hpp"

namespace des {

std::string describe(RunState state) {
    switch (state) {
        case RunState::Ready:     return "Ready";
        case RunState::Running:   return "Running";
        case RunState::Paused:    return "Paused";
        case RunState::Finished:  return "Finished";
        case RunState::Cancelled: return "Cancelled";
        case RunState::Failed:    return "Failed";
    }
    return "Unknown";
}

RunSnapshot snapshotOf(const SimulationSystem& sim) {
    const Model& model = sim.model();
    const SimTime measured = sim.measuredTime();

    RunSnapshot s;
    s.now            = sim.now();
    s.measuredTime   = measured;
    s.arrived        = sim.statistics().numberArrived();
    s.exited         = sim.statistics().numberServed();
    s.numberInSystem = sim.numberInSystem();

    for (std::size_t i = 0; i < model.stationCount(); ++i) {
        const Station& st = model.stationAt(i);
        BlockSnapshot b;
        b.name        = st.name();
        b.queueLength = static_cast<double>(st.queue().length());
        b.served      = st.stats().numberServed();
        b.utilisation = st.stats().utilisation(measured, st.resource().capacity());
        s.blocks.push_back(std::move(b));
    }

    for (std::size_t i = 0; i < model.resourceCount(); ++i) {
        const Resource& r = model.resourceAt(i);
        s.resources.push_back(ResourceSnapshot{r.name(),
                                               static_cast<double>(r.unitsBusy()),
                                               static_cast<double>(r.capacity())});
    }

    for (const std::string& name : model.variables().names())
        s.variables.push_back(VariableSnapshot{name, model.variables().get(name)});

    return s;
}

}  // namespace des
```

> If any accessor named here does not exist with that exact spelling, **stop and read the header** rather than adding one. Every one of them was checked against the code when this plan was written; a mismatch means the code moved, and inventing an accessor is how a snapshot ends up reporting a number nothing else in the engine agrees with.

- [ ] **Step 6: Register the files**

In `CMakeLists.txt`, add `src/RunController.cpp` to `add_library(des_engine ...)` after `src/Compiler.cpp`. In `include/des.hpp`, add after `#include "Experiment.hpp"`:

```cpp
#include "RunSetup.hpp"
#include "RunController.hpp"
```

- [ ] **Step 7: Run the tests**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add include/RunController.hpp src/RunController.cpp include/RunSetup.hpp include/Model.hpp include/des.hpp CMakeLists.txt tests/runtime_tests.cpp
git commit -m "feat: RunProgress and RunSnapshot -- what a watcher sees

Built on demand from accessors that already existed, so nothing is added to
the inner loop and a run is bit-for-bit identical whether or not anyone was
watching. That is asserted, not assumed.

Model gained resourceCount/resourceAt: it could find a resource by name and
not list them, and a snapshot has to list them."
```

---

### Task 5: The controller, for one replication

**Files:**
- Modify: `include/RunController.hpp`, `src/RunController.cpp`
- Test: `tests/runtime_tests.cpp`

**Interfaces:**
- Consumes: `RunState`, `RunProgress`, `RunSnapshot`, `SimulationSystem::stepOnce`, `RunSetup` (default-constructed only in this task).
- Produces: `class RunController` with `advance`, `pause`, `resume`, `cancel`, `state`, `progress`, `snapshot`, `runToCompletion`, `failure`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("The controller drives one replication");
    {
        auto build = [](SimulationSystem& sim) {
            sim.model().arrivals("EXPO(1.0)")
                       .station("Serve", 1, FIFO, "EXPO(0.8)")
                       .entryAt("Serve");
            sim.stopAt(200.0);
        };

        RunSetup setup;                       // one replication, seed 12345
        RunController c(setup, build);
        check(c.state() == RunState::Ready, "a controller starts Ready");
        check(c.progress().replication == 0, "and on no replication yet");

        const std::size_t first = c.advance(10);
        check(first == 10, "advance(10) does ten events");
        check(c.state() == RunState::Running, "and it is now Running");
        check(c.progress().replication == 1, "on replication 1");
        check(c.snapshot().now > 0.0, "with a clock that has moved");

        c.pause();
        check(c.state() == RunState::Paused, "pause() pauses");
        check(c.advance(1000) == 0, "and a paused controller does NO events");
        const SimTime held = c.progress().now;
        check(c.advance(1000) == 0, "however many times it is asked");
        check(c.progress().now == held, "with the clock held exactly where it was");

        c.resume();
        check(c.state() == RunState::Running, "resume() resumes");
        c.runToCompletion();
        check(c.state() == RunState::Finished, "and it finishes");
        check(c.results().size() == 1, "with one replication result");
        check(c.results()[0].served > 0, "that served somebody");

        check(c.advance(10) == 0, "advance() after the end is 0, not an error");
    }

    section("Cancel is terminal, and a failure keeps its reason");
    {
        auto build = [](SimulationSystem& sim) {
            sim.model().arrivals("EXPO(1.0)")
                       .station("Serve", 1, FIFO, "EXPO(0.8)")
                       .entryAt("Serve");
            sim.stopAt(1000.0);
        };
        RunController c(RunSetup{}, build);
        c.advance(50);
        c.cancel();
        check(c.state() == RunState::Cancelled, "cancel() cancels");
        check(c.advance(50) == 0, "and nothing runs afterwards");
        c.resume();
        check(c.state() == RunState::Cancelled, "resume() cannot undo it");
        check(c.snapshot().now > 0.0, "the run so far is still readable");

        RunController bad(RunSetup{}, [](SimulationSystem& sim) {
            sim.model().arrivals("EXPO(1.0)").entryAt("NoSuchBlock");
            sim.stopAt(10.0);
        });
        bad.runToCompletion();
        check(bad.state() == RunState::Failed, "a model that will not build Fails");
        check(!bad.failure().empty(), "and says why");
    }
```

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build
```

Expected: `error: 'RunController' was not declared in this scope`.

- [ ] **Step 3: Declare the class**

Append to `include/RunController.hpp`, before the closing `}  // namespace des`:

```cpp
class RunController {
public:
    // Declared HERE and not taken from Experiment: Experiment is refactored
    // onto this class, so naming Experiment::Builder in this header would make
    // the two include each other.
    using ModelBuilder = std::function<void(SimulationSystem&)>;

    RunController(RunSetup setup, ModelBuilder build);
    ~RunController();

    RunController(const RunController&) = delete;
    RunController& operator=(const RunController&) = delete;

    RunState           state()   const { return m_state; }
    const std::string& failure() const { return m_failure; }

    // Do at most maxEvents events, rolling on to the next replication when one
    // ends. Returns how many it actually did, which is 0 when paused, finished,
    // cancelled or failed -- a front end that polls past the end is normal, not
    // an error.
    std::size_t advance(std::size_t maxEvents);

    void pause();
    void resume();
    void cancel();

    RunProgress progress() const;
    RunSnapshot snapshot() const;

    const std::vector<ReplicationResult>&          results() const { return m_results; }
    const std::vector<std::vector<double>>&        series()  const { return m_series; }

    // Drives to the end. Loops on STATE rather than on advance()'s return
    // value: a replication that produces no events at all would return 0 and
    // stop a caller that trusted the count, with the study unfinished.
    void runToCompletion();

private:
    bool startRun();     // false when there is nothing left to start
    void finishRun();

    RunSetup     m_setup;
    ModelBuilder m_build;
    RunState     m_state{RunState::Ready};
    std::string  m_failure;

    int       m_replication{0};     // 0-based index of the one in progress
    bool      m_mirrorHalf{false};  // antithetic: the mirrored half of a pair
    long long m_events{0};

    std::unique_ptr<SimulationSystem> m_sim;
    ReplicationResult                 m_firstHalf;   // antithetic pairing
    std::vector<ReplicationResult>    m_results;
    std::vector<std::vector<double>>  m_series;
};
```

- [ ] **Step 4: Implement it**

Add to `src/RunController.cpp`. Include `<stdexcept>` and `"ModelError.hpp"`, `"Build.hpp"`, `"TerminationRule.hpp"`:

```cpp
namespace {

// The stopping rule a RunSetup describes, or nothing when it describes none.
//
// An empty AnyOf is deliberate rather than a null: initialise() asserts that a
// rule exists, and an AnyOf with no children is never met -- so the run ends
// when the future event list empties, which is exactly right for a model whose
// Create has a Max Arrivals and no horizon.
std::unique_ptr<ITerminationRule> ruleFrom(const RunSetup& s) {
    auto any = std::make_unique<AnyOf>();
    if (s.length)          any->add(timeLimit(*s.length));
    if (s.maxEntities)     any->add(entityLimit(*s.maxEntities));
    if (s.stopWhenDrained) any->add(whenDrained());
    if (any->empty()) return nullptr;      // leave whatever the builder set
    return any;
}

}  // namespace

RunController::RunController(RunSetup setup, ModelBuilder build)
    : m_setup(std::move(setup)), m_build(std::move(build)) {}

RunController::~RunController() = default;

bool RunController::startRun() {
    if (m_replication >= m_setup.replications) {
        m_state = RunState::Finished;
        return false;
    }
    // A DIFFERENT SEED PER REPLICATION, DERIVED FROM ONE BASE SEED. Different,
    // or every run is the same run and the sample has no variance at all.
    // Derived, so the whole study is reproducible from a single number.
    const unsigned seed = m_setup.baseSeed + static_cast<unsigned>(m_replication);
    m_sim = std::make_unique<SimulationSystem>(seed);
    try {
        m_build(*m_sim);
        if (m_setup.separateStreams) m_sim->useSeparateStreams();
        if (m_mirrorHalf)            m_sim->useAntithetic();
        if (m_setup.warmUp > 0.0)          m_sim->setWarmUp(m_setup.warmUp);
        if (m_setup.observeInterval > 0.0) m_sim->setObservationInterval(m_setup.observeInterval);
        // Only when the setup actually names one. Experiment's callers set the
        // rule inside the builder, and overriding it here would silently
        // replace what they asked for.
        if (auto rule = ruleFrom(m_setup)) m_sim->setTermination(std::move(rule));
        m_sim->initialise();
    } catch (const ModelError& bad) {
        m_failure = bad.what();
        m_state   = RunState::Failed;
        m_sim.reset();
        return false;
    }
    return true;
}

void RunController::finishRun() {
    if (!m_sim) return;
    const Station& entry   = m_sim->model().stationAt(0);
    const SimTime  measured = m_sim->measuredTime();

    ReplicationResult res;
    res.seed                = m_setup.baseSeed + static_cast<unsigned>(m_replication);
    res.served              = m_sim->statistics().numberServed();
    res.averageWait         = m_sim->statistics().averageWaitingTime();
    res.averageTimeInSystem = m_sim->statistics().averageTimeInSystem();
    res.Lq                  = m_sim->statistics().timeAverageA(measured);
    res.L                   = m_sim->statistics().timeAverageB(measured);
    res.utilisation         = entry.stats().utilisation(measured, entry.resource().capacity());
    res.measuredTime        = measured;
    if (m_setup.observeInterval > 0.0) m_series.push_back(m_sim->observations());
    m_sim.reset();

    if (m_setup.antithetic && !m_mirrorHalf) {
        // The first half of a pair. Hold it and run the mirror.
        m_firstHalf  = res;
        m_mirrorHalf = true;
        return;
    }
    if (m_setup.antithetic) {
        // Average the mirrored run INTO this replication rather than adding it
        // as a second one. The pair is ONE observation: its halves are
        // negatively correlated, so treating them as independent would
        // understate the interval, which is the one direction that matters.
        res.averageWait         = 0.5 * (m_firstHalf.averageWait + res.averageWait);
        res.averageTimeInSystem = 0.5 * (m_firstHalf.averageTimeInSystem + res.averageTimeInSystem);
        res.Lq                  = 0.5 * (m_firstHalf.Lq + res.Lq);
        res.L                   = 0.5 * (m_firstHalf.L  + res.L);
        res.utilisation         = 0.5 * (m_firstHalf.utilisation + res.utilisation);
        res.served              = (m_firstHalf.served + res.served) / 2;
        res.seed                = m_firstHalf.seed;
        m_mirrorHalf = false;
    }
    m_results.push_back(res);
    ++m_replication;
}

std::size_t RunController::advance(std::size_t maxEvents) {
    if (m_state == RunState::Paused || m_state == RunState::Finished ||
        m_state == RunState::Cancelled || m_state == RunState::Failed) return 0;

    if (m_state == RunState::Ready) {
        m_state = RunState::Running;
        if (!startRun()) return 0;
    }

    std::size_t done = 0;
    while (done < maxEvents) {
        if (m_sim && m_sim->stepOnce()) { ++done; ++m_events; continue; }
        finishRun();
        if (!startRun()) break;
    }
    return done;
}

void RunController::pause() {
    if (m_state == RunState::Running) m_state = RunState::Paused;
}

void RunController::resume() {
    if (m_state == RunState::Paused) m_state = RunState::Running;
}

void RunController::cancel() {
    if (m_state != RunState::Finished) m_state = RunState::Cancelled;
}

void RunController::runToCompletion() {
    while (m_state == RunState::Ready || m_state == RunState::Running)
        advance(4096);
}

RunProgress RunController::progress() const {
    RunProgress p;
    p.state           = m_state;
    p.replications    = m_setup.replications;
    p.eventsProcessed = m_events;
    // 1-based, and it counts REPLICATIONS rather than runs: with antithetic
    // pairing one replication is two runs, and `replications(n)` means n pairs
    // to the caller who asked for it.
    p.replication = (m_state == RunState::Ready) ? 0 : m_replication + 1;
    if (m_sim) {
        p.now = m_sim->now();
        if (const ITerminationRule* rule = m_sim->termination())
            p.fraction = rule->progress(*m_sim);
    }
    return p;
}

RunSnapshot RunController::snapshot() const {
    return m_sim ? snapshotOf(*m_sim) : RunSnapshot{};
}
```

- [ ] **Step 5: Expose the rule**

`progress()` needs to ask the running system's rule. In `include/SimulationSystem.hpp`, in the read-only views block, add:

```cpp
    const ITerminationRule* termination() const { return m_termination.get(); }
```

- [ ] **Step 6: Run the tests**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS. If `advance(10)` returns fewer than 10, the model ran out of events sooner than expected — check the stopping rule in the test's builder rather than weakening the assertion.

- [ ] **Step 7: Commit**

```bash
git add include/RunController.hpp src/RunController.cpp include/SimulationSystem.hpp tests/runtime_tests.cpp
git commit -m "feat: RunController -- advance, pause, cancel

Pausing is not calling; cancelling is not calling ever again. advance() past
the end returns 0 rather than throwing, because a front end that polls after
a run finishes is normal rather than wrong.

runToCompletion() loops on STATE, not on advance()'s return value: a
replication that produced no events would return 0 and strand a caller that
trusted the count with the study unfinished.

ruleFrom() returns null when the setup names no stopping condition, so
whatever the builder set survives. An empty AnyOf would have satisfied
initialise()'s assert and silently replaced the caller's rule."
```

---

### Task 6: The replication loop, and `Experiment` on top of it

**Files:**
- Modify: `include/Experiment.hpp`, `src/Experiment.cpp`
- Test: `tests/runtime_tests.cpp`

**Interfaces:**
- Consumes: `RunController`.
- Produces: `Experiment::Builder` as an alias of `RunController::ModelBuilder`; `Experiment::run()` unchanged in behaviour.

- [ ] **Step 1: Write the failing test**

```cpp
    section("The controller drives a whole study");
    {
        auto build = [](SimulationSystem& sim) {
            sim.model().arrivals("EXPO(1.0)")
                       .station("Serve", 1, FIFO, "EXPO(0.8)")
                       .entryAt("Serve");
            sim.stopAt(100.0);
        };
        RunSetup setup;
        setup.replications = 4;
        setup.baseSeed     = 5000u;

        RunController c(setup, build);
        c.advance(20);
        check(c.progress().replications == 4, "it knows how many replications");
        c.runToCompletion();
        check(c.state() == RunState::Finished, "and runs them all");
        check(c.results().size() == 4, "producing four results");
        check(c.results()[0].seed != c.results()[1].seed,
              "each with its own seed, or the sample has no variance");

        // The decisive claim of this task: a study driven in pieces gives the
        // SAME numbers as Experiment's own loop, which is the loop this
        // replaces.
        Experiment e("same", build);
        e.replications(4).baseSeed(5000u);
        e.run();
        check(e.results().size() == 4, "Experiment ran four too");
        for (std::size_t i = 0; i < 4; ++i) {
            checkClose(c.results()[i].averageWait, e.results()[i].averageWait, 1e-12,
                       "replication matches Experiment's, exactly");
            checkClose(c.results()[i].Lq, e.results()[i].Lq, 1e-12,
                       "Lq matches Experiment's, exactly");
        }
    }
```

- [ ] **Step 2: Run it**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS, because Task 5 already built the replication loop. If the numbers differ, **that is the finding** — the controller and `Experiment` disagree, and the next step is about to make one of them the other.

- [ ] **Step 3: Refactor `Experiment` onto the controller**

In `include/Experiment.hpp`, add `#include "RunController.hpp"` and replace the `Builder` alias:

```cpp
    // v12: the same type the controller takes. Experiment IS a RunController
    // with a report on top, and two spellings of one function type would be
    // two things to keep in step.
    using Builder = RunController::ModelBuilder;
```

Delete the now-duplicated `#include <functional>` only if nothing else in the header uses it.

In `src/Experiment.cpp`, replace the whole body of `Experiment::run()` with:

```cpp
void Experiment::run() {
    // One implementation of the replication loop, two callers -- exactly the
    // move v11 made with validate() and checkStructure(). The alternative was
    // two loops that must agree about seeding, antithetic pairing and warm-up,
    // and the one that drifts is always the one nobody runs.
    RunSetup setup;
    setup.replications    = m_replications;
    setup.baseSeed        = m_baseSeed;
    setup.warmUp          = m_warmUp;
    setup.observeInterval = m_observeInterval;
    setup.separateStreams = m_separateStreams;
    setup.antithetic      = m_antithetic;
    // Deliberately NO stopping rule: an Experiment's caller sets it inside the
    // builder, and RunController leaves the builder's rule alone when the setup
    // names none.

    RunController controller(setup, m_build);
    controller.runToCompletion();
    m_results = controller.results();
    m_series  = controller.series();
}
```

Remove the now-unused includes from `src/Experiment.cpp` if the compiler warns.

- [ ] **Step 4: Run the whole suite**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS, and **every v4 replication and confidence-interval check passes unchanged**. If one fails, stop: `Experiment` is v4's evidence that a single run is one sample from a random variable, and a refactor that changes its numbers has changed the answer, not the code.

- [ ] **Step 5: Byte-identical gate**

```bash
bash tools/baseline.sh check
```

Expected: `BASELINE CLEAN`. Several examples run experiments, so this covers the refactor directly.

- [ ] **Step 6: Commit**

```bash
git add include/Experiment.hpp src/Experiment.cpp tests/runtime_tests.cpp
git commit -m "refactor: Experiment::run() becomes a wrapper over RunController

One implementation of the replication loop, two callers -- the move v11 made
with validate() and checkStructure(). Two loops would have had to agree about
seeding, antithetic pairing, warm-up and the observation grid, and the one
that drifts is always the one nobody runs.

Experiment passes no stopping rule: its callers set one inside the builder,
and the controller leaves that alone when the setup names none.

Every v4 replication and confidence-interval check passes unchanged, and a
study driven in 4096-event pieces gives numbers identical to Experiment's own
loop to 1e-12."
```

---

### Task 7: `RunSetup` and the `[Run]` module

**Files:**
- Create/replace: `include/RunSetup.hpp`, `src/RunSetup.cpp`
- Modify: `src/ModuleSchemas.cpp`, `include/ModelDocument.hpp`, `src/ModelDocument.cpp`, `src/Compiler.cpp`, `CMakeLists.txt`
- Test: `tests/runtime_tests.cpp`

**Interfaces:**
- Consumes: `ModelDocument`, `ModuleRegistry`, `Diagnostic`.
- Produces: `struct RunSetup`, `RunSetup readRunSetup(const ModelDocument&, std::vector<Diagnostic>&)`, and `std::string ModelDocument::cellOrDefault(type, row, column) const`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("A model file carries its own run length");
    {
        ModelDocument d;
        d.addRow("Run");
        d.setCell("Run", 0, "Name", "Setup");
        d.setCell("Run", 0, "Length", "480");
        d.setCell("Run", 0, "Warm-up", "50");
        d.setCell("Run", 0, "Replications", "5");
        d.setCell("Run", 0, "Base Seed", "777");
        d.setCell("Run", 0, "Stop When Drained", "true");

        std::vector<Diagnostic> problems;
        const RunSetup s = readRunSetup(d, problems);
        check(!hasErrors(problems), "a well-formed [Run] row reads cleanly");
        check(s.length.has_value() && *s.length == 480.0, "Length");
        checkClose(s.warmUp, 50.0, 1e-12, "Warm-up");
        check(s.replications == 5, "Replications");
        check(s.baseSeed == 777u, "Base Seed");
        check(s.stopWhenDrained, "Stop When Drained");
        check(!s.maxEntities.has_value(), "an unset Max Entities stays UNSET, not zero");

        {
            ModelDocument m;
            std::vector<Diagnostic> out;
            const RunSetup def = readRunSetup(m, out);
            check(!hasErrors(out), "a document with no [Run] is not an error");
            check(!def.length.has_value() && def.replications == 1,
                  "it just means the defaults");
        }
        {
            ModelDocument m;
            m.addRow("Run"); m.setCell("Run", 0, "Name", "A");
            m.setCell("Run", 0, "Length", "10");
            m.addRow("Run"); m.setCell("Run", 1, "Name", "B");
            std::vector<Diagnostic> out;
            (void)readRunSetup(m, out);
            bool second = false;
            for (const Diagnostic& g : out)
                if (g.cell && g.cell->moduleType == "Run" && g.cell->row == 1) second = true;
            check(second, "a SECOND [Run] row is reported at that row");
        }
        {
            // Legitimate: a Create with Max Arrivals is finite and the future
            // event list empties on its own. A warning, not a refusal.
            ModelDocument m;
            m.addRow("Run"); m.setCell("Run", 0, "Name", "A");
            std::vector<Diagnostic> out;
            (void)readRunSetup(m, out);
            check(!hasErrors(out), "a [Run] with no stopping condition is NOT an error");
            bool warned = false;
            for (const Diagnostic& g : out)
                if (g.severity == Severity::Warning) warned = true;
            check(warned, "but it warns that the run ends only when events run out");
        }
        {
            ModelDocument m;
            m.addRow("Run"); m.setCell("Run", 0, "Name", "A");
            m.setCell("Run", 0, "Length", "soon");
            std::vector<Diagnostic> out;
            (void)readRunSetup(m, out);
            bool badCell = false;
            for (const Diagnostic& g : out)
                if (g.cell && g.cell->column == "Length") badCell = true;
            check(badCell, "a non-numeric Length is reported AT the cell");
        }
    }
```

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build
```

Expected: `error: 'readRunSetup' was not declared in this scope`.

- [ ] **Step 3: Add the schema**

In `src/ModuleSchemas.cpp`, inside `buildSchemas()` in the `--- data modules ---` group, after the `Expression` schema, add:

```cpp
    // Arena's Run Setup. At most one row, checked by readRunSetup rather than
    // by the schema, because "how many rows may this table have" is not a
    // property of a column.
    s.push_back(make("Run", ModuleKind::Data,
                     {ident("Name"), real("Length", ""), real("Warm-up", "0"),
                      integer("Replications", "1"), integer("Base Seed", "12345"),
                      boolean("Stop When Drained", "false"),
                      integer("Max Entities", ""),
                      boolean("Separate Streams", "false"),
                      boolean("Antithetic", "false")}));
```

- [ ] **Step 4: Give `ModelDocument` a default-aware read**

v11's build pass has a private `valueOf(doc, schema, type, row, column)` that falls back to the schema default. `readRunSetup` needs the same thing, and two copies would drift. Promote it.

In `include/ModelDocument.hpp`, beside `cell()`, declare:

```cpp
    // The cell, or the schema's default when it is empty. v11's build pass had
    // this privately and v12 needs it too; two copies of "what does an empty
    // cell mean" is one too many.
    std::string cellOrDefault(const std::string& type, std::size_t row,
                              const std::string& column) const;
```

In `src/ModelDocument.cpp`, add `#include "ModuleSchema.hpp"` and:

```cpp
std::string ModelDocument::cellOrDefault(const std::string& type, std::size_t row,
                                         const std::string& column) const {
    const std::string v = cell(type, row, column);
    if (!v.empty()) return v;
    const ModuleSchema* schema = ModuleRegistry::instance().find(type);
    if (schema == nullptr) return std::string();
    const Column* c = schema->column(column);
    return c == nullptr ? std::string() : c->defaultValue;
}
```

In `src/Compiler.cpp`, delete the local `valueOf` helper and replace every call `valueOf(doc, *schema, type, r, X)` with `doc.cellOrDefault(type, r, X)`, and `valueOf(doc, s, "Variable", r, X)` with `doc.cellOrDefault("Variable", r, X)` — same for `Resource` and `AssignField`. The `const ModuleSchema& s = *reg.find(...)` locals that existed only to feed `valueOf` become unused; delete them, or `-Wunused-variable` will fail the gate.

- [ ] **Step 5: Declare the reader**

The `RunSetup` struct already exists from Task 4. Append its reader to
`include/RunSetup.hpp`, inside `namespace des`, after the struct:

```cpp
// Reads the [Run] row -- Arena's Run Setup: one row, in the document, where
// the model is. A document with no [Run] is not an error: it means the
// defaults. Never throws.
RunSetup readRunSetup(const ModelDocument& doc, std::vector<Diagnostic>& out);
```

- [ ] **Step 6: Write `RunSetup.cpp`**

```cpp
#include "RunSetup.hpp"

#include <cstdlib>
#include "Diagnostic.hpp"
#include "ModelDocument.hpp"

namespace des {
namespace {

void complain(std::vector<Diagnostic>& out, Severity severity, std::size_t row,
              const std::string& column, const std::string& message) {
    out.push_back(Diagnostic{severity, SourceSpan{}, message,
                             CellRef{"Run", row, column}});
}

// True and the parsed value, or false when the text is not a number. strtod's
// end pointer is the whole check: "480x" must not read as 480.
bool asReal(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

}  // namespace

RunSetup readRunSetup(const ModelDocument& doc, std::vector<Diagnostic>& out) {
    RunSetup setup;
    const std::size_t rows = doc.rowCount("Run");
    if (rows == 0) return setup;

    for (std::size_t r = 1; r < rows; ++r)
        complain(out, Severity::Error, r, "Name",
                 "a model has at most one [Run]; this one is extra");

    const auto text = [&](const char* column) {
        return doc.cellOrDefault("Run", 0, column);
    };
    const auto number = [&](const char* column, double& into) {
        const std::string s = text(column);
        if (s.empty()) return false;
        if (!asReal(s, into)) {
            complain(out, Severity::Error, 0, column, "'" + s + "' is not a number");
            return false;
        }
        return true;
    };
    const auto flag = [&](const char* column) {
        const std::string s = text(column);
        if (s == "true")  return true;
        if (s == "false" || s.empty()) return false;
        complain(out, Severity::Error, 0, column, "'" + s + "' must be true or false");
        return false;
    };

    double v = 0.0;
    if (number("Length", v))       setup.length = v;
    if (number("Warm-up", v))      setup.warmUp = v;
    if (number("Replications", v)) setup.replications = static_cast<int>(v);
    if (number("Base Seed", v))    setup.baseSeed = static_cast<unsigned>(v);
    if (number("Max Entities", v)) setup.maxEntities = static_cast<int>(v);
    setup.stopWhenDrained = flag("Stop When Drained");
    setup.separateStreams = flag("Separate Streams");
    setup.antithetic      = flag("Antithetic");

    if (setup.replications < 1)
        complain(out, Severity::Error, 0, "Replications", "must be at least 1");

    // A WARNING, not a refusal. It looks like a model that runs forever, and
    // for a Create with Max Arrivals set it is not: the future event list
    // empties and the run ends on its own. Refusing it would reject a correct
    // model; saying nothing would leave an author waiting.
    if (!setup.length && !setup.maxEntities && !setup.stopWhenDrained)
        complain(out, Severity::Warning, 0, "Length",
                 "no stopping condition; this run ends only when the model "
                 "runs out of events");

    return setup;
}

}  // namespace des
```

- [ ] **Step 7: Register and run**

Add `src/RunSetup.cpp` to `add_library(des_engine ...)` in `CMakeLists.txt`, before `src/RunController.cpp`.

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS, and all 779 v11 checks still pass — Step 4 touched `src/Compiler.cpp`, which is v11's build pass.

- [ ] **Step 8: Commit**

```bash
git add include/RunSetup.hpp src/RunSetup.cpp src/ModuleSchemas.cpp include/ModelDocument.hpp src/ModelDocument.cpp src/Compiler.cpp CMakeLists.txt tests/runtime_tests.cpp
git commit -m "feat: a [Run] module, so a model file carries its own run length

Arena's Run Setup, closing the open item v11 left. An expectation whose
meaning depends on a horizon stored outside the model stops being an
expectation the moment that horizon changes -- which is why the regression
harness needed this before it could exist.

A [Run] naming no stopping condition WARNS rather than erroring. It looks
like a model that runs forever, and for a Create with Max Arrivals it is not:
the event list empties and the run ends. Refusing it would reject a correct
model.

v11's private valueOf() became ModelDocument::cellOrDefault(), because two
copies of 'what does an empty cell mean' is one too many."
```

---

### Task 8: `fromDocument`, and `des run` over the controller

**Files:**
- Modify: `include/RunController.hpp`, `src/RunController.cpp`, `cli/main.cpp`, `examples/models/*.des`
- Test: `tests/runtime_tests.cpp`

**Interfaces:**
- Consumes: `readRunSetup`, `compileInto`, `readDocumentFile`.
- Produces: `static std::unique_ptr<RunController> RunController::fromDocument(const ModelDocument&, std::vector<Diagnostic>&)`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("A controller built straight from a document");
    {
        ModelDocument d;
        d.addRow("Run");
        d.setCell("Run", 0, "Name", "Setup");
        d.setCell("Run", 0, "Length", "120");
        d.setCell("Run", 0, "Replications", "3");
        d.addRow("Create");
        d.setCell("Create", 0, "Name", "In");
        d.setCell("Create", 0, "Interarrival", "EXPO(1.0)");
        d.setCell("Create", 0, "Next", "Serve");
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.6)");
        d.setCell("Process", 0, "Next", "Out");
        d.addRow("Dispose");
        d.setCell("Dispose", 0, "Name", "Out");

        std::vector<Diagnostic> problems;
        std::unique_ptr<RunController> c = RunController::fromDocument(d, problems);
        check(c != nullptr, "a valid document produces a controller");
        check(!hasErrors(problems), "with no diagnostics");
        if (c) {
            c->runToCompletion();
            check(c->state() == RunState::Finished, "and it runs to the end");
            check(c->results().size() == 3,
                  "THREE replications, because the document said so");
            check(c->results()[0].seed != c->results()[2].seed,
                  "each with its own seed");
        }

        ModelDocument bad;
        bad.addRow("Process");
        bad.setCell("Process", 0, "Name", "Lonely");
        bad.setCell("Process", 0, "Service", "EXPO(1");
        std::vector<Diagnostic> out;
        check(RunController::fromDocument(bad, out) == nullptr,
              "a document that will not compile produces NO controller");
        check(hasErrors(out), "and says why, at the cell");
    }
```

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build
```

Expected: `error: 'fromDocument' is not a member of 'des::RunController'`.

- [ ] **Step 3: Declare it**

In `include/RunController.hpp`, in the public block above the constructor:

```cpp
    // Reads the [Run] module, then compiles once so every bad cell is reported
    // before anything runs. Returns null when the document did not compile.
    //
    // It KEEPS A COPY of the document, because each replication needs a fresh
    // SimulationSystem with a fresh Model built into it: a Model can be neither
    // reused across replications nor copied. The builder it installs is
    // "compile this document into that system", and it runs once per
    // replication. A document is rows of strings; copying one is cheap.
    static std::unique_ptr<RunController>
    fromDocument(const ModelDocument& doc, std::vector<Diagnostic>& out);
```

- [ ] **Step 4: Implement it**

In `src/RunController.cpp`, add `#include "Compiler.hpp"`, `#include "ModelDocument.hpp"`, `#include "RunSetup.hpp"` and:

```cpp
std::unique_ptr<RunController>
RunController::fromDocument(const ModelDocument& doc, std::vector<Diagnostic>& out) {
    const RunSetup setup = readRunSetup(doc, out);

    // Compile once, HERE, so a bad cell is reported before a single event runs
    // rather than from inside the first replication where a caller has no
    // diagnostics list to receive it.
    {
        Model probe;
        if (!compileInto(doc, probe, out)) return nullptr;
    }

    ModelDocument copy = doc;
    auto build = [copy](SimulationSystem& sim) {
        std::vector<Diagnostic> ignored;
        if (!compileInto(copy, sim.model(), ignored))
            throw ModelError("the document stopped compiling between replications");
    };
    return std::make_unique<RunController>(setup, std::move(build));
}
```

> The `throw` is unreachable in practice — the same document compiled a moment ago — and it is a `ModelError` rather than a silent return because reaching it would mean `compileInto` is not a function of its input, which is a programmer error and not a user's.

- [ ] **Step 5: Give the shipped models a `[Run]` row**

Add to the top of each of `examples/models/teller.des`, `decide.des`, `variables.des` and `shared.des`, immediately after the `version = 1` line and its blank line:

```
# How to run it. Arena keeps this in Run Setup; here it is a module like any
# other, so the file says everything needed to reproduce a result.
[Run]
Name         = Setup
Length       = 480
Replications = 1

```

For `decide.des`, `variables.des` and `shared.des` — which cap their arrivals — use `Length = 480` all the same: a capped model drains before the horizon, so the horizon is a backstop rather than the thing that stops it.

> This does not disturb v11's decisive test. That test compiles each file with `compileInto`, which does not read `[Run]`, and sets `stopAt(200.0)` on both sides itself.

- [ ] **Step 6: Reimplement `des run` over the controller**

In `cli/main.cpp`, replace everything from `ReadResult read = readDocumentFile(path);` to the end of `main` with:

```cpp
    ReadResult read = readDocumentFile(path);
    printDiagnostics(read.diagnostics, path);
    if (hasErrors(read.diagnostics)) return 1;

    std::vector<Diagnostic> problems;
    std::unique_ptr<RunController> run = RunController::fromDocument(read.document, problems);
    printDiagnostics(problems, path);
    if (run == nullptr) return 1;

    if (verb == "check") {
        std::cout << path << ": ok\n";
        return 0;
    }

    // `des run` goes through advance() rather than run(), so the interface a
    // front end will drive is the one this program uses. v11 gave the document
    // layer a consumer inside this repo for the same reason: an interface with
    // no caller is an interface nobody has checked.
    while (run->state() == RunState::Ready || run->state() == RunState::Running) {
        run->advance(4096);
        const RunProgress p = run->progress();
        if (p.replications > 1)
            std::cout << "\rreplication " << p.replication << " of " << p.replications
                      << std::flush;
    }
    if (run->progress().replications > 1) std::cout << "\n";
    if (run->state() == RunState::Failed) {
        std::cout << path << ": error: " << run->failure() << "\n";
        return 1;
    }
    run->report(std::cout);
    return 0;
```

The `until` argument now overrides `Length`. Between reading the setup and building the controller, `des run model.des 900` must win over the file, so add an optional third parameter to `fromDocument`:

```cpp
    static std::unique_ptr<RunController>
    fromDocument(const ModelDocument& doc, std::vector<Diagnostic>& out,
                 std::optional<SimTime> lengthOverride = std::nullopt);
```

and in the implementation, immediately after `readRunSetup`:

```cpp
    RunSetup setup = readRunSetup(doc, out);
    if (lengthOverride) setup.length = lengthOverride;
```

`RunController` needs a `report` that prints the study. Add to the header and source:

```cpp
    // One replication prints the run's own report; several print the
    // confidence intervals, because quoting one number from thirty runs is the
    // thing v4 exists to stop.
    void report(std::ostream& os) const;
```

```cpp
void RunController::report(std::ostream& os) const {
    if (m_results.size() == 1 && m_lastReport.tellp() > 0) {
        os << m_lastReport.str();
        return;
    }
    os << "\n=== " << m_results.size() << " replications ===\n";
    os << std::fixed << std::setprecision(4);
    const auto line = [&](const char* label, double ReplicationResult::* field) {
        std::vector<double> xs;
        for (const ReplicationResult& r : m_results) xs.push_back(r.*field);
        os << "  " << std::setw(24) << std::left << label << std::right
           << std::setw(12) << Summary::mean(xs)
           << "  +/- " << std::setw(10) << Summary::halfWidth95(xs) << "\n";
    };
    line("average wait",      &ReplicationResult::averageWait);
    line("time in system",    &ReplicationResult::averageTimeInSystem);
    line("Lq",                &ReplicationResult::Lq);
    line("L",                 &ReplicationResult::L);
    line("utilisation",       &ReplicationResult::utilisation);
}
```

For the single-replication case, capture the run's own report in `finishRun()` before the system is destroyed, adding a `std::ostringstream m_lastReport;` member and, in `finishRun()` immediately before `m_sim.reset()`:

```cpp
    // The single-replication report has to be taken HERE: it is a view of a
    // SimulationSystem, and the system does not survive the replication.
    m_lastReport.str(std::string());
    m_sim->report(m_lastReport);
```

Add `#include <iomanip>`, `#include <ostream>` and `#include <sstream>` to `src/RunController.cpp`, and `#include <sstream>` to the header for the member.

- [ ] **Step 7: Try it by hand**

```bash
cmake --build build && ./build/des.exe run examples/models/teller.des
```

Expected: no "carries no run length" note any more — the file says 480 — and a report ending in the station table.

```bash
./build/des.exe run examples/models/teller.des 100
```

Expected: `total simulated time` near 100, not 480.

- [ ] **Step 8: Gates, then commit**

```bash
./build/des_tests.exe && bash tools/baseline.sh check
```

```bash
git add include/RunController.hpp src/RunController.cpp cli/main.cpp examples/models tests/runtime_tests.cpp
git commit -m "feat: des run goes through the controller, and a document carries its run

fromDocument compiles once up front so every bad cell is reported before an
event runs, then keeps a COPY of the document: a Model can be neither reused
across replications nor copied, so each replication compiles the document
again into its own system.

des run drives advance() rather than run(). v11 gave the document layer a
consumer inside this repo for the same reason -- an interface with no caller
is an interface nobody has checked.

The four shipped models gained a [Run] row. v11's decisive test is unaffected:
it compiles with compileInto, which does not read [Run], and sets its own
horizon on both sides."
```

---

### Task 9: The regression harness

**Files:**
- Create: `include/Regression.hpp`, `src/Regression.cpp`, `tests/regression/manifest`, `tests/regression/*.expected`
- Modify: `cli/main.cpp`, `CMakeLists.txt`, `.gitignore`
- Test: `tests/runtime_tests.cpp`

**Interfaces:**
- Consumes: `RunController::fromDocument`, `readDocumentFile`.
- Produces: `struct RegressionCase`, `struct RegressionOutcome`, `struct RegressionReport`, `RegressionReport runRegression(const std::string& dir, bool capture, bool force)`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("The regression harness");
    {
        // Run one model twice and confirm the text is reproducible, which is
        // the property the harness rests on. If this is false, every stored
        // expectation is a coin toss.
        const std::string dir = "tests/regression";
        RegressionReport a = runRegression(dir, false, false);
        check(a.cases > 0, "the manifest names at least one model");
        check(a.failures == 0, "and every model matches its expectation");
        check(a.missing == 0, "with no expectation missing");

        RegressionReport b = runRegression(dir, false, false);
        check(a.failures == b.failures && a.cases == b.cases,
              "and running it twice gives the same verdict");

        // A gate that measured nothing must never report success. baseline.sh
        // shipped without this and reported BASELINE CLEAN over zero examples.
        RegressionReport empty = runRegression("tests/regression_empty", false, false);
        check(!empty.ok(), "an empty manifest is a FAILURE, not a pass");
    }
```

Create `tests/regression_empty/manifest` containing only:

```
# Deliberately empty. A harness that measured nothing must not report success,
# and this is the fixture that proves it does not.
```

- [ ] **Step 2: Write the header**

Create `include/Regression.hpp`:

```cpp
// ============================================================================
// Regression.hpp  --  v12: many models, run and diffed
// ============================================================================
// The standard is BYTE-IDENTICAL report text, not a mean inside a tolerance.
// A tolerance passes a real regression that happens to land inside the band,
// which is the "near-miss average" standard this project rejected in v10.
//
// Expectations are `.expected` rather than `.txt` on purpose: .gitignore
// excludes *.txt with one hand-maintained negation, and a silently ignored
// expectation file is a gate covering less than it appears to.

#pragma once
#include <string>
#include <vector>

namespace des {

struct RegressionOutcome {
    std::string model;        // path as the manifest gave it
    std::string detail;       // the first differing line, or why it could not run
    bool matched{false};
    bool hadExpectation{false};
};

struct RegressionReport {
    int cases{0};
    int failures{0};
    int missing{0};           // no stored expectation
    int captured{0};
    int refused{0};           // capture declined because one already exists
    std::vector<RegressionOutcome> outcomes;

    // A harness that ran nothing has not passed. It has not run.
    bool ok() const { return cases > 0 && failures == 0 && missing == 0; }
};

// dir holds `manifest` (one model path per line, # comments allowed) and one
// <stem>.expected per model. capture writes expectations instead of comparing;
// force allows overwriting one that already exists.
RegressionReport runRegression(const std::string& dir, bool capture, bool force);

}  // namespace des
```

- [ ] **Step 3: Write the source**

Create `src/Regression.cpp`:

```cpp
#include "Regression.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include "Compiler.hpp"
#include "DocumentFormat.hpp"
#include "RunController.hpp"

namespace des {
namespace {

std::string stemOf(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    const std::size_t stop = (dot == std::string::npos || dot < start) ? path.size() : dot;
    return path.substr(start, stop - start);
}

std::string readFile(const std::string& path, bool& found) {
    std::ifstream in(path, std::ios::binary);
    found = in.good();
    if (!found) return std::string();
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

std::string firstDifference(const std::string& want, const std::string& got) {
    std::istringstream a(want), b(got);
    std::string la, lb;
    for (int line = 1; ; ++line) {
        la.clear(); lb.clear();
        const bool gotA = static_cast<bool>(std::getline(a, la));
        const bool gotB = static_cast<bool>(std::getline(b, lb));
        if (!gotA && !gotB) return "identical";
        if (la != lb) {
            std::ostringstream os;
            os << "line " << line << ": expected [" << la << "] got [" << lb << "]";
            return os.str();
        }
    }
}

std::vector<std::string> readManifest(const std::string& dir, bool& found) {
    std::vector<std::string> models;
    std::ifstream in(dir + "/manifest");
    found = in.good();
    if (!found) return models;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        models.push_back(line);
    }
    return models;
}

}  // namespace

RegressionReport runRegression(const std::string& dir, bool capture, bool force) {
    RegressionReport report;
    bool haveManifest = false;
    const std::vector<std::string> models = readManifest(dir, haveManifest);
    if (!haveManifest) {
        report.outcomes.push_back(RegressionOutcome{dir + "/manifest",
                                                    "no manifest", false, false});
        return report;
    }

    for (const std::string& path : models) {
        RegressionOutcome out;
        out.model = path;
        ++report.cases;

        ReadResult read = readDocumentFile(path);
        if (hasErrors(read.diagnostics)) {
            out.detail = "does not read";
            report.outcomes.push_back(out);
            ++report.failures;
            continue;
        }
        std::vector<Diagnostic> problems;
        std::unique_ptr<RunController> run =
            RunController::fromDocument(read.document, problems);
        if (run == nullptr) {
            out.detail = "does not compile";
            report.outcomes.push_back(out);
            ++report.failures;
            continue;
        }
        // Every regression model must say how to run it. An expectation whose
        // meaning depends on a horizon stored somewhere else stops being an
        // expectation the moment that horizon changes.
        if (read.document.rowCount("Run") == 0) {
            out.detail = "no [Run] module";
            report.outcomes.push_back(out);
            ++report.failures;
            continue;
        }

        run->runToCompletion();
        std::ostringstream got;
        run->report(got);

        const std::string wantPath = dir + "/" + stemOf(path) + ".expected";
        bool exists = false;
        const std::string want = readFile(wantPath, exists);
        out.hadExpectation = exists;

        if (capture) {
            if (exists && !force) {
                out.detail = "expectation exists; --force to replace";
                ++report.refused;
                report.outcomes.push_back(out);
                continue;
            }
            std::ofstream w(wantPath, std::ios::binary);
            w << got.str();
            out.matched = true;
            out.detail  = "captured";
            ++report.captured;
            report.outcomes.push_back(out);
            continue;
        }

        if (!exists) {
            out.detail = "no expectation stored";
            ++report.missing;
            report.outcomes.push_back(out);
            continue;
        }
        out.matched = (want == got.str());
        out.detail  = out.matched ? "identical" : firstDifference(want, got.str());
        if (!out.matched) ++report.failures;
        report.outcomes.push_back(out);
    }
    return report;
}

}  // namespace des
```

- [ ] **Step 4: Wire in the CLI verb**

In `cli/main.cpp`, extend `usage()`:

```cpp
    std::cout << "usage: des check   <model.des>\n"
                 "       des run     <model.des> [until]\n"
                 "       des regress [dir] [--capture] [--force]\n";
```

and handle the verb before the model-file path is read:

```cpp
    if (verb == "regress") {
        std::string dir = "tests/regression";
        bool capture = false, force = false;
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--capture")    capture = true;
            else if (arg == "--force") force = true;
            else                       dir = arg;
        }
        const RegressionReport r = runRegression(dir, capture, force);
        for (const RegressionOutcome& o : r.outcomes)
            std::cout << (o.matched ? "ok      " : "DIFFERS ")
                      << o.model << "  " << o.detail << "\n";
        std::cout << "\n" << r.cases << " models, " << r.failures << " differ, "
                  << r.missing << " without an expectation";
        if (r.refused > 0) std::cout << ", " << r.refused << " capture(s) refused";
        std::cout << "\n";
        if (capture) {
            std::cout << r.captured << " captured\n";
            return r.refused > 0 ? 1 : 0;
        }
        // A gate that measured nothing has not passed; it has not run.
        std::cout << (r.ok() ? "REGRESSION CLEAN\n" : "REGRESSION FAILED\n");
        return r.ok() ? 0 : 1;
    }
```

Adjust the argument checks at the top of `main` so `regress` is allowed with 2 to 5 arguments while `check` and `run` keep theirs.

- [ ] **Step 5: Create the manifest and capture**

```bash
mkdir -p tests/regression tests/regression_empty
```

Write `tests/regression/manifest`:

```
# One model per line. Every one must carry a [Run] module: an expectation whose
# meaning depends on a horizon stored somewhere else stops being an expectation
# the moment that horizon changes.
examples/models/teller.des
examples/models/decide.des
examples/models/variables.des
examples/models/shared.des
```

```bash
cmake --build build && ./build/des.exe regress --capture
```

Expected: `4 captured`. Read one of the `.expected` files and confirm it is a report, not an error.

- [ ] **Step 6: Prove the harness can fail**

```bash
./build/des.exe regress
```
Expected: `REGRESSION CLEAN`.

Now break one deliberately and confirm it is caught:

```bash
sed -i 's/Service  = EXPO(0.5)/Service  = EXPO(0.55)/' examples/models/shared.des && ./build/des.exe regress; sed -i 's/Service  = EXPO(0.55)/Service  = EXPO(0.5)/' examples/models/shared.des
```

Expected: `DIFFERS examples/models/shared.des  line NN: ...` and `REGRESSION FAILED`, then clean again after the revert. **A harness that has never been seen to fail is not evidence.**

Confirm capture refuses to overwrite:

```bash
./build/des.exe regress --capture
```

Expected: `4 capture(s) refused` and exit status 1.

- [ ] **Step 7: Register everything**

Add `src/Regression.cpp` to `add_library(des_engine ...)`, `#include "Regression.hpp"` to `include/des.hpp`, and add to `.gitignore` nothing at all — `.expected` is not matched by any existing pattern, which is why that extension was chosen. Verify with:

```bash
git check-ignore -v tests/regression/teller.expected; echo "exit=$?"
```

Expected: no output and `exit=1`, meaning the file is **not** ignored.

- [ ] **Step 8: Run the gates and commit**

```bash
./build/des_tests.exe && bash tools/baseline.sh check && bash tools/verify.sh
```

```bash
git add include/Regression.hpp src/Regression.cpp cli/main.cpp include/des.hpp CMakeLists.txt tests/regression tests/regression_empty tests/runtime_tests.cpp
git commit -m "feat: des regress -- many models, byte-identical

No tolerances. A stored mean with a tolerance passes a real regression that
lands inside the band, which is the near-miss-average standard v10 rejected.

Three things it has at birth because v11 taught them the hard way: it FAILS
when it checked nothing, --capture refuses to overwrite an existing
expectation without --force, and expectations are .expected rather than .txt
because .gitignore excludes *.txt with one hand-maintained negation.

Verified by making it fail: perturbing one service time in shared.des is
caught at the differing line, and an empty manifest is a failure, not a pass."
```

---

### Task 10: The decisive test

**Files:**
- Test: `tests/runtime_tests.cpp`
- Modify: `.gitignore`

- [ ] **Step 1: Write it**

```cpp
    section("Chunk size does not change the run");
    {
        // The claim this whole version rests on, over every shipped model
        // rather than one hand-made in the test. If a caller can change the
        // answer by choosing a different budget, none of the rest is safe.
        const char* files[] = {"teller.des", "decide.des", "variables.des", "shared.des"};
        for (const char* file : files) {
            auto traceOf = [file](const std::string& out, std::size_t chunk,
                                  bool withPause) {
                {
                    SimulationSystem sim(20260905u);
                    ReadResult read = readDocumentFile(modelPath(file));
                    std::vector<Diagnostic> problems;
                    if (!compileInto(read.document, sim.model(), problems)) return std::string();
                    sim.enableTrace(out, TraceLevel::Events);
                    sim.stopAt(200.0).initialise();
                    if (chunk == 0) {
                        sim.run();
                    } else {
                        bool more = true;
                        while (more) {
                            more = false;
                            for (std::size_t i = 0; i < chunk; ++i) {
                                if (!sim.stepOnce()) break;
                                more = true;
                            }
                            // A pause is the absence of a call, so this is what
                            // one looks like from the engine's side: nothing.
                            if (withPause) (void)snapshotOf(sim);
                        }
                    }
                }
                std::ifstream in(out, std::ios::binary);
                return std::string((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
            };

            const std::string whole = traceOf("chunk_whole.md", 0, false);
            check(whole.size() > 500, std::string(file) + ": the trace is substantial");
            check(whole == traceOf("chunk_1.md", 1, false),
                  std::string(file) + ": one event at a time is identical to run()");
            check(whole == traceOf("chunk_13.md", 13, false),
                  std::string(file) + ": thirteen at a time is identical to run()");
            check(whole == traceOf("chunk_watched.md", 5, true),
                  std::string(file) + ": and watching it changes nothing");
        }
    }
```

`modelPath()` already exists in `tests/document_tests.cpp`'s anonymous namespace. Copying it into `tests/runtime_tests.cpp` would be a second definition of one fact. Instead, move it: delete it from `document_tests.cpp`, add to `tests/harness.hpp`

```cpp
// Where the shipped .des models are. Compiled in rather than assumed relative
// to the working directory, because the sanitiser leg does not run from the
// repository root.
std::string modelPath(const std::string& file);
```

and to `tests/harness.cpp`

```cpp
#ifndef DES_MODEL_DIR
#define DES_MODEL_DIR "examples/models"
#endif

std::string modelPath(const std::string& file) {
    return std::string(DES_MODEL_DIR) + "/" + file;
}
```

with `using des_test::modelPath;` added to both test files.

- [ ] **Step 2: Run it**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS, 16 new checks.

- [ ] **Step 3: Prove it can fail**

Temporarily change one chunk loop to skip a step — for instance make the `chunk_13` call `traceOf("chunk_13.md", 13, false)` pass `chunk` as 13 but add `if (i == 3) continue;` inside the inner loop — rebuild, and confirm the test FAILS naming the file. Then revert.

> A test that has only ever passed is not evidence. v10 shipped one that used `constant()` and therefore consumed nothing from the stream it claimed to be testing; v11 shipped six that looped over an empty list. Both were counted as evidence and neither could fail.

- [ ] **Step 4: Ignore the artefacts**

Add to `.gitignore`:

```
/chunk_whole.md
/chunk_1.md
/chunk_13.md
/chunk_watched.md
```

- [ ] **Step 5: Full gates, then commit**

```bash
./build/des_tests.exe && bash tools/baseline.sh check && bash tools/verify.sh
```

```bash
git add tests/runtime_tests.cpp tests/document_tests.cpp tests/harness.hpp tests/harness.cpp .gitignore
git commit -m "test: the size of the pieces does not change the run

Every shipped model, traced four ways: run(), stepped one event at a time,
stepped thirteen at a time, and stepped with a snapshot taken between every
chunk. Byte-identical all four ways, so a caller cannot change the answer by
choosing a budget, and watching a run does not perturb it.

Made to fail before being believed: dropping one step in the chunked loop is
caught, and names the model."
```

---

### Task 11: The documents

**Files:**
- Create: `V12_READLOG.md`
- Modify: `CHANGELOG.md`, `README.md`, `ARENA_MAP.md`, `examples/README.md`, `tools/manual_data.py`, `DES_Engine_Reference.pdf`

- [ ] **Step 1: Write `V12_READLOG.md`**

Follow `V11_READLOG.md`: what the version demanded, numbered sections, then the bugs and what is still open. Cover at minimum:

1. **Why stepping and not threads.** The engine's whole claim is that one seed gives one run. A stepped loop gives a front end responsiveness without putting a lock between the clock and the statistics.
2. **What the extraction touched, and how it was made safe.** The integrals-clock-handler order, `m_stopped` replacing a `return`, and why `canStep()` does not test `m_initialised`.
3. **Progress that admits it cannot tell.** The fourth occurrence, with the three earlier ones named.
4. **One replication loop, two callers.** `Experiment` refactored onto `RunController`, and why duplicating it was the worse option.
5. **Antithetic pairing is where the abstraction leaked.** One replication is two runs; what that does to a progress bar, and how it is reported.
6. **A run length in the model.** Why the harness forced the issue, and why a `[Run]` with no stopping condition warns rather than refusing.
7. **The harness, and what v11 taught it.** Fails when it measured nothing; refuses to overwrite; `.expected` rather than `.txt`.
8. Anything that went wrong while building it. **The bugs are the most valuable part of these documents.**

- [ ] **Step 2: Update `CHANGELOG.md`**

A `[12.0.0]` section at the top, in the established style: Added, Changed, Fixed, Compatibility.

- [ ] **Step 3: Update `README.md`**

- "Current state" becomes v12, and the v12 paragraph joins the v10 and v11 ones.
- A "Watching a run" section showing `RunController`: build, `advance`, `progress`, `snapshot`, `cancel`.
- The `[Run]` module in the "A model as a file" section, and `des regress` beside `des check` and `des run`.
- The check count, and `V12_READLOG.md` in the documents table.
- The "v12 — the order to do it in" tail becomes v13, with the four v9 items that remain.

- [ ] **Step 4: Update `ARENA_MAP.md`**

- `[Run]` beside the other data modules, mapped to Arena's **Run Setup**.
- Under the differences: replication length, warm-up and replications live in the model file here as they do in Arena, but **there is no Base Time Units**: this engine's clock is dimensionless and every duration is in the same unit as every other.

- [ ] **Step 5: Update `examples/README.md`**

Add `[Run]` to the model-file section, and a short "Running a model from code" showing `RunController::fromDocument` with a loop over `advance`.

- [ ] **Step 6: Update the manual**

```bash
python tools/manual.py
```

It will report the v12 types as undocumented. Add prose for `RunState`, `RunProgress`, `BlockSnapshot`, `ResourceSnapshot`, `VariableSnapshot`, `RunSnapshot`, `RunController`, `RunSetup`, `RegressionOutcome`, `RegressionReport` to `TYPE_DOC` in `tools/manual_data.py`, member prose to `MEMBER_DOC`, a **Chapter 7 "Runtime Control"** inserted after the document layer with the later chapters renumbered, and a call-flow diagram for `advance()`. Update `VERSION_LINE`.

Expected afterwards: `types documented N / N`.

- [ ] **Step 7: Final verification**

```bash
cmake --build build --clean-first
```
```bash
./build/des_tests.exe
```
```bash
bash tools/baseline.sh check
```
```bash
./build/des.exe regress
```
```bash
bash tools/verify.sh
```

Expected: no warnings, all checks pass, `BASELINE CLEAN`, `REGRESSION CLEAN`, `VERIFY CLEAN`.

- [ ] **Step 8: Commit**

```bash
git add V12_READLOG.md CHANGELOG.md README.md ARENA_MAP.md examples/README.md tools/manual_data.py DES_Engine_Reference.pdf
git commit -m "v12: the run becomes something you can drive"
```

---

## Self-review notes

**Spec coverage.** Step extraction → Task 2. `RunController` and the state machine → Tasks 5 and 6. Progress and snapshots → Tasks 3 and 4. `Experiment` refactor → Task 6. `RunSetup` and `[Run]` → Task 7. `fromDocument` and `des run` → Task 8. The harness → Task 9. The decisive test → Task 10. The `report(ostream)` overload the harness needs → Task 1, where it lands before anything depends on it. The `V11_READLOG` correction → Task 1. Docs → Task 11.

**Two places this plan is deliberately less prescriptive than v11's.** Task 11's readlog and manual prose are described by what they must cover rather than written out, because both depend on what actually goes wrong during implementation — and the bugs are the part worth writing. Everything with an interface in it is exact.

**Three things added while writing the plan that the spec did not name.**
`SimulationSystem::termination()` — `RunProgress::fraction` has to ask the running rule and there was no accessor. `RunController::report(std::ostream&)` — `des run` and the harness both need a study's report and only a single run had one. `RunSetup::observeInterval` — `Experiment` sets Welch's observation grid and the refactor would silently drop it; it is a struct field and deliberately **not** a `[Run]` column, because a diagnostic instrument does not belong in the model file.

**Check counts are not predicted.** What matters per task is zero FAIL lines and a total that grew.
