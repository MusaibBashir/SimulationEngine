# v10 Read Log — "the model stops needing a compiler"

Through v9 a model was built out of C++ objects. A service time was
`exponential(0.8)`; a condition was a lambda. That works, and it means changing
a model requires a compiler.

v10 makes every one of those fields **text**. Not for convenience — because a
spreadsheet cell can hold `"EXPO(0.8)"` and cannot hold a lambda, and the next
two versions are a document layer and a terminal front end.

Status: **509/509 checks** (293 in v9), clean under GCC 14.2 and Clang 19.1 with
`-Wall -Wextra -Wpedantic`, clean under MSVC AddressSanitizer, and all 15
examples byte-identical to their captured baselines.

---

## What had to be true first

| Wanted | Blocked by |
|---|---|
| A spreadsheet that edits a model | Fields hold *code*, not data |
| Arena's Variable data module | `WIP = WIP + 1` needs an expression with a global on both sides |
| A model saved to a file | `std::function` and `unique_ptr<IDistribution>` members cannot be serialised |

All three are the same blocker wearing different hats, which is why one version
clears all of them.

---

## 1. There is no distribution field any more

The design decision this version turns on, and it came from reading Arena rather
than from the code.

**Arena has no such thing as a distribution field.** A Process's Delay cell holds
an *expression*: type `5` and it is constant, type `TRIA(1,2,3)` and it samples
each time, type `SetupTime * 2` and it computes. There is one kind of field.

So v10 does not add a distribution parser beside an expression parser.
`EXPO(0.8)` is a **function call in the grammar** whose AST node owns one of the
twelve `IDistribution` objects that already existed. Nothing was reimplemented:
`draw()`, `mean()`, `clone()` and `useStream()` were all already there, and the
variance-reduction machinery from v8 kept working untouched.

A whole category collapsed. `Station`, `DelayNode` and `CreateNode` stopped
holding distributions and started holding expressions, and the concept
"distribution field" disappeared from the engine.

The test that settles it: a **parsed** `EXPO(0.8)` drives a 500-unit run
identically to a **constructed** `exponential(0.8)` from the same seed. Same
objects underneath.

## 2. EvalContext is NodeContext, a second time

`DecideNode` must hold an expression, so the core depends on the expression
layer. But `NQ(Teller)` needs live model state, which points the dependency
straight back up. A cycle.

v6 solved exactly this shape for nodes and wrote down why: *publish a
role-specific interface, not the whole class.* `NodeContext` exposes six
operations and a node cannot touch the FEL or the statistics.

The same move works here. The expression layer **declares** `IModelState` — the
four questions an expression may ask about a running system — and
`SimulationSystem` **implements** it. The expression layer never includes
`Model.hpp`.

Worth recording that the *pattern* transferred, not the code. That is the
difference between a lucky abstraction and a real one.

## 3. Saying you do not know, for the third time

`Model::offeredLoad()` called `dist->mean()` to check stability before a run.
An expression's mean may not exist in advance: `EXPO(Rate)` depends on a variable
that changes during the run.

There were three options and only one is honest:

- Treat the unknown mean as **0** — the check passes on a model it never
  examined. A check that passes when it should not is worse than no check.
- **Refuse** any model it cannot judge — rejects perfectly good models.
- **Say so.** `meanIfKnown()` returns `nullopt`, `loadIsKnown()` distinguishes
  "no load" from "cannot tell", and `stability()` names the blocks it skipped.

This is the third time this project has chosen "report that it cannot tell" over
a plausible default, after `VisitRatios::exact` in v7 and the silently-zero WIP
in v9. Three occurrences is no longer a habit; it is the house rule.

## 4. Errors are data, not exceptions

`ModelError` throws because a *programmer* wired the model wrong: there is one
mistake, it is a bug, stopping is right.

A malformed expression is different. It is a person typing into a cell, and
v11's spreadsheet has to show **every** bad cell at once. An exception carries
one failure and unwinds past the rest.

So the lexer and parser never throw. They record a `Diagnostic` carrying a
`SourceSpan`, recover, and keep going — one call, many problems. Error positions
are asserted character-exact in the tests, because v11 will put a cursor on that
offset and "an error occurred" is not enough.

The two callers get what each needs: `parseExpression()` returns diagnostics,
and `expr()` throws, because on the C++ API a bad expression really *is* a
programmer error and there is no cell to point at.

## 5. One namespace for variables and attributes

Declaring a variable named `priority` when an attribute of that name exists is a
hard error.

The alternative is a resolution order — check variables first, then attributes.
That always works and one of the two silently reads the wrong thing. Same shape
as *"a default return value is a place for a bug to hide"*. Refused instead.

For the same reason, `set()` on an undeclared variable throws rather than
auto-declaring. Arena would create it for you; here a typo would otherwise
become a second variable nobody notices.

## 6. A v9 item solved by construction

Item 5 on the v9 list — *"streams for every block; Delay durations and Decide
draws still share the common stream"* — needed no feature.

They shared a stream because they shared a **code path**. Once every sampling
site is a distinct AST node, `useStream()` recurses into each one and they are
independent. There is a test that changes a Delay's duration and asserts the
service stream does not shift.

Generalising something made the middle smaller for the fifth time in this
project.

## 7. Nothing broke, and why that was a constraint

All 293 v9 checks pass unchanged, and 15 examples are byte-identical.

That was not luck. The old API became a **constructor for the new
representation**, never a second code path kept alive beside it:
`LambdaExpression` wraps a `std::function`, `DistributionExpression` wraps an
`IDistribution`. One evaluation route.

Two places needed deliberate restraint. `DecideNode::describe()` still prints
`when ->` rather than the condition text — printing it would move every
example's model description for no gain. And the `Assign` trace line keeps the
attribute case exactly as it was.

---

## The bugs

The valuable part.

**A pre-existing MSVC warning that incremental builds hid.** Editing
`SimulationSystem.cpp` forced a recompile and surfaced a `C4244` in
`reportArenaStyle` that had been there since v9. A clean rebuild confirmed it
was the only one. Warning-clean under GCC and Clang says nothing about MSVC, and
incremental builds say nothing at all.

**`IExpression` needed a virtual `reset()`.** `AssignNode::reset()` reset its
distribution, and a `Deterministic` walks a cursor through a list. Wrapping it in
an expression without plumbing `reset()` through would have made a second
replication draw from where the first stopped — the exact class of bug the v2.1
reset pass existed to kill, reintroduced by an abstraction.

**`arrivals ~ <none>`.** The trace header read `m_interarrival`, the copy kept
for the stability check, which a model built with `arrivals("EXPO(1.0)")` never
fills in. It printed `<none>` for a model that plainly had arrivals. It asks the
source now.

**Two wrong test expectations, both mine.** One predicted an arrival count
instead of asserting the relationship under test; the other used an accessor
`Statistics` does not have. Both are now written to assert relationships rather
than numbers guessed in advance — which is what they should have been.

**A build process that outlived its build.** A backgrounded `cmake --build` left
`cl.exe` holding a header against edits, producing permission errors that look
like a filesystem fault and are not.

---

## Verified

- 509/509 checks; 216 new. They cover operator precedence and associativity
  against hand-computed values, error positions asserted character-exact, parser
  recovery reporting several problems from one call, `meanIfKnown` on all four
  of its cases, variable time-averages against a hand-worked case, warm-up
  handling, name-collision refusal, and validation catching an attribute
  reference in a field that has no entity.
- **The decisive one:** the same model built twice from one seed — once from C++
  objects, once from strings — traces **identically, 1,137 event lines, byte for
  byte**. The comparison is of the event table, not the whole file: the header
  prints each field's own description, so `EXPO(1)` against `Exponential(mean=1)`
  is the two spellings reporting themselves honestly rather than a difference in
  what was simulated.
- 15 examples byte-identical to their baselines. `12_shared_resources` is
  excluded, and that exclusion prints on every run — see below.
- Clean under GCC 14.2, Clang 19.1, and MSVC AddressSanitizer.

## Still open

**`12_shared_resources` is not reproducible.** Six consecutive runs give six
different outputs despite a fixed seed of `2718u`, which breaks this project's
own rule that the same seed means the same run. It is a **v9 bug**, not a v10
one, and it is excluded from the byte-identical gate with the exclusion printed
every time — a gate that silently skips a case is not a gate.

What is known, measured before any v10 code existed: lines 1–17 of the output —
the model description, visit ratios and stability check — are identical between
runs, and divergence begins at line 18, the first run result. So the
*simulation* diverges, not the analysis around it. That rules out the obvious
suspect, the pointer-keyed `unordered_map` in `VisitRatios`, since ASLR would
have shown above line 18. The queue disciplines resolve ties to the earliest
index deliberately, and `Resource::m_users` is a vector in construction order, so
neither explains it. An uninitialised read fits the evidence and no sanitiser
available on this platform detects one.

**UBSan is unavailable here.** MinGW ships no `libubsan`, MinGW Clang has no
runtime for the windows-gnu target, and MSVC has none at all. Recorded rather
than quietly dropped, because "the sanitiser said nothing" and "the sanitiser
did not run" look identical in a terminal.

Also still open from v9, untouched: resource schedules, preemption, batch means,
distribution fitting. Variable arrays (Arena's 1-D and 2-D) are deliberately not
in v10.

---

## v11 — the order to do it in

1. **Module schemas.** Each module type publishes its columns: name, type,
   default, validation. Discovered at runtime, so a data module added in v13
   renders in an unmodified front end.
2. **`ModelDocument`** — rows of typed cells, as text. It may be invalid,
   half-finished and contradictory, which a `Model` may never be.
3. **`compile()`** → a `Model` or a list of diagnostics carrying row and column.
   `Model::checkExpressions()` already returns exactly that list; v11 wraps cell
   identity around the spans instead of throwing.
4. **A file format**, versioned, migrating forward. Models are the durable
   artifact, not binaries.
5. **The data modules**: Variable, Entity, Queue, Resource, Expression.
