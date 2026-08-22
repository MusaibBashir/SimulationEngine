# Using the DES engine — a guide

You know C++. You have never seen this codebase. This folder is everything you
need to build queueing models with it, and you should not have to read the
engine's source to do so.

Ten programs, each self-contained and heavily commented. Read them in order; each
one adds one idea.

---

## What this thing is

A **discrete-event simulator** for queueing systems. You describe a system —
things arrive, wait in queues, get served by resources, maybe move to another
queue, then leave — and it tells you how long they waited, how busy the servers
were, and how long the queues got.

"Discrete-event" means the clock does not tick. It **jumps** from one event to
the next, because nothing happens in between. A day of simulated bank traffic
runs in milliseconds.

You never write the simulation loop. You describe a **Model** and the engine
runs it.

---

## Build and run

From the `sim/` directory:

```sh
cmake -S . -B build
cmake --build build
./build/examples/01_hello_mm1
```

Or compile one directly, which is faster while you are experimenting:

```sh
g++ -std=c++17 -Iinclude examples/01_hello_mm1.cpp src/*.cpp -o ex01 && ./ex01
```

Everything lives in `namespace des`, and `des.hpp` pulls in the whole engine:

```cpp
#include "des.hpp"
using namespace des;
```

Recompiling `src/*.cpp` every time is slow. Build the engine once into a static
library and link against it:

```sh
mkdir -p obj && for f in src/*.cpp; do g++ -std=c++17 -Iinclude -c "$f" -o obj/$(basename $f .cpp).o; done
ar rcs libdes.a obj/*.o
g++ -std=c++17 -Iinclude examples/01_hello_mm1.cpp libdes.a -o ex01
```

Some examples write files (`trace_*.md`, `welch_*.csv`) into the working
directory. Run them from a directory you don't mind cluttering.

---

## The ten examples

| # | File | What it teaches |
|---|---|---|
| 01 | `01_hello_mm1.cpp` | The five-step shape of every program. Start here. |
| 02 | `02_hand_check.cpp` | Deterministic inputs + a trace file, so you can verify the engine by hand. **Read this second.** |
| 03 | `03_multi_server.cpp` | Multiple servers at one station, and why "which is better" depends on which metric you chose. |
| 04 | `04_queue_disciplines.cpp` | FIFO / LIFO / SPT / EDD / Priority / Random, and entity attributes. |
| 05 | `05_restaurant_chain.cpp` | Chains of stations, routing, and finding the bottleneck. |
| 06 | `06_distributions.cpp` | All five distributions, and why variability creates queues even when the mean doesn't change. |
| 07 | `07_warmup_welch.cpp` | Warm-up removal: your run starts in the wrong state. |
| 08 | `08_replications_ci.cpp` | **The most important one.** One run is one sample. Report an interval. |
| 09 | `09_capacity_decision.cpp` | Using all of it to actually decide something. |
| 10 | `10_stopping_and_tracing.cpp` | Termination rules, trace files, priorities, seeds. |

If you are short of time: **01, 02, 08**. Those three are the difference between
using the tool correctly and producing confident nonsense.

---

## The shape of every program

```cpp
#include "des.hpp"
using namespace des;

SimulationSystem sim(12345u);          // engine + seed

sim.model()
   .arrivals(exponential(1.0))                       // how often things arrive
   .station("Teller", 1, FIFO, exponential(0.8))     // name, servers, rule, service
   .entryAt("Teller");                               // where arrivals land

sim.stopAt(480.0);                     // when to stop
sim.execute().report();                // initialise + run, then print
```

Eight lines, and every one of them is about the queueing system rather than
about C++ memory management.

To get numbers out instead of printing them:

```cpp
RunResults r = sim.results();
r.averageWait;
r.station("Teller").utilisation;
r.station("Teller").maxQueueLength;
```

`results()` has already divided by the right thing — see the warm-up note below.

## API reference

### `SimulationSystem`

| Call | Does |
|---|---|
| `SimulationSystem sim(seed)` | Create the engine. Same seed ⇒ identical run. |
| `sim.model()` | The `Model&` you configure. |
| `sim.stopAt(t)` / `sim.stopAfter(n)` / `sim.stopWhen(rule)` | When to stop. Required. |
| `sim.warmUpFor(t)` | Discard statistics collected before time `t`. |
| `sim.traceTo(path)` | Write an event log. |
| `sim.execute()` | `initialise()` then `run()`. |
| `sim.report()` | Print a summary plus a per-station table. |
| `sim.results()` | **`RunResults`** — the same numbers, as data. |
| `sim.clock().now()` | Current simulated time. |
| `sim.setObservationInterval(dt)` | Sample number-in-system every `dt` (for Welch). |

All of these return `SimulationSystem&`, so they chain.

### `RunResults` / `StationResults`

```cpp
RunResults r = sim.results();
```

`r`: `simulatedTime`, `warmUpDiscarded`, `measuredTime`, `arrived`, `exited`,
`averageWait`, `averageTimeInSystem`, `maxWait`, `averageNumberInQueue` (Lq),
`averageNumberInSystem` (L), `stillWaitingAtStop`, `stations`.

`r.station("Teller")`: `capacity`, `served`, `averageWait`, `averageTimeHere`,
`maxWait`, `averageQueueLength`, `maxQueueLength`, `utilisation`.

Every time average in there is already divided by the **measured** period rather
than the clock. That distinction matters the moment you use a warm-up, and it is
the sort of thing an API should not leave to the caller.

`initialise()` fully resets. Calling `initialise(); run();` twice gives the
identical answer, not a continuation.

### `Model`

| Call | Does |
|---|---|
| `arrivals(dist)` | How often entities arrive. Required. |
| `station(name, capacity, discipline, serviceDist)` | Add a service point. |
| `route("A", "B")` | After being served at A, go to B. |
| `entryAt("A")` | Where arrivals land. Defaults to the first station added. |
| `attribute("priority", dist)` | Give every arriving entity an attribute. |
| `offeredLoad(station)` | ρ for that station. Checked for you at `execute()`. |

All return `Model&`, so they chain. (The older names — `setInterarrival`,
`addStation`, `connect`, `setEntry`, `assignOnArrival` — still work and return
`Station*` where useful, e.g. for `setServiceFromAttribute`.)

A station with nothing `connect`ed after it is an **exit** — that is how entities
leave the system.

### `Station`

| Call | Does |
|---|---|
| `setServiceFromAttribute("serviceTime")` | Take service time from the entity's attribute instead of drawing it. |
| `stats()` | This station's `Statistics`. |
| `queue().maxLengthObserved()` | Longest this queue ever got. |
| `resource().capacity()` | How many servers. |

### Distributions

| Class | Use for |
|---|---|
| `exponential(mean)` | Random arrivals; memoryless service. The default. |
| `constant(v)` | No variability. |
| `uniform(low, high)` | Equally likely across a range. |
| `triangular(low, mode, high)` | Min / most-likely / max. **Best when you have no data but do have an opinion** — which is most coursework. |
| `fixedTimes({a, b, c})` | A fixed list, in order. For hand-checking. |

> **`Exponential` takes the MEAN, not the rate.** `Exponential(4.0)` means "4
> minutes on average". Passing `0.25` because "the rate is 0.25/min" gives a
> model wrong by a factor of 16 that runs perfectly happily.

### Queue disciplines

| Name | Serves next | Needs attribute |
|---|---|---|
| `FIFO` | longest waiting | — |
| `LIFO` | most recent arrival | — |
| `PRIORITY` | highest `attr::priority` | yes |
| `SPT` | lowest `attr::serviceTime` | yes |
| `EDD` | lowest `attr::dueDate` | yes |
| `RANDOM` | uniformly at random | — |

Use the `attr::` constants rather than string literals — the disciplines look
attributes up **by string**, so a typo means "absent", which reads as `0.0` for
every entity, which ties every comparison, which turns SPT into FIFO in silence.

> **Priority, SPT and EDD read entity attributes.** If nothing calls
> `assignOnArrival` with that exact name, every entity reads `0.0`, every
> comparison ties, and all three silently behave as FIFO. This is the single
> most common way to waste an afternoon with this engine. See example 04.

Ties break in favour of the earliest arrival (FIFO among equals).

### Termination rules

| Rule | Stops when |
|---|---|
| `timeLimit(t)` | the clock reaches `t` |
| `entityLimit(n)` | `n` entities have **left** (exits, not arrivals) |
| `whenDrained()` | the system is empty |
| `anyOf(a, b, ...)` | any of them fires |

`DrainedRule` models a **terminating** system (a clinic that closes, a batch of
jobs). Pair it with a `TimeLimit` as a safety net, since a system fed by endless
arrivals may never empty.

### `Experiment` and `Summary` — for answers you can defend

```cpp
Experiment e("my study", [](SimulationSystem& s) { /* build the model */ });
e.replications(20).baseSeed(9000u).warmUp(500.0);
e.run();
e.report();                                    // mean +/- 95% interval

std::vector<double> wq = e.column(&ReplicationResult::averageWait);
double m = Summary::mean(wq);
double h = Summary::halfWidth95(wq);
```

`ReplicationResult` fields: `seed`, `served`, `averageWait`,
`averageTimeInSystem`, `Lq`, `L`, `utilisation`, `measuredTime`.

For warm-up analysis: `.observeEvery(dt)`, then `e.suggestWarmUp()` (MSER) and
`e.writeWelchSeries("welch.csv", 25)` to plot.

### Errors

Mistakes **you** make throw `des::ModelError`, in every build:

- a station that cannot keep up (ρ ≥ 1)
- routing to, or entering at, a station that does not exist
- a duplicate station name, a routing loop, capacity < 1
- no arrival distribution, no entry station, no termination rule
- using a reserved attribute name (`waitTime`, `waitHere`, `stationEntry`)

They are exceptions rather than asserts on purpose: asserts compile to nothing
under `-DNDEBUG`, so in a release build a typo'd station name would have silently
done nothing. Anything that is *your* mistake must be checked in every build;
`assert` is reserved for the engine's own invariants.

---

## Mistakes to avoid

**Not checking ρ.** `ρ = (mean service time) / (capacity × mean interarrival
time)`. If ρ ≥ 1 the queue grows forever and every average is meaningless.
As of v5 **the engine checks this for you** and throws rather than printing
confident nonsense — but know the number anyway, because ρ = 0.95 is legal and
still means very long queues.

**Quoting one run.** A run is one sample from a random variable. Example 08.

**Reading `Exponential` as a rate.** See above.

**Using PRIORITY/SPT/EDD without `attribute()`.** Silently degenerates to FIFO.

**Dividing by `clock().now()` when you used a warm-up.** Use `results()`, which
divides by `measuredTime()` for you.

**Comparing two designs whose intervals overlap** and declaring a winner. You
haven't shown one.

**Picking the seed that gave a nice number.** That is choosing your data.

**Optimising whichever number you happened to print.** Decide your objective
first — example 03 shows two reasonable metrics choosing opposite ends of the
same range.

---

## A checklist for coursework

1. Know ρ for every station. The engine refuses ρ ≥ 1; you still want to know
   whether you are at 0.5 or 0.95, because the difference is enormous.
2. Build a `Deterministic` version first and check it by hand against a trace
   (example 02). Only then switch to random distributions.
3. Sanity-check against theory where it exists — M/M/1 gives
   `Wq = ρ/(μ−λ)`, and Little's Law `L = λW` holds for *any* system.
4. Measure the warm-up (example 07). Discard the transient, and no more.
5. Run ≥ 20 replications. Report **mean ± half-width**, and say how many.
6. State which inputs are measured and which you assumed, and show how the
   answer moves when the assumed ones change. That sensitivity analysis is
   usually worth more marks than the simulation.
7. Report the **maximum** as well as the mean whenever you use a priority or
   shortest-job rule — those rules buy a good average by treating somebody
   badly, and the average hides it.
