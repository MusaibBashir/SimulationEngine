# DES Engine — a discrete-event simulator, built version by version

Written from the simulation theory table up, as a way of learning OOP and system
design rather than as a way of getting a simulator.

**Current state: v8.** A flowchart simulator in the spirit of Arena's Basic
Process template — Process, Delay, Assign, Decide, Batch, Separate, Record,
Dispose — with **shared resources**, balking and reneging, warm-up removal,
replications and confidence intervals. Refuses to run an unstable model, working
out the offered load by walking the flowchart and summing across every block that
shares a resource. Random number generation is built from a single `u01()`
primitive, with pluggable engines, generator-quality tests, twelve distributions,
and both major variance-reduction techniques. Builds clean under
`-Wall -Wextra -Wpedantic` and ASan/UBSan; 160/160 unit checks pass; a
deterministic run still reproduces a hand-worked table event for event.

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
./build/des          # five demonstration scenarios
./build/des_tests    # the unit suite
cd build && ctest
```

Without CMake:

```
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude main.cpp src/*.cpp -o des
```

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
| `examples/README.md` | How to use the engine: API reference, gotchas, checklist |

## Layout

```
sim/
├── CMakeLists.txt   README.md   CHANGELOG.md   V2_READLOG.md   V3_READLOG.md
├── main.cpp         five scenarios
├── tests/           85 unit checks
├── include/         19 headers
└── src/             18 sources
```

Headers declare, sources define. One-line getters stay inline. Every `.cpp`
includes its own header first — a free self-test that the header stands alone.

## Theory table → file

| Theory term | Where it lives |
|---|---|
| System | `SimulationSystem` (engine) + `Model` (what is simulated) |
| Entity | `Entity` |
| Attribute | `Entity::m_attributes` |
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

# v9 — the order to do it in

1. **Resource schedules** — a nurse who goes off shift at 5pm. Arena has it; it
   needs a capacity that varies with time, which interacts with every ρ
   calculation in the model.
2. **Preemption** — a high-priority entity taking a resource off a low-priority
   one mid-service. Lazy cancellation does *not* solve this: the interrupted
   entity has to requeue carrying its remaining service time.
3. **Batch means** — one long run split into batches, as an alternative to
   independent replications when the warm-up is expensive to repeat.
4. **Config file input.** Nothing has blocked it since v5.
5. **Streams for every block**, finishing what v8 started — Delay durations and
   Decide draws still share the common stream.
6. **Distribution fitting** — hand it data, have it suggest a distribution and
   report a goodness-of-fit statistic. `StreamTests` already has the chi-square
   and Kolmogorov–Smirnov machinery; this is mostly wiring plus parameter
   estimation.
