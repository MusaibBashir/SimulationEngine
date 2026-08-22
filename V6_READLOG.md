# v6 Read Log — "a model is a flowchart"

Through v5 a model was a chain: arrive, get served, maybe get served again,
leave. That covers a queue. It does not cover a system — real ones branch, wait
for groups, scrap things and count them.

v6 makes a model a **graph of blocks**, in the spirit of Arena's Basic Process
template. It is also the version where the interface argument this project has
been having with itself since v1 finally resolves.

Status: **189/189 checks** (160 in v5), clean under `-Wall -Wextra -Wpedantic`
and ASan/UBSan. All v1–v5 tests pass unchanged, and every existing example
produces the same numbers it did in v5.

---

## The block set

| Block | Does | Takes time? |
|---|---|---|
| **Process** | seize a resource, delay, release | yes, and queues |
| **Delay** | hold for a time, no resource | yes, never queues |
| **Assign** | set attributes | no |
| **Decide** | branch, by chance or by condition | no |
| **Batch** | accumulate N entities into one | holds them |
| **Separate** | split a batch, or duplicate an entity | no |
| **Record** | tally a value without changing anything | no |
| **Dispose** | leave the system | no |

The line in example 11:

```
arrive → Stamp → [Machining ×2] → (conveyor) → [Inspect]
                                                   │
                                    8% fail → Record → Dispose(Scrapped)
                                   92% pass → Batch(4) → [Packing]
                                              → Record(time) → Dispose(Shipped)
```

---

## 1. The interface argument, resolved

Five versions running, this project declined to build an `IEventHandler`
hierarchy, always for the same reason, and I wrote it down each time:

> handler objects outside `SimulationSystem` would need `admit()`,
> `startNextService()`, `createEntity()` and `refreshState()` made public.
> Widening the public interface to satisfy an abstraction is a worse trade than a
> switch that fits on a screen.

I also named the trigger that would change the answer: **handlers that carry
state.** `Batch` is exactly that — it accumulates entities across events, which a
switch statement cannot do.

So v6 builds the hierarchy. But **the resolution was not to widen
`SimulationSystem`'s public interface.** It is `NodeContext`: a narrow facade
holding the six operations a block may perform, and nothing else.

```cpp
class NodeContext {
    SimTime now() const;
    RandomStream& rng();
    Trace& trace();
    void route(Entity*, INode*);
    void scheduleReturn(SimTime, Entity*, INode*);
    Entity* createEntity();
    void destroy(Entity*);
};
```

A block cannot reach the event list, the statistics, the entity table or the
clock. The engine's invariants stay the engine's.

**That is the general shape of the answer whenever an abstraction claims it needs
your internals: publish an interface for its ROLE, not your whole class.** The
five versions of saying "not yet" were not stalling — they were waiting for a
case where the role was clear enough to draw a line around.

## 2. The engine forgot how service works

The real structural change is what *left* `SimulationSystem`. `admit()`,
`startNextService()` and the seize/queue/release logic all moved into
`Station::enter()` and `Station::onScheduledEvent()`.

The engine now does exactly three things: advance the clock to the next event,
hand the entity to the block that asked for it, and keep the statistics. It has
no idea what a server is.

The `m_activeDelays` map went with it. A waiting entity's `Delay` now lives in
the queue it is waiting in, which is where the fact belongs.

## 3. Visit ratios — the stability check had to learn to read the graph

v5's ρ check assumed every entity visits every station once. That is true of a
chain and false the moment you branch or batch, and the failure mode is bad in
the wrong direction: **it rejects models that are fine.**

`visitRatios()` walks the flowchart carrying a weight:

- Decide by chance *p* → true branch gets *w·p*, false gets *w·(1−p)*
- **Batch of n → the outflow is w/n**
- **Duplicate ×k → the outflow is w·(k+1)**

Those last two are why it cannot be a plain reachability walk. Batching and
duplication change the flow *rate*, not just the path.

From the tests: a station behind a 10% branch with a service time five times the
arrival gap has ρ = 0.5, and one after a batch of 4 with service three times the
gap has ρ = 0.675. Both look overloaded on naive arithmetic; both are fine.

**And it says when it cannot know.** Behind a condition-based Decide the split
depends on entity state, which is an *output* of the simulation. `VisitRatios`
carries an `exact` flag, the check relaxes, and the error message says so. That
is better than a confident wrong number, which is this project's recurring theme.

## 4. Batch, and the timestamp that is easy to get wrong

A batch representative inherits the **oldest member's** creation time. So a
carton's time-in-system includes the wait the first part spent waiting for three
companions — nearly always the number you wanted.

Setting it to `now()` would have been the obvious implementation, would have run
perfectly, and would have understated carton age by exactly the batching delay.
There is a test pinning it: four arrivals a minute apart give a batch age of
exactly 3.0.

Permanent batches destroy their members; temporary ones keep them for a later
`Separate`. A temporary batch that reaches an exit without being separated takes
its members with it, or the entity table would grow forever.

## 5. Two things that were nearly silent

**`typeinfo for des::INode` undefined at link time.** Every virtual was defined
inline in the header, so the class had no *key function*, so nothing emitted its
vtable or typeinfo — and `dynamic_cast` needs typeinfo. The error reads like a
build-system fault and is not. `src/Node.cpp` exists mainly to define one virtual
out of line. This is why polymorphic bases conventionally have a `.cpp` even when
they look like they need none.

**Block counters ignored the warm-up.** `Statistics` objects respected
`setWarmUp()`; `Record`, `Decide`, `Batch` and `Dispose` counters did not. Two
numbers side by side in one report, measuring different periods. Added
`INode::resetStatistics(now)`, called for every block at warm-up. Caught by
noticing that example 11 reported more parts inspected than entities arrived.

## 6. A segfault that was not a bug

Example 11 crashed after a rebuild. It was a stale `ar` archive — `ar rcs` adds
members without removing ones whose source no longer matches, so the link mixed
old and new object files. Rebuilding from an empty directory fixed it, and ASan
had been clean the whole time. Worth remembering: **when a crash appears without
a plausible code change, suspect the build before the code.**

---

## Deliberately not done

**Resource sets.** A Process owns its servers. Real systems share one operator
across three machines, and Arena models that with a Resource several blocks can
seize. It is the biggest remaining modelling gap and the top of the v7 list.

**N-way Decide.** Two branches cover most cases; a list of (condition, target)
can wait until a model needs three.

**Batch by attribute** — grouping only entities that match, e.g. same lot number.
Same reasoning.

---

## Verified

- 189/189 checks; the 29 new ones cover every block, the warm-up reset, routing
  loops through both Decide branches, `nodeAs<>` type mismatches, and visit
  ratios through branches and batches.
- Clean under ASan/UBSan.
- Every v1–v5 example produces the same numbers as before. Example 05's
  per-station served counts move by one or two at the tail, because a station now
  records completion when service ends rather than when the engine routed the
  entity onward; every wait and utilisation is identical.
