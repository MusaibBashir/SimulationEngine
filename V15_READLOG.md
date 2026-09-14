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

---

## 15.1 — something you can send

The ask was one executable, sent to someone else and run on their laptop with no
codebase, no CMake and no compiler. Four things stood between the 15.0 build and
that, and none of them were visible from inside this machine's own setup —
which is the point of the ask.

### The exe only ran here

The development build imports `libstdc++-6.dll` and `libgcc_s_seh-1.dll`, which
exist only inside a MinGW install. Sent as it was, it would have opened on
someone else's laptop with *"libstdc++-6.dll was not found"*.

It demonstrated that without leaving the desk. An older `C:\MinGW\bin` sits
ahead of WinLibs on this machine's `PATH`. The moment the tests used
`std::filesystem`, the test binary stopped loading — **exit 127, and not one
line of output**, because a process that never reaches `main` prints nothing.
It looked exactly like a crash in a test. `des.exe` and `des_tui.exe` went on
loading against the old DLL only because they did not use the missing symbols
*yet*.

The package links statically, and `tools/package.sh` checks what the result
imports against an allowlist of Windows' own libraries, then copies it out of
the repository and starts it with `PATH` cut down to `C:\Windows`. It is built
`-O2` **without** `-DNDEBUG`: sections 2 and below are a record of asserts
guarding things a person can type, and a release build that deletes them runs
wrong instead of stopping.

### A model could still kill it

Sent to strangers, "the window closed" is the entire bug report. So before
packaging, every kind of failure a person can type was run — **each in its own
process**, because in one process an abort takes the buffered output of every
case before it, and the first attempt at this printed one assert message and
nothing to say which input had caused it:

| Service field | Before | After |
|---|---|---|
| `SQRT(0 - 1)`, `MOD(1, 0)` | the error escaped `RunController::advance()` and ended the program | the run fails, with the message |
| `EXPO(1) - 2` | **aborted** on the scheduler's past-time assert | *process 'Work': its service time came out negative (-1.86)* |
| `NORM(1, 5)` | finished | finished |

`advance()` now catches model errors the way `startRun()` always had. The
scheduler's `assert(t >= now)` is a throw: it was written for the engine's own
bugs, but every duration a model computes passes through it, so it is also
where a person's typing arrives — and it still stops right where the mistake
is made. `Station::drawService` checks first, so the common case names the
process.

**The first test written for this was wrong.** It used `NORM(1, 5)`, expecting
a throw, and reported that nothing escaped *and* that the run had not failed —
which only makes sense if nothing was thrown at all. It wasn't: the `NORM`
builder clamps a Normal's left tail at zero, so there was never anything to
catch. The test now uses the inputs that actually failed, and keeps
`NORM(1, 5)` as the case that must go on finishing.

### A double-click was an error

Started with no file, `des_tui` printed a usage line and exited — which, from a
double-click, is a window that flashes and vanishes, and looks exactly like a
program that is broken. It now opens `untitled.des` in `Documents\DES Models`
and reopens that file next time. The Documents folder is asked of Windows, not
built from `%USERPROFILE%`, because on a great many laptops it has been moved
into OneDrive and the other one is an empty folder nobody looks in. `save()`
creates the folder, since on a laptop that has never run this it is not there —
the first `^S` a new person made would have said "could not write".

A message printed on the way out now holds the window open, but only when this
program is alone on its console; in a terminal somebody opened themselves, the
window stays anyway and a pause would just be in the way.

### Without a console it spun

With its input redirected, the Win32 terminal's mode calls failed quietly, every
`ReadConsoleInputW` failed, `nextKey()` returned *Unknown*, and the loop asked
again — forever, at full speed. `openTerminal` now checks first and says why it
cannot start. That same check is what lets `package.sh` prove the packaged exe
*loads*: redirected, the right behaviour is a message and exit 1, and only a
program that started can give it.

The POSIX terminal changed with it, and **no gate compiles that file** —
`verify.sh`'s Linux leg builds the tests and `src/`, not `tui/`. It was checked
by hand under WSL's GCC with the full warning set, and that is recorded here
because the next change to it will have to be checked the same way.

### The zip was malformed

`Compress-Archive` wrote the zip's entry names with backslashes —
`DES-Simulator\DES-Simulator.exe` — and so did .NET's `ZipFile`, tried next on
the strength of its documentation. The zip format does not allow backslashes.
Windows Explorer copes, which is why it looked fine; `unzip` on a Mac or a Linux
machine extracts that as a single file with a backslash in its name, so a zip
forwarded through anybody else's computer would have arrived broken.

Windows' own `bsdtar` writes forward slashes, so the zip is built with that.
Each candidate was judged by reading back the entries it had actually written.

**The first guard against this coming back measured nothing.** It listed the
zip with `tar -tf` and looked for a backslash — and run against a
`Compress-Archive` zip known to be full of them, it stayed quiet, because
`bsdtar` turns backslashes into forward slashes when it *lists* an archive. A
check that reads through a tool which hides the defect cannot find the defect.
The guard now reads the zip's own bytes: entry names are stored uncompressed,
so `DES-Simulator\` followed by a name is either in the file or it is not.

That version was tested the way the first should have been: the function was
pulled out of `package.sh` as written — not retyped, since retyping it through a
shell had already turned its backslash pattern into a grep error that read as
"quiet" — and run against both zips. It fires on the `Compress-Archive` one and
stays quiet on the `bsdtar` one.

### Backstop

If anything still escapes the loop, `main` restores the terminal *first* — so
the message is not drawn on a screen about to be discarded — writes an unsaved
model to `<file>.recovered`, and holds the window. None of the failures found so
far reach it. "None found so far" is exactly the claim this project has been
wrong about, and here the cost of being wrong is somebody's afternoon of work.

### Rules this added

- **A program people are sent is tested as the file they are sent** — the
  optimised static build runs the suite as itself, and is started away from the
  toolchain that built it.
- **Suspect inputs run one per process.** An abort does not only fail its own
  case; it erases the evidence of every case before it.
- **A file no gate compiles is checked by hand, and it says so.**

### Still open (15.1)

- The exe is not code-signed, so Windows SmartScreen warns on first run.
- Nothing is packaged for macOS or Linux.
- There is no Open or Save As inside the program: another model is opened by
  dragging it onto the exe, or by double-clicking it after installing.
