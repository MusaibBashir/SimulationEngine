# DES Engine — a discrete-event simulator, built version by version

A single-server queueing simulator written from the theory table up, as a way of
learning OOP and system design rather than as a way of getting a simulator.

**Current state: v2 — it runs.** Builds clean under
`-Wall -Wextra -Wpedantic`, simulates an M/M/1 queue, and passes Little's Law to
four decimal places.

## Build and run

```
cmake -S . -B build
cmake --build build
./build/des
```

Or, without CMake:

```
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude main.cpp src/*.cpp -o des
```

`main` runs four things: the v1 storage check, an FEL ordering check, an FEL
tie-break check, and a 20,000-minute M/M/1 run validated against Little's Law
and the closed-form steady-state result.

## Documents

| File | What it is |
|---|---|
| `README.md` | This file — layout, design rules, v3 plan |
| `CHANGELOG.md` | What changed in each version |
| `V2_READLOG.md` | Detailed review of v2: every bug found and why it mattered |

## Layout

```
sim/
├── CMakeLists.txt      README.md      CHANGELOG.md      V2_READLOG.md
├── main.cpp            acceptance checks + the M/M/1 run
├── include/            14 headers
└── src/                13 sources
```

Headers declare, sources define. One-line getters stay inline in the header.
Every `.cpp` includes its own header first — a free self-test that the header is
complete on its own.

## Theory table → file

| Theory term | Where it lives |
|---|---|
| System | `SimulationSystem` |
| Entity | `Entity` |
| Attribute | `Entity::m_attributes` |
| Resource | `Resource` |
| State ★ | `SystemState` (snapshot, refreshed by `refreshState()`) |
| Event ★ | `EventType` + `EventNotice` |
| Queue | `EntityQueue` |
| Queue discipline | `QueueDiscipline` enum + `EntityQueue::pop()` |
| Clock ★ | `Clock` |
| Future Event List ★ | `FutureEventList` |
| Event notice | `EventNotice` |
| Activity | `Activity` — duration known at construction |
| Delay | `Delay` — duration decided later, by the system |
| Statistical accumulators ★ | `Statistics` |
| Termination condition | `TerminationCondition` |
| Initialization | `SimulationSystem::initialise()` |
| *(not in the table)* | `RandomStream` — the one source of randomness |

## How the simulation actually runs

```
initialise()
  reset clock, stats, state, RNG
  resolve the model (server + queue) ONCE, cache the pointers
  schedule Arrival at t=0            <- carries NO entity
  schedule EndSimulation at maxTime  <- unless maxTime is infinite

run()
  while FEL not empty and not terminated:
      notice = popImminent()                 <- earliest event
      stats.updateTimeIntegrals(...)         <- OLD state, interval just ended
      clock.advanceTo(notice.time())         <- the clock JUMPS
      dispatch on notice.type()

handleArrival
  create the entity HERE (so creationTime is correct)
  record the arrival
  schedule the NEXT arrival            <- forget this and the run stops at one
  server free?  seize, draw an Activity, schedule its Departure
  server busy?  push to the queue, open a Delay

handleDeparture
  release the server
  record the departure (wait time comes from the entity's Delay)
  queue non-empty?  pop per discipline, close its Delay, seize, schedule Departure
```

**The order inside `run()` is not negotiable.** Accumulate the integrals for the
interval that just ended, *then* move the clock, *then* let the handler change
state. Swap any two and every time-average goes quietly wrong with no error.

## Recurring design rules

These show up in nearly every file, and they are most of the point of the
exercise:

- **Derive, never duplicate.** `unitsAvailable()`, `Activity::endTime()`, every
  average — computed on demand. A second stored copy of a fact eventually
  disagrees with the first, and no amount of reading tells you which is right.
- **Wrap a value in a class only when there is an invariant to protect.** `Clock`
  qualifies: time never goes backwards, and `advanceTo` is the only door.
- **`unique_ptr` = I own this. Raw pointer = I observe this.** One owner
  (`SimulationSystem`), many observers.
- **Forward-declare when you only store a pointer.** `EntityQueue.hpp`
  forward-declares `Entity` and `RandomStream`; the includes land in the `.cpp`.
- **Internal steps are `private`.** `handleArrival`, `handleDeparture` and
  `refreshState` are steps of `run()`, not interface.
- **A function that cannot do its job must assert, not return a plausible
  value.** Returning `nullptr` from an unimplemented queue discipline lost
  entities silently for a whole version.
- **One source of randomness.** Same seed, same run, or you cannot debug it.
- **`std::move` on anything `const` is a silent no-op.** It compiles, it warns
  about nothing, and it copies.

## Known and deliberate

- `m_entities` grows for the whole run and is never trimmed. Fine at 20k
  entities, fatal at 10⁸. v4.
- `EventNotice::s_nextSequenceNumber` is a mutable static — works, not
  thread-safe, not reset between replications.
- `Wq` measured 3.2864 against M/M/1 theory 3.2000. Not a bug: no warm-up
  removal and a single replication. v4 addresses both.
- `resource()` / `queue()` are linear scans, now called once in `initialise()`
  rather than per event. Leave them alone.

---

# v3 — the order to do it in

v3 is where abstraction is finally *earned*. Every item below exists because v2
made the problem concrete: you have felt the duplication, so the fix will mean
something. Do them in this order — each step is safe to stop at, and each one
leaves the program working.

**Commit after every numbered step, and re-run `main` every time. Little's Law
must still hold to four decimals after each one. That check is your regression
suite until step 9 gives you a real one.**

### 1. Extract `IDistribution` — the easiest abstraction to get right

`RandomStream::exponential(mean)` is hardcoded into both handlers. Real models
need uniform, normal, triangular, and empirical service times.

- Abstract base `IDistribution` with `virtual SimTime draw(RandomStream&) const = 0`
  and a virtual destructor (**write the virtual destructor — a base class without
  one deletes derived objects wrongly, and it is silent**).
- Concretes: `Exponential`, `Uniform`, `Triangular`, `Constant`.
- `SimulationSystem` holds `std::unique_ptr<IDistribution>` for interarrival and
  service instead of two `SimTime` means.
- Start here because it is small, the interface is obvious, and it teaches the
  virtual-destructor rule before anything depends on it.

### 2. Queue disciplines become strategies

`EntityQueue::pop()`'s switch, and the `extractBest` helper you now have, are the
motivation. You have written the duplication and then collapsed it once — this is
the principled version.

- `IQueueDiscipline` with `virtual std::size_t selectIndex(const std::deque<Entity*>&) const = 0`.
- `Fifo`, `Lifo`, `PriorityRule`, `ShortestProcessingTime`, `EarliestDueDate`,
  `RandomOrder`.
- `EntityQueue` holds a `unique_ptr<IQueueDiscipline>` instead of an enum.
- **Note what this buys and what it costs**: adding a discipline no longer means
  editing `EntityQueue`, but you can no longer `switch` exhaustively and let
  `-Wswitch` remind you. That trade is the actual lesson.

### 3. Multiple servers, then multiple resources

- Let `Resource` capacity > 1 actually work end-to-end (`seize`/`release` already
  support it; the handlers assume one).
- Then more than one resource, each with its own queue.
- This is where `m_server` / `m_line` as single cached pointers finally break,
  which is the point — it motivates step 4.

### 4. A model description, separate from the engine

`setModel("Teller", "TellerQueue")` is a placeholder for this.

- A `Model` object holding resources, queues, distributions, and the routing
  between them.
- `SimulationSystem` takes a `Model` and stops knowing any names at all.
- **This is the real system-design step in the project**: separating *what is
  being simulated* from *the machinery that simulates it*. Everything before it
  is object-oriented programming; this is architecture.

### 5. Events become polymorphic

Only now — the `run()` switch is still readable at four cases, so doing this
earlier would be abstraction for its own sake.

- `IEventHandler` with `virtual void handle(SimulationSystem&, const EventNotice&) = 0`.
- A dispatch table from `EventType` to handler.
- Adding an event type stops meaning "edit `run()`".

### 6. `ITerminationCondition`

The abstract base that `TerminationCondition.hpp` has been flagging since v1.

- `TimeBased`, `CountBased`, `SteadyStateDetected`, `CompositeOr`.
- Do it *after* you have a third real condition, not before.

### 7. Config file input

- Read the model from JSON or a simple key-value file.
- The moment external data enters, `assert` is the **wrong** tool — asserts vanish
  under `NDEBUG`. Validation of untrusted input needs real checks and real error
  reporting. Good place to learn the difference between an invariant and a
  validation.

### 8. `Trace` output

- A `Trace` class that logs every event with a verbosity level.
- Diff two traces to find where a refactor changed behaviour. This becomes your
  main debugging tool for the rest of the project.

### 9. Unit tests

- Catch2 or doctest, single-header, added to `CMakeLists.txt`.
- Test the pieces that are pure functions of their inputs first: `Statistics`
  accumulators against hand-computed rectangles, each queue discipline against a
  known ordering, `FutureEventList` ordering and tie-breaks, `Delay` and
  `Activity` arithmetic.
- **Deliberately last.** Writing tests against the v2 interfaces would mean
  rewriting them through steps 1–5. Test the shape once it stops moving.

### Then v4

Replications, warm-up removal (Welch's method), confidence intervals, and entity
lifetime management. Only then will the 3.2864-against-3.2000 question have an
honest answer.
