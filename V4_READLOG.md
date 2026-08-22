# v4 Read Log — "how confident are we?"

Since v2 every version has reported `Wq = 3.2864` against a theoretical 3.2000
and written "not a bug — that's v4". This is v4, and the answer is:

**the simulator was never wrong. The reporting was.**

Status: builds clean under `-Wall -Wextra -Wpedantic`, clean under ASan/UBSan,
**124/124 unit checks pass**, and M/M/1 theory now falls inside the 95%
confidence interval for all five reported quantities.

---

## The result

```
=== experiment: honest: 10 replications, warm-up removed ===
replications             : 10  (seeds 9000..9009)
warm-up discarded        : 3920.0000
measured period per rep  : 16080.0824

  quantity                    mean      95% half-width      interval
  ---------------------------------------------------------------------
  average wait (Wq)         3.0920  +/-   0.1192   [   2.9728,    3.2112]
  time in system (W)        3.8908  +/-   0.1213   [   3.7695,    4.0121]
  number in queue (Lq)      3.0907  +/-   0.1296   [   2.9611,    3.2203]
  number in system (L)      3.8889  +/-   0.1347   [   3.7542,    4.0236]
  utilisation (rho)         0.7982  +/-   0.0066   [   0.7916,    0.8048]
```

M/M/1 theory: Wq 3.20, W 4.00, Lq 3.20, L 4.00, ρ 0.80. **Every one lands inside
its interval.** There is an automated test asserting exactly this, so it cannot
quietly stop being true.

Nothing about the simulation engine changed to make that happen. What changed is
that a single sample of a random variable stopped being quoted to four decimal
places as though it were an answer.

---

## 1. Warm-up removal

A run starts empty and idle. That is not the steady state, and the early
low-queue observations are averaged in with the rest.

`SimulationSystem::setWarmUp(t)` schedules a `WarmUpEnd` event. When it fires:

```
m_stats.restartAt(now);
for each station: stats().restartAt(now); queue().resetStatistics();
```

**The system state is deliberately untouched.** Entities in service stay in
service; queues keep their contents. That is the entire point — measurement now
begins from a realistically loaded system rather than an empty one.

`Statistics::restartAt(now)` is `reset()` plus one line:

```cpp
m_lastUpdateTime = now;
```

That one line is the whole of warm-up removal. Area accumulated from here on is
divided by `(clock - now)`, not by the clock.

Which forced a second change: every time-average now takes the **measured
period**, not `clock.now()`. Passing the clock would divide post-warm-up area by
total elapsed time and understate every average by exactly the fraction
discarded. `SimulationSystem::measuredTime()` exists so that the distinction is
visible at every call site instead of being assumed.

## 2. Welch's method — finding the warm-up length

Guessing the warm-up period defeats the purpose. Welch's procedure:

1. Run N replications, sampling number-in-system on a **fixed time grid**.
2. Average replication *i*'s observation *k* with replication *j*'s observation
   *k*.
3. Smooth the averaged series with a moving average.
4. Read off where it flattens.

Three implementation points worth the space:

**Sample on a grid, not at events.** Step 2 needs observation *k* to mean the
same time in every replication, and event times never line up. Hence
`EventType::Observe` rescheduling itself at fixed `dt`.

**Average across replications first, smooth second.** The cross-replication
average is what removes the noise; the moving average only makes the remaining
trend readable. Doing it the other way round smooths noise into the trend.

**Trim to the shortest series.** Replications run to the same time limit but
produce slightly different observation counts. Averaging index *k* over a
different number of replications for different *k* would make the tail noisier
for a reason that has nothing to do with the system.

The output (`welch_series.csv`) shows the transient plainly — it starts at 2.15
and settles around 4.0, which is exactly the theoretical *L*:

```
time,smoothed_number_in_system
0.000000,2.150000
5.000000,2.300000
...
2000.000000,4.078049
```

> **CORRECTED IN v4.1.** The tolerance-band heuristic originally shipped here was
> wrong. It returned roughly two thirds of the run length *whatever the run
> length* — 13230 for a 20000-minute run, 26660 for a 40000-minute one — because
> on a noisy series some late point always falls outside the band, so the answer
> collapsed to the cap. It is now **MSER**: choose the truncation that minimises
> the standard error of the remaining mean, trading bias against the data given
> up. See the v4.1 entry in `CHANGELOG.md`.

Welch's method is properly finished by eye on a plot, which is why the CSV is
written; `suggestWarmUp()` is a starting point, not an answer.

## 3. Replications and confidence intervals

`Experiment` takes a builder function, runs N replications with seeds derived
from one base seed, and collects a `ReplicationResult` each.

**Different seed per replication, all derived from one base.** Different, or
every run is the same run and the sample has no variance at all. Derived, so the
entire experiment reproduces from a single number — there is a test for that.

Two statistical decisions that are easy to get wrong and were worth the comments:

**Sample standard deviation divides by n−1, not n.** Dividing by *n* gives the
standard deviation *of the sample*, which systematically underestimates the
population's. At 10 replications that is a ~5% error, and it errs by making your
interval **narrower** than the truth — the one direction you must never err in.

**The t distribution, not 1.96.** At n=10 the critical value is 2.262, not 1.96.
Using the normal would make the interval 13% too narrow. `tCritical95` is a
lookup table rather than a computed inverse CDF: thirty fixed values cover every
sensible replication count, and a table is impossible to get subtly wrong.

`Experiment::column()` uses a pointer-to-member so one function serves every
numeric field rather than six near-identical getters. Worth knowing the syntax
exists; worth using only where the alternative is real duplication, as here.

## 4. Two cleanups the plan had listed

**`Statistics` members renamed.** They were `m_areaUnderQueueLength` and
`m_areaUnderServerBusy` — accurate for a station, a lie for the system-level
object, which was being fed `(numberInQueue, numberInSystem)`. Names that are
true in one caller and false in another are worse than neutral names.

Now the class integrates two series, A and B, and the **caller labels them** at
construction. The labels come back out in report headings, so nothing is lost to
the reader and nothing is claimed that isn't true.

**The `EventNotice` sequence static is gone.** The tie-break counter now lives in
`FutureEventList`, which stamps the number in `schedule()` through a **private
setter reached by a `friend` declaration**.

Why `friend` rather than a public `setSequenceNumber()`: a public setter would
let any code renumber an event, *including one already sitting in the heap*,
which silently breaks the heap invariant. `friend` narrows that power to exactly
the one class that needs it. Friendship gets described as "breaking
encapsulation"; used this way it is the opposite, because the alternative was a
setter open to everyone.

Side benefits, both free: the counter rewinds with `clear()` so nobody has to
remember a separate reset call, and two simulations in one process can no longer
interfere.

---

## Deliberately NOT built, again: `IEventHandler`

v4 was supposed to be where the `run()` switch finally justified virtual
dispatch. The switch did grow — four cases to six, with `WarmUpEnd` and
`Observe`. And the answer is still no, for a reason v4 made **clearer** rather
than weaker:

> The cost of the abstraction is not the hierarchy. It is that handler objects
> living outside `SimulationSystem` would need `admit()`, `startNextService()`,
> `createEntity()` and `refreshState()` made public — or five `friend`
> declarations. **Widening the public interface to satisfy an abstraction is a
> worse trade than a switch that fits on a screen**, and `-Wswitch` still flags a
> new `EventType` for free.

The thing that would actually change the answer: handlers that carry **state** —
pre-emption, balking, reneging. A switch cannot hold state; an object can. Build
it then.

That is the fourth time in this project the same judgement has been applied
(v1 termination rules, v2 queue disciplines, v3 events, v4 events again), and
twice the answer flipped to yes when the third real case arrived. The rule is
working.

## Config file input — also still not built

Same reasoning as v3: it is I/O plumbing, not design. The one interesting idea in
it — that `assert` is the **wrong** tool for external data, because asserts
vanish under `NDEBUG` and untrusted input needs validation that survives a
release build — is a paragraph, not a subsystem.

---

## Verified

- Clean under `-Wall -Wextra -Wpedantic` and `-fsanitize=address,undefined`.
- **124/124 checks** (was 85 in v3).
- New tests cover: `Summary` arithmetic against hand-computed values, the n−1
  divisor, the t table, `restartAt` semantics, warm-up not changing utilisation,
  grid sampling, per-replication seed variation, whole-experiment
  reproducibility, and **theory inside the interval for all five quantities**.
- The v1/v3 checks still pass unchanged, including the deterministic hand-worked
  run: 5 served, waits 0/1/0/3/1, average exactly 1.0, last exit t=13.

## What is left

- **Entity lifetime under long runs.** Entities are destroyed at departure, so
  memory is flat — but `m_nextEntityId` is a plain `int` and a very long run
  would overflow it. `EntityId` is a typedef; widening it is one line.
- **Variance reduction** (common random numbers, antithetic variates) — the
  natural v5. `Constant::draw` already deliberately consumes nothing from the
  stream so that swapping distributions does not shift every other draw, which
  is a precondition for common random numbers working at all.
- **Steady-state detection** as a termination rule, rather than a fixed run
  length. `ITerminationRule` receives the whole system, so it is expressible now.
