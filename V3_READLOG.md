# v3 Read Log — "abstraction, earned"

What was built, what each abstraction bought, what it cost, and the two steps I
deliberately did **not** build.

Status: builds clean under `-Wall -Wextra -Wpedantic`, clean under
ASan/UBSan, **85/85 unit checks pass**, and a deterministic run reproduces a
hand-worked table event for event.

---

## The headline

The engine no longer knows what it is simulating.

`SimulationSystem.cpp` contains no station name, no capacity, no distribution,
no `"Teller"`. It owns a `Model` and runs it. That sentence is the whole point of
v3 — everything before it was object-oriented programming, this is architecture.

The test of whether it worked is that a restaurant with three stations and a
single-server M/M/1 queue are the same code path:

```
Host(1) → Waiters(3) → Cashier(1)          vs          Server(1)
```

Neither required touching the engine.

---

## Step 1 — `IDistribution`

Five concretes: `Exponential`, `Constant`, `Uniform`, `Triangular`,
`Deterministic`.

**Bought:** service and interarrival times became a property of the model rather
than a hardcoded `m_rng.exponential(mean)` in two handlers. And `Deterministic`
made hand-checking possible, which is worth more than the other four combined.

Three decisions worth defending:

- **`draw()` is not `const`.** `Deterministic` walks a cursor, so drawing mutates
  it. Declaring the method `const` would have forced `mutable` on the cursor —
  a lie told to the compiler. Let the signature admit that sampling mutates.
- **`reset()` is `virtual` with a default no-op, not pure virtual.** Only
  `Deterministic` has anything to do. A pure virtual would force four classes to
  write an empty body.
- **`Constant::draw` consumes nothing from the stream.** If it drew and discarded
  a number, swapping `Exponential` for `Constant` would shift every *other* draw
  in the program and two runs would stop being comparable. Subtle, and it breaks
  variance-reduction experiments later.

And the virtual destructor, which is the reason this step went first: without it,
`delete` through an `IDistribution*` runs only the base destructor and leaks the
derived part, silently. `unique_ptr<IDistribution>` does exactly that delete.
Learn the rule where it is cheap.

## Step 2 — queue disciplines as strategies

`IQueueRule` with `FifoRule`, `LifoRule`, `HighestAttributeRule`,
`LowestAttributeRule`, `RandomRule`.

Note there are **five** classes for six disciplines. Once you say out loud that
Priority, SPT and EDD are all "scan for the extreme value of an attribute", the
difference between SPT and EDD is a *string*, not a class. That collapse is the
real result — the v2 triplication was a symptom of naming the disciplines instead
of naming what they do.

**The interface decision:** `selectIndex()` returns an index, not the entity. The
rule decides *who*; the queue does the removing. A rule that could erase from the
deque could also corrupt `m_maxLengthObserved`, and two things would be
responsible for one invariant.

**What it cost, and this is the actual lesson:** you can no longer `switch` over
the enum and let `-Wswitch` tell you a new discipline is unhandled. The compiler
stops being your checklist. I kept the enum and `makeQueueRule()` precisely so
that *one* file still gets that warning.

## Steps 3 & 4 — `Station`, `Model`, and routing

A `Station` bundles the four things a service point owns: a `Resource`, a queue,
a service distribution, and **where entities go next**. That last field is what
turns a queue into a network.

`Model` owns the stations, the interarrival distribution, and the entry point.

The routing primitive is one function:

```
admit(Entity*, Station*)   // seize if free, else queue and open a Delay
```

Called from `handleArrival` (into the entry station) and from `handleDeparture`
(into the *next* station). **One function for both paths is what makes chains
work at all** — if arrival and transfer had been written separately they would
have drifted within a week.

`Model::validate()` runs before the simulation and walks the route from the entry
looking for a cycle. A routing loop means entities never leave, the run never
drains, and the symptom is a program that simply does not stop with no clue why.
Checking it up front turns a hang into a message.

**Statistics went per-station.** Utilisation and queue length are properties of a
station, not of a system. From the restaurant run:

```
  station        cap   served    avg wait   time-avg Q    util   maxQ
  Host            1      386      0.1824       0.0117  0.1269      2
  Waiters         3      386     20.3620       1.3130  0.8228      9
  Cashier         1      382      0.1625       0.0103  0.1592      2
```

The bottleneck is visible at a glance. One system-wide number would have hidden
exactly the thing you build the model to find.

## Step 6 — `ITerminationRule`

`TimeLimit`, `EntityLimit`, `DrainedRule`, `AnyOf`.

Compare the shapes:

```cpp
// v1
bool isMet(SimTime now, int served) const;          // a hardcoded OR
// v3
virtual bool isMet(const SimulationSystem&) const;  // asks the system
```

**Passing the whole system is worth more than the polymorphism.** A rule can now
look at queue length, utilisation, or a v4 confidence interval without the
signature changing again. `DrainedRule` — stop when the system empties — was
simply not expressible in the v1 two-field struct, which is the concrete evidence
the hierarchy was earned rather than anticipated.

`AnyOf::add` returns `*this` so rules can be chained. Small thing, pleasant API.

## Step 8 — `Trace`

Writes a markdown table, one row per event, to a file.

Two design points:

- **`TraceLevel::Off` opens no file and every method returns immediately.**
  Tracing costs nothing when disabled, so call sites never need to guard.
- **A failed `open()` degrades to `Off` and returns false.** The trace is
  diagnostics, not the product. It must never take the simulation down.

The header row is written lazily on the first event so that a `note()` before
then lands *above* the table rather than splitting it in half.

The deterministic trace, checked against the table I worked by hand:

| t | event | entity | detail |
|---:|---|---:|---|
| 0.0000 | Seize | 1 | server free, service 3.0000 until 3.0000 |
| 2.0000 | Queue | 2 | all 1 busy, queued at position 1 |
| 3.0000 | Exit | 1 | exits; total wait 0.0000, time in system 3.0000 |
| 3.0000 | Seize | 2 | pulled from queue after waiting 1.0000 |
| 10.0000 | Exit | 3 | … |
| 10.0000 | Arrival | 5 | … |

Note t=10: a departure and an arrival at the identical instant, and the departure
comes first because it was scheduled first. That is the sequence-number
tie-break, visible.

## Step 9 — unit tests

85 checks, no external framework. Catch2 and doctest are both fine, but adding a
dependency to a twenty-line harness is a poor trade; swap it when the suite
outgrows this.

Tests are **last in the v3 order on purpose**. Written against the v2 interfaces
they would have been rewritten three times as distributions, rules and the Model
landed. Test the shape once it stops moving.

The two tests that matter most are the end-to-end ones:

- **Deterministic**: 5 served, waits 0/1/0/3/1, average exactly 1.0, last exit at
  exactly t=13 — compared to a table worked by hand, not to a tolerance.
- **Chain routing**: one entity, `Constant(2)` at A and `Constant(3)` at B, time
  in system exactly 5.0. If routing silently dropped the second station this
  would read 2.0.

---

## Deliberately NOT built

### Step 5 — `IEventHandler`

The `run()` switch is four cases and fits on a screen. Replacing it with virtual
dispatch would add a class hierarchy, an ownership question, and a dispatch
table, in exchange for nothing you can point at today — and it would *cost* the
`-Wswitch` warning that currently tells us when a new `EventType` appears.

This is the same judgement v1 made about `ITerminationCondition` and v2 made
about queue rules: **wait until the third case hurts.** Revisit when events reach
eight, or when v4's pre-emption needs handlers that carry state.

### Step 7 — config file input

Skipped for a different reason: it is I/O plumbing, not design. The interesting
part — that `assert` is the *wrong* tool for external data, because asserts
vanish under `NDEBUG` and untrusted input needs checks that survive release
builds — is a paragraph, not a subsystem.

It also wants the `Model` API to settle first. Writing a parser against an
interface that is one version old means rewriting the parser.

---

## Breaking changes from v2

- `TerminationCondition` **deleted**, replaced by the `ITerminationRule`
  hierarchy. `SimulationSystem`'s constructor now takes only a seed.
- `EventNotice` names a `Station*`, not a `Resource*`. In a network "where did
  this happen" is a station; naming the resource was only ever adequate because
  there was one.
- `SimulationSystem::addResource` / `addQueue` / `setModel` /
  `setMeanInterarrival` / `setMeanService` are gone. Build a `Model` instead.
- `EntityQueue` holds a `unique_ptr<IQueueRule>` rather than an enum, and is
  therefore no longer copyable.

## Known / still open

- The system-level `Statistics` object is fed `(numberInQueue, numberInSystem)`
  rather than `(queueLength, serversBusy)`, so its two integrals mean L_q and L.
  It works, but it is a small abuse of the member *names* — a fair sign that in
  v4 they should become `areaUnderA` / `areaUnderB` with meaning supplied by the
  caller.
- `EventNotice::s_nextSequenceNumber` is still a mutable static. Reset between
  replications, still not thread-safe.
- Wq still measures ~3-5% above closed-form theory in the M/M/1 and M/M/3 runs.
  Still not a bug: no warm-up removal, single replication. **That is v4.**
