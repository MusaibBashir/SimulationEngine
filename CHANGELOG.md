# Changelog

All notable changes to the DES engine.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

---

## [2.0.0] — 2026-08-22 — "It runs"

The engine simulates. An M/M/1 model runs for 20,000 simulated minutes and the
results satisfy Little's Law to four decimal places.

Full narrative review in `V2_READLOG.md`.

### Added

- **`RandomStream`** (new) — one `std::mt19937`, one seed, one place. Every draw
  in the program comes through it, so the same seed reproduces the same run.
  `exponential()` is parameterised by **mean**, not rate; the 1/λ conversion
  happens in exactly one line.
- `SimulationSystem::refreshState()` — recomputes `SystemState` from the
  authoritative `Resource` and `EntityQueue` after every state change.
- `SimulationSystem::setModel()`, `setMeanInterarrival()`, `setMeanService()`,
  and a seed parameter on the constructor.
- `m_activeDelays` (`std::map<EntityId, Delay>`) — tracks the `Delay` each
  waiting entity is inside, which is where waiting time now comes from.
- `EntityQueue::setRandomStream()` — non-owning injection for the Random
  discipline.
- `main` now runs a real M/M/1 model, checks FEL tie-breaking, and validates the
  output against Little's Law and closed-form M/M/1 theory.

### Fixed

- **All statistics would have been zero.** `recordArrival`, `recordDeparture`
  and `updateTimeIntegrals` were empty stubs while the four derived getters that
  divide them were fully implemented. Consumers written before producers.
- **Entities were born at the wrong time.** The next entity was created at the
  current clock and attached to a future Arrival, so `creationTime` preceded
  actual arrival and every time-in-system was inflated by an interarrival gap.
  Restructured: Arrival events carry no entity, and `handleArrival` creates it —
  the timestamp is now correct by construction.
- **Waiting time was hardcoded `0.0`.** The `Delay` was never created, ended, or
  read. Now tracked in `m_activeDelays`, closed on service start, and stored as
  the entity's `waitTime` attribute.
- **The Random discipline silently lost entities** by returning `nullptr` from a
  non-empty queue. Now asserts and draws a uniform index. `nullptr` from `pop()`
  means "empty" and nothing else.
- **`report()` recomputed averages** that `Statistics` already provides, guards
  included. Now delegates.
- **`SystemState` was never updated** — the duplication flagged as deliberately
  wrong in v1, hit exactly as predicted. Resolved as option (b), snapshot.
- **`EndSimulation` could be scheduled at infinity.** Guarded with `std::isinf`.
- Seven compile errors: `EntityQueue::size` (is `length`), missing
  `Activity.hpp` include, undeclared `interarrival` / `serviceDraw`, and
  `report() const` calling non-const `queue()` / `resource()`.

### Changed

- **Priority / SPT / EDD collapsed** from three copies of one scan loop into a
  single `extractBest(deque&, attribute, wantLargest)` helper in an anonymous
  namespace. Deliberately not the full strategy-object abstraction — that is v3
  step 2.
- **The engine no longer hardcodes `"Teller"`.** Names are configurable and
  resolved once in `initialise()` into cached `m_server` / `m_line` pointers,
  instead of a linear string search on every event.
- `README.md` rewritten: build instructions, run-loop walkthrough, design rules,
  and a nine-step ordered plan for v3.

### Verified

- Clean build under `-Wall -Wextra -Wpedantic`.
- FEL ordering (3, 7, 10) and tie-breaking (Arr, Dep, Arr at t=5.0) both pass.
- 20,000 min at ρ=0.8: utilisation 0.8022 (theory 0.8000);
  Little's Law relative error 0.0000.

### Known / deliberate

- `Wq` measures 3.2864 against theory 3.2000. **Not a bug** — no warm-up removal
  and a single replication. v4 answers this properly with confidence intervals;
  do not tune constants to close the gap.
- `m_entities` grows unboundedly (20,182 `unique_ptr`s for this run).
- `EventNotice::s_nextSequenceNumber` is a mutable static: not thread-safe, not
  reset between replications.
- `operator>` uses `==` on `SimTime` for the tie-break. Correct here, but exact
  FP equality is fragile in general.

---

## [1.0.0] — 2026-08-22 — "It stores, and does nothing else"

First working skeleton. Every noun from the simulation theory table exists as a
class that holds data. **Nothing simulates yet, and that is the definition of
done for this version.** Method stubs are declared so the interfaces are frozen;
filling the bodies is v2.

### Added

**Core value types**

- `Common.hpp` — `SimTime`, `EntityId` aliases; scoped enums `EventType`,
  `QueueDiscipline`, `ResourceState`. Depends on nothing; sits at the bottom of
  the dependency graph.
- `Entity` — id, creation time, and a `std::map<std::string,double>` attribute
  bag. No `setId()`: identity is fixed at birth.
- `Resource` — name, capacity, units busy. Invariant `0 <= busy <= capacity`
  documented at the member. `unitsAvailable()` is derived, never stored.
- `EntityQueue` — `std::deque<Entity*>` plus a discipline. Deque because FIFO
  pops the front and LIFO pops the back. Raw pointers because the queue observes
  entities, it does not own them.
- `EventNotice` — (type, time, entity, resource). Immutable: no setters, because
  mutating an element already inside a heap breaks the heap invariant silently.
- `Activity` (duration known at construction) and `Delay` (duration unknowable
  at construction, `end()` called later by the system). The theory distinction
  is expressed as two different constructor signatures rather than a flag.

**Engine parts**

- `Clock` — wraps one `SimTime` so that `advanceTo()` is the only way to change
  it, making backwards time travel impossible by construction rather than by
  everyone remembering.
- `FutureEventList` — wraps
  `std::priority_queue<EventNotice, vector, std::greater>`. `std::greater` plus
  `EventNotice::operator>` turns the default max-heap into a min-heap, so
  `popImminent()` returns the earliest event.
- `SystemState` — number in system, number in queue, server status.
- `Statistics` — counters and two time integrals (∫L dt, ∫B dt) with
  `m_lastUpdateTime` to measure the rectangles. Averages are computed on demand,
  never stored.
- `TerminationCondition` — max time OR max entities, using
  `numeric_limits::infinity()` / `INT_MAX` as sentinels so the check stays a
  single expression.
- `SimulationSystem` — owns every object via `vector<unique_ptr<T>>` and hands
  out raw observing pointers. Factory methods `createEntity`, `addResource`,
  `addQueue`. Copy operations `= delete`d, so an accidental copy of the whole
  simulation is a compile error rather than a runtime mystery.

**Build & project**

- `CMakeLists.txt` — C++17, sources listed explicitly (no `file(GLOB)`),
  `-Wall -Wextra -Wpedantic` on GCC/Clang and `/W4` on MSVC.
- `main.cpp` — v1 acceptance test: builds one of each object, prints their
  state, and schedules three events at t = 10, 3, 7 to confirm the FEL returns
  them as 3, 7, 10.
- `README.md` — write order, theory-table-to-file map, recurring design rules.
- `.gitignore`, `CHANGELOG.md`.

### Fixed

- **`Resource` constructor moved nothing.** It took `const std::string name` and
  called `std::move(name)`. `std::move` on a `const` object yields a
  `const std::string&&`, which cannot bind to the move constructor — overload
  resolution silently falls back to the **copy** constructor. No warning, no
  error, no move. Dropped the `const` so the sink-parameter idiom actually
  works. *Rule: `std::move` on anything `const` is a no-op.*
- **`SystemState::reset()` re-listed every zero value** already given by the
  in-class initialisers. Replaced with `*this = SystemState{};` so the header is
  the single source of truth for the t = 0 state. Matches `Statistics::reset()`.
- **`SimulationSystem` initialised `m_nextEntityId` twice** — once in-class,
  once again in the constructor's init list. Removed the duplicate.
- **Unused `#include <algorithm>`** in `SimulationSystem.cpp` — the lookups are
  plain range-for loops. Removed.
- **`-Wunused-parameter` warnings** on every stub. Parameter names are now
  commented out (`int /*units*/`) rather than deleted, so the signature still
  documents itself and the warning is gone.

### Changed

- `EntityQueue.hpp` forward-declares `class Entity;` instead of including
  `Entity.hpp`. It only stores `Entity*`, so the full definition is not needed;
  the cost moves into `EntityQueue.cpp` in v2 when `pop()` reads attributes.
- `SimulationSystem::handleArrival` / `handleDeparture` moved from `public` to
  `private`. They are internal steps of `run()`, not interface — nothing outside
  the class should be able to fire a departure by hand.
- Every stub body now carries a specific `// TODO v2` describing the lines to
  write, in place of a bare `// TODO v2`.

### Verified

- Compiles clean under `g++ -std=c++17 -Wall -Wextra -Wpedantic`: zero warnings.
- `main` runs and prints the expected values.
- FEL ordering test outputs `3, 7, 10` — the min-heap comparator is correct.

### Known / deliberate

- `SystemState` duplicates data held by `Resource` and `EntityQueue`. Left in on
  purpose, flagged in the header. Decide in v2 whether it becomes a computed
  view or a logging-only snapshot.
- `EventNotice` has no tie-break for equal event times, so runs are not yet
  reproducible. Add a sequence number in v2.
- `SimulationSystem::resource()` / `queue()` are linear scans. Fine for five
  resources; add a `std::map` index when the profile says so, not before.
- Deleting the copy operations also suppresses the implicit move constructor, so
  `SimulationSystem` is neither copyable nor movable. Intended for now.

---

## [Unreleased] — v3, "abstraction, earned"

Ordered plan with rationale in `README.md`. Summary:

1. `IDistribution` — exponential/uniform/triangular/constant behind an interface
2. Queue disciplines become strategy objects
3. Multi-server, then multi-resource
4. A `Model` object separate from the engine  ← the real architecture step
5. `IEventHandler` — polymorphic event dispatch
6. `ITerminationCondition`
7. Config file input (and the assert-vs-validation distinction)
8. `Trace` output
9. Unit tests — deliberately last, once the interfaces stop moving

Then v4: replications, warm-up removal, confidence intervals, entity lifetime.
