# v9 Read Log — "solve the coursework"

Three Arena lab problems, and what the engine needed before it could express
them. Every feature here exists because a specific question demanded it.

Status: **293/293 checks** (260 in v8), clean under `-Wall -Wextra -Wpedantic`
and ASan/UBSan.

---

## What the problems demanded

| Problem | Needed |
|---|---|
| Gear manufacturing: batch of 5 plates | batching-station queue times **per member** |
| Urine samples: split with 50% to a duplicate | Separate with **two exits**; a way to run an **overloaded** model |
| Three ball types, sets of one of each | **multiple sources**, **entity types**, **matched batching** |

---

## 1. Create blocks, and the engine getting smaller

Through v8 a model had exactly one arrival stream. Three ball types arriving
independently, each capped at 1000, cannot be said that way.

`CreateNode` is a source: interarrival distribution, entity type, a maximum, a
first-arrival time, entities per arrival.

The part worth noticing is that **it is an `INode` like any other.** Its
scheduled callback makes an entity, routes it, and reschedules itself — so
`SimulationSystem::handleArrival` was **deleted**. The engine no longer has a
special case for arrivals, because arrivals stopped being special.

That is the fourth time in this project generalising something made the middle
smaller (v3's Model, v6's nodes, v7's resources, now this). It is the most
reliable signal that an abstraction is the right one.

One ordering bug worth recording: `CreateNode` schedules its *next* arrival
**before** routing the current entity, because routing is synchronous and may
travel a long way through the flowchart. Scheduling afterwards would make the
stream depend on what happens downstream.

## 2. Entity types

`Entity::type()`, and per-type `NumberIn`, `NumberOut`, `WIP`, `TotalTime` — the
statistics Arena reports and a three-stream model is meaningless without.

The rule for what counts: **only entities from a `Create` are arrivals.** Batch
representatives and Separate duplicates are made with the same `createEntity()`
call but are things the model manufactured, not demand the system received.
Counting them would inflate every arrival rate in the report. `registerArrival()`
marks the difference; exits check the mark.

Entities consumed by a *permanent* batch do count as leaving — a plate swallowed
by a batch has certainly left the system.

## 3. Matched batching

`BatchNode::Rule` gained `SameAttribute` (group entities that agree — same lot,
same order) and `DistinctAttribute` (**one of each**).

The last one is the rule plain batching cannot fake. With three streams running
at different speeds, "the first three to arrive" is often three of the same
thing. There is a test contrasting them directly: with a fast and a slow stream,
matched batching produces ~25 sets in 100 minutes (governed by the slow stream)
while a plain batch of two produces >50. **Same inputs, different model.**

Selection is oldest-first within each distinct value, so nothing is starved by a
later, busier value.

## 4. Batching-station queue times, per member

Arena reports one waiting-time observation **per member** at a batching station,
not per batch. For a batch of five that is a factor of five in the observation
count, and comparing that average against a machine queue's without noticing is
comparing different things.

There is an exact test: 20 arrivals one minute apart into batches of five wait
4, 3, 2, 1, 0 minutes — average exactly 2.0.

## 5. Separate has two exits

Arena's Separate block has an **Original** exit and a **Duplicate** exit.

This matters for the urine-sample problem, where the question says "the sample is
split into two with **50% of the original sample sent to the duplicate**". That
50% is Arena's *Percent Cost to Duplicates* — it splits the cost of the work — and
it is extremely easy to read as a routing probability. The original goes to one
test bench and the duplicate to the other; a `decideByChance(0.5)` gives a
similar-looking model that is not the one described.

`route()` is the Original exit; `routeDuplicate()` is the Duplicate exit. Copies
also inherit the original's creation time, because they are the same work.

## 6. Terminating runs of overloaded models

**The most interesting thing v9 fixed, and it was a design error of mine.**

The urine-sample problem has samples arriving twice as fast as they can be
processed: ρ = 2. Since v5 this engine has refused ρ ≥ 1 — and I wrote at the
time that "the queue grows without bound and every average is meaningless".

That is true of a **steady-state** study and false of a **terminating** one. "Run
the clinic for four hours and tell me what happened" is a perfectly good question
about an overloaded shift — an overloaded shift is a real thing that happens, and
refusing to model it was conflating two kinds of study.

`allowOverload()` says you know. The report then carries:

```
*** OVERLOADED MODEL: at least one queue grows without bound.
*** These are TERMINATING-run results for this horizon only.
*** They are not steady-state values and will change if the
*** run length changes.
```

The check was right to exist and wrong to be unconditional. Refusing is the
correct default; refusing with no way through was not.

## 7. A statistic that was silently zero

Problem 5's model is made entirely of Create, Assign, Batch and Dispose blocks —
no Process anywhere. It reported **WIP of exactly 0.0000** while 32 balls sat
waiting at the batching station.

`refreshState()` had been summing over Process blocks only, which was complete in
every earlier version because a queue was the only place an entity could wait.
Now it counts batch queues too, and WIP is the number of live entities wherever
they are.

**A statistic that is silently zero is worse than one that is missing**, because
zero gets copied into an answer. Third time this project has hit the same shape:
a value that used to be equivalent to another stopped being so.

---

## The answers

**Problem 1** (batch of 5, 4 hours): the batching queue holds essentially all the
waiting — a plate waits for four companions before the machine sees it, and the
machine queue is two orders of magnitude smaller.

And a trap in the question. The words say the plates "are separated and are
dispatched", which means a *temporary* batch and five times as many entities
leaving. The reference Arena model used a *permanent* batch, which makes the
Separate a silent no-op. Both readings are defensible; not noticing which one you
built is not. The example prints both: 97 entities out, or 485.

**Problem 3** (urine samples): Adding Chemicals is pinned near utilization 1.0
and its queue grows all afternoon. The two test benches sit near 0.5–0.7 and are
*not* the bottleneck — a third tester would change nothing. Test results are
twice the samples that clear chemicals, because each is duplicated.

Also worth saying: at 2 arrivals/hour over 4 hours, a single replication sees
about 8 arrivals. The reference Arena run saw 15 and this one saw 6. **Neither
number means much on its own** — this is exactly the situation example 08 exists
for, and the honest answer needs replications.

**Problem 5** (one of each): 237 sets in 4 hours, with all three types consumed
in exactly equal numbers and the surplus of each waiting for partners. Change one
stream's rate and the whole thing is governed by the slowest ingredient.

---

## Verified

- 293/293 checks; the 33 new ones cover arrival caps, routing into a Create,
  unwired sources, matched vs plain batching contrasted on identical streams,
  same-attribute batching, per-member batch queue observations against an exact
  hand-computed average, overload refusal and permission, both Separate exits,
  per-type accounting, and `clone()`.
- Clean under ASan/UBSan.
- `ARENA_MAP.md` maps every Basic Process module to its call here, and lists the
  three places this engine deliberately differs.
