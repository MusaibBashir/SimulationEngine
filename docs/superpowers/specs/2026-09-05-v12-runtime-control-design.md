# v12 — runtime control

**Status:** design approved, not yet implemented.
**Date:** 2026-09-05
**Branch:** `v12-runtime-control`, built on `v11-document-layer`.

---

## Where this sits

v10 made a model's fields text. v11 made the model itself data. v12 makes the
**run** controllable: something a front end can start, watch, pause and cancel,
and something a harness can drive over many models and diff.

The TUI remains a **separate repository, after v12**. This version builds the
interface it will drive, and — as in v11, where the `des` CLI existed so the
document layer had a consumer inside this repo — `des run` is reimplemented over
that interface so it is load-bearing rather than decorative.

Decisions carried forward from the three-version design, not reopened:

- The TUI is a separate repo; the engine keeps versioning after it exists.
- Compatibility is source-level, not ABI. **Model files are the durable
  artifact** and migrate forward.
- Set and Schedule data modules stay deferred.

> **Correction to `V11_READLOG.md`.** Its closing section names v12 as "a
> terminal front end". That contradicts the arc agreed at v10, where v12 is
> runtime control and the TUI is a separate repo built afterwards. Task 1 fixes
> that section.

## The five decisions this version turns on

| # | Question | Decision |
|---|---|---|
| 1 | How does a front end drive a run? | **Step-based. The caller owns the loop.** |
| 2 | What can it see while running? | **Sampled progress and live statistics. Nothing per-event.** |
| 3 | One replication or a whole study? | **A whole study. `Experiment` is refactored onto it.** |
| 4 | Where does a run length live? | **A `[Run]` data module in the document.** |
| 5 | What does the harness compare? | **Report text, byte-identical, no tolerances.** |

---

## Architecture

```
  des-tui                          SEPARATE REPO (after v12)
  ═════════════════ repo boundary ═════════════════
  v12  RunController · RunProgress · RunSnapshot
       RunSetup / [Run] module · regression harness
  ─────────────────────────────────────────────────
  v11  ModuleRegistry · ModelDocument · Compiler · des CLI
  ─────────────────────────────────────────────────
  v1–v10  Model · INode · Expression · Diagnostic
```

Four units, each with one job:

| Unit | Responsibility |
|---|---|
| `SimulationSystem::stepOnce()` | One event, and the loop that was `run()`. |
| `RunController` | The replication loop, the state machine, and the control surface. |
| `RunProgress` / `RunSnapshot` | What a watcher sees, built on demand. |
| `RunSetup` + `[Run]` | A run length that lives in the model file. |
| `Regression` | Many models, run and diffed. |

---

## 1. `SimulationSystem` gains a step

`run()` is one `while` loop whose body handles the imminent event. v12 extracts
the body without changing it:

```cpp
// A step is possible: there is an event, and no stopping rule is met yet.
bool canStep() const;

// Process the imminent event. Returns false when nothing was done, either
// because canStep() was false or because the event was EndSimulation.
bool stepOnce();

void run() { while (stepOnce()) {} }
```

**The order inside the body does not move.** Close the integrals for the
interval that just ended, using the old state; then advance the clock; then let
the handler change state. Swapping any two silently corrupts every
time-weighted average, with no error anywhere. This is the highest-risk edit in
the version and the reason the decisive test below is what it is.

One structural change is unavoidable: `EventType::EndSimulation` currently
`return`s out of `run()`. It becomes a member flag, `m_stopped`, that
`canStep()` reads. Nothing else about dispatch changes.

## 2. `RunController`

```cpp
enum class RunState { Ready, Running, Paused, Finished, Cancelled, Failed };

class RunController {
public:
    // From a document: reads the [Run] module, compiles once to report every
    // bad cell before anything runs, and is then Ready.
    //
    // It KEEPS A COPY of the document, because each replication needs a fresh
    // SimulationSystem with a fresh Model built into it -- a Model cannot be
    // reused across replications, and cannot be copied either. So the builder
    // it installs is "compile this document into that system", which runs once
    // per replication. A document is rows of strings and copying one is cheap.
    static std::unique_ptr<RunController>
    fromDocument(const ModelDocument& doc, std::vector<Diagnostic>& out);

    // From a builder. The type is declared HERE, not in Experiment: Experiment
    // is refactored onto RunController, so naming Experiment::Builder in this
    // header would make the two include each other. Experiment::Builder becomes
    // an alias for this.
    using ModelBuilder = std::function<void(SimulationSystem&)>;
    RunController(RunSetup setup, ModelBuilder build);

    RunState state() const;

    // Do at most maxEvents events. Returns how many it actually did. Rolls on
    // to the next replication when one finishes. This is the whole surface.
    std::size_t advance(std::size_t maxEvents);

    void pause();     // Running -> Paused. advance() then does nothing.
    void resume();    // Paused  -> Running.
    void cancel();    // -> Cancelled, permanently. Results so far are readable.

    RunProgress progress() const;
    RunSnapshot snapshot() const;
    const std::vector<ReplicationResult>& results() const;

    // Convenience for callers that do not want to own a loop.
    void runToCompletion() { while (advance(4096) > 0) {} }
};
```

**State machine.** `Ready → Running` on the first `advance()`. `Running ⇄
Paused` on `pause()`/`resume()`. `Running → Finished` when the last replication
ends. Any state except `Finished` → `Cancelled` on `cancel()`; `Cancelled` and
`Finished` are terminal, and `advance()` on either returns 0 rather than
throwing — a front end polling after the end is normal, not an error.
`Failed` is entered when building a replication throws `ModelError`; the
message is kept and readable.

**`Experiment::run()` becomes a wrapper** over a `RunController`, exactly as
v11's `validate()` became a wrapper over `checkStructure()`. One implementation
of the replication loop, two callers. Duplicating it would leave two versions to
drift, and the one that drifts is always the one nobody runs.

**Antithetic pairing is the complication to design around.** With
`antitheticPairs(true)`, one replication is *two* runs — the second with every
uniform mirrored — averaged into a single `ReplicationResult`. The controller
therefore tracks a replication index *and* which half of a pair it is in.
`RunProgress::replication` counts replications, not runs, because that is what
`replications(n)` means to the caller; `RunProgress::fraction` is the fraction
through the current *run*, and the pair's second half restarts it at zero. This
must be documented where a front end will read it, or a progress bar will
appear to go backwards.

## 3. Watching

```cpp
struct RunProgress {
    RunState  state{RunState::Ready};
    int       replication{0};        // 1-based; 0 before the first advance()
    int       replications{1};
    SimTime   now{0.0};
    long long eventsProcessed{0};    // across the whole study
    std::optional<double> fraction;  // through the CURRENT run, or nothing
};
```

`ITerminationRule` gains one method, with a default so every existing rule
compiles unchanged:

```cpp
virtual std::optional<double> progress(const SimulationSystem&) const {
    return std::nullopt;
}
```

| Rule | Answer |
|---|---|
| `TimeLimit` | `now / maxTime`, clamped to [0, 1] |
| `EntityLimit` | `exited / maxEntities`, clamped |
| `DrainedRule` | **nothing** |
| `AnyOf` | the largest value among the children that know; nothing if none do |

**`nullopt` means "cannot tell", never "zero".** This is the fourth time this
project has had to state that rule — after `VisitRatios::exact`, v9's silently
zero WIP, and v10's unknowable arrival mean, which made ρ = 2 look stable. A
progress bar that reads 0% for the whole of a `whenDrained()` run and then jumps
to 100% is not an approximation; it is a lie the caller cannot detect. A front
end that gets `nullopt` shows a spinner.

```cpp
struct BlockSnapshot    { std::string name; double queueLength; long long served;
                          double utilisation; };
struct ResourceSnapshot { std::string name; double busy; double capacity; };
struct VariableSnapshot { std::string name; double value; };

struct RunSnapshot {
    SimTime   now{0.0}, measuredTime{0.0};
    long long arrived{0}, exited{0};
    double    numberInSystem{0.0};
    std::vector<BlockSnapshot>    blocks;
    std::vector<ResourceSnapshot> resources;
    std::vector<VariableSnapshot> variables;
};
```

Every field comes from an accessor that already exists. **Nothing is added to
the inner loop**: a snapshot costs only when asked for, so a caller that never
watches pays nothing, and — the point that matters most here — the run is
bit-for-bit what it would have been unwatched.

## 4. `RunSetup` and the `[Run]` module

```cpp
struct RunSetup {
    std::optional<SimTime>   length;        // no value = no time limit
    SimTime                  warmUp{0.0};
    int                      replications{1};
    unsigned                 baseSeed{12345u};
    bool                     stopWhenDrained{false};
    std::optional<int>       maxEntities;
    bool                     separateStreams{false};
    bool                     antithetic{false};
};

// Reads the [Run] row, or returns defaults when the document has none.
RunSetup readRunSetup(const ModelDocument& doc, std::vector<Diagnostic>& out);
```

Schema, added to `buildSchemas()` as a Data module:

| Column | Type | Required | Default |
|---|---|---|---|
| Name | Identifier | yes | |
| Length | Real | no | *(empty)* |
| Warm-up | Real | no | `0` |
| Replications | Integer | no | `1` |
| Base Seed | Integer | no | `12345` |
| Stop When Drained | Boolean | no | `false` |
| Max Entities | Integer | no | *(empty)* |
| Separate Streams | Boolean | no | `false` |
| Antithetic | Boolean | no | `false` |

**At most one row**, as Arena has one Run Setup. A second is an error reported
at the cell, naming the row.

A `[Run]` with no stopping column set at all is a **warning**, not an error. It
is tempting to refuse it -- a model that says how to run and not when to stop
looks like a model that runs forever -- but that is wrong for a legitimate case
this engine already supports: a Create with `Max Arrivals` set produces a finite
model whose future event list simply empties, and `canStep()` ends the run.
Refusing it would reject a correct model. The warning says the run ends only
when the model runs out of events, which is the fact the author needs.

**v11's signatures are untouched.** `readRunSetup` is a free function and
`CompileResult` gains a `RunSetup setup` field — additive at the only site that
constructs one. `compileInto` does not change; a caller that wants both calls
both. Adding a parameter would have broken every v11 caller for no gain.

`des run model.des [until]` reads the `[Run]` module when present and the
command-line argument overrides `Length`. With no `[Run]` and no argument it
prints the note it prints today and uses the stated default horizon.

## 5. The regression harness

```
tests/regression/
    manifest            one model path per line, # comments allowed
    teller.expected     the report text this model must produce
    decide.expected
    ...
```

`des regress` runs every model in the manifest through a `RunController`,
captures what `report()` prints, and diffs against the stored expectation.
`des regress --capture` writes expectations.

**`report()` has to become capturable.** It writes to `std::cout` today, so
`void report(std::ostream&) const` is added and the existing no-argument form
becomes a wrapper passing `std::cout`. Redirecting `cout` instead would work
and is worse: a global side effect, in a harness whose whole purpose is that its
result be trustworthy. `reportArenaStyle()` gets the same treatment for the same
reason.

**Every model in the manifest must carry a `[Run]` row**, and the harness errors
on one that does not. An expectation whose meaning depends on a default horizon
stored somewhere else stops being an expectation the moment that default
changes.

**Byte-identical, no tolerances.** A stored mean with a tolerance passes a real
regression that lands inside the band, which is exactly the "near-miss average"
standard this project rejected in v10.

Three properties it has from the start, each because v11 shipped a gate without
one:

- **It fails when it checked nothing.** `tools/baseline.sh` reported
  `BASELINE CLEAN` over zero examples for one revision because an `exit` fired
  inside a command substitution. A gate that measured nothing must never report
  success.
- **`--capture` refuses to overwrite an existing expectation** unless
  `--force` is given, and says which file it refused. Re-capturing after a
  change is how a gate silently stops being a gate.
- **Expectations are `.expected`, not `.txt`.** `.gitignore` excludes `*.txt`
  with one hand-maintained negation for `tests/baseline/*.txt`; a second
  negation is a trap, and a silently ignored expectation file is a gate that
  covers less than it appears to.

The harness does **not** absorb `tools/baseline.sh`. That script gates fifteen
compiled example programs, which means launching external binaries — the one
thing a shell script is genuinely better at.

---

## Testing

**The decisive test.** For every model in `examples/models/`, a run driven by
`advance(1)`, by `advance(7)`, by `advance(1000)` and by plain `run()` produces
**byte-identical traces**. Chunk size must not change the run. This is the same
standard as v10's text-versus-code traces and v11's file-versus-code traces, and
it is the only thing that makes the step extraction safe to believe.

A pause between two chunks, and a resume, must not change it either — the run is
identical whether or not anyone was watching.

Then:

| What | Asserted |
|---|---|
| State machine | Every legal transition; `advance()` after `cancel()` returns 0; `pause()` makes `advance()` do nothing; `Failed` keeps its message |
| Progress | `TimeLimit` and `EntityLimit` give exact fractions; `DrainedRule` gives **nothing**; `AnyOf` gives the largest known and nothing when none know |
| Snapshot | Values match the same run's `report()` at the same instant, and taking one does not perturb the trace |
| `[Run]` module | Every column, its default, a second row refused at the cell, and a `[Run]` with no stopping condition refused |
| `Experiment` refactor | Every v4 replication and confidence-interval check passes **unchanged**, antithetic pairing included |
| Harness | Catches a deliberate perturbation; fails when the manifest is empty; `--capture` refuses to overwrite |

**Constraints, as in v11:**

- All **779 v11 checks pass unchanged**. If one needs changing, stop and raise it.
- The 15 gated examples stay byte-identical: `bash tools/baseline.sh check`.
- `bash tools/verify.sh` prints `VERIFY CLEAN`.
- C++17, no external dependencies, sources listed in `CMakeLists.txt`.
- Comments only where they carry design rationale.

---

## Explicitly not in v12

| Deferred | Why |
|---|---|
| Threads | The engine's claim is that one seed gives one run. Concurrency buys responsiveness a stepped loop already provides. |
| A per-event observer | Chosen against: a callback firing 1.4M times puts the front end inside the inner loop. Add it when something needs animation. |
| The TUI | Separate repo, after this. |
| Editing a model while it runs | A `Model` is fixed once `initialise()` has run; live editing is a v13+ question about what a partial reset means. |
| Set and Schedule data modules | Still deferred; they change simulation semantics. |
| Variable arrays | Still YAGNI. |
| Absorbing `tools/baseline.sh` | It launches external binaries; a shell script is the right tool. |

## Still open, carried forward

`12_shared_resources` is not reproducible run-to-run (a v9 bug, excluded from
the byte-identical gate with the exclusion printed every run). A `Separate`
duplicate is counted as an exit it never arrived for. Resource schedules,
preemption, batch means and distribution fitting are untouched.
