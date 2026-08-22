# v7 Read Log — "the servers stop belonging to one block"

Three modelling gaps closed, and one real bug found by the first test written
against the new feature.

Status: **215/215 checks** (189 in v6), clean under `-Wall -Wextra -Wpedantic`
and ASan/UBSan. All v1–v6 tests pass unchanged.

---

## 1. Shared resources

Through v6 a `Process` owned its servers by value. That made a very ordinary
thing inexpressible: **two nurses covering both triage and the vaccination
room.** You could give triage two nurses and vaccination two nurses, but then
you had four.

v7 separates the two ideas:

```cpp
.resource("Nurse", 2)                                        // the people
.stationUsing("Triage",      "Nurse", PRIORITY, exponential(3.0))
.stationUsing("Vaccination", "Nurse", FIFO,     exponential(5.0))
```

The Model owns the resources; a Process holds a non-owning pointer. Same
owner/observer rule as everywhere else in this project since v1.

### The question that has no default answer

When a shared unit is freed, **which waiting queue gets it?** The engine cannot
duck this, and any choice changes the results.

The rule chosen is **global first-come first-served**: among every block waiting
on that resource, the one holding the entity that has been waiting longest. That
needed a small interface, `IResourceUser`, so the resource can ask its candidates
rather than guess:

```cpp
virtual bool    hasWaiting() const = 0;
virtual SimTime headOfLineSince() const = 0;
virtual void    startFromQueue(NodeContext&) = 0;
```

The alternative — fixed block priority, so machine A always beats machine B — is
equally defensible and gives different answers. FCFS is chosen because it is what
people assume when they do not say, and **it is written in the header** so nobody
has to read the implementation to discover which model they are running.

Note also what `Station::onScheduledEvent` now does on release: it calls
`m_resource->offerFreedUnit(ctx)` rather than serving its own queue. Helping
itself first would have silently given the releasing block priority.

### The bug the first test found

The test asserted that two blocks sharing one operator have utilisations that
**sum** to the operator's. It failed: got 0.6, expected 0.3 — exactly double.

`updateAllIntegrals` was feeding each station `resource().unitsBusy()`. For a
private resource that is the block's own usage, which is why it was right for
four versions. For a *shared* resource it is the total across every block using
it, so both blocks reported the whole pool and the two summed to twice the truth.

Fixed by giving `Station` an `m_unitsHeld` counter: **a block must measure what
it holds, not what the resource is doing.** Worth noticing that this is the third
time in this project a number was silently wrong because a value that used to be
equivalent to another stopped being so.

### And the stability check had to learn to add up

Two blocks each at ρ = 0.6 are fine alone and impossible together. `validate()`
now sums the offered load across every block sharing a resource and reports the
*pool* as oversubscribed, naming it rather than any one block.

---

## 2. Balking and reneging

**Balking** — refusing to join a queue that is already too long:

```cpp
.balkAt("Triage", 6, "WalkedOut")
```

This changes what you *measure*, not just who is happy. Without it the queue
absorbs every arrival and reports an average wait nobody would have tolerated;
with it the queue is capped and the demand you turned away is counted separately,
which is usually the number the decision hangs on.

**Reneging** — joining, waiting, then giving up:

```cpp
.renegeAfter("Consult", exponential(40.0), "LeftUntreated")
```

### Lazy cancellation — the v1 warning, paid off

`FutureEventList.hpp` has said since v1 that a binary heap *cannot* remove an
arbitrary element, and that the day cancellation was needed, only that file would
change. Reneging needs cancellation: when a patient is finally called, their
patience timer must not fire.

It turns out that file did **not** have to change, because the standard answer to
"my priority queue cannot cancel" is not to cancel:

> Schedule the timer. Let it fire regardless. When it does, ask whether the
> entity is *still in the queue*. If not, it was served long ago and the timer is
> stale — ignore it.

One extra event per queued entity, no heap surgery, and no way for a cancelled
event to be missed. `INode::onRenegeTimeout` defaults to ignoring, which is why
"ignore" is the right default rather than an error.

---

## 3. N-way Decide

`Decide` was two-way. Now:

```cpp
.decideNWayByChance("Sort")
.branch("Sort", 0.5, "Small")
.branch("Sort", 0.3, "Medium")
.branch("Sort", 0.2, "Large")
```

Three decisions worth defending:

**One draw, walked against a cumulative probability.** Drawing once per branch
would burn several random numbers and, worse, would not produce the branch
probabilities you asked for.

**Leftover probability falls through to `route()`.** So "10% get inspected,
everyone else carries on" is written the way you would say it, with one branch.

**All-chance or all-condition, never mixed.** Refused with a `ModelError` rather
than guessed at: chance branches must share one draw to sum correctly, conditions
are evaluated in order, and a reader could not tell which rule applied to which
branch. For conditions, **first match wins**, so ordering is a modelling decision
— put the most specific condition first. There is a test pinning that.

`visitRatios()` follows N-way chance branches exactly and flags condition-based
ones inexact, as before.

---

## 4. `EntityId` widened

```cpp
using EntityId = long long;   // was int
```

One line, because it was an alias from v1 — which is the entire argument for
those two aliases in `Common.hpp`. A wrapped id would start colliding with live
entities in the id-keyed maps: silently, and only on the longest runs, which is
the worst possible combination.

---

## Three tests that were wrong before the code was

Three of the first balking and reneging tests failed, and the code was right.
They used **constant** arrivals and **constant** service — a D/D/1 queue with
ρ < 1, where a queue *never forms at all*. Nobody could balk because nobody ever
waited.

Worth internalising: balking and reneging only mean anything where a queue
**fluctuates**. Deterministic models are the right tool for hand-checking
arithmetic (example 02) and the wrong one for anything about congestion.

The oversubscription test failed for a duller reason — my arithmetic. Three
branches at a third each with 0.7-minute services is 0.7 total, not the ≥ 1 the
test claimed to be building.

---

## Deliberately not done

**Common random numbers and antithetic variates.** Still the top statistical
item, and still separable from modelling power. `constant()` already draws
nothing from the stream, which is the precondition.

**Resource schedules** — a nurse who goes off shift at 5pm. Arena has it, it is
genuinely useful, and it needs a resource whose capacity varies with time, which
interacts with every ρ calculation in the model. That is a version of its own.

**Preemption** — a high-priority entity taking a resource off a low-priority one
mid-service. Needs the ability to cancel a scheduled departure, and lazy
cancellation does not help there: the entity must go back into a queue with its
remaining service time. Also its own version.

---

## Verified

- 215/215 checks; the 26 new ones cover shared-resource arbitration, the
  utilisation summation, oversubscription, units-exceeding-capacity, balking
  caps, reneging direction, stale-timer handling, N-way chance splits,
  fall-through, condition ordering, mixed-mode refusal, and the id width.
- Clean under ASan/UBSan.
- Every v1–v6 example reproduces its previous numbers.
