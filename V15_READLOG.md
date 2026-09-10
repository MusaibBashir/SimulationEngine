# v15 Read Log — "the text is the model"

v13 put a spreadsheet grid on the screen. v14 taught every cell in it to
explain itself. Both were built on the same idea: the `ModelDocument` is the
thing you edit, and the file is what it gets written to.

v15 turns that around. The **file** is the thing you edit, and the document is
what gets parsed out of it. A module palette on the left, the `.des` text on
the right, and Flow, Runs and Results as tabs across the top.

Status: **1368/1368 checks**, clean under GCC 14.2 and Clang 19.1 with
`-Wall -Wextra -Wpedantic`, clean under MSVC AddressSanitizer and under WSL
Linux GCC with ASan **and UBSan**, all 15 gated examples byte-identical, and
`REGRESSION CLEAN` over four model files.

---

## Why the grid went

Three things fall out of the inversion, and each of them was work in v13:

**The byte-identical round trip is free.** v11's central guarantee — open a
file, save it, get the same bytes — turned out to be *false* the first time v13
exercised it, and fixing it took per-row source blocks, an attributor that
decides which comment belongs to which record, and a merged writer. All of that
still exists, and `des` still uses it. But the editor no longer needs any of
it: what it saves is the buffer, and the buffer is what was typed.

**A value can only be wrong in one place.** In v13 a cell held text and the
file held text and a writer stood between them. Here there is one text.

**Comments and ordering are just text.** In v13 they were data the writer had
to be careful with. Here you edit them with the arrow keys, like everything
else.

What is lost is the scannable table — eleven Process columns across the top
with one row per block. That was worth having, and `^F` is the partial
replacement: it answers *what does this model look like*, which is the question
the grid was mostly being used for.

---

## 1. The buffer is the truth

`TextBuffer` is a vector of lines, a caret, a selection and an undo stack.
Everything above it re-reads and re-compiles after every change, which for a
file measured in kilobytes costs nothing worth measuring.

That "after every change" is the whole design, and it is also what broke.

---

## 2. The assert that had been one keystroke away since v11

The cold-build test aborted. Not failed — **aborted**, taking the whole suite
with it and printing one line:

```
Assertion failed: mean > 0.0, file src/Distribution.cpp, line 17
```

Compiling every *prefix* of a model as it is typed found it in seconds:

```
prefix 73: version = 1\n\n[Create]\nName = Arrivals\n...\nInterarrival = EXPO(
```

`EXPO(` — the open bracket and nothing else. The parser reports the missing
bracket and substitutes a zero placeholder for the argument. The placeholder is
a constant, so the call builds its distribution eagerly, and `exponential(0)`
aborted the program.

`buildCall` wraps every distribution constructor in a `catch (ModelError)`,
under a comment that says:

> A distribution constructor refused these parameters. That is a USER mistake
> in a cell, so it becomes a diagnostic rather than an exception — otherwise
> one bad cell aborts a whole spreadsheet compile instead of contributing one
> message.

The intent was exactly right. **An assert is not a throw**, so it went straight
past the net. Six of the nine distributions in that file threw a `ModelError`
on a bad parameter and three asserted — and the three that asserted,
Exponential, Uniform and Triangular, are precisely the three a user can reach
by typing `EXPO(`, `UNIF(1,` or `TRIA(1,2,`.

This is not a v15 bug. `des check` on a file containing a half-typed `EXPO(`
has aborted since v11:

```
$ ./build/des check halftyped.des
Assertion failed: mean > 0.0, file src/Distribution.cpp, line 17
```

It surfaced now because a text editor compiles on every keystroke, so `EXPO(`
is a state the compiler sees **every single time** somebody types `EXPO(1.0)`.

And the release build is worse than the debug one. `NDEBUG` deletes the assert
rather than the problem, leaving an `Exponential` whose mean is zero — which
draws zero forever and wedges the event loop at time 0, silently.

---

## 3. Three keys that never arrived

The POSIX terminal put the console in raw mode by clearing `ECHO` and `ICANON`
and stopping there. It should also have cleared:

| Flag | What it did |
|---|---|
| `ISIG` | `^C` raised SIGINT and killed the program, so `^C` could never mean copy |
| `IXON` | `^S` was swallowed by the driver as XOFF and froze the output |
| `IEXTEN` | `^V` is VLNEXT — "take the next key literally" — so a paste ate the keystroke after it |

v13 bound `^S` to **save** and printed `^S save` in its own footer, and on
POSIX that keystroke never reached the program at all. Nobody noticed because
the only POSIX use of this project is WSL, where the tests run and the UI does
not.

---

## 4. A diagnostic names a cell; an editor needs a line

This is the join v15 rests on. `Diagnostic` carries a `CellRef` — module type,
row, column — which is exactly right for a grid and useless in a text editor.

`lineOf(document, diagnostic)` turns it back:

- a reader diagnostic already carries its line;
- a compiler diagnostic carries a cell, and a cell knows the line it was read
  from;
- a diagnostic about a field that is **not there** — "Create needs a Name" —
  has no cell, so `Row::headerLine` is the nearest true thing.

It returns 0 for "cannot say", and the renderer draws no marker rather than
guessing a line. Fourth occurrence of that rule.

The payoff is the answer to a question asked while building this:

> why is this not running?

Before: `cannot run: the document does not compile`, and four tabs to search.
Now: the offending line is marked in the gutter, `^J` goes to it, and `^R`
refuses by name — `cannot run -- no Entity named 'Gears'   (^J goes there)`.

---

## 5. One layout function, because a click has to be undone

`layoutFor(state, width, height)` computes the geometry; `render()` draws with
it and the input layer hit-tests with it. A click has to be turned back into
"the third palette entry" by exactly the arithmetic that put it there.

Two copies of that arithmetic is a bug this project has already had once: v13's
tab bar measured without the scroll markers and drew with them, so the two
passes disagreed by one column and the selected tab could still fall off the
end — which is the bug the scrolling existed to prevent.

---

## 6. What only looking at it found

Six, rendered to stdout and read. Not one would have failed an assertion.

**`F1` after `^G` showed the field help again, for ever.** The key map is the
help overlay with an empty title; `^G` sets the title and nothing cleared it,
so "no title" was never true a second time.

**The status line outlived the view it was about.** "type it instead — the name
does not have to exist yet" sat under the Runs tab and under the Results tab,
advice about a list that was no longer open.

**The error marker ran into the text it marked**: `5 EEntity Type = Gears`. The
`E` reads as part of the line rather than as a note about it.

**The Runs header was two columns adrift** of the rows beneath it, because it
was written out by hand while the rows were built from widths.

**Overlays cleared only their own rectangle**, so sentence fragments showed
either side — `Expression+-- Process.Service` on the left, a stray `a result.`
on the right. Fragments of a line read as text, not as background.

**The blank page opened with a red error marker on line 1.** An empty file has
no `version = 1`, which is true and unhelpful: a file with nothing in it is not
a malformed model file, it is an empty one.

---

## 7. Two tests that damaged the thing they were testing

**One saved over a shipped model.** The quit-and-save case opened
`examples/models/teller.des`, typed into it, and pressed `(s)ave and quit` —
which saved to the path it opened. Every later test in the file then read the
damage back, and the failures it caused pointed everywhere except at the cause.
A test that can write to one of its inputs is a test that can break the next
one; they all open copies now, and one of them checks the original is
untouched.

**One depended on what the last run left behind.** The cold-build test opened
`mm1_v15.des` — and saved to it at the end. So it passed the first time and
failed every time after, reading back the model the previous run had written
instead of starting from a blank page. The ordinary build never noticed,
because I kept deleting the file; **both sanitiser legs of `verify.sh` caught
it**, one after the other, each running the suite in a directory the previous
leg had already used. It starts `fromText("")` now, which is the claim the test
is actually about; whether a *missing* file opens empty has its own test.

**One span forever.** The cold-build test walks to the end of the buffer with
`while (caret + 1 < lineCount) Down`. With a pick list open, `Down` moves the
list rather than the caret, so the loop never ended — a fair imitation of what
that would do to a person. Bounded now, and it *fails* rather than hanging.

And the harness itself had a hole: `std::cout << std::unitbuf` was inside an
`#ifdef _MSC_VER`, though the reasoning above it never mentioned MSVC. Under
MinGW an `abort()` discarded every line printed before it, so the first run of
this printed the assert message and nothing else — a stack trace with the stack
removed.

---

## 8. Many runs

Arena keeps one Run Setup per model. A file can hold several, and editing the
same four numbers back and forth to compare a long horizon against a short one
loses what they were.

`readRunSetup(doc, out, which)` takes a position and `runNames(doc)` lists
them; v11 through v14 reported the second `[Run]` as an error. An index past
the end reads as the defaults rather than as an error, because a caller holding
a stale index after a row was deleted is not a broken model.

`des` gained `--run <name>` to match, and names what it has when you get the
name wrong:

```
$ des run two.des --run Typo
two.des: no [Run] named 'Typo' -- it has: Short Long
```

Otherwise a model the editor can run four ways runs only one way from a script,
and the script is the half that gets automated. The report now names the run
whenever there is a choice, because a report that does not say which settings
made it is a report you cannot file.

While adding it: `des run` wrote its replication ticker to **stdout**. That is a
carriage-return line that overwrites itself, which is right on a terminal and
garbage in a file — `des run m.des > results.txt` collected
`replication 1 of 5replication 2 of 5replication 2 of 5...` above the results.
Progress is not the output; it goes to stderr.

---

## Rules this version added

- **A constructor that can be reached from user text must throw, not assert.**
  A `catch` cannot catch an assert, and the file that had both had six of one
  and three of the other.
- **A test must never be able to write to one of its inputs.** Open a copy.
- **A loop driven by keys must be bounded.** An overlay can swallow the key the
  loop is waiting for, and a hanging suite says less than a failing one.
- **Geometry is computed once and shared** between what draws and what
  hit-tests.
- **A view's status line belongs to that view.**
- **A test must be idempotent in a dirty directory.** Running the suite twice
  in a row is the cheapest version of that check, and two sanitiser legs did it
  for free.

---

## Still open

- The scannable grid is gone. `^F` covers "what does this model look like" but
  not "show me every Process side by side".
- No search in the editor. For a two-hundred-line model that will start to
  matter.
- Undo of a pick-list choice restores the line but not the caret column.
- Undo is a whole-buffer snapshot per edit. Fine at this size, and not fine at
  ten thousand lines.
