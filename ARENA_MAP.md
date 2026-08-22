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

## Three differences worth knowing

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
