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
#include "SimulationSystem.hpp"

SimulationSystem sim(12345u);          // 1. engine + seed
Model& m = sim.model();                // 2. describe the system

m.setInterarrival(std::make_unique<Exponential>(1.0));
m.addStation("Teller", 1, QueueDiscipline::FIFO,
             std::make_unique<Exponential>(0.8));
m.setEntry("Teller");

sim.setTermination(std::make_unique<TimeLimit>(480.0));   // 3. when to stop
sim.initialise();                                          // 4. run
sim.run();
sim.report();                                              // 5. results
```

Everything is `std::unique_ptr` because the engine takes ownership. Use
`std::make_unique`.

---

## API reference

### `SimulationSystem`

| Call | Does |
|---|---|
| `SimulationSystem sim(seed)` | Create the engine. Same seed ⇒ identical run. |
| `sim.model()` | The `Model&` you configure. |
| `sim.setTermination(rule)` | Required. See termination rules below. |
| `sim.setWarmUp(t)` | Discard statistics collected before time `t`. |
| `sim.setObservationInterval(dt)` | Sample number-in-system every `dt` (for Welch). |
| `sim.enableTrace(path, level, markdown)` | Write an event log. |
| `sim.initialise()` | Reset everything and schedule the first arrival. |
| `sim.run()` | Process events until the termination rule fires. |
| `sim.report()` | Print a summary plus a per-station table. |
| `sim.statistics()` | System-wide `Statistics`. |
| `sim.clock().now()` | Current simulated time. |
| `sim.measuredTime()` | Clock minus warm-up. **Divide by this, not the clock.** |
| `sim.model().station("X")` | A `Station*` for per-station numbers. |

`initialise()` fully resets. Calling `initialise(); run();` twice gives the
identical answer, not a continuation.

### `Model`

| Call | Does |
|---|---|
| `setInterarrival(dist)` | How often entities arrive. Required. |
| `addStation(name, capacity, discipline, serviceDist)` | Add a service point. Returns `Station*`. |
| `connect("A", "B")` | After being served at A, go to B. |
| `setEntry("A")` | Where arrivals land. Defaults to the first station added. |
| `assignOnArrival("priority", dist)` | Give every arriving entity an attribute. |

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
| `Exponential(mean)` | Random arrivals; memoryless service. The default. |
| `Constant(v)` | No variability. |
| `Uniform(low, high)` | Equally likely across a range. |
| `Triangular(low, mode, high)` | Min / most-likely / max. **Best when you have no data but do have an opinion** — which is most coursework. |
| `Deterministic({a, b, c, ...})` | A fixed list, in order. For hand-checking. |

> **`Exponential` takes the MEAN, not the rate.** `Exponential(4.0)` means "4
> minutes on average". Passing `0.25` because "the rate is 0.25/min" gives a
> model wrong by a factor of 16 that runs perfectly happily.

### Queue disciplines

| `QueueDiscipline::` | Serves next | Needs attribute |
|---|---|---|
| `FIFO` | longest waiting | — |
| `LIFO` | most recent arrival | — |
| `Priority` | highest `"priority"` | `"priority"` |
| `SPT` | lowest `"serviceTime"` | `"serviceTime"` |
| `EDD` | lowest `"dueDate"` | `"dueDate"` |
| `Random` | uniformly at random | — |

> **Priority, SPT and EDD read entity attributes.** If nothing calls
> `assignOnArrival` with that exact name, every entity reads `0.0`, every
> comparison ties, and all three silently behave as FIFO. This is the single
> most common way to waste an afternoon with this engine. See example 04.

Ties break in favour of the earliest arrival (FIFO among equals).

### Termination rules

| Rule | Stops when |
|---|---|
| `TimeLimit(t)` | the clock reaches `t` |
| `EntityLimit(n)` | `n` entities have **left** (exits, not arrivals) |
| `DrainedRule()` | the system is empty |
| `AnyOf` | any child rule fires — `add()` them |

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

### Reserved attribute names

The engine uses `waitTime`, `waitHere` and `stationEntry`. `assignOnArrival`
will assert if you try to use them.

---

## Mistakes to avoid

**Not checking ρ before running.** Compute the offered load by hand:
`ρ = (mean service time) / (capacity × mean interarrival time)`. If ρ ≥ 1 the
queue grows forever, every average is meaningless, and the simulator will print
them to four decimal places anyway. This is the number-one error.

**Quoting one run.** A run is one sample from a random variable. Example 08.

**Reading `Exponential` as a rate.** See above.

**Using Priority/SPT/EDD without `assignOnArrival`.** Silently degenerates to
FIFO.

**Dividing by `clock().now()` when you used a warm-up.** Use `measuredTime()`.

**Comparing two designs whose intervals overlap** and declaring a winner. You
haven't shown one.

**Picking the seed that gave a nice number.** That is choosing your data.

**Optimising whichever number you happened to print.** Decide your objective
first — example 03 shows two reasonable metrics choosing opposite ends of the
same range.

---

## A checklist for coursework

1. Compute ρ by hand. Confirm ρ < 1.
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
