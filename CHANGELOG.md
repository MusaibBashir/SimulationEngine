# Changelog

All notable changes to the DES engine.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

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

## [Unreleased] — v2, "it runs"

Planned, in this order:

1. `RandomStream` — `std::mt19937` plus exponential/uniform/normal draws, seeded
   explicitly so runs reproduce.
2. `Clock::advanceTo` and the `Statistics` update routines.
3. `EntityQueue::push` / `pop` with all six disciplines written out longhand.
4. `SimulationSystem::initialise()`.
5. The `run()` loop.
6. `handleArrival` / `handleDeparture` last.
7. `report()`, then validate against a hand-worked M/M/1 table and Little's Law.
