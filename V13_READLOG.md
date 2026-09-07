# v13 Read Log — "the spreadsheets get a screen"

v10 made a model's fields text. v11 made the model itself data. v12 made the
run something a caller drives. Every one of those was built for a front end
that did not exist, and each was argued for on the strength of a consumer
nobody had written.

v13 is that consumer. It is also the first version that found out whether the
three below it were telling the truth.

Status: **1069/1069 checks** (930 in v12), clean under GCC 14.2 and Clang 19.1
with `-Wall -Wextra -Wpedantic`, clean under MSVC AddressSanitizer and under
WSL Linux GCC with ASan **and UBSan**, all 15 gated examples byte-identical,
and `REGRESSION CLEAN` over four model files.

---

## What had to be true first

| Wanted | Blocked by |
|---|---|
| A spreadsheet per module type | Nothing published what columns a module has — until v11 |
| Editing a cell | A field held a lambda — until v10 |
| A run you can watch and stop | `run()` did not return until it was over — until v12 |
| Any of it tested | A terminal UI is normally tested by looking at it |

---

## 1. Why the TUI came home

The v10 design put the front end in a **separate repository**, and the v11 and
v12 specs both restated it. The argument was good: a repository boundary is
what stops the engine growing a method because a widget found it convenient.

Two things had changed by the time it was due. v11 and v12 had each ended up
adding a consumer *inside* this repo — the `des` CLI — for one reason, written
into both readlogs: **an interface with no caller is an interface nobody has
checked.** The TUI is the real consumer of `ModuleRegistry`, `ModelDocument`
and `RunController`, and keeping it out left those unchecked in the repo that
defines them. And the layering the boundary was protecting turned out to be
available from the target graph instead: `des_ui` links `des_engine`, never the
reverse.

The asymmetry decided it. Splitting a directory out later is mechanical;
merging two repositories later is not.

What replaces the boundary is a rule no gate can check, so it is written down:
**`des_ui` links `des_engine` and never the reverse, and no engine header may
include `Screen.hpp` or a `Tui*` header.** `des.hpp` and `des_ui.hpp` are two
umbrellas with one direction between them — the engine's umbrella does not pull
in the UI, or every example and every engine test would, which is the first step
towards an engine that knows one exists.

## 2. The screen is a value

A renderer fills a `Screen`; the terminal blits it. `render(state, screen)` is
pure and `handleKey(state, key)` touches nothing outside the state, so a test
builds a document, types a scripted key sequence, and asserts on the screen **as
text** — no terminal, no timing, no sleeps.

That decision paid twice. It is what makes the UI testable at all. And it is why
the **portable half lives in `src/`**: `tools/verify.sh` compiles `src/*.cpp`
and its WSL leg compiles `tests/*.cpp src/*.cpp`, so the whole user interface
goes through GCC, Clang, AddressSanitizer and UndefinedBehaviorSanitizer **with
no change to any gate script**. Only `tui/` — three methods of platform code
plus `main()` — sits outside, and platform code cannot be tested anyway.

## 3. The tab bar is computed, and both halves matter

It lists every type `ModuleRegistry` publishes, then any type the document holds
that the registry does not. **No module type name appears anywhere in the UI's
source.**

The first half is the property v11's flat child tables were chosen to protect: a
module added in a later version renders here without this code changing. The
second half is easy to miss and matters as much. v11 *preserves* unknown module
types on purpose, and a UI that showed only what it understood would hide
exactly the rows telling a person their editor is older than the file they
opened. An unknown type has no schema, so its columns come from its own rows.

## 4. Why there is a detail pane

A `Process` has eleven columns — Name, Capacity, Resource, Units, Discipline,
Service, Balk At, Balk To, Renege After, Renege To, Next. That is about 130
characters of grid in an 80-column terminal.

So the grid is for scanning and the **detail pane is for reading and editing**,
which is what Arena does with a spreadsheet and a dialog. It is the only place
editing happens: two editing surfaces would mean two sets of key handling, two
places to get the commit-versus-abandon rule right, and a wide table still
unreadable in one of them.

## 5. Undo is a stack of whole documents

`ModelDocument` is copyable — v12's `RunController::fromDocument` already copies
one — so every mutation pushes a snapshot and `^Z` pops it. A snapshot cannot be
wrong about its own inverse; a hand-written undo for `moveRow` can. A model
document is kilobytes, so the crude answer is also the correct one.

`undo()` leaves the document **dirty** on purpose. Undoing back to what is on
disk is not the same as knowing you are there — that needs a saved-at marker in
the stack — and claiming "not dirty" wrongly is how work gets lost.

## 6. The fourth-time rule, finally drawn

v12 gave `ITerminationRule` a `progress()` returning `std::optional<double>`,
with `nullopt` meaning **cannot tell**, and `DrainedRule` deliberately not
overriding it. v13 is the first thing to render that.

When the fraction is empty the run view draws **no bar** and says the rule
cannot tell. A bar sitting at zero until it jumps to full is a lie the reader
has no way to detect, and it is the fourth time this project has had to write
that rule down — after `VisitRatios::exact`, v9's silently-zero WIP and v10's
unknowable arrival mean.

---

## The bugs

The valuable part, and this version's are the most valuable it has had.

### v11 had the bug its own readlog said it did not

`V11_READLOG.md` says a front end that silently reformats a file and destroys
the comments is **worse than one that cannot save at all**, and then, two
sentences later: *"Only edited rows are re-emitted, in canonical schema-column
order; everything else comes back untouched."*

That second sentence was **false when it was written and stayed false for a
whole version.** `ModelDocument::m_edited` was one `bool` for the entire
document: an untouched file wrote back verbatim, but *any* edit re-emitted
everything canonically and threw away every comment in the file.

Nothing caught it because nothing could edit-and-save except code that did not
care about comments. The TUI is the first caller to reach it, and the scripted
session test failed on its first run — the one test written specifically to ask
whether v11's central guarantee survived contact with a user.

Each row now carries its **own** source block and edited flag, and the block
includes the blank line and comments that *preceded* the header — so a record
carries its annotation when it moves and takes it away when it is deleted, which
is what makes the writer correct under insert, delete and move rather than only
under a cell edit. Editing one cell of `teller.des` now produces a four-line
diff, all inside the record that changed. The readlog now carries the correction
rather than the claim.

One limit, stated rather than discovered later: an edited document comes out
grouped by module type, because that is the order a document holds records in. A
file that interleaved types would be regrouped by an edit. Every file this
project ships already groups them, and an untouched file still round-trips
byte-identically whatever its order.

### Six layout bugs that no assertion could have caught

The UI was rendered to standard output and *read*. Every one of these had passed
every assertion above it, because they all pass a `find()` on the screen text:

| What was wrong | Why it mattered |
|---|---|
| The selected tab was never drawn | Sixteen module types do not fit in eighty columns; the bar stopped at the edge, so `[Process]` — the tab being edited — was invisible |
| The first fix for that was still wrong | It measured the layout without the scroll markers and drew it with them, so the passes disagreed by a column and the selected tab could still fall off |
| The file name was trimmed from the left | On an absolute path that left `C:/Users/User A/Documents/Indu/Simulation` on screen and cut off the file name — every character kept was useless |
| The detail pane did not scroll | Eleven columns plus diagnostic lines; a field below the fold was invisible and unreachable |
| The enum hint ran on from the value | `FIFO FIFO LIFO PRIORITY SPT EDD RANDOM`, which reads as though the cell held all of them |
| Text overflowed the frame | The box was left open on the header row |

And one more of the same kind after the run view was rendered: a `TimeLimit(480)`
run — which always knows how far along it is — reported that **the rule could
not say**, because the controller had let its system go once the run finished.
One message meaning two different things is how a message stops being believed;
a finished run says `Finished` now.

**This is the honest limit of the screen-as-a-value design.** It makes rendering
*testable*; it does not make it *self-evaluating*. Every one of these needed a
person to look, and every one now has an assertion pinning it so it cannot come
back.

### The "too small" message was too big

The terminal-too-small message was 44 characters wide and got clipped to
`...at least 80 x` on the 40-column terminal it was complaining about. The test
caught it because `Screen` **clips rather than crashing** — the behaviour chosen
so a layout bug shows as a missing character instead of an assertion in front of
a user. It made this bug visible instead of fatal.

### v12's staleness guard cried wolf

The guard added in v12 compares the newest source against the newest example
binary. v13 put the UI in `src/`, where no example links it, so CMake correctly
does not relink the examples when `TuiRender.cpp` changes — their binaries are
permanently older than the newest source, and the gate refused to run at all
even straight after a successful build.

A false alarm is the failure mode most likely to end with somebody deleting the
check, which would cost more than the bug it guards against. It compares against
the newest build *artefact* now, which is the question it actually wants
answered: has a build happened since the last edit.

---

## Verified

- 1069/1069 checks; 139 new. They cover the screen as a value including its
  clipping, key normalisation, the computed tab bar with unknown types, opening
  a missing file as a new document, all four mutations and undo across each,
  the read-only refusal, rendering the grid and the detail pane, navigation
  clamping, confirm-before-quit, editing swallowing control keys, and the run
  view.
- **The decisive one:** opening `teller.des`, navigating three module types and
  a detail pane and saving gives back **byte-identical bytes**; editing one cell
  leaves every comment, blank line and untouched table verbatim and re-reads to
  a document carrying the edit. That is v11's round-trip guarantee exercised for
  the first time through the surface a person actually uses, and it failed on
  its first run.
- Each of the six layout bugs has an assertion that pins it.
- The quit guard was verified by removing it: five named failures.
- The comment fix was verified by reverting to v11's behaviour: the test fails,
  naming the comments.
- 15 examples byte-identical; `REGRESSION CLEAN` over four model files.
- Clean under GCC 14.2 and Clang 19.1, MSVC AddressSanitizer, and WSL Linux GCC
  with ASan and UBSan — **including every line of the user interface**, which is
  what putting the portable half in `src/` bought.

## Still open

**No module palette.** v13 edits the types a document already has and the rows
within them. Adding a `Process` to a document that has none needs a picker and
an answer to "where does it go in file order", which is a design question rather
than a missing keystroke.

**An edited document regroups its records by module type.** Stated above; only
reachable by editing a file that interleaved them, and nothing ships one.

**The platform layer has no test.** `Terminal_win32.cpp` and
`Terminal_posix.cpp` are exercised by running the program and nothing else,
which is why they are three methods long.

**No redo.** The snapshot stack gives undo; redo needs a second stack and a rule
for what invalidates it.

**No mouse, no search, no colour themes, no multiple open files.** Deliberate.

Also still open from v9 and unchanged: `12_shared_resources` is not reproducible
run-to-run and remains excluded from the byte-identical gate with the exclusion
printed every run; a `Separate` duplicate is counted as an exit it never arrived
for. Resource schedules, preemption, batch means and distribution fitting are
untouched.

---

## v14 — the order to do it in

1. **A module palette**, so a model can be built from nothing rather than only
   edited.
2. **Resource schedules and preemption** — the two v9 items that change what the
   engine can model rather than how it is driven, and the two Arena has that
   this does not.
3. **Redo**, and a saved-at marker in the undo stack so undoing back to disk
   clears the dirty flag honestly.
4. **Batch means and distribution fitting**, the remaining v9 items.
