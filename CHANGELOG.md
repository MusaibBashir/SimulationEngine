# Changelog

All notable changes to the DES engine.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

---

## [8.0.0] — 2026-08-22 — "Where the numbers come from"

Random number generation, from the bit source up. Narrative in `V8_READLOG.md`.

### Added

- **`u01()` as the single primitive.** Every variate is now one uniform pushed
  through an inverse CDF. Not tidiness: common random numbers need streams that
  line up, and antithetic variates need exactly one uniform per variate,
  monotonically transformed. `std::exponential_distribution` is opaque about how
  many uniforms it consumes, so it could not be paired with anything.
- **Pluggable engines**: `EngineKind::MersenneTwister` (default),
  `LinearCongruential`, and **`Randu`** — IBM's famously broken generator,
  included so the tests can catch it.
- **`StreamTests`**: chi-square uniformity, Kolmogorov–Smirnov, runs up-and-down,
  autocorrelation at lag k, and a **serial test in 3D**. RANDU passes every
  one-dimensional test and fails the 3-D one by a factor approaching twenty, with
  257 of 4096 cells never visited.
- **Seven distributions**: `Normal` (truncated at zero, or throwing — a negative
  duration is nonsense, not a subtlety), `Lognormal` (+ `lognormalFrom(mean, sd)`,
  since the parameters are the mean and sd of the *log*), `Weibull`, `Erlang`,
  `Discrete`, `Empirical`, `Poisson`.
- **Independent substreams.** `useSeparateStreams()` gives each distribution a
  stream named for its role, so changing the service time no longer shifts the
  arrival pattern.
- **Antithetic variates**: `Experiment::antitheticPairs()`. Each replication runs
  twice, mirrored, averaged into **one** observation. 24% narrower interval for
  identical compute.
- **Common random numbers**: `Experiment::compare(a, b, field)` pairs replication
  by replication and forms an interval on the differences. In example 14 the two
  designs' own intervals overlap while the paired difference is **25× tighter**
  and clearly non-zero.

### Fixed

- **`Empirical::mean()` reported the average of the observations**, but `draw()`
  interpolates between order statistics, so what it samples has the trapezoidal
  mean. Since that number feeds the stability check it must be the mean of what
  is drawn (4.786) rather than of what was measured (4.796). Caught by example 13
  printing both.
- `serial3D` defaulted to 8 bins per axis, coarse enough that RANDU scraped a
  pass. Now 16.

### Changed

- **Random-driven examples print different numbers than in v7**, because the
  generators changed. The deterministic ones (example 02's hand-worked table) are
  bit-identical. This is the one version where "every example reproduces its
  previous numbers" could not be the acceptance criterion.

### Deliberately incomplete

- Only arrivals, arrival attributes and Process service times get their own
  stream; Delay and Decide draws share the common one.
- No general Gamma: a non-integer shape needs acceptance–rejection, which
  consumes an unpredictable number of uniforms per variate and destroys the
  property everything above relies on. Erlang covers integer shapes.

### Verified

- **260/260 checks** (215 in v7); clean under `-Wall -Wextra -Wpedantic` and
  `-fsanitize=address,undefined`.

---

## [7.0.0] — 2026-08-22 — "The servers stop belonging to one block"

### Added

- **Shared resources.** `Model::resource(name, capacity)` declares them and
  `stationUsing(block, resource, discipline, service, units)` seizes them.
  Several Process blocks can share one pool — "two nurses covering both triage
  and the vaccination room" was inexpressible before. When a unit frees, it goes
  to the block holding the **longest-waiting** entity (global FCFS); the rule is
  documented in the header because the alternative (fixed block priority) is
  equally defensible and gives different answers.
- **Balking**: `balkAt(process, queueLength, target)` — refuse to join a queue
  already that long.
- **Reneging**: `renegeAfter(process, patience, target)` — join, wait, give up.
  Implemented by **lazy cancellation**: the timer fires regardless and the block
  asks whether the entity is still queued. A binary heap cannot remove an
  arbitrary element, which `FutureEventList.hpp` has said since v1 — and it
  turns out that file did not have to change.
- **N-way Decide**: `decideNWayByChance/ByCondition` plus `branch()`. One draw
  walked against a cumulative probability; leftover probability falls through to
  `route()`; conditions are evaluated in order, first match wins. All-chance or
  all-condition, never mixed — refused rather than guessed at.
- `IResourceUser`, so a shared resource can ask its candidates who has waited
  longest rather than guess.

### Changed

- `EntityId` is `long long` (was `int`). One line, because it was an alias from
  v1 — which is the whole argument for those aliases.
- `validate()` sums offered load across every block sharing a resource: two
  blocks at ρ = 0.6 each are fine alone and impossible together.
- `Model` owns the resources; `Station` holds a non-owning pointer, and the Model
  resets them exactly once.

### Fixed

- **Per-station utilisation double-counted a shared resource.** Each block fed
  `resource().unitsBusy()` into its statistics — this block's own usage for a
  private resource, and the *total across every user* for a shared one, so two
  blocks sharing an operator each reported the whole pool and the two summed to
  twice the truth. A block now measures `unitsHeld()`. Caught by the very first
  test written against shared resources.

### Verified

- **215/215 checks** (189 in v6); clean under `-Wall -Wextra -Wpedantic` and
  `-fsanitize=address,undefined`; every v1–v6 example reproduces its numbers.
- Three of the new balking/reneging tests failed first and the *tests* were
  wrong: they used constant arrivals and constant service, a D/D/1 queue where
  no queue ever forms, so nobody could balk. Balking and reneging only mean
  anything where a queue fluctuates.

---

## [6.0.0] — 2026-08-22 — "A model is a flowchart"

A model is now a graph of blocks in the spirit of Arena's Basic Process template,
not a chain of stations. Narrative in `V6_READLOG.md`.

### Added

- **Blocks**: `Process` (the old Station: seize-delay-release), `Delay` (time
  without a resource), `Assign`, `Decide` (by chance or by condition), `Batch`
  (permanent or temporary), `Separate` (split a batch, or duplicate), `Record`
  (count / attribute / time in system), `Dispose`.
- **`INode` and `NodeContext`.** After five versions of declining to build an
  event-handler hierarchy, v6 builds it — because blocks must live outside the
  engine and `Batch` is exactly the "handler that carries state" named as the
  trigger. The resolution was *not* to widen `SimulationSystem`'s public
  interface: `NodeContext` is a narrow facade with the six operations a block may
  perform and nothing else.
- **Visit ratios.** `Model::visitRatios()` walks the flowchart carrying a weight
  — chance branches split it, a batch of *n* divides it by *n*, a duplicate of
  *k* multiplies it by *k+1*. The ρ check uses them, so a station behind a 10%
  branch is no longer judged as if it saw every entity. Where the split depends
  on entity state (a condition-based Decide) the result is flagged inexact and
  the check relaxes rather than inventing a number.
- `Entity` batching support: `addMember`, `members`, `isBatch`,
  `copyAttributesFrom`, and `setCreationTime` (used only so a batch inherits its
  oldest member's birth time).
- `Model::nodeAs<T>(name)`, `route`/`routeTrue`, and per-block counters.
- Example 11: the full block set as a manufacturing line, plus a second model
  showing temporary batching, `Separate`, and a condition-based Decide.

### Changed

- **The engine no longer knows how service works.** `admit()` and
  `startNextService()` moved into `Station`; the engine advances the clock, hands
  entities to blocks, and keeps statistics. The `m_activeDelays` map moved to the
  queue the entity is waiting in.
- `EventNotice` names an `INode*`, not a `Station*`.
- `Model::addStation` is now `Model::station` / `process` and returns `Model&`.

### Fixed

- **Block counters ignored the warm-up** while `Statistics` respected it — two
  numbers in one report measuring different periods. Added
  `INode::resetStatistics(now)`, called for every block at warm-up. Caught by
  example 11 reporting more parts inspected than entities arrived.
- **`typeinfo for des::INode` undefined at link time.** Every virtual was inline
  in the header, so the class had no key function and nothing emitted its vtable
  or typeinfo, which `dynamic_cast` needs. `src/Node.cpp` exists to define one
  virtual out of line.

### Verified

- **189/189 checks** (160 in v5); clean under `-Wall -Wextra -Wpedantic` and
  `-fsanitize=address,undefined`.
- All v1–v5 tests pass unchanged and every existing example reproduces its v5
  numbers.

---

## [5.0.0] — 2026-08-22 — "The same engine, easier to hold"

**No simulation logic changed.** Every example produces byte-identical output to
v4.1 and every v1–v4 test passes untouched. v5 is entirely about what the person
*using* the engine has to type, read and remember. Narrative in `V5_READLOG.md`.

### Added

- **`namespace des`** around the whole engine, and **`des.hpp`** as an umbrella
  header. The engine had 47 global names including `Model`, `Entity`, `Clock`,
  `Uniform`, `Constant`, `Summary` and `Trace` — every one a name user code might
  want.
- **Factory functions**: `exponential(m)`, `constant(v)`, `uniform(a,b)`,
  `triangular(a,m,b)`, `fixedTimes({...})`, `timeLimit(t)`, `entityLimit(n)`,
  `whenDrained()`, variadic `anyOf(...)`. The ten examples contained 53
  `std::make_unique` calls; they now contain none.
- **`attr::priority` / `attr::serviceTime` / `attr::dueDate`** — the disciplines
  look attributes up by string, and a typo silently turns SPT into FIFO. That
  exact failure shipped in v3 and survived to v4.1.
- **`RunResults` / `StationResults`** and `sim.results()` — every number with the
  division by the *measured* period already done. `report()` now prints
  `results()` rather than recomputing, so one place knows how each number is
  derived. `results().station("nope")` throws rather than returning a zeroed
  struct that reads as a perfectly idle server.
- **`des::ModelError`** — user mistakes are now exceptions, checked in every
  build. `assert` compiles to nothing under `-DNDEBUG`, so `connect("A","Typo")`
  would previously have done nothing at all in a release build. Asserts remain
  for engine invariants a user cannot trigger.
- **The stability check.** `Model::offeredLoad()` plus a pure-virtual
  `IDistribution::mean()`. Every version of the docs said "compute ρ by hand
  before trusting anything"; the engine now computes it and refuses to run an
  unstable model, with a message saying what to change. It follows the
  service-time attribute too, so job shops — the models most likely to be
  accidentally unstable — are checked rather than skipped.
- **Chaining and shortcuts**: `Model::arrivals/station/route/entryAt/attribute`
  return `Model&`; `SimulationSystem::stopAt/stopAfter/stopWhen/warmUpFor/traceTo`
  return `SimulationSystem&`; `execute()` is `initialise()` then `run()`.
- **`Experiment::waits() / timesInSystem() / queueLengths() / utilisations()`**
  and `Experiment::estimate(xs)` returning `{mean, halfWidth}` with `low()`,
  `high()` and `covers(v)` — the mean and its half-width are always wanted
  together, and separating them invites quoting the mean alone.

### Changed

- Examples and tests rewritten against the new API; example 01 is now eight lines
  and example 10 gains a section on catching `ModelError`.
- Older names (`setInterarrival`, `addStation`, `connect`, `setEntry`,
  `assignOnArrival`, `initialise`/`run`) all still work.

### Deliberately not done

- A full fluent builder (`station("A").capacity(3).fifo()`) — the chained `Model`
  methods get most of the readability for a fraction of the API surface.
- Renaming `SimulationSystem` to `Simulation` — it would break every existing
  line for a shorter name, and the namespace already removed the collision risk.
- `IEventHandler`, for the fifth time. Same reasoning; the trigger is still
  handlers that carry state.

### Verified

- **160/160 checks** (128 in v4.1). The 32 new ones cover every `ModelError`
  path, distribution means, `RunResults` dividing by the measured period, the
  build helpers, variadic `anyOf`, and `Estimate`.
- All ten examples produce identical output to v4.1 — the interface changed, the
  simulation did not.

---

## [4.1.0] — 2026-08-22 — "A folder your friend can learn from"

An `examples/` folder of ten runnable programs covering every feature, plus two
engine additions they needed and one real bug they exposed.

### Added

- **`examples/`** — ten self-contained programs and a `README.md` written for
  someone who knows C++ and has never seen this codebase: build instructions, a
  full API reference, the reserved attribute names, a list of mistakes to avoid
  and a coursework checklist. Every example is built by CMake into
  `build/examples/`.
- **`Model::assignOnArrival(name, distribution)`** — gives every arriving entity
  an attribute drawn from its own distribution. **Without this the Priority, SPT
  and EDD disciplines did nothing**: every entity read `0.0`, every comparison
  tied, and all three silently behaved as FIFO. Half the shipped queue
  disciplines were unusable and no test caught it, because every test used FIFO.
- **`Station::setServiceFromAttribute(name)`** — service duration read from the
  entity's attribute instead of drawn. Job-shop models need it: SPT must sequence
  on the number that is actually used, not an unrelated estimate.

### Fixed

- **`Experiment::suggestWarmUp()` was broken.** The tolerance-band heuristic
  returned about two thirds of the run length regardless of the run length —
  13230 for a 20000-minute run, 26660 for a 40000-minute one. On a noisy series
  some late point always falls outside the band, so the answer collapsed to the
  search cap. Replaced with **MSER**: pick the truncation minimising the standard
  error of the remaining mean, which trades bias against discarded data instead
  of guessing. Caught only because an example printed it at two run lengths.
- **The v4 headline demo was passing on luck.** `main` and the "theory inside the
  interval" test used 10 replications, where the half-width is marginal enough
  that coverage depends on the seed. Both now use 20. At 20 replications theory
  is inside at every run length tried (20000 / 50000 / 200000), with or without
  a warm-up — so the v4 claim stands, but it now stands for a reason.

### Changed

- Example 03's stated conclusion was **backwards** and the data said so. For
  fixed total capacity, more slower servers gives *lower* waiting time (Wq 3.20
  → 2.84 → 2.59) but *higher* time in system (W 4.00 → 4.44 → 4.99), matching
  Erlang-C. The example now shows both columns and makes the disagreement the
  lesson: which arrangement is "better" depends on the metric you chose.
- Example 07 rebuilt around a ρ = 0.9 queue. The original used ρ = 0.8, whose
  transient turns out to last only tens of minutes — so it could not demonstrate
  warm-up removal at all. The example now also shows that warm-up removal *widens*
  the confidence interval, and that a short run stays biased no matter how you
  truncate it.
- `CMakeLists.txt` builds every example. This is the one place `file(GLOB)` is
  used, and the file says why.

### Verified

- Clean under `-Wall -Wextra -Wpedantic`; `ctest` passes; **128/128 checks**.
- All ten examples build and run, and each one's stated conclusion was checked
  against its actual output — which is how three of the errors above were found.

---

## [4.0.0] — 2026-08-22 — "How confident are we?"

Since v2 every version has reported `Wq = 3.2864` against a theoretical 3.2000
and written "not a bug — that's v4". This is v4, and the answer is that **the
simulator was never wrong; the reporting was.**

With warm-up removal and 10 replications, M/M/1 theory falls inside the 95%
confidence interval for **all five** reported quantities. There is a test
asserting it. Full narrative in `V4_READLOG.md`.

### Added

- **Warm-up removal.** `SimulationSystem::setWarmUp(t)` schedules a `WarmUpEnd`
  event that discards every statistic collected so far while leaving the system
  state untouched — so measurement begins from a realistically loaded system
  instead of an empty one. `Statistics::restartAt(now)` is `reset()` plus
  `m_lastUpdateTime = now`, and that one line is the whole of the method.
- **`measuredTime()`.** Every time-average now divides by the measured period,
  not `clock.now()`. Using the clock would understate each average by exactly the
  fraction of the run discarded.
- **Welch's method.** `EventType::Observe` samples number-in-system on a fixed
  time grid (grids, not events, because replications must line up index by
  index). `Experiment::welchAverages()` averages across replications and then
  smooths; `suggestWarmUp()` returns a heuristic starting point;
  `writeWelchSeries()` dumps CSV to plot, because the method is properly finished
  by eye.
- **`Experiment`** — runs N replications from one base seed, collects a
  `ReplicationResult` each, and reports mean ± 95% interval.
- **`Summary`** — sample mean, sample standard deviation (n−1, not n), standard
  error, and a Student-t interval with a 30-entry critical-value table. At n=10
  the t value is 2.262; using 1.96 would make every interval 13% too narrow.

### Changed

- **`Statistics` members renamed.** `m_areaUnderQueueLength` /
  `m_areaUnderServerBusy` were accurate for a station and a lie for the
  system-level object. The class now integrates two series A and B, and the
  caller labels them at construction; the labels appear in report headings.
- **The `EventNotice` sequence-number static is gone.** The counter lives in
  `FutureEventList`, which stamps it in `schedule()` via a private setter reached
  by a `friend` declaration — narrowing that power to the one class that needs it
  rather than opening a setter to everyone. It rewinds with `clear()`, and two
  simulations in one process can no longer interfere.
- `EntityQueue::resetStatistics()` — clears the observed maximum but keeps the
  waiting entities, which is what warm-up removal requires.
- `EventType` gains `WarmUpEnd` and `Observe`.

### Deliberately not built

- **`IEventHandler`, for the fourth time.** The `run()` switch grew from four
  cases to six, and the answer is still no — for a reason v4 sharpened: handler
  objects outside `SimulationSystem` would need `admit()`, `startNextService()`,
  `createEntity()` and `refreshState()` made public, or five friend declarations.
  Widening the public interface to satisfy an abstraction is a worse trade than a
  switch that fits on a screen. What would change the answer: handlers that carry
  **state** (pre-emption, balking, reneging). A switch cannot hold state.
- **Config file input.** Still I/O plumbing rather than design.

### Verified

- Clean under `-Wall -Wextra -Wpedantic` and `-fsanitize=address,undefined`.
- **124/124 checks pass** (85 in v3).
- Wq [2.9728, 3.2112] contains 3.2000; W [3.7695, 4.0121] contains 4.0000;
  Lq [2.9611, 3.2203] contains 3.2000; L [3.7542, 4.0236] contains 4.0000;
  ρ [0.7916, 0.8048] contains 0.8000.
- The Welch series starts at 2.15 and settles near 4.0 — the theoretical L.
- The deterministic hand-worked run is unchanged: 5 served, waits 0/1/0/3/1,
  average exactly 1.0000, last exit t=13.

---

## [3.0.0] — 2026-08-22 — "Abstraction, earned"

The engine no longer knows what it is simulating. `SimulationSystem.cpp` contains
no station name, no capacity and no distribution — it owns a `Model` and runs it.
A three-stage restaurant and a single-server queue are the same code path.

Full narrative in `V3_READLOG.md`.

### Added

- **`IDistribution`** + `Exponential`, `Constant`, `Uniform`, `Triangular`,
  `Deterministic`. `Deterministic` is what makes a run hand-checkable.
- **`IQueueRule`** + `FifoRule`, `LifoRule`, `HighestAttributeRule`,
  `LowestAttributeRule`, `RandomRule`. Five classes for six disciplines: SPT and
  EDD differ by a string, not a class. `makeQueueRule()` bridges the old enum.
- **`Station`** — a `Resource`, a queue, a service distribution, and where
  entities go next. The last field is what turns a queue into a network.
- **`Model`** — owns the stations, the interarrival distribution and the entry
  point. `validate()` catches routing loops before the run instead of hanging
  during it.
- **`ITerminationRule`** + `TimeLimit`, `EntityLimit`, `DrainedRule`, `AnyOf`.
  Rules receive the whole `SimulationSystem`, so a rule can query anything.
- **`Trace`** — markdown or plain-text event log to file. `TraceLevel::Off`
  opens no file and returns immediately, so tracing is free when disabled; a
  failed open degrades to `Off` rather than taking the simulation down.
- **Routing**: `SimulationSystem::admit(Entity*, Station*)`, used by both arrival
  and station-to-station transfer. One function for both paths is what makes
  chains work.
- **Per-station statistics** — utilisation and queue length are station
  properties. The restaurant run shows Waiters at 82% and Host at 13%; one
  system-wide number would hide the bottleneck.
- **`tests/tests.cpp`** — 85 checks, no external framework, plus a `ctest`
  target. Includes two end-to-end tests compared against hand-worked numbers,
  not tolerances.
- `main.cpp` now runs five scenarios: M/M/1, M/M/3, a deterministic
  hand-checkable run with a trace, a three-station restaurant, and replication
  reproducibility.

### Changed / breaking

- `TerminationCondition` **deleted**; `SimulationSystem`'s constructor takes only
  a seed and `setTermination()` takes an `ITerminationRule`.
- `EventNotice` names a `Station*`, not a `Resource*`.
- `SimulationSystem::addResource` / `addQueue` / `setModel` /
  `setMeanInterarrival` / `setMeanService` removed — build a `Model`.
- `EntityQueue` holds a `unique_ptr<IQueueRule>` and is no longer copyable.

### Deliberately not built

- **Step 5, `IEventHandler`.** The `run()` switch is four cases on one screen.
  Virtual dispatch would add a hierarchy and an ownership question and *lose* the
  `-Wswitch` warning that flags a new `EventType`. Same judgement v1 made about
  termination rules: wait until the third case hurts.
- **Step 7, config file input.** I/O plumbing rather than design, and it wants
  the `Model` API to settle first. The interesting part — that `assert` is the
  wrong tool for external data, since asserts vanish under `NDEBUG` — is a
  paragraph, not a subsystem.

### Verified

- Clean under `-Wall -Wextra -Wpedantic` and under `-fsanitize=address,undefined`.
- **85/85 unit checks pass.**
- Deterministic run reproduces the hand-worked table exactly: 5 served, waits
  0/1/0/3/1, average 1.0000, last exit t=13.0000 — and the trace file matches
  event for event, including the FEL tie-break at t=10.
- M/M/1: Little's Law relative error 0.0000, utilisation 0.8022 (theory 0.8000).
- M/M/3: utilisation 0.6651 (Erlang-C 0.6667).
- Two replications with the same seed produce identical output.

---

## [2.1.0] — 2026-08-22 — "It runs twice"

Defect pass over v2. No new features; these are all things that should have been
right in v2. Every fix below is a bug that compiled, ran, and produced
plausible-looking output.

### Fixed

- **A second replication served ZERO entities.** `initialise()` reset the clock,
  statistics, state, RNG and delays — but not the FEL, not the resources, and
  not the queues. The server stayed seized from the previous run, so with
  capacity 1 it was busy forever: run two queued 2,037 entities and served none,
  while printing a report that looked like a simulation.
  Added `Resource::reset()`, `EntityQueue::reset()`, `FutureEventList::clear()`
  and `EventNotice::resetSequenceCounter()`, and `initialise()` now calls all of
  them plus clearing entities and resetting the id counter.
  *Rule: every object holding run state needs a `reset()`, and `initialise()`
  must call all of them. Configuration survives; run state does not.*
- **Entities were never destroyed** — 20,182 live `Entity` objects for a
  20,000-minute run. `m_entities` is now
  `std::unordered_map<EntityId, unique_ptr<Entity>>` and `handleDeparture`
  destroys the entity once it has left. Live count during a run is now 1.
  The safety invariant (nothing may hold a raw pointer to a destroyed entity) is
  documented at `destroyEntity` and asserted in debug builds.
- **A missing `waitTime` attribute would have read as `0.0`.**
  `Entity::attribute()` returns 0.0 for an absent key, so any path that forgot
  to set it would silently skew every wait average. `handleDeparture` now
  asserts the attribute exists before reading it.
- **A termination condition with no finite limit** would have looped until the
  machine gave up. `initialise()` now asserts at least one limit is finite.

### Changed

- `EventNotice::operator>` documented as intentionally using exact `SimTime`
  equality to detect ties, with the caveat that FP equality is fragile in
  general.
- Stale `// TODO v2` blocks removed from function bodies now that the code is
  written; the design rationale comments stay.
- `report()` prints `live entity objects`, so the memory leak cannot come back
  unnoticed.
- `main` gains a **replication reproducibility check**: two runs, same seed,
  same process, must produce identical numbers. This guards the entire reset
  path and is the regression test for the headline bug above.

### Verified

- Clean build under `-Wall -Wextra -Wpedantic`.
- Clean under `-fsanitize=address,undefined`: no memory errors, no leaks — which
  is the real proof that destroying entities at departure is safe.
- Two replications with seed 12345 produce identical output (`served 2041,
  avg wait 3.7876` both times).
- M/M/1 results unchanged from v2 — utilisation 0.8022, Little's Law relative
  error 0.0000 — confirming the fixes changed correctness, not the physics.

### Still known / deliberate

- `EventNotice::s_nextSequenceNumber` remains a mutable static. It is now reset
  between replications, but it is still not thread-safe and still gives
  construction a side effect. Removing it means letting `FutureEventList` stamp
  the number, which needs a setter on a class meant to be immutable — a v3
  decision, not a v2.1 patch.
- `Wq` 3.2864 against theory 3.2000: still no warm-up removal, still one
  replication. v4.
- `resource()` / `queue()` remain linear scans, called once in `initialise()`.

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

## [Unreleased] — v9

1. **Resource schedules** — a nurse who goes off shift at 5pm. Needs a capacity
   that varies with time, which interacts with every rho calculation.
2. **Preemption** — a high-priority entity taking a resource mid-service. Lazy
   cancellation does not help: the interrupted entity must requeue carrying its
   remaining service time.
3. **Batch means** — one long run split into batches, as an alternative to
   replications when the warm-up is expensive to repeat.
4. **Config file input** — nothing has blocked it since v5.
5. **Streams for every block**, finishing what v8 started.
6. **Distribution fitting** — take data, suggest which distribution fits, and
   report a goodness-of-fit statistic. `StreamTests` already has the chi-square
   and KS machinery.
