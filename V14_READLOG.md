# v14 Read Log — "somebody who has used Arena, and nothing else"

v13 put a screen on the three layers below it. You could open a model, edit it,
save it without disturbing what you had not touched, and run it.

You could not build one.

That is not a missing keystroke — `^N` had added a row since the first day. It
is that a person who knows Arena and does not know *this* had no way to find
out what any of it meant. Seventeen tabs with no indication which held
anything. Sixty-nine columns with names and no explanations. A `Next` column
that is the single most important idea in the format — it is where a
*connection* lives, the thing Arena draws as a line between two shapes — and
nothing on screen said so.

Status: **1397/1397 checks** (1069 in v13), clean under GCC 14.2 and Clang 19.1
with `-Wall -Wextra -Wpedantic`, clean under MSVC AddressSanitizer and under
WSL Linux GCC with ASan **and UBSan**, all 15 gated examples byte-identical,
and `REGRESSION CLEAN` over four model files.

---

## The bar

> Somebody who has used Arena sits down cold and builds M/M/1.

Not "reads the README and then builds M/M/1". The screen has to be enough.

That is a testable claim, and the test for it is the last section of
`tests/tui_tests.cpp`. Every key in it is a key — no `setCell`, no `setColumn`,
no `setType`. If a field cannot be reached by pressing what is on screen, it
fails.

---

## 1. The schema explains itself

`Column` gained a `help` line and `ModuleSchema` gained one. Seventeen module
descriptions and sixty-nine column lines, written for somebody who knows Arena.

The obvious place to put that prose was the terminal UI. It went into the
**schema** instead, for the same reason the registry exists at all: the schema
is the one thing a front end reads in order to render, and a second front end
would need the same sentence. Prose in the UI would be prose one front end has
and another does not.

Every helper in `src/ModuleSchemas.cpp` takes `help` as a parameter, so a
column cannot be *declared* without one. A test walks the registry and fails on
any column whose help is empty or no longer than the column's own name — so a
column added in v18 cannot ship unexplained. That is the same shape as the
registry itself: a rule enforced by walking what exists rather than by a list
somebody has to remember to update.

The one that matters most:

> **Next** — Where entities go from here. THIS IS THE CONNECTION: Arena draws a
> line between two modules, this engine names the next block here. Blank means
> they leave the system.

---

## 2. Two things that broke on the path this version exists to serve

Both were found by asking what happens when you actually do the thing, and
neither could have been caught by an existing test, because **every model file
in the repo carries a `[Run]` row**.

**Pressing `^R` on a model built from scratch aborted the program.** No `[Run]`
row means no stopping rule, and `initialise()` asserts one exists. In the
terminal UI that is worse than a crash: `abort()` skips destructors, so the
console is left in raw mode with no cursor and the user's shell is broken.

`readRunSetup` already *warned* that a model with no stopping condition "ends
only when the model runs out of events". The promise was right; nothing made it
true. An empty `AnyOf` is never met, so the run ends when the event list
empties. `AnyOf::describe()` says `until the model runs out of events` rather
than `AnyOf[]`.

**A run could not be interrupted.** The loop advanced and `continue`d without
ever reading a key, so `Escape` was never seen — and an unbounded model would
have run until the process was killed. `ITerminal` gained a fourth method,
`keyPending()`, and the loop blocks on a key only when nothing is running.

The Win32 side has a trap worth naming: `PeekConsoleInputW` reports mouse,
focus and resize records too. Those are **drained** rather than treated as a
keypress — otherwise the handle stays signalled and the run never advances at
all. The same bug wearing the opposite face.

---

## 3. Pick lists, and the one case that must not break

Arena has a drop-down on every enum and every reference. `Enter` on such a cell
now opens a list.

The design question was not how to draw it. It was **where the list comes
from**, and the answer decided everything else: the list and the compiler's
reference pass are now the **same function**.

```cpp
std::vector<std::string> referenceCandidates(const ModelDocument&,
                                             const std::string& targetType);
```

`isDeclared()`, which the reference pass has used since v11, is four lines over
it. Two lists built from the same *idea* drift, and the way that failure
surfaces is a menu whose choices are then rejected — which is worse than no
menu, because it makes the tool look broken rather than the model.

**And then the case that must not break.** Building a model means naming things
before they exist: you wire `Serve → Out` while `Out` is still an idea. No list
can offer that.

So `Escape` in the picker does not cancel. It drops into the **text editor**,
carrying the cell's current text, and the footer says `Esc  type it instead`. A
second `Escape` abandons. When nothing has been declared at all, the list does
not open empty — it goes straight to the editor and says why.

That is the whole feature in one decision: the list is for when you know, and
it gets out of the way the instant you do not.

---

## 4. The blank page

v13's answer to a blank document was an empty box.

`^T` fills it with a working single-server model — arrivals, one server, an
exit, a `[Run]` row — with comments in it, that runs as it stands. It refuses
on a document that already has rows: a single keystroke must not be able to
discard a model.

A starter model that needs a fix before it works teaches the wrong first
lesson, so the test runs it rather than only compiling it.

`?` lists the keys and says the one thing an Arena user will not guess:

> There is no canvas. A module type is a tab, its rows are a table, and a
> CONNECTION is the name of the next block typed into a Next cell.

`^F` shows the wiring. It reads the **cells**, not a compiled `Model` — a
document with a dangling exit does not compile, and that is exactly when
somebody needs to see the shape of what they have. It names exits that point
nowhere and blocks nothing arrives at, and `Enter` jumps to the block under the
marker: a picture you cannot navigate from is a second place to look rather
than a way of getting around.

Tabs carry their row count and empty types are dimmed, because seventeen tabs
with no way to tell which hold anything meant tabbing through all of them to
find out what a model contained — which is the question the tab bar is for.

---

## 5. What only looking found

v13 established that "the screen is a value" makes rendering *testable* but not
*self-evaluating*. v14 found four more this way, by rendering to stdout and
reading it. Not one of them would have failed a test that existed.

**A Decide's `Next` is its ELSE exit**, taken only when no branch matched. The
flow view printed it first:

```
Sort    Decide    Next -> Small   size > 7 -> Big
```

which reads as *Small is the default, tried first* — the exact opposite of what
happens. Row order is semantic in this format and the view scrambled it. It now
reads:

```
Sort    Decide    size > 7 -> Big   else -> Small
```

The view built to make wiring visible was the thing misrepresenting it.

**Help was cut off mid-word at the border** — `Arena draws a line, thi` — which
reads as a rendering fault rather than as help. The empty-grid teaching block
already wrapped; nothing else did. `wrapText()` now does it in one place, and
where a hard cap still applies the last line ends in `...` rather than stopping
silently mid-sentence.

**The selected field's help is drawn under the field**, so a cursor resting on
the bottom visible row had nowhere to put it: the one field that shows its help
was exactly the field that could not. The detail pane now scrolls three lines
early.

**Opening any model at all landed on `Variable`** — an empty table explaining
global counters, with the model itself two tabs away and no sign of it. It had
been that way since v13 and nobody noticed, because the tabs did not yet say
which held anything. Opening now lands on the first *flowchart* type with rows.
`teller.des` begins with its `[Run]` record, and settings are not what a person
opened it to see.

---

## 6. The decisive test passed first time, which was the problem

Four versions running, the test written to prove a version's central claim has
failed on first contact and exposed a defect in the layer below. v14's passed.

In this project that has meant a test that measures nothing, so it was
sabotaged three ways:

| Sabotage | Caught? |
|---|---|
| `Escape` in the picker cancels instead of dropping into the editor | Yes — the whole chain collapses. `Serve` is never wired, so the Process row is never reached |
| The picker writes the display label `(none)` rather than the empty value | Yes — by exactly one check, the one that unwires an exit |
| `referenceCandidates()` short by one row | Yes — and it takes the *compiler's* own tests down with it |

The third is the interesting one. Breaking the pick list broke the reference
pass, because they are one function. That is what the design was for, and the
sabotage is the only thing that demonstrates it.

---

## 7. What v12 duplicated, and what it cost

Reported by hand, from running it: a multi-replication run printed only the
summary.

The root cause was not a formatting bug. `RunController::report()` had
**invented a second, poorer report format** rather than reusing
`Experiment::report()`'s — two places that answer "what did this study find",
which is one question. With an unlabelled `+/-` beside each number a reader
could not tell whether they were looking at the last replication or an average
over all of them.

Both now call `reportReplicationTable()` and `reportReplicationSummary()`. That
`Experiment::report()`'s output did not change by a single byte is proven by
the baseline gate, because `examples/08_replications_ci.cpp` calls it and is
byte-compared.

v4 exists to say that a single run is one sample from a random variable.
Hiding the samples argues against that.

---

## Rules this version added

- **A list a person chooses from and the check that validates the choice are
  one function.** Two lists built from the same idea drift, and the failure
  surfaces as a menu whose choices are rejected.
- **Prose that explains a field belongs in the schema, not in the front end.**
  The schema is what a front end reads in order to render; a second front end
  would need the same sentence.
- **A picker must have a way out into typing.** Anything you build by naming
  things before they exist has a moment the list cannot help with.
- **A cut sentence says it was cut.** Stopping at a line boundary in the middle
  of a sentence reads as a rendering fault, which is worse than an ellipsis.
- **A view built to make something visible is the thing most worth checking for
  misrepresenting it.** The flow view had the Decide's branch order backwards.

---

## Still open

- A model built row by row still gets no `[Run]` row, so it runs to event-list
  exhaustion. Honest, and not what somebody wanted.
- No redo, and undoing back to what is on disk still leaves the dirty flag set.
  That needs a saved-at marker in the undo stack; claiming "not dirty" wrongly
  is how work gets lost, so it stays set until it can be right.
- The grid truncates columns at the right edge, `Next` included, which is the
  one a person most wants to scan. The detail pane is the answer today.
