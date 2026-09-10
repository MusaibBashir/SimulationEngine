# DES Engine — a discrete-event simulator, built version by version

Written from the simulation theory table up, as a way of learning OOP and system
design rather than as a way of getting a simulator.

**Current state: v13.** A flowchart simulator in the spirit of Arena's Basic
Process template — Process, Delay, Assign, Decide, Batch, Separate, Record,
Dispose — with **shared resources**, balking and reneging, warm-up removal,
replications and confidence intervals. Refuses to run an unstable model, working
out the offered load by walking the flowchart and summing across every block that
shares a resource. Random number generation is built from a single `u01()`
primitive, with pluggable engines, generator-quality tests, twelve distributions,
and both major variance-reduction techniques. Builds clean under
`-Wall -Wextra -Wpedantic` and ASan/UBSan; 1368/1368 unit checks pass; a
deterministic run still reproduces a hand-worked table event for event.

**v10: a model can be written as text.** Every duration, condition and
assignment value is an expression, so `station("Cut", 1, FIFO, "size * 0.5")`
and `decideWhen("Busy?", "NQ(Machine) > 3")` are models, not code. Arena's
Variable data module arrived with it. This is what a spreadsheet and a
front end need, and neither is possible while a field holds a lambda.

**v11: a model can be written as a file.** Every module type publishes its
columns, a `ModelDocument` holds rows of text cells, and a `.des` file
round-trips through them byte for byte. `compile()` turns one into a runnable
model, or into diagnostics that point at individual cells. See **A model as a
file** below.

**v12: a run can be driven.** `run()` is `while (stepOnce())` now, so a caller
owns the loop: advance a budget of events, redraw, advance again. Pause, cancel
and live statistics fall out of that, and `des regress` runs a manifest of
models and diffs their reports. See **Watching a run** below.

**v13: a model can be edited on a screen.** `des_tui` opens a `.des` file as
one spreadsheet per module type, edits cells with the diagnostics shown against
the cell that caused them, saves without disturbing the records you did not
touch, and runs the model. See **The terminal UI** below.

**v14: a model can be built from nothing, by somebody who has only used
Arena.** Every column in every module carries a line saying what it is for —
in the *schema*, so any front end gets it. `Enter` on an enum or a reference
opens Arena's drop-down, built from the same function the compiler validates
against, and `Escape` drops into typing so a block can be named before it
exists. `^T` fills a blank page with a working model, `^F` shows the wiring,
and a key map says the one thing an Arena user will not guess: there is no
canvas, and a connection is a name typed into a `Next` field.

**v15: the model IS the text.** A module palette on the left, the `.des` file
in an editor on the right, and Flow, Runs and Results as tabs across the top —
with the mouse working throughout. The buffer is the source of truth and
everything else is derived from it after every keystroke, so what you save is
what you typed. `[Run]` records can be named and kept side by side, and the
Results tab remembers which one produced what.

```cpp
#include "des.hpp"
using namespace des;

SimulationSystem sim(12345u);
sim.model().arrivals(exponential(1.0))
           .station("Teller", 1, FIFO, exponential(0.8))
           .entryAt("Teller");
sim.stopAt(480.0).execute().report();
```

## Build and run

```
cmake -S . -B build && cmake --build build
./build/des_demo     # five demonstration scenarios
./build/des          # the model-file tool: des check / des run / des regress
./build/des_tui      # the terminal UI: edit a model file
./build/des_tests    # the unit suite
cd build && ctest
```

Without CMake:

```
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude main.cpp src/*.cpp -o des
```

**The verification gate, in one command each:**

```
bash tools/verify.sh      # both compilers warning-clean, plus MSVC AddressSanitizer
bash tools/baseline.sh check   # every example still byte-identical
./build/des regress            # every model file still produces its report
```

All three FAIL when they measured nothing. A gate that checked zero cases has
not passed, it has not run -- `baseline.sh` reported CLEAN over zero examples
once, and that is why every one of them now says so.

`tools/verify.sh` finds a C++17-capable compiler itself and refuses one that is
too old. **UBSan does not exist on the Windows side** — MinGW ships no
`libubsan` and MSVC has none — so the script runs ASan and UBSan through **WSL's
Linux GCC** instead, and skips that leg loudly if WSL is absent rather than
passing silently.

Before and after any refactor — entities are destroyed at departure, so a
routing mistake becomes a use-after-free, and this is what proves it hasn't:

```
g++ -std=c++17 -g -O1 -fsanitize=address,undefined -Iinclude main.cpp src/*.cpp -o des_asan && ./des_asan
```

## Writing a model

The engine takes a `Model`. Nothing else about your system reaches it.

```
SimulationSystem sim(seed);
Model& m = sim.model();

m.setInterarrival( <IDistribution> );
m.addStation(name, capacity, discipline, <IDistribution> service);
m.connect("From", "To");        // omit and the station is an exit
m.setEntry("First");

sim.setTermination( <ITerminationRule> );
sim.enableTrace("trace.md", TraceLevel::Events);   // optional
sim.setWarmUp(4000.0);                             // optional, v4
sim.initialise();
sim.run();
sim.report();
```

## Writing a model as text

Every field that held a distribution or a lambda now takes a string. The two
spellings build the same thing, and a test asserts they trace identically.

```cpp
sim.model()
   .variable("Rejected", 0.0)             // a global, Arena's Variable module
   .attribute("size", uniform(1.0, 4.0))
   .arrivals("EXPO(2.0)")
   .station("Machine", 1, FIFO, "size * 0.5")     // reads the entity
   .decideWhen("Busy?", "NQ(Machine) > 3")        // reads the queue
   .assignVariable("Fail", "Rejected", "Rejected + 1")
   .entryAt("Busy?");
```

**The grammar.** C-style operators, `^` right-associative, unary minus binding
looser than `^`, `&&` and `||` short-circuiting.

```
|| && == != < <= > >= + - * / % ^ ! ( ) ,
```

| Kind | Names |
|---|---|
| Distributions | `EXPO` `CONS` `UNIF` `TRIA` `NORM` `LOGN` `WEIB` `ERLA` `POIS` `DISC` |
| Model state | `NQ(block)` `NR(name)` `MR(name)` `WIP()`, and the constant `TNOW` |
| Maths | `MIN` `MAX` `ABS` `ROUND` `TRUNC` `SQRT` `LN` `EXP` `MOD` |

Names resolve to entity attributes, declared variables, or `Entity.Type`.

**Gotchas, each of which is a real trap:**

- **`EXPO` is the exponential distribution; `EXP` is e^x.** Arena's collision,
  kept deliberately.
- **`DISC` takes cumulative probabilities**, as Arena does —
  `DISC(0.3, 1, 0.8, 2, 1.0, 3)` means P = 0.3, 0.5, 0.2. The engine's own
  `discrete()` takes individual ones.
- **`LOGN` takes the mean and sd of the variable**, not of its logarithm.
- **`NQ()` takes a block name, not a value.** `NQ(Teller)` is the block called
  Teller; `NQ(x + 1)` is an error.
- **A variable may not share a name with an attribute.** Refused at declaration,
  because a resolution order means one of the two reads silently wrong.
- **An interarrival field has no entity.** `arrivals("size * 2")` is refused at
  `initialise()`, not mid-run.
- **There is no distribution field.** `5`, `EXPO(0.8)` and `size * 0.5` are the
  same kind of field, exactly as in Arena.

A bad expression on the C++ API throws `ModelError` listing every problem with
its column. `parseExpression()` returns those diagnostics instead of throwing,
which is what the document layer uses.

## A model as a file

A model is a set of **spreadsheets**, one per module type, and a `.des` file is
those spreadsheets written down. Nothing in it is code.

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

A `[Run]` module carries the run length, so the file says everything needed to
reproduce a result:

```
[Run]
Name         = Setup
Length       = 480
Replications = 1
```

```
./build/des check   examples/models/teller.des     # compile and report
./build/des run     examples/models/teller.des     # run as the file says
./build/des run     examples/models/teller.des 100 # ...or override the length
./build/des regress                                # every model, diffed
```

`check` reports **every** bad cell, not the first, and names the module, the row
and the column — with the character offset inside the cell when the expression
parser found one:

```
teller.des: Process row 1, Service (col 9): error: expected ')'
```

**Regression over model files.** `des regress` runs every model in
`tests/regression/manifest` and compares its report against a stored
`.expected`, byte for byte — no tolerances, because a stored mean with a
tolerance passes a real regression that lands inside the band.

```
./build/des regress
./build/des regress --capture     # refuses to overwrite; --force to insist
```

It fails when it checked nothing. A gate that measured nothing has not passed;
it has not run.

**Traps, each of them real:**

- **Row order is semantic.** A Decide takes the first branch that matches and an
  Assign runs its fields in order, so moving two `[DecideBranch]` or
  `[AssignField]` records past each other changes what the model does.
- **An empty exit means "leaves the system".** A `Next` that is blank is not an
  error; it is a departure.
- **Unknown columns and unknown module types are kept, and warned about.** A
  file written by a later version opens here with its extra columns intact — a
  front end that silently deleted them would corrupt the file it was asked to
  edit.
- **Routing lives in a column.** Arena draws connections on a canvas; a terminal
  cannot, so `Next`, `Duplicate`, `Balk To` and `Renege To` are cells.
- **Queue is read-only.** There is no queue object apart from its Process, so
  the discipline is set on the Process row.
- **Read then write gives back the same bytes**, comments and spacing included.

In C++ the same file is:

```cpp
ReadResult read = readDocumentFile("examples/models/teller.des");
SimulationSystem sim(12345u);
std::vector<Diagnostic> problems;
if (compileInto(read.document, sim.model(), problems))
    sim.stopAt(480.0).execute().report();
```

## The terminal UI

```
./build/des_tui examples/models/teller.des
```

`des_tui newmodel.des` on a file that does not exist opens a blank page.

A **module palette** on the left, the **`.des` file itself** on the right, and
the other three views as tabs across the top:

```
  Model ^B  Flow ^F  Runs ^U  Results ^E                 examples/models/teller.des
+- Modules ------- 25 lines --------------------------------------------------+
| Variable      |   1   version = 1                                           |
| Entity        |   2                                                         |
| Resource      |   3   # How to run it. Arena keeps this in Run Setup; here  |
| Expression    |   4   # it is a module like any other.                      |
| Run 1         |   5   [Run]                                                 |
| Create 1      |   6   Name         = Setup                                  |
| Process 1     |   7   Length       = 480                                    |
| Delay         |   8   Replications = 1                                      |
| Assign        |   9                                                         |
| Decide        |  10   [Create]                                              |
| Batch         |  11   Name         = Arrivals                               |
| Separate      |  12   Interarrival = EXPO(1.0)                              |
| Record        |  13   Next         = Serve                                  |
| Dispose 1     |  14                                                         |
| DecideBranch  |  15   [Process]                                             |
| AssignField   |  16   Name       = Serve                                    |
+-----------------------------------------------------------------------more v-+
examples/models/teller.des
^S save  ^R run  ^L list  ^G help  ^J error  ^T starter  F1 keys
```

**The text is the model.** The buffer holds the file and everything else — the
parsed document, the diagnostics, the flow, the list of runs — is derived from
it after every keystroke. So what you save is what you typed: comments,
spacing and ordering survive because nothing rewrites them.

| Key | |
|---|---|
| `F1` | the key map, and the one thing an Arena user will not guess |
| `^B` `^F` `^U` `^E` | Model, Flow, Runs, Results |
| `Tab` | swap between the palette and the text |
| `Enter` | in the palette: write that module into the text |
| `^G` | explain whatever the cursor is on |
| `^L` | list the values a field allows — Arena's drop-down |
| `^J` | jump to the first error |
| `^T` | write a working model into an empty file |
| `^R` | run it; `^N` on the Runs tab adds another `[Run]` |
| `^W` | save the results |
| `^S` `^Z` `^Y` `^X` `^C` `^V` `^A` | save, undo, redo, cut, copy, paste, select all |
| `^Q` | quit — and it **asks** if there is unsaved work |

**The mouse works too**: click a tab, click a module to insert it, right-click
one to read what it does, click into the text to put the caret there, and the
wheel scrolls whatever is under it.

**There is no canvas, and that is the one thing to know.** A model is this
text, and a *connection* is the name of the next block typed into a `Next`
field. Everything else maps onto Arena directly — `ARENA_MAP.md` has the table.

**Picking a module writes the whole record**, header and every field, blank:

```
[Process]
Name =
Capacity =
Resource =
Units =
Discipline =
Service =
Balk At =
...
```

Every field, not only the required ones — a template that hid the optional half
would hide balking, reneging and entity types behind knowing they exist. Delete
the lines you do not need; a blank field means "unset", which is what the
schema's default already says.

**`^L` on an enum or a reference is Arena's drop-down.** The list is built by
the same function the compiler's reference pass checks against, so it cannot
offer a name that is then rejected. `Escape` closes it and leaves you typing,
because naming a block *before that block exists* is the ordinary way a model
gets built and no list can offer that.

**Errors are marked in the gutter, on the line they are about:**

```
|   4   Name = A                                       |
|   5 E Entity Type = Gears                            |
|   6   Interarrival = EXPO(0.6)                       |
|   7 E Next = Nowhere                                 |
```

A diagnostic names a *cell*; `lineOf()` turns that back into a line, and `^J`
takes you to the first one. Pressing `^R` on a model that does not compile
names the actual problem rather than saying it will not run.

**Many `[Run]` records, chosen by name.** Arena keeps one Run Setup and makes
you edit it to try a longer horizon; a file can hold several:

```
+-- runs --------------------------------------------------------------+
|  name              length   warm-up   reps   seed                    |
|> Short             50       0         1      12345                   |
|  Long              5000     500       20     12345                   |
+----------------------------------------------------------------------+
Up/Down choose   Enter runs it   ^N adds one   F1 keys
```

**Results are a tab**, dimmed until there are any, and the run lands on them
when it finishes. `^W` writes the report beside the model, carrying which run
produced it.

**^F shows the wiring**, read from the cells rather than from a compiled model
— so it works on a file that does *not* compile, which is when you most need
it. It names exits that point nowhere and blocks nothing arrives at, and
`Enter` puts the cursor on that record. A `Decide` lists its branches **in the
order they are tried**, with its `Next` shown last as `else ->`.

**A model with no `[Run]` record runs anyway**, until the event list empties —
which is what the reader's warning has always promised.

## Watching a run

`run()` blocks until the stopping rule is met, which is right for a program and
wrong for anything that has to redraw. `RunController` inverts it: the caller
owns the loop.

```cpp
ReadResult read = readDocumentFile("examples/models/teller.des");
std::vector<Diagnostic> problems;
auto run = RunController::fromDocument(read.document, problems);

while (run->state() == RunState::Ready || run->state() == RunState::Running) {
    run->advance(4096);                 // do some work, then come back
    const RunProgress p = run->progress();
    if (p.fraction) draw(*p.fraction);  // or a spinner when it cannot tell
    const RunSnapshot s = run->snapshot();
    for (const BlockSnapshot& b : s.blocks) drawQueue(b.name, b.queueLength);
}
run->report(std::cout);
```

**Pausing is not calling.** `pause()` makes `advance()` do nothing; `resume()`
un-does it; `cancel()` is permanent and the run so far stays readable.
`advance()` past the end returns 0 rather than throwing, because a front end
that polls after a run finishes is normal rather than wrong.

**`progress().fraction` may be empty, and that is information.** A `TimeLimit`
knows how far through it is; `whenDrained()` cannot, because whether a system
will next be empty is not knowable in advance. A bar that reads 0% for a whole
run and then jumps to 100% is a lie the caller cannot detect, so the rule
returns nothing and a front end shows a spinner.

**Watching does not change the run.** A snapshot is built when it is asked for
and nothing is added to the inner loop — asserted, not assumed: the same model
stepped one event at a time, thirteen at a time, and with a snapshot taken
between every chunk traces byte-identically to `run()`.

For several replications the controller rolls over on its own, and
`progress().replication` says which one. `Experiment` is the same machinery with
confidence intervals on top.

## Getting an answer you can defend

One run of a simulation is **one sample from a random variable**. Quoting it to
four decimals implies a precision that does not exist. Use `Experiment`:

```
Experiment e("my study", [](SimulationSystem& s){ /* build the model */ });
e.replications(10).baseSeed(9000u).observeEvery(5.0);
e.run();
e.writeWelchSeries("welch.csv", 20);        // plot it, find the transient
SimTime w = e.suggestWarmUp();              // MSER; a starting point

e.replications(10).warmUp(w);
e.run();
e.report();                                 // mean +/- 95% interval
```

Output:

```
  quantity                    mean      95% half-width      interval
  average wait (Wq)         3.0920  +/-   0.1192   [   2.9728,    3.2112]
  utilisation (rho)         0.7982  +/-   0.0066   [   0.7916,    0.8048]
```

M/M/1 theory is Wq 3.2000 and rho 0.8000 — both inside. That is what "the
simulation agrees with theory" actually looks like.

A restaurant is three `addStation` calls and two `connect` calls:

```
Host(1) ──▶ Waiters(3) ──▶ Cashier(1) ──▶ exit
```

**Distributions**: `Exponential(mean)`, `Constant(v)`, `Uniform(a,b)`,
`Triangular(a,mode,b)`, `Deterministic({...})`.
Exponential takes a **mean**, not a rate — the 1/λ conversion happens in exactly
one line inside `RandomStream`.

**Disciplines**: `FIFO`, `LIFO`, `Priority`, `SPT`, `EDD`, `Random` — or build an
`IQueueRule` directly for anything else.

**Termination**: `TimeLimit(t)`, `EntityLimit(n)`, `DrainedRule()`, or `AnyOf`
composing several.

## Checking a run by hand

Use `Deterministic` for both interarrival and service times, cap it with
`EntityLimit`, and turn the trace on. You get a markdown table you can compare
line by line against a worked example from your notes:

| t | event | entity | detail |
|---:|---|---:|---|
| 0.0000 | Seize | 1 | server free, service 3.0000 until 3.0000 |
| 2.0000 | Queue | 2 | all 1 busy, queued at position 1 |
| 3.0000 | Exit | 1 | exits; total wait 0.0000, time in system 3.0000 |
| 3.0000 | Seize | 2 | pulled from queue after waiting 1.0000 |

Traces also `diff`. A refactor that changes behaviour shows up as a diff rather
than as a slightly-off average — which is a far stronger statement.

## New here?

**Start with [`examples/`](examples/README.md)** — ten runnable programs and a
guide written for someone who knows C++ but not this codebase. You should not
need to read the engine's source to build a model with it.

## Documents

| File | What it is |
|---|---|
| `CHANGELOG.md` | What changed in each version |
| `V2_READLOG.md` | Review of v2 and v2.1: every bug found and why it mattered |
| `V3_READLOG.md` | What each v3 abstraction bought, what it cost, what was skipped |
| `V4_READLOG.md` | Warm-up removal, Welch's method, confidence intervals |
| `V5_READLOG.md` | The ergonomics pass: namespace, factories, ModelError, ρ check |
| `V6_READLOG.md` | Flowchart blocks, INode, and why NodeContext beat a wider public interface |
| `V7_READLOG.md` | Shared resources, balking, reneging, N-way Decide |
| `V8_READLOG.md` | Random number generation, RANDU, variance reduction |
| `V10_READLOG.md` | The expression layer: one grammar, saying you do not know, and the bugs |
| `V9_READLOG.md` | Multiple sources, entity types, matched batching, terminating runs |
| `V11_READLOG.md` | The document layer: schemas, the .des format, and two gates that were not gating |
| `V12_READLOG.md` | Runtime control: stepping, progress that can say it does not know, and three sabotages that proved nothing |
| `V13_READLOG.md` | The terminal UI, and the v11 guarantee it proved false |
| `V14_READLOG.md` | Building a model from nothing: pick lists, the starter model, and four bugs only looking found |
| `V15_READLOG.md` | The grid becomes a text editor, and the assert that had been one keystroke away since v11 |
| `ARENA_MAP.md` | Arena module → this engine, and where the two differ |
| `examples/README.md` | How to use the engine: API reference, gotchas, checklist |

## Layout

```
sim/
├── CMakeLists.txt   README.md   CHANGELOG.md   V2_READLOG.md   V3_READLOG.md
├── main.cpp         five scenarios (des_demo)
├── cli/             the des command: check, run and regress
├── tui/             des_tui: the terminal UI, and the only platform code
├── examples/models/ hand-written .des models
├── tests/           1368 unit checks
├── include/         50 headers
└── src/             42 sources
```

Headers declare, sources define. One-line getters stay inline. Every `.cpp`
includes its own header first — a free self-test that the header stands alone.

## Theory table → file

| Theory term | Where it lives |
|---|---|
| System | `SimulationSystem` (engine) + `Model` (what is simulated) |
| Entity | `Entity` |
| Attribute | `Entity::m_attributes` |
| Variable ★ | `VariableStore` — global, time-persistent |
| Expression ★ | `IExpression`, built by `Parser` |
| Resource | `Resource`, owned by a `Station` |
| State ★ | `SystemState`, a snapshot summed across stations |
| Event ★ | `EventType` + `EventNotice` |
| Queue | `EntityQueue` |
| Queue discipline | `IQueueRule` and its subclasses |
| Clock ★ | `Clock` |
| Future Event List ★ | `FutureEventList` |
| Event notice | `EventNotice` |
| Activity | `Activity` — duration known at construction |
| Delay | `Delay` — duration decided later, by the system |
| Statistical accumulators ★ | `Statistics`, per station and system-wide |
| Termination condition | `ITerminationRule` and its subclasses |
| Initialization | `SimulationSystem::initialise()` |
| *(not in the table)* | `RandomStream`, `Distribution`, `Station`, `Trace` |

## How a run actually works

```
initialise()
  validate the model (routing loops caught HERE, not as a hang)
  reset everything holding run state; configuration survives
  schedule Arrival at t=0            <- carries NO entity

run()
  while FEL not empty and no termination rule is met:
      notice = popImminent()               <- earliest event
      updateAllIntegrals(notice.time())    <- OLD state, interval just ended
      clock.advanceTo(notice.time())       <- the clock JUMPS
      dispatch on notice.type()

handleArrival
  create the entity HERE (so creationTime is correct)
  schedule the NEXT arrival        <- forget this and the run stops at one
  admit(entity, model.entry())

handleDeparture
  release the server at this station
  station has a next?  startNextService(here); admit(entity, next)
  otherwise            record exit stats; startNextService(here); destroy entity

admit(entity, station)
  server free?  seize, draw an Activity, schedule its Departure
  server busy?  push to the station queue, open a Delay
```

**The order inside `run()` is not negotiable.** Close the integrals for the
interval that just ended, *then* move the clock, *then* change state. Swap any
two and every time-average goes quietly wrong with no error.

## Design rules, accumulated across three versions

- **Derive, never duplicate.** `unitsAvailable()`, `Activity::endTime()`, every
  average. A second stored copy of a fact eventually disagrees with the first.
- **Wrap a value in a class only when there is an invariant to protect.** `Clock`
  qualifies: time never goes backwards, and `advanceTo` is the only door.
- **`unique_ptr` = I own this. Raw pointer = I observe this.**
- **Forward-declare when you only store a pointer.** The include lands in the
  `.cpp`.
- **Internal steps are `private`.** `admit`, `handleArrival`, `refreshState`.
- **A function that cannot do its job must assert, not return a plausible
  value.** Returning `nullptr` from an unimplemented discipline lost entities
  silently for a whole version.
- **One source of randomness.** Same seed, same run, or you cannot debug it.
- **Every object holding run state needs a `reset()`, and `initialise()` must
  call all of them.** Missing two made a second replication serve zero entities.
- **A default return value is a place for a bug to hide.** `attribute()` returns
  0.0 for a missing key; assert at the call sites that matter.
- **`std::move` on anything `const` is a silent no-op.**
- **Write the virtual destructor.** Without it, `delete` through a base pointer
  leaks the derived part, with no warning.
- **Don't abstract until the third case hurts.** Every hierarchy here was built
  only after the duplication was written out longhand and felt.

## Known / still open

- Wq reads 3-5% above closed-form theory in every run. **Not a bug** — no warm-up
  removal, single replication. v4.
- The system-level `Statistics` object holds L_q and L in members named
  `areaUnderQueueLength` / `areaUnderServerBusy`. Works; the names now lie.
- `EventNotice::s_nextSequenceNumber` is a mutable static. Reset between
  replications, still not thread-safe.
- `IEventHandler` and config-file input were deliberately skipped — reasons in
  `V3_READLOG.md`.

---

# v15 — the order to do it in

1. **Resource schedules** — a nurse who goes off shift at 5pm. Arena has it; it
   needs a capacity that varies with time, which interacts with every ρ
   calculation in the model.
2. **Preemption** — a high-priority entity taking a resource off a low-priority
   one mid-service. Lazy cancellation does *not* solve this: the interrupted
   entity has to requeue carrying its remaining service time.
3. **Redo**, and a saved-at marker in the undo stack so that undoing back to
   what is on disk clears the dirty flag honestly instead of leaving it set.
4. **Batch means** — one long run split into batches, as an alternative to
   independent replications when the warm-up is expensive to repeat.
5. **Distribution fitting** — hand it data, have it suggest a distribution and
   report a goodness-of-fit statistic. `StreamTests` already has the chi-square
   and Kolmogorov–Smirnov machinery; this is mostly wiring plus parameter
   estimation.
6. **A `[Run]` row from the UI without typing one.** ^T writes one; a model
   built row by row does not get one, and it runs to event-list exhaustion
   instead. That is honest but it is not what somebody wanted.

Config-file input, item 4 on the v9 list, is what v11 turned out to be. The
module palette, item 1 on the v14 list, turned out not to need a picker or an
answer to "where does it go in file order": `^N` on a tab already appends, and
the question was only ever which *type* — which the tab bar already answers.
