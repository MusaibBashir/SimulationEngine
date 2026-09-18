# Read this before merging

**Branch:** `claude/pensive-volta-usgu5f` — "a time limit stops the clock ON the limit, not past it"

This branch leaves two gates RED on purpose. That is not an oversight and it is
not a thing to fix by running `capture`. Read the rest of this page first.

---

## 1. What was wrong

`sim.stopAt(45).execute()` finished with `sim.now() == 50`. The first event past
the limit — a Create arrival at t=50 — was processed before the run stopped. So
a run configured as 45 minutes long reported 50 minutes of queue length,
utilisation and WIP, and counted an entity that arrived after the horizon.

The cause is structural rather than arithmetic. `canStep()` asks the termination
rule BETWEEN events, and `TimeLimit::isMet` is `now >= maxTime`. By the time the
rule is asked, `stepOnce()` has already run `m_clock.advanceTo(notice.time())`.
A between-events test cannot stop a clock on a limit: it can only ever notice
that the clock has already jumped past one.

`EventType::EndSimulation` had existed in the enum since v1 and had never once
been scheduled.

## 2. Why it is a bug, measured against paper rather than against a baseline

A model with no randomness anywhere, so the correct answers are arithmetic that
anyone can check: arrivals every 10 minutes from t=0, service exactly 2 minutes,
`stopAt(45)`. A 45-minute horizon contains arrivals at 0, 10, 20, 30 and 40, and
10 minutes of busy server.

|                    | clock at stop | entities arrived | utilisation   |
| ------------------ | ------------- | ---------------- | ------------- |
| by hand            | 45            | 5                | 10/45 = 0.2222|
| engine before      | **50**        | **6**            | **0.2000**    |
| engine after       | 45            | 5                | 0.2222        |

The old engine divides the busy time by a denominator nobody configured. That is
wrong independently of any file in `tests/`, which is the point of checking it
this way round.

## 3. What the fix does

- **`ITerminationRule::stopTime()`** — the clock time at which a rule ends the
  run, when that time is knowable before the run starts, or nothing. `TimeLimit`
  knows its own. `AnyOf` reports the earliest its children know. `EntityLimit`
  and `DrainedRule` honestly cannot say, and return nothing — the same honest
  nothing `progress()` returns, for the same reason. This is why the fix is not
  a special case for `TimeLimit` buried inside the engine.

- **`initialise()` schedules `EndSimulation` at that time, FIRST.** A deadline
  known in advance is an event, and the FEL is the one thing in this engine that
  can land the clock exactly on a time. Going first means the FEL's sequence
  tie-break makes the deadline beat anything dated exactly on the limit, whether
  it was scheduled before the run or during it. The run therefore covers
  `[0, limit)` with the clock stopping ON the limit — which is what
  `now >= maxTime` always said, and does not depend on the order a model happened
  to be described in.

- **`canStep()` treats a list holding nothing but the deadline as EMPTY.** See
  section 5; this one is a judgement call and you may want to overrule it.

- **`FutureEventList::peekImminent()`**, the other half of `nextEventTime()`, so
  the engine can tell real work from its own bookkeeping.

- **The termination test asserted `now() >= 20.0`.** That is how the overshoot
  survived this long: an assertion loose enough to pass either way measures
  nothing. It now asserts the clock reads exactly the limit.

## 4. The gates, and the decision waiting for you

```
./build/des_tests            1393 / 1395   2 FAILURES   (both are the stale teller expectation)
./build/des regress          4 models, 1 differ         (teller.des)
bash tools/baseline.sh check 15 checked, 12 differ, 1 excluded
```

`tests/baseline/` and `tests/regression/teller.expected` are NOT updated on this
branch. Twelve of the fifteen baselines and the teller expectation record the
overshoot. This project treats those files as its byte-identical gate, and a gate
that gets re-captured as a side effect of a fix stops being a gate — so the diffs
are explained here and the files are left alone until somebody agrees to them.

**Every diff has the same signature**: a printed run length of `Length + ε`
becoming exactly `Length`, plus the statistics shifting by the sliver that was
removed.

| example                 | what moved                                                |
| ----------------------- | --------------------------------------------------------- |
| 01_hello_mm1            | 480.0846 → 480.0000; exits 493 → 492, one left in service  |
| 03_multi_server         | W 4.8196 → 4.8197                                          |
| 04_queue_disciplines    | all five rules shift in the 3rd decimal                    |
| 05_restaurant_chain     | 6004.4675 → 6000.0000; arrivals 329 → 328                  |
| 07_warmup_welch         | Wq shifts in the 4th decimal at all three run lengths      |
| 08_replications_ci      | measured period per rep 19500.2830 → 19500.0000            |
| 09_capacity_decision    | cost/hr 391.78 → 391.79, 175.65 → 175.66                   |
| 10_stopping_and_tracing | stopped at t=500.55 after 227 → t=500.00 after 226         |
| 11_flowchart_line       | 20000.1377 → 20000.0000; exits 6329 → 6328                 |
| 14_variance_reduction   | all four estimates shift in the 4th decimal                |
| 15_lab_problems         | 240.2530 → 240.0000, 4.0182 → 4.0000, 240.7482 → 240.0000  |
| 16_expressions          | 480.4535 → 480.0000; exits 235 → 234                       |
| teller.des (regression) | 480.0846 → 480.0000; exits 493 → 492                       |

Things that did NOT change, and are worth checking stayed that way:

- No example's horizon got SHORTER than its configured Length.
- No count changed except for entities arriving or departing inside the removed
  sliver. In 01, arrivals stay 494 and max wait stays 12.1343; what disappears is
  the departure at t=480.0846.
- No conclusion any example draws moved. SPT still wins in 04. Six agents is
  still cheapest in 09. The overloaded-model warnings in 15 still stand.
- 03, 04, 07, 09 and 14 print no run-length line, which is why they show only
  4th-decimal shifts: each of their replications shed its own random sliver.
  Measured directly on the same models, old engine vs new:

  ```
                            limit        old end    overshoot     new end
  M/M/1 rho=0.8 (ex 14)    3000.00     3000.6803      0.6803     3000.0000
  M/M/1 rho=0.8 seed+1     3000.00     3001.1011      1.1011     3000.0000
  long run (ex 07)        20000.00    20000.4462      0.4462    20000.0000
  disciplines (ex 04)      2000.00     2000.0038      0.0038     2000.0000
  ```

**If you agree, re-capture deliberately** — on Windows, with a fresh MinGW build,
so the captured bytes are the ones this project actually ships:

```
cmake --build build
bash tools/baseline.sh check            # read the diffs one more time
bash tools/baseline.sh capture
./build/des regress --capture --force   # --force: it refuses to replace an
                                        # existing expectation without it
./build/des_tests                       # expect 1395 / 1395
```

Both capture steps rewrite every case, not just the ones that moved. The three
regression models that are still byte-identical (`decide`, `variables`,
`shared`) will simply be written back unchanged — check `git diff` afterwards and
expect exactly `teller.expected` and twelve files under `tests/baseline/` to
appear as modified. Anything else in that diff means something other than this
fix moved, and is worth stopping for.

## 5. The second decision hiding inside the first

Putting the deadline on the event list initially kept DRAINED models alive to the
full Length. `decide.des`, `variables.des` and `shared.des` run out of arrivals
at t≈188, 158 and 191 under a 480-minute Length, and they ended at 480 instead —
four regression expectations changed rather than one.

I rejected that. `canStep()` now treats a list holding nothing but the deadline as
empty, so a model that runs out of work still ends where the work ended, and those
three are byte-identical again.

That is a defensible call, not an obvious one. **Arena would run the full 480**
and divide every time average by 480, and there is a real argument that a
terminating simulation with `Length = 480` should report a 480-minute
replication whether or not anything was still happening. If you want that
instead, it is a one-line change in `canStep()` — delete the deadline-only test —
and it changes those four expectations plus anything else that drains early.
It is a bigger change than the bug report asked for, so it is not on this branch.

## 6. Two things this branch could not do

**The Match test.** There is no `tests/v16_tests.cpp` and no Match module in this
repository — the branch is at v15.2 (`9f93bf4 merge v15.2`), and `Match` appears
only as a station NAME in the v9 batching test. The v16 work is not pushed, so
the "Any" case expecting 3 left in lane LA could not be updated. When you bring
it over: that 3 was counting the t=50 arrival, so it should become 2. The same
two-Create model (Interarrival 10 and 20) is reproduced as a new section in
`tests/tests.cpp` — "A time limit stops the clock ON the limit (v16)" — with the
arithmetic worked out in the comments.

**The platform.** `build/` was empty in the environment this was fixed in, so the
work was built and gated with Linux GCC, not MinGW. Examples get no `.exe` suffix
there, so `baseline.sh` was pointed at symlinks. What makes the 12 diffs
attributable to the fix rather than to the platform: the baselines were
byte-clean on Linux BEFORE the change and 12 differ after it. Still, run
`tools/verify.sh` on Windows before trusting a re-capture — MSVC ASan and the
two-front-end warnings leg did not run here. ASan + UBSan under Linux GCC are
clean, with no diagnostics.
