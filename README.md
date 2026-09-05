# DES Engine — a discrete-event simulator, built version by version

Written from the simulation theory table up, as a way of learning OOP and system
design rather than as a way of getting a simulator.

**Current state: v11.** A flowchart simulator in the spirit of Arena's Basic
Process template — Process, Delay, Assign, Decide, Batch, Separate, Record,
Dispose — with **shared resources**, balking and reneging, warm-up removal,
replications and confidence intervals. Refuses to run an unstable model, working
out the offered load by walking the flowchart and summing across every block that
shares a resource. Random number generation is built from a single `u01()`
primitive, with pluggable engines, generator-quality tests, twelve distributions,
and both major variance-reduction techniques. Builds clean under
`-Wall -Wextra -Wpedantic` and ASan/UBSan; 779/779 unit checks pass; a
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
./build/des          # the model-file tool: des check / des run
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
```

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

```
./build/des check examples/models/teller.des     # compile and report
./build/des run   examples/models/teller.des 480 # compile and run to t = 480
```

`check` reports **every** bad cell, not the first, and names the module, the row
and the column — with the character offset inside the cell when the expression
parser found one:

```
teller.des: Process row 1, Service (col 9): error: expected ')'
```

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
| `ARENA_MAP.md` | Arena module → this engine, and where the two differ |
| `examples/README.md` | How to use the engine: API reference, gotchas, checklist |

## Layout

```
sim/
├── CMakeLists.txt   README.md   CHANGELOG.md   V2_READLOG.md   V3_READLOG.md
├── main.cpp         five scenarios (des_demo)
├── cli/             the des command: check and run a model file
├── examples/models/ hand-written .des models
├── tests/           779 unit checks
├── include/         40 headers
└── src/             35 sources
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

# v12 — the order to do it in

1. **A terminal front end** over `ModuleRegistry` and `ModelDocument`: a module
   list, a spreadsheet per module type, a cell editor. It reads the schema at
   runtime, so it must never name a module type in its own source — that is the
   property the flat child tables were chosen to protect.
2. **Diagnostics rendered in the grid**, using the `CellRef` v11 added. The
   mechanism exists; nothing displays it yet.
3. **A run length in the document.** There is no Run module, so `des run` takes
   the horizon on the command line and says so.
4. **Undo** — which is why document rows are addressed by position and why the
   document keeps its source lines.

Still open from v9, untouched by v10 or v11:

1. **Resource schedules** — a nurse who goes off shift at 5pm. Arena has it; it
   needs a capacity that varies with time, which interacts with every ρ
   calculation in the model.
2. **Preemption** — a high-priority entity taking a resource off a low-priority
   one mid-service. Lazy cancellation does *not* solve this: the interrupted
   entity has to requeue carrying its remaining service time.
3. **Batch means** — one long run split into batches, as an alternative to
   independent replications when the warm-up is expensive to repeat.
4. **Distribution fitting** — hand it data, have it suggest a distribution and
   report a goodness-of-fit statistic. `StreamTests` already has the chi-square
   and Kolmogorov–Smirnov machinery; this is mostly wiring plus parameter
   estimation.

Config-file input, item 4 on the v9 list, is what v11 turned out to be.
