# DES Engine — v1 skeleton (pseudocode only)

Every file here is **comments only**. Each numbered step tells you what line to
write, what type to use, and why that choice over the alternatives. You write
the actual C++. Nothing in v1 simulates anything.

## What v1 is

Every noun from the theory table exists as a class that *stores* data. No
behaviour, no wiring, no run loop. Method stubs are **declared** so the
interfaces are fixed; filling the bodies is v2.

## Order to write them in

Bottom of the dependency graph upward — each file only needs the ones above it:

1. `include/Common.hpp` — types and enums, depends on nothing
2. `include/Entity.hpp` + `src/Entity.cpp`
3. `include/Resource.hpp` + `src/Resource.cpp`
4. `include/EntityQueue.hpp` + `src/EntityQueue.cpp`
5. `include/Clock.hpp` + `src/Clock.cpp`
6. `include/EventNotice.hpp` + `src/EventNotice.cpp`
7. `include/FutureEventList.hpp` + `src/FutureEventList.cpp`
8. `include/Activity.hpp`, `include/Delay.hpp` + their `.cpp`
9. `include/SystemState.hpp` + `src/SystemState.cpp`
10. `include/Statistics.hpp` + `src/Statistics.cpp`
11. `include/TerminationCondition.hpp` + `src/TerminationCondition.cpp`
12. `include/SimulationSystem.hpp` + `src/SimulationSystem.cpp`
13. `main.cpp`, `CMakeLists.txt`

**Compile after every single file.** Thirteen small errors beat one large one.
`g++ -std=c++17 -Iinclude -fsyntax-only include/Entity.hpp` checks a header on
its own without a full build.

## Theory table → file

| Theory term | Where it lives |
|---|---|
| System | `SimulationSystem` |
| Entity | `Entity` |
| Attribute | `Entity::m_attributes` |
| Resource | `Resource` |
| State ★ | `SystemState` |
| Event ★ | `EventType` + `EventNotice` |
| Queue | `EntityQueue` |
| Queue discipline | `QueueDiscipline` enum |
| Clock ★ | `Clock` |
| Future Event List ★ | `FutureEventList` |
| Event notice | `EventNotice` |
| Activity | `Activity` |
| Delay | `Delay` |
| Statistical accumulators ★ | `Statistics` |
| Termination condition | `TerminationCondition` |
| Initialization | `SimulationSystem::initialise()` |

## Recurring design rules (they show up in nearly every file)

- **Derive, never duplicate.** `unitsAvailable()`, `endTime()`, every average —
  computed on demand. A second stored copy of a fact will eventually disagree
  with the first, and no amount of reading the code will tell you which is right.
- **Header declares, source defines.** One-line getters may stay inline.
- **Include your own header first in each `.cpp`** — it self-tests that the
  header is complete.
- **Forward-declare when you only store a pointer** (`EntityQueue` → `Entity`).
- **Wrap a value in a class only when there is an invariant to protect.** `Clock`
  qualifies (time never goes backwards). A bare counter would not.
- **`unique_ptr` = I own this. Raw pointer = I observe this.** One owner
  (`SimulationSystem`), many observers.

## Two things left deliberately wrong

Both are flagged in the files. Find them yourself in v2 before reading the note:

1. `SystemState` duplicates data already in `Resource` and `EntityQueue` — the
   exact thing rule 1 forbids.
2. `EventNotice` has no tie-break for equal event times, so runs are not
   reproducible.

## Roadmap

| Version | Scope |
|---|---|
| **v1** | These classes, storage only, compiles and prints. |
| **v2** | Fill every stub. RNG (`std::mt19937` + distributions), the `run()` loop, event handlers, statistics. Reproduce a hand-worked M/M/1 table from your notes. Verify with Little's Law. |
| **v3** | Polymorphism where it earned its place: `IEventHandler`, `ITerminationCondition`, `IDistribution`, queue-discipline strategies. Multi-server. Model from a config file. Unit tests. |
| **v4** | Replications, confidence intervals, warm-up removal, trace output. |
