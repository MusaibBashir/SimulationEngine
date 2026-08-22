# v2 Read Log — "it runs"

What you wrote, what was broken, and what I changed to make v2 work. Read the
**Bugs** section carefully; four of them are the kind that compile and run and
give you wrong numbers without complaint.

Status at the end: builds clean under `-Wall -Wextra -Wpedantic`, runs a real
M/M/1 model, and passes Little's Law to four decimal places.

---

## 1. What you had done

Correct, and left alone:

- `Clock::advanceTo` / `reset` — the `>=` assert is right. Several events
  legitimately share an instant, so advancing to "now" must be a legal no-op.
- `Delay::end` / `duration` — including `assert(m_ended)` before answering.
- `Resource::seize` / `release` — both invariant halves asserted.
- `TerminationCondition::isMet` — the sentinels do their job, still one line.
- `Statistics` derived getters — all four guarded against divide-by-zero.
- `EntityQueue::push` — including the max-length update riding along.
- `EventNotice` sequence-number tie-break — the right fix, and the implicit copy
  constructor propagates the number correctly, which is what you want.
- The `run()` loop skeleton — integrals, then clock, then dispatch. Order right.

**It did not compile.** Seven errors, listed in §2. But the shape was right, and
every bug below is a logic bug rather than a misunderstanding of the design.

---

## 2. Compile errors fixed

| Error | Cause | Fix |
|---|---|---|
| `EntityQueue has no member 'size'` | The method is `length()` | Call `length()` |
| `'interarrival' was not declared` | No RNG existed yet | Added `RandomStream` |
| `'serviceDraw' was not declared` | Same | Same |
| `'Activity' was not declared` | `SimulationSystem.cpp` never included it | Added the include |
| `'service' was not declared` | Knock-on from the above | — |
| `passing 'const SimulationSystem' as 'this' discards qualifiers` (×2) | `report() const` called the non-const `queue()` / `resource()` | `report()` now reads the cached `m_line` / `m_server` members |

---

## 3. Bugs — the ones that would have compiled and lied to you

### 3.1 Every statistic would have been zero

`recordArrival`, `recordDeparture` and `updateTimeIntegrals` were still empty
stubs. You implemented the four *derived* getters that divide the accumulators,
but nothing ever fed the accumulators. The report would have printed a full page
of `0.0000` with no error anywhere.

Worth internalising: you built the consumers before the producers. Next time,
write one accumulator and its getter together and print it once, before moving
on.

**Fixed:** all three implemented in `Statistics.cpp`.

### 3.2 Entities were born at the wrong time

```cpp
Entity* next = createEntity();
scheduleEvent(EventType::Arrival, m_clock.now() + interarrival(), next);
```

`createEntity()` stamps `creationTime = m_clock.now()`. So an entity that will
arrive at t+3 is stamped as created at t. Then
`inSystem = now - creationTime` includes an interarrival gap the entity spent
not existing, and **every time-in-system comes out inflated**. Utilisation and
queue length would have looked fine, which is what makes it nasty.

**Fixed by restructuring, not by patching:** the Arrival event now carries **no
entity**, and `handleArrival` creates the entity for itself on arrival. The
timestamp is correct by construction rather than by remembering to fix it.

General move worth stealing: when a value can be *derived at the wrong moment*,
change when it is computed rather than adding a correction.

### 3.3 Waiting time was hardcoded to zero

```cpp
m_stats.recordDeparture(m_clock.now(), 0.0, inSystem); // Placeholder
```

You left the placeholder in and the `Delay` was never created, ended, or read —
so average wait, max wait, and Little's Law would all have been silently wrong.
This was the design decision I flagged and deliberately did not make for you:
*where does the Delay live?*

**Fixed** with `std::map<EntityId, Delay> m_activeDelays` on `SimulationSystem`.
On queueing, a `Delay` is created; on being served, it is ended, its `duration()`
is stored as the entity's `waitTime` attribute, and the map entry is erased.

Why a map on the system rather than a `Delay` member on `Entity`: a delay is a
fact about the entity's relationship to *this system*, not an intrinsic property
of the entity, and an entity is only ever in one delay at a time. Keeping it out
of `Entity` also means `Entity` still knows nothing about queues.

### 3.4 The Random discipline silently lost entities

```cpp
case QueueDiscipline::Random:
    return nullptr; // v1 placeholder
```

The caller checks `!isEmpty()`, calls `pop()`, gets `nullptr`, and dereferences
it — or worse, doesn't, and the entity is simply gone from the simulation while
the queue length drops. A discipline that cannot be served must **assert**, never
return a plausible-looking null.

**Fixed:** `EntityQueue` now holds a non-owning `RandomStream*`, injected by
`initialise()`, and `Random` draws a uniform index. `nullptr` from `pop()` now
means one thing only: the queue was empty.

### 3.5 Three copies of the same scan loop

Priority, SPT and EDD were the same eight lines three times, differing only in an
attribute name and a comparison direction. That is exactly the triplication I
told you to write out longhand so you would see it — and you saw it, so it has
now been collapsed into one file-local helper:

```cpp
Entity* extractBest(std::deque<Entity*>&, const std::string& attribute, bool wantLargest);
```

in an **anonymous namespace**, so it has internal linkage and is invisible
outside `EntityQueue.cpp`. That is the right home for a helper nobody else needs.

Note this is deliberately *not* the v3 abstraction (a discipline strategy
object). It is the smallest change that removes the duplication — usually the
right first move, because it costs nothing if the design later changes.

### 3.6 `report()` recomputed what `Statistics` already computes

```cpp
<< (m_stats.numberServed() > 0 ? m_stats.totalWaitingTime() / m_stats.numberServed() : 0.0)
```

You had just written `Statistics::averageWaitingTime()` with exactly that guard,
and then didn't call it. Two copies of one formula; the day you change the
definition of "average wait" you will change one of them.

**Fixed:** `report()` now asks `Statistics` for every derived value.

### 3.7 The engine was welded to the string `"Teller"`

`run()`, `handleArrival`, `handleDeparture` and `report()` each did
`resource("Teller")` / `queue("TellerQueue")` — a linear string search **per
event**, hardcoded, and a null dereference if the model used different names.

**Fixed:** names are configurable via `setModel()`, resolved **once** in
`initialise()` into cached `m_server` / `m_line` pointers, with asserts that say
what you forgot to call. v3 replaces this with a real model description.

### 3.8 `SystemState` was never updated

The duplication I flagged in v1 as "deliberately wrong" — you hit it exactly as
predicted: the handlers changed `Resource` and `EntityQueue` and left
`SystemState` frozen at zero.

**Resolved as option (b), snapshot:** a new private `refreshState()` recomputes
all three fields from the authoritative objects after every state change.
`Resource` and `EntityQueue` remain the single source of truth; `SystemState`
never decides anything, it only reports. Option (a), the computed view, is still
defensible — but (b) keeps the read path free of indirection and the smell is now
contained in one four-line function instead of scattered across the handlers.

### 3.9 `EndSimulation` at infinity

`initialise()` scheduled the stop event at `m_termination.maxTime()`
unconditionally. If a caller passes `infinity` (the documented "no time limit"
sentinel), that event goes on the FEL, and if the list ever drained the clock
would advance to infinity.

**Fixed:** guarded with `std::isinf`.

---

## 4. Added

### `RandomStream` (new file pair)

One `std::mt19937`, one seed, one place. Not `std::rand()`: tiny period, poor
spectral properties, no independent streams. Not an engine hidden inside each
class: the entire point is that **the same seed reproduces the same run**, and
you cannot promise that if randomness comes from four places.

The one interface decision worth arguing about: `exponential()` is parameterised
by **mean**, not by rate. `std::exponential_distribution` wants λ; queueing
theory and your notes speak in means (1/λ). Mixing them up gives answers wrong by
a factor of mean² that still look plausible. The conversion happens in exactly
one line, inside `RandomStream::exponential`.

### Tie-break verification in `main`

Three events at t = 5.0 scheduled Arr, Dep, Arr must come back Arr, Dep, Arr.
Your sequence number makes this pass; without a test it would silently regress.

### A real M/M/1 run plus a Little's Law check

`main` now runs 20,000 simulated minutes at λ=1.0, μ=1.25 (ρ=0.8) and checks the
result against both Little's Law and the closed-form M/M/1 answer.

---

## 5. Verification

```
--- simulation report -------------------------------------
seed                     : 12345
total simulated time     : 20000.0000
entities arrived         : 20182
entities served          : 20181
average waiting time     : 3.2864
average time in system   : 4.0813
max waiting time         : 30.3204
time-average queue length: 3.3162
max queue length observed: 34
server utilisation       : 0.8022
-----------------------------------------------------------

--- Little's Law check ------------------------------------
lambda_eff               : 1.0091
Lq measured              : 3.3162
lambda_eff * Wq          : 3.3163
relative error           : 0.0000
theory rho=0.8           : Wq 3.2000  Lq 3.2000  util 0.8000
```

**Little's Law holds to four decimals.** That is the strong result: it says the
three accumulators (`m_areaUnderQueueLength`, `m_totalWaitingTime`,
`m_numberArrived`) are mutually consistent. It is a much better test than
comparing to theory, because it holds for *any* queueing system, not just M/M/1.

Utilisation 0.8022 against theory 0.8000 — good.

`Wq` 3.2864 against theory 3.2000, about 2.7% high. **This is not a bug.** Two
honest causes, and separating them is v4's job:

1. **No warm-up removal.** The run starts empty and idle, which is not the
   steady state, and those early low-queue observations are in the average.
2. **One replication.** A single run of an M/M/1 queue at ρ=0.8 has a genuinely
   wide sampling distribution. One number without a confidence interval cannot be
   said to agree or disagree with anything.

Do not "fix" this by tuning constants. Fix it in v4 with replications and a
confidence interval, then check whether 3.2000 falls inside it.

---

## 6. Things left deliberately unresolved

- **`m_entities` grows forever** — 20,182 `unique_ptr`s for a 20,000-minute run,
  none freed. Fine at this scale, fatal at 10⁸ events. v4 needs a recycling pool
  or departure-time destruction. Note that raw `Entity*` in queues and event
  notices is exactly what makes freeing them delicate.
- **`EventNotice::s_nextSequenceNumber` is a mutable static.** It works and it is
  not reset between replications, which is harmless for ordering but means the
  program is not thread-safe and never can be while it stands. v4 problem.
- **`operator>` compares `SimTime` with `==`** for the tie-break. Exact equality
  is genuinely what you want here (identical scheduled instants), but be aware
  that two times computed by different arithmetic paths will not compare equal.
- **`resource()` / `queue()` are still linear scans.** They are now called once
  in `initialise()` instead of once per event, so the cost is gone. Leave them.
