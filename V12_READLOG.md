# v12 Read Log — "the run becomes something you can drive"

v10 made a model's fields text. v11 made the model itself data. Both left the
*run* the same shape it had in v1: `run()` blocks until the stopping rule is
met, and the caller gets control back when it is over.

That is the right shape for a program and the wrong one for a front end. There
is no moment at which it can redraw, and no way to stop. v12 turns it inside
out: `stepOnce()` does one event, `RunController` owns the loop around it and
the replication loop above that, and a caller advances a budget of events,
redraws, and advances again.

Status: **930/930 checks** (779 in v11), clean under GCC 14.2 and Clang 19.1
with `-Wall -Wextra -Wpedantic`, clean under MSVC AddressSanitizer and under
WSL Linux GCC with ASan **and UBSan**, all 15 gated examples byte-identical to
their captured baselines, and `REGRESSION CLEAN` over four model files.

---

## What had to be true first

| Wanted | Blocked by |
|---|---|
| A front end that can redraw mid-run | `run()` does not return until it is finished |
| Pause and cancel | Nothing to pause; the loop is inside the engine |
| A progress bar | No rule can say how far through it is |
| A harness over many models | A model file carries no run length |
| Comparing report text | `report()` writes to `std::cout` and nowhere else |

---

## 1. Stepping, not threads

The obvious way to make a run watchable is to put it on its own thread and poll
a mutex-guarded snapshot. It was rejected, and the reason is the same one that
has decided most of this project's arguments: **the engine's entire claim is
that one seed gives one run.** Concurrency buys responsiveness that a stepped
loop already provides, and pays for it with a lock between the clock and the
statistics, and with a whole class of bug that no byte-identical gate can
reliably reproduce.

A stepped loop needs no locks, no atomics and no new failure modes. Pausing is
not calling. Cancelling is not calling ever again. And `run()` becomes
`while (stepOnce()) {}`, which means the fifteen byte-identical examples are
testing the stepped path too, without a line of new test code.

## 2. What the extraction touched — and a comment that was wrong since v2

Splitting `run()` meant touching the loop this project has warned about since
v2:

> ORDER IS EVERYTHING: close the integrals for the interval that just ended —
> using the OLD state — then move the clock, then let the handler change state.
> **Swap any two and every time average is wrong.**

Because this was the riskiest edit in the version, the warning was tested by
actually swapping the lines. Two of the three pairs were tried:

- **Integrals *after* the handler: 11 of the 15 gated examples change.** True,
  and it is the pair that matters — the integrals would measure the *new* queue
  length over an interval that ended before it changed.
- **Integrals and the clock swapped: nothing changes at all.**
  `updateAllIntegrals` takes its target time as a *parameter*, and every
  accumulator carries its own `lastUpdate`. It never reads the clock. Those two
  lines commute.

So the warning was over-broad in a third of its cases. The line order is
unchanged, because reading it in causal order is worth something; the comment
now names the pair that is load-bearing. **A warning that is wrong in a third of
its cases stops being read**, which is worse than no warning at all.

One structural change was unavoidable. `EventType::EndSimulation` used to
`return` out of `run()`, which a caller-owned loop has no way to observe. It
sets `m_stopped` and `stepOnce()` reports `false`: that event *ends* the run
rather than advancing it. And `canStep()` deliberately does **not** test
`m_initialised` — being initialised is a precondition both callers assert, and
folding it in would turn "you forgot to initialise" into a run that silently
does nothing.

## 3. Progress that admits it cannot tell

`ITerminationRule::progress()` returns `std::optional<double>`, and **nothing
means cannot tell, never zero.**

`TimeLimit` divides. `EntityLimit` divides. `AnyOf` reports the largest
fraction any child knows, because the run ends when the *first* rule is met and
the most advanced child is the honest estimate. `DrainedRule` has **no
override at all**, with a comment saying so: whether a system will next be empty
is not knowable in advance.

A progress bar that reads 0% for the whole of a `whenDrained()` run and then
jumps to 100% is not an approximation. It is a lie the caller has no way to
detect, and a front end that receives `nullopt` shows a spinner instead.

This is the fourth time this project has had to write that rule down, after
`VisitRatios::exact`, v9's silently-zero WIP, and v10's unknowable arrival mean
that made ρ = 2 look stable.

## 4. One replication loop, two callers

`Experiment` had its own loop over replications; `RunController` needed one that
could be driven in pieces. Duplicating it would have left two implementations
that must agree about seed derivation, antithetic pairing, warm-up and the
observation grid — and the one that drifts is always the one nobody runs.

So `Experiment::run()` is a wrapper, exactly as v11 made `validate()` a wrapper
over `checkStructure()`.

The order the evidence was gathered in is the only order in which it means
anything: **the controller was shown to match `Experiment` to 1e-12 on every
replication *before* the old loop was deleted.** Then it was deleted and the
numbers did not move.

`Experiment` passes no stopping rule, and `RunController` leaves the builder's
rule alone when the setup names none. The first version built an empty `AnyOf`
instead of a null — which satisfies `initialise()`'s assert and *silently
replaces* whatever the caller set inside their builder. Every `Experiment` in
the project sets its rule that way.

## 5. Antithetic pairing is where the abstraction leaks

With `antitheticPairs(true)`, one replication is **two runs** — the second with
every uniform mirrored — averaged into a single result, because the pair is one
observation and treating its halves as independent would understate the
interval.

That does not fit "advance N events and report progress" cleanly.
`RunProgress::replication` counts replications rather than runs, because that is
what `replications(n)` means to the caller who asked for it; `fraction` is the
fraction through the current *run*, and the pair's second half restarts it at
zero. A front end showing a per-replication bar has to say which half it is on
or the bar appears to go backwards. There is no tidier answer — the abstraction
genuinely leaks here, and the honest response is to document where.

## 6. A run length in the model

v11 left `des run` taking a horizon on the command line, and the regression
harness forced the issue: **an expectation whose meaning depends on a number
stored outside the model stops being an expectation the moment that number
changes.**

So `[Run]` — Arena's Run Setup — is a data module like any other: Length,
Warm-up, Replications, Base Seed, Stop When Drained, Max Entities, Separate
Streams, Antithetic. At most one row, and that is checked by `readRunSetup`
rather than by the schema, because "how many rows may this table have" is not a
property of a column.

A `[Run]` naming **no** stopping condition is a *warning*, not an error. It
looks like a model that runs forever, and for a Create with `Max Arrivals` set
it is not: the future event list empties and the run ends on its own. Refusing
it would reject a correct model.

v11's `compileInto` signature is untouched. `readRunSetup` is a free function,
because adding a parameter would have broken every v11 caller for no gain.

## 7. The harness, and what v11 taught it

`des regress` runs a manifest of models and diffs their report text
**byte-identically, with no tolerances**. A stored mean with a tolerance passes
a real regression that happens to land inside the band, which is the
"near-miss average" standard v10 rejected.

Three properties it has from birth, each because v11 shipped a gate without one:

- **It fails when it checked nothing.** `baseline.sh` reported `BASELINE CLEAN`
  over zero examples for one revision.
- **`--capture` refuses to overwrite** an existing expectation without
  `--force`, and says which file it refused.
- **Expectations are `.expected`, not `.txt`** — `.gitignore` excludes `*.txt`
  with one hand-maintained negation, and a second one is a trap. Verified with
  `git check-ignore` rather than assumed.

It does not absorb `tools/baseline.sh`. That script gates fifteen compiled
example programs, which means launching external binaries — the one thing a
shell script is genuinely better at.

---

## The bugs

The valuable part.

**A harness that could never capture anything.** `readFile` used
`in.good()` to mean "the file opened". On the GCC this project builds with,
constructing an `ifstream` on a file that does not exist leaves `good()`
reporting **true** while `is_open()` reports false — so every missing
expectation looked present, and `--capture` refused all four, every time, into
an empty directory.

What made this worth chasing rather than assuming was that a standalone probe,
compiled with the `g++` on `PATH`, reported `good() == false` for the same path.
Two compilers, two answers, one of them the one the product is built with. Both
readers use `is_open()` now, which is the unambiguous question anyway: *did this
file open*, not *is this stream in a usable state*.

**A test that could not fail — again, and it took three attempts to find out.**
The chunk test traces every shipped model four ways and compares byte for byte.
It passed on the first run, so it was attacked:

| Sabotage | Result |
|---|---|
| Skip an iteration of the budget loop | **no failure** |
| A watcher that secretly calls `stepOnce()` | **no failure** |
| A watcher that consumes one random number | caught, all four models |

The first two are not test bugs; they are the point. `run()` *is*
`while (stepOnce())`, and the engine is deterministic given its event list, so
nothing about call scheduling can change the sequence. Those assertions are
close to tautological, and the file now says so rather than letting them read as
evidence they are not.

The claim only has content one level up, where `RunController` carries a
replication index, an antithetic half-flag and a captured report **across**
`advance()` calls. A three-replication study driven at budgets of 1, 7, 97 and
1,000,000 must give identical seeds and identical numbers — antithetic pairing
included, where a boundary can land between the halves of a pair. That test was
attacked too: making the seed depend on `m_events` broke the v4 and v8 interval
tests but **not** this one, because `m_events` at each rollover is the same
whatever the budget. Making it depend on the number of `advance()` calls — which
is exactly the leak being excluded — produces 25 named failures.

Three sabotages that proved nothing before one that proved something. That
ratio is the honest cost of finding out whether a test is evidence.

**"Replication 4 of 3."** Found by running `des run` by hand on a three-
replication model, not by any test. `m_replication` has already been incremented
past the last one by the time the study finishes, and `progress()` reported it
unclamped. Nothing covered the number a front end would actually render; there
is a test now.

**`.expected` files would have been CRLF on a fresh clone.** The same trap
`.des` hit in v11, one layer out: the harness compares byte for byte against
report text whose lines end in LF, so a clone would have failed the gate on a
tree nobody had touched. `.gitattributes` pins them, and a throwaway `git clone`
confirmed the checkout is LF rather than trusting the setting.

**A third gate that could pass without measuring anything — found by writing a
sentence about it.** The README gained a line saying all three gates fail when
they checked nothing. Before committing that claim it was worth testing, and
`tools/verify.sh` did not: if no compiler is found, `warnings()` has nothing to
do, `FAILED` stays 0, and a machine with no toolchain at all prints
`VERIFY CLEAN` — the same hole `baseline.sh` had, in the other gate. It counts
the legs that actually ran now and refuses to pass over zero, verified by
running it with an empty `PATH`.

Writing the claim down is what exposed it. That is an argument for documents
that state properties rather than describe behaviour: a property can be checked.

**Writing the CLI verb before the code it calls.** `cli/main.cpp` was rewritten
with a `regress` verb in the task before `Regression.cpp` existed, which does
not compile. Reverted and done in the right order. Trivially self-inflicted, and
recorded because "I will just add this now while I am in the file" is how a task
boundary stops meaning anything.

**A `sed`-style replacement that silently did nothing.** One of the edits that
stripped that verb also meant to restore a two-line usage string and did not
match, so the usage text kept mentioning a verb that no longer existed. It went
unnoticed until the CLI was run by hand. A replacement that finds no match
should be an error, not a no-op — and in the scripts driving these edits, it is
one everywhere except where it silently was not.

---

## Verified

- 930/930 checks; 151 new. They cover the state machine and every legal
  transition, `advance()` after cancel, progress on all four rule types
  including the one that refuses to answer, a snapshot agreeing with `report()`
  at the same instant to 1e-12, every `[Run]` column with its default, the
  second-row refusal, the no-stopping-condition warning, and the harness
  catching a deliberate perturbation.
- **Chunk size does not change a run**, over every shipped model, traced four
  ways including one with a watcher between every chunk. Its limits are
  measured and documented rather than assumed.
- **Budget size does not change a study**, at 1, 7, 97 and 1,000,000 events per
  `advance()`, antithetic pairing included. Proven capable of failing.
- Every v4 replication and confidence-interval check passes **unchanged**
  through the refactored `Experiment`.
- 15 examples byte-identical to their baselines; `REGRESSION CLEAN` over four
  model files.
- Clean under GCC 14.2 and Clang 19.1, MSVC AddressSanitizer, and WSL Linux GCC
  with ASan and UBSan.

## Still open

**No per-event observer.** Deliberately: a callback firing 1.4 million times
puts a front end inside the inner loop. Sampled progress and snapshots cover a
progress bar and a live results panel; animation would need the observer, and it
can be added when something needs it.

**A snapshot is a copy.** `snapshotOf` builds vectors every time it is called.
That is fine at a few times a second and wrong at every event, which is another
reason the per-event observer is not this shape.

**`[Run]` has no Base Time Units.** Arena's Run Setup does. This engine's clock
is dimensionless and every duration is in the same unit as every other, so there
is nothing for it to convert.

**`RunSetup::observeInterval` is not a `[Run]` column.** Welch's warm-up grid is
an experiment-level instrument rather than a property of the model, and
`Experiment` sets it directly. Putting a diagnostic tool in the model file next
to the model would be the wrong boundary.

Also still open from v9 and unchanged: `12_shared_resources` is not reproducible
run-to-run and remains excluded from the byte-identical gate with the exclusion
printed every run; a `Separate` duplicate is counted as an exit it never arrived
for. Resource schedules, preemption, batch means and distribution fitting are
untouched.

---

## v13 — the order to do it in

1. **The TUI**, in its own repository, over `ModuleRegistry`, `ModelDocument`
   and `RunController`. Everything it needs now exists; nothing renders it.
2. **Diagnostics in the grid**, using v11's `CellRef`.
3. **Undo**, which is why document rows are addressed by position and why the
   document keeps its source lines.
4. **Resource schedules and preemption**, the two v9 items that change what the
   engine can model rather than how it is driven.
