# v13 — a terminal UI

**Status:** design approved, not yet implemented.
**Date:** 2026-09-07
**Branch:** `v13-terminal-ui`, built on `main` at the v12 merge.

---

## Where this sits

v10 made a model's fields text, v11 made the model itself data, v12 made the
run something a caller drives. Every one of those was built for a front end
that did not exist. v13 is that front end.

**It lives in this repository**, which reverses the decision taken at v10 and
restated in the v11 and v12 specs. That decision was not wrong when it was
made, and the argument for it is real: a repository boundary is what stops the
engine growing a method because a widget found it convenient. Two things
changed.

- v11 and v12 both ended up adding a consumer *inside* this repo — the `des`
  CLI — for one reason: an interface with no caller is an interface nobody has
  checked. The TUI is the real consumer of `ModuleRegistry`, `ModelDocument`
  and `RunController`, and keeping it out leaves those unchecked in the repo
  that defines them.
- The layering that separation protected can be had from the target graph
  instead: `des_ui` links `des_engine`, never the reverse.

The asymmetry decides it: splitting a directory out later is mechanical,
merging two repositories later is not.

## The five decisions this version turns on

| # | Question | Decision |
|---|---|---|
| 1 | Terminal or GUI? | **Terminal.** v11's data model was shaped for one. |
| 2 | How is it structured? | **The screen is a value.** `render(state)` fills a `Screen`; `handleKey(state, key)` returns the next state. |
| 3 | Where does the code live? | **Portable half in `src/`, platform half in `tui/`.** |
| 4 | How is a 130-column table shown in 80 columns? | **Tab bar, grid, and a row-detail pane.** Scan in the grid, edit in the detail. |
| 5 | Undo? | **A snapshot stack.** `ModelDocument` is copyable. |

---

## Architecture

```
  v13  Screen · Key · Terminal (Win32 | POSIX) · TuiState · TuiRender · TuiInput
  ───────────────────────────────────────────────────────────────────────
  v12  RunController · RunSetup · Regression
  v11  ModuleRegistry · ModelDocument · DocumentFormat · Compiler
  v1–v10  Model · INode · Expression · Diagnostic
```

**Where the code goes, and why it is not tidiness.** `tools/verify.sh` compiles
`src/*.cpp`, and its WSL leg compiles `tests/*.cpp src/*.cpp`. Putting the
portable half of the UI in `src/` means **the whole of it goes through GCC,
Clang, AddressSanitizer and UndefinedBehaviorSanitizer with no change to any
gate script.** Platform code — which cannot be tested anyway — is the only
thing outside, in `tui/`.

| Unit | Responsibility |
|---|---|
| `include/Screen.hpp` / `src/Screen.cpp` | A grid of `{char, Attr}`. Comparable, and printable as plain text. |
| `include/Key.hpp` | A key event as a value. Header-only; it is a tagged struct and a factory. |
| `include/Terminal.hpp` | Three methods, and the only platform surface. |
| `include/TuiState.hpp` / `src/TuiState.cpp` | Everything the UI knows: document, cursor, mode, diagnostics, undo stack. |
| `include/TuiRender.hpp` / `src/TuiRender.cpp` | `render(const TuiState&, Screen&)`. Pure. |
| `include/TuiInput.hpp` / `src/TuiInput.cpp` | `handleKey(TuiState&, Key)`. Pure with respect to the terminal. |
| `tui/Terminal_win32.cpp`, `tui/Terminal_posix.cpp` | The two implementations. Untested by construction. |
| `tui/main.cpp` | present → read → handle → repeat. |

**The rule no gate can check, so it is written here:** `des_ui` links
`des_engine` and never the reverse, and **no engine header may include a `Tui*`
or `Screen` header.** If the engine ever needs a change for the UI's
convenience, that change gets its own entry in a spec, argued on the engine's
terms.

## 1. `Screen` — the value the whole design rests on

```cpp
enum class Attr { Normal, Bold, Dim, Reverse, Error };

class Screen {
public:
    // NESTED, and not called Cell. `des::Cell` would sit one lookup away from
    // ModelDocument::Cell, which is a spreadsheet cell -- the thing this whole
    // project means by the word. A screen position holding a character is a
    // glyph.
    struct Glyph {
        char ch{' '};
        Attr attr{Attr::Normal};
    };

    Screen(int width, int height);
    int width() const;
    int height() const;

    void clear();
    void put(int x, int y, char ch, Attr attr = Attr::Normal);
    // Returns the x it stopped at, so callers can chain runs of text.
    int  text(int x, int y, const std::string& s, Attr attr = Attr::Normal);
    void hline(int x, int y, int length, char ch = '-');
    void box(int x, int y, int width, int height);

    const Glyph& at(int x, int y) const;

    // One row as plain text, trailing blanks trimmed. THIS IS THE TEST
    // INTERFACE: a test asserts on what a person would read, not on an
    // attribute grid.
    std::string line(int y) const;
    std::string asText() const;         // every line, newline-separated
};
```

Out-of-range writes are **silently dropped**, not asserted. A renderer that
computes a column one past the edge on an 80-wide terminal is a layout bug that
should show as a missing character, not as a crash in front of the user — and
clamping keeps every renderer free of bounds arithmetic.

## 2. `Key` — input as a value

```cpp
enum class KeyKind {
    Char, Enter, Escape, Backspace, Delete, Tab, BackTab,
    Up, Down, Left, Right, Home, End, PageUp, PageDown, Ctrl, Unknown
};

struct Key {
    KeyKind kind{KeyKind::Unknown};
    char    ch{0};      // Char: the character. Ctrl: the letter, uppercase.

    static Key character(char c);
    static Key control(char c);
    static Key special(KeyKind k);
};
```

A `Key` is what a test feeds. The platform layer's whole job on the input side
is turning a `KEY_EVENT_RECORD` or an escape sequence into one of these.

## 3. `Terminal` — the only platform code

```cpp
struct TerminalSize { int width{80}; int height{24}; };

class ITerminal {
public:
    virtual ~ITerminal() = default;
    virtual TerminalSize size() const = 0;
    virtual void present(const Screen& screen) = 0;
    // Blocks. Returns Unknown on anything it does not understand, which the
    // input layer ignores -- an unrecognised key must never be an error.
    virtual Key nextKey() = 0;
};

std::unique_ptr<ITerminal> openTerminal();   // the platform's implementation
```

`present()` writes the whole screen each time, positioned with `\033[H`, with
no diffing. A model spreadsheet is at most a few thousand cells and a person
types at human speed; a diffing optimisation would be the first place a
rendering bug could hide, and it is not needed.

Windows: `SetConsoleMode` with `ENABLE_VIRTUAL_TERMINAL_PROCESSING` for output
and `ReadConsoleInputW` for keys. POSIX: `termios` raw mode, `TIOCGWINSZ`, and
`read()`. Both ship with the operating system; **neither is a dependency.**

## 4. `TuiState` — what the UI knows

```cpp
enum class Mode { Grid, Detail, Editing, Running, Confirm };

class TuiState {
public:
    // Reads the file. A file that does not PARSE still opens -- v11 preserves
    // what it read -- with the reader's diagnostics shown. A file that does
    // not EXIST opens as an empty document, so `des_tui new.des` starts a new
    // model rather than refusing; the status line says which of the two
    // happened, because "empty" and "unreadable" must not look alike.
    static TuiState open(const std::string& path);

    Mode mode() const;
    const ModelDocument& document() const;
    const std::string& path() const;
    bool dirty() const;
    const std::vector<Diagnostic>& diagnostics() const;
    const std::string& status() const;

    // The cursor: which module type, which row, which column.
    const std::string& type() const;
    std::size_t row() const;
    std::size_t column() const;      // index into the schema's columns

    // Mutations. Each pushes an undo snapshot first.
    void setCell(const std::string& column, const std::string& text);
    void addRow();
    void removeRow();
    void moveRow(int delta);
    void undo();

    bool save();                     // writeDocumentFile; false on failure
    void recompile();                // fills diagnostics(); called on load and commit
};
```

**Diagnostics recompute on cell commit and on load, never per keystroke.**
`compile()` reports every bad cell in one pass, so a commit is the natural
beat — and per-keystroke would flag `EXPO(0.8` as broken while the user is
still typing it.

**Undo is a stack of whole documents.** `ModelDocument` is copyable — v12's
`RunController::fromDocument` already copies one — so a snapshot cannot be
wrong about its own inverse, which a per-operation undo can. Bounded to 64
deep; a model document is kilobytes.

**The modes, and what leaves each one.** `Grid` moves the row cursor and
switches tabs; `Enter` goes to `Detail`. `Detail` moves between a row's fields;
`Enter` goes to `Editing`, `Escape` back to `Grid`. `Editing` holds a live text
buffer; `Enter` commits and recompiles, `Escape` abandons it. `Running` is
entered by `^R` and left by `Escape`. `Confirm` is entered by `q` with unsaved
changes and asks once: save, discard, or cancel.

## 5. Layout

```
  Run  Create [Process] Decide  Dispose  Resource        teller.des*
 +------------------------------------------------------------------+
 | Name     Cap  Resource  Discipline  Service      Next            |
 | Serve    1              FIFO        EXPO(0.8)    Out             |
 |>Check    1    Clerk     FIFO        EXPO(0.3)    Sort            |
 +-- row 2 of 2 ----------------------------------------------------+
 | Name         Check                                               |
 | Capacity     1                                                   |
 | Resource     Clerk          -> Resource                          |
 | Discipline   FIFO           FIFO LIFO PRIORITY SPT EDD RANDOM     |
 | Service     [EXPO(0.3      ]                                     |
 |             error: expected ')'                                  |
 +------------------------------------------------------------------+
 ^S save  ^R run  ^N new  ^D delete  Tab pane  q quit
```

The tab bar lists **every type the registry publishes, followed by any type the
document holds that it does not** — not a list written in the UI's source. The
first half is the property the flat child tables were chosen to protect: a
module added in a later version renders here without this code changing. The
second half matters just as much and is easy to miss: v11 deliberately
*preserves* unknown module types rather than dropping them, so a file written
by a later version must be visible here, marked unknown and not editable. A UI
that showed only what it understood would hide exactly the rows a person needs
to see before deciding whether their editor is too old for the file.

A read-only schema (`Queue`) renders with its rows dimmed and refuses edits,
saying why.

The **detail pane** is where a 130-column table becomes readable and **the only
place editing happens**, exactly as Arena has a spreadsheet to scan and a
dialog to edit in. `Enter` in the grid moves focus to the detail pane; `Enter`
on a field there opens it for editing. Two editing surfaces would mean two
sets of key handling, two places to get the commit-versus-abandon rule right,
and a wide table still unreadable in one of them. It shows the current row's fields stacked, each with its enum values
or its reference target beside it, and any diagnostic for that cell underneath.

A terminal smaller than **80×24** renders a single message saying so and
nothing else. Drawing a layout that does not fit produces garbage that looks
like a bug in the model.

## 6. Running

`^R` compiles the document through `RunController::fromDocument` and enters
`Mode::Running`. The loop calls `advance(4096)`, redraws a progress line from
`progress()`, and repeats. `Esc` calls `cancel()`.

`progress().fraction` is a `std::optional`. When it is empty — a `[Run]` with
`Stop When Drained` and no length — the UI draws a **spinner, not a zero-length
bar**. That is the fourth-time rule from v12 reaching the surface it was
written for.

When the run finishes, the report is shown in a scrollable pane. `Esc` returns
to the grid.

---

## Testing

**The decisive test is not about pixels.** Opening a `.des` file and saving it
without editing must produce **byte-identical bytes**, and after editing one
cell every *untouched* row must still come back verbatim. That is v11's
round-trip guarantee, exercised for the first time through the surface a person
actually uses. If the UI marks rows edited that the user never touched, this
catches it and nothing else would.

Then:

| What | Asserted |
|---|---|
| Rendering | A known document renders to known text, via `Screen::asText()` |
| Tab bar | Lists what the registry publishes, in registry order, with row counts |
| Navigation | Arrows, Tab, PageUp/Down move the cursor and clamp at the edges |
| Editing | A cell edit commits on Enter, is abandoned on Escape, and marks dirty |
| Diagnostics | A bad expression shows against the right column in the detail pane |
| Undo | Restores across all four mutation kinds, and is bounded |
| Read-only | A `Queue` row refuses an edit and says why |
| Small terminal | 40×10 renders the message and nothing else |

Every one of these runs headless: build a `TuiState`, feed `Key` values, assert
on `Screen` text. No terminal, no timing, no sleeps — so the whole UI runs
under ASan and UBSan in the WSL leg like everything else.

**Constraints, as in v11 and v12:**

- All **930 v12 checks pass unchanged**. If one needs changing, stop and raise it.
- `bash tools/baseline.sh check` prints `BASELINE CLEAN`.
- `./build/des regress` prints `REGRESSION CLEAN`.
- `bash tools/verify.sh` prints `VERIFY CLEAN`.
- C++17, no external dependencies, sources listed in `CMakeLists.txt`.
- Comments only where they carry design rationale.

---

## Explicitly not in v13

| Deferred | Why |
|---|---|
| A module palette — adding a Process to a document that has none | v13 edits the types a document already has, plus rows within them. Creating a type needs a picker and a "where does it go in file order" answer. |
| Mouse | Adds a second input path through every mode, for a spreadsheet the keyboard already reaches. |
| Search, multiple files, colour themes | YAGNI. |
| A live animated run dashboard | The run view is a progress line and the report. Per-event animation needs v12's deferred observer. |
| Redo | The snapshot stack gives undo. Redo needs a second stack and a rule for what invalidates it. |
| Diffing `present()` | A person types at human speed. It is the first place a rendering bug could hide. |

## Still open, carried forward

`12_shared_resources` is not reproducible run-to-run (a v9 bug, excluded from
the byte-identical gate with the exclusion printed every run). A `Separate`
duplicate is counted as an exit it never arrived for. Resource schedules,
preemption, batch means and distribution fitting are untouched.
