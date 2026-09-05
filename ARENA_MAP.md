# Arena → this engine

A translation table, for when the coursework is written in Arena's vocabulary.

| Arena module | Here | Notes |
|---|---|---|
| **Create** | `source(name, entityType, dist, maxArrivals, firstAt, perArrival)` | Several may coexist. `arrivals(dist)` is the one-source shorthand. |
| **Dispose** | `dispose(name)` | Or route to nothing. Named exits count separately. |
| **Process** (Seize-Delay-Release) | `station(name, capacity, discipline, service)` | Private resource. |
| **Process** using a shared Resource | `resource(name, cap)` + `stationUsing(block, resource, discipline, service, units)` | |
| **Delay** (Process with Delay type) | `delay(name, duration)` | No resource, no queue, no contention. |
| **Assign** | `assign(block, attribute, value)` | Call again with the same block name to add more. |
| **Decide** 2-way by chance | `decideByChance(name, p)` + `routeTrue` / `route` | |
| **Decide** N-way | `decideNWayByChance(name)` + `branch(name, p, to)` | Leftover probability falls through to `route()`. |
| **Decide** by condition | `decideByCondition` / `decideNWayByCondition` + `branch` | First match wins; order matters. |
| **Batch**, Permanent | `batch(name, size, true)` | Members are consumed. |
| **Batch**, Temporary | `batch(name, size, false)` | Members survive for a later Separate. |
| **Batch**, Rule = Any Entity | `batch(...)` | |
| **Batch**, matched by attribute | `batchBySameAttribute(name, size, attr, permanent)` | Same value. |
| **Batch**, one of each | `batchOneOfEach(name, size, attr, permanent)` | Distinct values. |
| **Separate**, Split Existing Batch | `separate(name)` | Only works on a *temporary* batch. |
| **Separate**, Duplicate Original | `duplicate(name, copies)` | `route()` is the **Original** exit, `routeDuplicate()` the **Duplicate** exit. |
| **Record** | `record(name)` / `recordAttribute(name, attr)` / `recordTimeInSystem(name)` | |
| **Resource** | `resource(name, capacity)` | |
| **Queue** with a discipline | 4th argument of `station` | `FIFO`, `LIFO`, `PRIORITY`, `SPT`, `EDD`, `RANDOM` |
| **Entity type / Picture** | 2nd argument of `source` | Pictures are a GUI thing; the type name is what matters. |
| **Variable** (data module) | `variable(name, initialValue)` | Global, numeric, time-persistent. |
| **Assign** to a Variable | `assignVariable(block, name, "expr")` | `"Rejected + 1"` — a global on both sides. |
| **Assign** to Entity Type | `assignEntityType(block, "\"Widget\"")` | Per-type reporting keys on it. |
| **Expression** in any Delay/Service field | `station(name, cap, rule, "text")`, `delay(name, "text")` | There is no separate distribution field, exactly as in Arena. |
| **Decide** by a typed condition | `decideWhen(name, "NQ(X) > 3")` / `branchWhen(...)` | Can read live model state; a lambda cannot. |
| **Replication length** | `stopAt(t)` | Also `stopAfter(n)`, `whenDrained()`, `anyOf(...)`. |
| **Warm-up period** | `warmUpFor(t)` | |
| **Number of replications** | `Experiment::replications(n)` | |

## Report line → what to call

| Arena report line | Here |
|---|---|
| `X.Queue.WaitingTime` (average) | `model().station("X")->stats().averageWaitingTime()` |
| `X.Queue.NumberInQueue` | `results().station("X").averageQueueLength` |
| `X.ScheduledUtilization` | `results().station("X").utilisation` |
| `X.NumberIn` / `NumberOut` | `results().station("X").served`, or `byType()` per entity type |
| `EntityType.WIP` | `byType().at("Type").areaWIP / measuredTime()` |
| `EntityType.TotalTime` | `byType().at("Type").totalTime / out` |
| `System.NumberOut` | `results().exited` |
| Batching station queue | `nodeAs<BatchNode>("X").queueStats()` — **one observation per member**, as Arena reports it |

`sim.reportArenaStyle()` prints all of it in Arena's layout.

## Arena's expression functions

| Arena | Here | Note |
|---|---|---|
| `EXPO(m)` `UNIF(a,b)` `TRIA(a,m,b)` `NORM(m,s)` | same | |
| `LOGN(m,s)` | same | Mean and sd of the **variable**, not of its logarithm. |
| `WEIB` `ERLA` `POIS` `CONS` | same | |
| `DISC(c1,v1,c2,v2,...)` | same | **Cumulative** probabilities, as Arena writes them. The engine's own `discrete()` takes individual ones. |
| `NQ(block)` `NR(res)` `MR(res)` `TNOW` | same | `WIP()` for number in system. |
| `MIN` `MAX` `ABS` `ROUND` `TRUNC` `SQRT` `LN` `EXP` `MOD` | same | `EXPO` is the distribution; `EXP` is e^x — Arena's collision, kept. |
| `Entity.Type` | same | Compares against a quoted string. |

`Empirical` and `Deterministic` have **no Arena spelling** and stay
programmatic-only. Variable arrays (1-D and 2-D) are not implemented.

## Three differences worth knowing

**A variable that does not exist.** Arena creates a Variable the moment you
assign to one. Here `assignVariable` to an undeclared name throws, because
otherwise a typo becomes a second variable nobody notices. Declare it with
`variable()` first.

**Scheduled utilization.** Arena divides busy resource-time by
(capacity × *scheduled* time). This engine has no resource schedules yet, so
every resource is scheduled for the whole run and scheduled utilization equals
utilization. When resource schedules arrive the two will part company.

**Separate on a permanent batch.** Arena silently does nothing. This engine
throws, because a Separate that quietly no-ops is how a model ends up reporting
one entity out per batch when the author expected five. If you want Arena's
behaviour, use a permanent batch and no Separate — and know that is what you did.

**Overloaded models.** Arena runs ρ ≥ 1 without comment. This engine refuses
unless you say `allowOverload()`, and then stamps the report with a warning. For
a terminating run that is legitimate; for a steady-state study the numbers are
meaningless, and the difference is not visible in the output otherwise.
