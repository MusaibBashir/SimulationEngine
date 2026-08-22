# v5 Read Log — "the same engine, easier to hold"

**No simulation logic changed.** Every number every example printed before v5 it
prints after v5, and the v1–v4 tests pass untouched. v5 is entirely about what
the person *using* the engine has to type, read and remember.

Status: 160/160 checks (128 in v4.1), clean under `-Wall -Wextra -Wpedantic`,
all ten examples produce byte-identical results to v4.1.

---

## The measure of success

Example 01, before:

```cpp
SimulationSystem sim(12345u);
Model& m = sim.model();
m.setInterarrival(std::make_unique<Exponential>(1.0));
m.addStation("Teller", 1, QueueDiscipline::FIFO, std::make_unique<Exponential>(0.8));
m.setEntry("Teller");
sim.setTermination(std::make_unique<TimeLimit>(480.0));
sim.initialise();
sim.run();
sim.report();
```

after:

```cpp
SimulationSystem sim(12345u);
sim.model()
   .arrivals(exponential(1.0))
   .station("Teller", 1, FIFO, exponential(0.8))
   .entryAt("Teller");
sim.stopAt(480.0);
sim.execute().report();
```

The second version is about a queueing system. The first is about `std::unique_ptr`.

---

## 1. `namespace des`

The engine defined 47 global names, including `Model`, `Entity`, `Clock`,
`Uniform`, `Constant`, `Summary` and `Trace`. Every one is a name your own code
might reasonably want, and the first collision would have been a wall of template
errors nowhere near the actual problem.

Also `des.hpp`, so "which header declares `Triangular`?" stops being a question.

Cost: one line of `using namespace des;` per file. Worth it.

## 2. Factory functions instead of `make_unique`

`exponential(0.8)` rather than `std::make_unique<Exponential>(0.8)`. Ten examples
contained 53 `make_unique` calls; they now contain none.

Lower-case, because at the call site they read as description — "service is
`exponential(0.8)`" — while the types stay capitalised. `fixedTimes({2,4,1,3})`
takes an `initializer_list` so nobody writes `std::vector<SimTime>` again.

`anyOf(timeLimit(500), entityLimit(50))` is variadic, replacing three lines of
`add()` calls.

**This is the cheapest kind of API design** — a naming layer over an interface
that was already right — and usually the highest-value.

## 3. `attr::` constants

`PRIORITY`, `SPT` and `EDD` look attributes up **by string**. A typo means
"absent", which reads as `0.0` for every entity, which ties every comparison,
which turns SPT into FIFO without a word of complaint. That exact failure mode
shipped in v3 and survived until v4.1.

`attr::serviceTime` makes a typo a compile error.

## 4. `RunResults` — results as data

Before, reading a station's utilisation was:

```cpp
sim.model().stationAt(0).stats().utilisation(sim.measuredTime(), capacity)
```

which requires knowing that you divide by `measuredTime()` and **not** by the
clock. Get that wrong with a warm-up in play and every time average is silently
understated by the fraction discarded.

Now:

```cpp
sim.results().station("Teller").utilisation
```

with the division already done, in one place, correctly. `report()` was rewritten
to print `results()` rather than recompute anything, so there is exactly one
place that knows how each number is derived.

`r.station("nope")` throws instead of returning a zeroed struct — a zeroed
`StationResults` reads as a perfectly idle server, which is a plausible-looking
wrong answer, and this project has a rule about those.

## 5. `ModelError` instead of `assert`

This is the change with the most substance behind it.

Through v4 every user-facing mistake was an `assert`. **Asserts compile to
nothing under `-DNDEBUG`.** In a release build, `connect("A", "Typo")` would have
done nothing at all and the model would have run happily with entities leaving
after the first station. Wrong answers, no message, no crash.

The distinction, which is worth learning properly:

| | means | who caused it | must survive `NDEBUG`? |
|---|---|---|---|
| `assert` | an invariant the code guarantees | an engine bug | no |
| exception | validation of supplied input | the user | **yes** |

`assert` stays for things like "the queue rule returned an out-of-range index" —
a user cannot trigger that. Everything a user can get wrong now throws.

This is also the thing v3 and v4 both said had to exist before config-file input
would be worth building, and it is why they declined to build it.

## 6. The stability check

`ρ = mean service / (capacity × mean interarrival)`. If ρ ≥ 1, work arrives
faster than it can be done, the queue grows for as long as you run, and every
average is a function of run length rather than a property of the system.

Every version of the documentation said "compute ρ by hand before trusting
anything" — which is exactly the kind of advice people skip. The engine now
computes it and refuses:

```
station 'Desk' is unstable: offered load rho = 1.4 (>= 1). Work arrives faster
than 1 server(s) can do it, so the queue grows without bound and every average
is meaningless. Add capacity, speed up service, or slow arrivals.
```

This needed `IDistribution::mean()` — a pure virtual, so a new distribution
cannot be added without answering the question. `Exponential` returns its
parameter, `Uniform` returns (a+b)/2, `Triangular` (a+m+b)/3, `Deterministic` the
list average.

And it follows the service-time attribute: a job shop where processing times ride
on the entity is checked against the distribution that stamps them. Job shops are
the models *most* likely to be accidentally unstable, so skipping them would have
been precisely backwards.

**Limit worth knowing:** this assumes every entity visits every station on its
route exactly once, which is true of this engine because there is no branching.
Add probabilistic routing and it needs visit ratios.

## 7. Chaining, `execute()`, named columns

`Model` setters return `Model&`; `SimulationSystem` setters return
`SimulationSystem&`. `execute()` is `initialise()` then `run()`.

`Experiment` gained `waits()`, `timesInSystem()`, `queueLengths()`,
`utilisations()` — `column(&ReplicationResult::averageWait)` is precise and also
the sort of thing you look up every time.

`Experiment::estimate(xs)` returns `{mean, halfWidth}` with `low()`, `high()` and
`covers(v)`, because the mean and its half-width are always wanted together and
separating them invites quoting the mean alone.

---

## What was deliberately not done

**A full fluent builder** (`sim.station("A").capacity(3).fifo().service(...)`).
The chained `Model` methods get most of the readability for a fraction of the API
surface, and every extra method is another thing to document and keep working.

**Renaming `SimulationSystem` to `Simulation`.** Tempting, and it would have
broken every line of code anyone had already written against v4 for a shorter
name. Not worth it; the namespace already removed the collision risk.

**`IEventHandler`, for the fifth time.** Unchanged reasoning: handler objects
outside `SimulationSystem` would need four private methods made public. The
trigger is still handlers that carry state.

---

## Verified

- 160/160 checks. The 32 new ones cover every `ModelError` path, distribution
  means, `RunResults` (including that it divides by the measured period),
  the build helpers, variadic `anyOf`, and `Estimate`.
- All ten examples produce **identical output to v4.1** — which is the point:
  v5 changed the interface, not the simulation.
