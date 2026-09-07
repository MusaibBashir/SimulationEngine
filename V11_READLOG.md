# v11 Read Log — "the model becomes a document"

Through v10 a model was still a program. The *fields* had become text —
`EXPO(0.8)` instead of `exponential(0.8)` — but the model itself was a sequence
of C++ calls, and changing one still required a compiler.

v11 makes the model **data**: a set of schema-described tables that round-trip
through a text file and compile into a runnable `Model`, or into diagnostics
that point at individual cells. The engine never learns what a document is.

Status: **779/779 checks** (543 in v10), clean under GCC 14.2 and Clang 19.1
with `-Wall -Wextra -Wpedantic`, clean under MSVC AddressSanitizer and under WSL
Linux GCC with ASan **and UBSan**, and all 15 gated examples byte-identical to
their captured baselines.

---

## What had to be true first

| Wanted | Blocked by |
|---|---|
| A spreadsheet a person edits | The columns of a module are not written down anywhere |
| A model saved to a file | Half the model is `route()` calls, which have no cell to live in |
| An error shown in a cell | v10's diagnostics carry a span, and nothing that says *which cell* |
| A front end that opens and saves | Reading a file and writing it back must not change it |

---

## 1. Routing became a column

Arena draws connections. You drag a line from one module to the next, and the
connection is a thing on a canvas with no row in any spreadsheet.

A terminal front end cannot draw lines. So the exits had to go **into the
tables**: a Process has a `Next` column, a Separate has `Next` and `Duplicate`,
a Process that balks has `Balk To`. The alternative — a separate Connections
table — was rejected because it puts the two halves of one fact in two places,
and a person editing a Process would have to go somewhere else to say where its
entities go.

This is not a compromise the engine had to be talked into. Every routing call in
`Model` already attaches an exit to a block: `route(from, to)` reaches into
`from` and sets a pointer. A column is what that always was.

## 2. Repeating groups became flat child tables

A Decide has N branches; an Assign has N fields. Arena shows those as a grid
*inside* the module's dialog — a table nested in a row.

They are separate top-level tables here, `DecideBranch` and `AssignField`, each
with a column naming its parent. The property being protected is the one from
v10's plan: **a module added in v13 renders in an unmodified front end**. That
holds only if there is *one shape* to render. A nested grid is a second shape,
and a front end would need to know which modules have one.

The consequence is not free, and it is the sharpest edge in the format: **row
order is semantic.** A Decide takes the first branch that matches, and an Assign
runs its fields in order so a later one reads what an earlier one wrote.
Reordering two rows in a text editor changes what the model does. Rows are
therefore addressed by **position** throughout — never by a stable id, because
the two disagree the moment a row moves.

## 3. Byte-identical round-trip, and what it cost

Read a `.des` file, write it back, get the same bytes — comments, blank lines
and spacing included.

Storing cell values cannot do that. A document holding only `Name = Serve` has
no way to reproduce the blank line above it or the comment beside it. So the
document keeps its **source lines** as well as its values, and writes them back
verbatim unless a row has actually been edited.

That is more state than a document layer wants, and it is worth being explicit
about why it earns its place. A front end that cannot save is annoying. A front
end that saves a file the user did not edit, silently reformatting it and
destroying their comments, is *worse than one that cannot save at all* — the
damage is invisible until they diff it. Only edited rows are re-emitted, in
canonical schema-column order; everything else comes back untouched.

The same property is why `.gitattributes` now pins `*.des` to LF. Git on Windows
would check the files out with CRLF, the reader drops CR and the writer emits
LF, and the guarantee this format is built on would be quietly false on the
platform it was developed on.

## 4. Queue had to become read-only

The module list agreed during design had an editable Queue module, matching
Arena. It did not survive contact with the engine.

There is no queue object in this engine apart from its Process. A discipline is
a constructor argument to `Station`; the queue is a member. An editable Queue
module would have held the same fact twice — set FIFO on the Process row, set
LIFO on the Queue row, and nothing decides which wins.

So `ModuleSchema` grew a `readOnly` flag, Queue is the first module to use it,
and the front end will render it as a view. Worth recording as a design error
caught while writing the spec rather than while writing code, which is the
cheapest place to catch one.

## 5. `validate()` refactored into `checkStructure()`

The reference pass needs the structural checks — an entry that exists, a Process
whose resource is declared — but as a *list*, not as a thrown exception.
`Model::validate()` had them, and threw on the first.

Three options, and the two that were rejected are the interesting ones.
*Catching the exception* would have reported one problem per compile, which is
the whole failure mode this layer exists to avoid. *Duplicating the checks in
the compiler* would have made two implementations that drift, and the one that
drifts is always the one nobody runs. So there is one implementation,
`checkStructure()`, returning `std::vector<Diagnostic>`; `validate()` is a
wrapper that throws the first error it finds, with the same message it always
threw. The v10 constraint — `validate()` must behave exactly as before — is what
kept the refactor honest.

## 6. Errors are data, one level up

v10 made a bad expression a diagnostic instead of an exception. v11 makes a bad
*cell* one, and the two compose: the span v10 measured survives **inside** the
cell v11 names. `EXPO(0.8` in a Process's Service column reports as
`Process row 1, Service (col 9): expected ')'` — module, row and column from the
compiler, character offset from the parser, which still has no idea what a table
is.

All four passes run, not just the first, so one compile reports every bad cell.
The exception is the structure pass, which cannot run with broken references; it
is skipped and `structureChecked` says so out loud, because reporting a model as
checked when it was not is the failure this project keeps refusing.

Unknown module types and unknown columns are **warnings**, and are preserved
verbatim. A v11 front end opening a v13 file must not silently delete the
modules it does not understand.

---

## The bugs

The valuable part.

**Two gates were not gating.** Both found while wiring up the CLI, and both
worth more than the feature that exposed them.

`tools/baseline.sh` ran `build/examples/Debug`, where the Visual Studio
generator puts binaries. The build directory had since been reconfigured with
MinGW Makefiles, which puts them in `build/examples` — and the Visual Studio
output from before the switch was still sitting there. So for tasks 8 and 9 the
byte-identical gate ran **three-week-old binaries that predated every line of
v11** and printed `BASELINE CLEAN` over code it had never executed. Re-run
against the real binaries: still 15 examples, still 0 differ. The gate was
right; it had not earned the right. It now asks the generator where the binaries
are, and refuses to run any older than the sources.

`tools/verify.sh` listed the test sources by hand, and `tests/document_tests.cpp`
was never added to that list. Every test written for v11 went through MSVC and
nothing else, while the script printed `VERIFY CLEAN`. It globs now, and the CLI
is a third link unit.

**Then the fix for the first one repeated the lesson.** The staleness guard added
to `baseline.sh` called `exit 1` from inside `$(examples)` — a command
substitution, so it killed only the subshell. The loop ran over an empty list and
`check` reported `BASELINE CLEAN` over **zero examples**: a gate that measured
nothing and said it was clean. `check` now fails when nothing was checked, and
the guard was verified by making it fire.

**Six assertions that could not fail.** Written as
`for (const Diagnostic& d : diagnostics) check(...)`, which runs **zero times**
when the list is empty — so a compile that reported nothing passed a test
asserting what it should have reported. Exactly the shape v10's readlog records
and calls out. Replaced with `complainedAbout()` value helpers; the check count
rose 727 → 730, which measures precisely how much those six were asserting
before: nothing.

**Adding a field to `Diagnostic` was additive for compiling and not for
warnings.** Fifteen v10 sites write `Diagnostic{severity, span, message}`, and
under `-Wextra` every one of them warned about the member they do not set. The
fix is a constructor with a defaulted parameter rather than aggregate
initialisation, and the general point is that "additive" has to mean
*warning-free*, or the gate stops being usable and then stops being run.

**v10's "there is no distribution field any more" was one short.** A Process's
reneging patience was still an `IDistribution`. The schema declares that column
as an Expression, so `Station::m_patience` became an `ExpressionPtr` and
`Model::renegeAfter(process, patienceText, to)` joined the text overloads. Not a
bug in v10 so much as an incomplete sweep that a schema written from the outside
found immediately — which is a fair argument for writing the schema.

**A `__placeholder` hack, refused.** A document declares a block and its fields
in separate tables, so the build pass needs to create an empty Assign and fill it
later; `Model` had no call for that. The first version passed a sentinel
attribute name and deleted it afterwards. It is `Model::assign(name)` now,
because a hack that exists to satisfy an API is a missing API.

**Two of my own v11 tests asserted more than they meant.** Both checked that a
whole compile succeeded for a document that is a single `Resource` row. That was
true while `compile()` stopped after the schema pass, and false the moment the
build pass ran and correctly reported no source and no entry. They assert what
they meant now — that the *schema pass* found nothing wrong.

**`des run` and `whenDrained()`.** The plan had the CLI stop on
`anyOf(whenDrained(), timeLimit(100000))`. `DrainedRule` is met the **first**
time the system happens to be empty, which for a model with random arrivals is
usually just after the first entity leaves — a one-entity report that looks like
a real one. This is the third time that rule has bitten this project. `des run`
takes the horizon as an argument and prints the default it used.

---

## Verified

- 779/779 checks; 236 new. They cover schema lookup and column typing, document
  row addressing by position, the reader and writer in both directions,
  round-trip as a *property* rather than an example, all four compiler passes
  reporting at the cell, unknown modules and columns surviving as warnings,
  child rows keeping their file order, and `checkStructure()` reporting what
  `validate()` throws.
- **The decisive one:** four hand-written `.des` files — a teller, a Decide by
  condition, a Variable written by an Assign with a service time that reads an
  attribute, and two Processes sharing a Resource — each compiled from the file
  and built again in C++ from one seed, traced event for event, **byte-identical
  over 1,371 lines**. Whole-file comparison this time, not v10's event table
  only: both runs are built through the same `Model` API, so the header must
  match too.
- The files are **hand-written, not exported**. An exporter would only have
  proved that the compiler round-trips its own output, and would have hidden
  whether a person can write the format at all.
- That test was verified to be capable of failing, twice: perturbing a service
  time fails it in the header at line 10, and moving a Decide's threshold from
  `size > 7` to `size > 5` — which changes no description at all — fails it at
  event line 45. A test that passes on the first run has earned a falsification
  check.
- 15 examples byte-identical to their baselines, now demonstrably against
  binaries built from the current source.
- Clean under GCC 14.2 and Clang 19.1, MSVC AddressSanitizer, and WSL Linux GCC
  with ASan and UBSan.

## Still open

**A document carries no run length.** There is no Run or Replicate module, so
`des run` takes the horizon on the command line. Adding one is a schema change
and belongs with whatever version owns experiments.

**`compile()` is one-way.** Document → Model only. A `Model` cannot be written
back out as a document, so there is no "import my C++ model" path. That was a
deliberate scope decision: the export direction needs every block to describe
itself in cells, and nothing needs it until something wants to generate models.

**Variable arrays** (Arena's 1-D and 2-D) are still absent, as in v10.

Also still open from v9 and unchanged: `12_shared_resources` is not reproducible
run-to-run and remains excluded from the byte-identical gate with the exclusion
printed every time; a `Separate` duplicate is counted as an exit it never arrived
for, absorbed by the `if (inSystem > 0)` guard in `noteExit`. Resource schedules,
preemption, batch means and distribution fitting are untouched.

---

## v12 — the order to do it in

Runtime control. The TUI is a **separate repository, after v12** — that was
settled in the three-version design at v10, and an earlier draft of this
section said otherwise.

1. **A steppable run.** `SimulationSystem::run()` becomes `while (stepOnce())`,
   so a caller can own the loop and therefore pause, cancel and watch.
2. **`RunController`** — the replication loop and the state machine, with
   `Experiment::run()` refactored onto it.
3. **Progress that admits when it cannot tell.** A `whenDrained()` run has no
   knowable fraction, and a progress bar that reads 0% and then jumps to 100%
   is worse than one that says it does not know.
4. **A `[Run]` module**, so a model file carries its own run length and the
   open item above closes.
5. **A regression harness** over model files, byte-identical, that fails when
   it checked nothing.
