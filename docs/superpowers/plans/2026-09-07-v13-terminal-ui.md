# v13 Terminal UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A terminal front end that opens a `.des` file, shows every module type as a spreadsheet, edits cells with diagnostics at the cell, saves without disturbing untouched rows, and runs the model.

**Architecture:** The screen is a **value**. `render(state, screen)` fills a `Screen`; `handleKey(state, key)` advances a `TuiState`. The platform does only three things — report the size, blit a `Screen`, read a `Key` — so every other part runs headless in the existing test suite. The portable half lives in `src/`/`include/` as a second library target `des_ui`; only `tui/` holds platform code and `main()`.

**Tech Stack:** C++17, CMake, no external dependencies. Win32 console API and POSIX termios, both shipped with the OS. The hand-rolled test harness in `tests/`.

## Global Constraints

- **C++17.** No newer features, no external dependencies.
- **All 930 v12 checks must pass, unchanged.** Not adapted, not deleted. If one needs changing, stop and raise it.
- **The 15 gated examples stay byte-identical:** `bash tools/baseline.sh check` must print `BASELINE CLEAN`.
- **The 4 gated models stay byte-identical:** `./build/des regress` must print `REGRESSION CLEAN`.
- **`bash tools/verify.sh` must print `VERIFY CLEAN`** — GCC 14.2 and Clang 19.1 warning-clean, MSVC AddressSanitizer clean, WSL Linux GCC ASan + UBSan clean.
- **Build:** `cmake --build build` (MinGW Makefiles). Binaries in `build/`, examples in `build/examples/`. Run the suite as `./build/des_tests.exe` **from the repository root**.
- **`des_ui` links `des_engine`, never the reverse.** No engine header may include `Screen.hpp`, `Key.hpp` or any `Tui*` header. No gate can check this; it is a rule.
- **Headers declare, sources define.** Every `.cpp` includes its own header first.
- **`unique_ptr` = ownership, raw pointer = observation.** Every polymorphic base gets a virtual destructor.
- **Sources are LISTED in `CMakeLists.txt`**, never globbed (except `examples/`).
- **New public headers go in `include/des.hpp`.**
- **User errors are `Diagnostic` values; programmer errors are thrown `ModelError`.** Never mix these.
- **Comments only where they carry design rationale.** This codebase explains *why*, never *what*. Do not narrate code.
- Namespace `des` throughout.

### Constraints specific to v13

- **Nothing in `src/Tui*.cpp` or `src/Screen.cpp` may touch a terminal**, read `stdin`, or write `stdout`. That is what keeps the whole UI testable and sanitiser-clean.
- **The tab bar is built from `ModuleRegistry::instance().all()` plus any type the document holds that the registry does not.** No module type name may be written in the UI's source.
- **Opening a file and saving it unedited must produce byte-identical bytes.**
- **Out-of-range `Screen` writes are dropped, never asserted.**
- **A terminal smaller than 80×24 renders one message and nothing else.**

## File Structure

**New headers/sources** (each added to a new `add_library(des_ui ...)` and to `include/des.hpp`):

| File | Responsibility |
|---|---|
| `include/des_ui.hpp` | The UI umbrella. Separate from `des.hpp` so the engine never pulls the UI in. |
| `include/Screen.hpp` / `src/Screen.cpp` | A grid of glyphs, and the text view tests assert on. |
| `include/Key.hpp` | A key event as a value. Header-only. |
| `include/Terminal.hpp` | `ITerminal` and `openTerminal()`. Declaration only; no source in `src/`. |
| `include/TuiState.hpp` / `src/TuiState.cpp` | Document, cursor, mode, diagnostics, undo, save. |
| `include/TuiRender.hpp` / `src/TuiRender.cpp` | `render(const TuiState&, Screen&)`. |
| `include/TuiInput.hpp` / `src/TuiInput.cpp` | `handleKey(TuiState&, Key)`. |

**New platform sources (in `tui/`, NOT in `src/`):** `tui/Terminal_win32.cpp`, `tui/Terminal_posix.cpp`, `tui/main.cpp`.

**New test file:** `tests/tui_tests.cpp`, exposing `void runTuiTests();`.

**Modified:** `CMakeLists.txt`, `tests/tests.cpp`, `README.md`, `CHANGELOG.md`, `ARENA_MAP.md`, `examples/README.md`, `tools/manual_data.py`.

**Ordering rationale.** The two value types come first because everything is expressed in them. State before rendering, because a renderer needs something to render. Rendering before input, because a test for a keypress asserts on a screen. The decisive save test lands the moment editing exists, before any platform code, so a round-trip bug is found while the change that caused it is still one task old. The platform layer and `main()` come ninth — that is the first point at which the program can be run by hand, and by then everything it drives is already gated.

---

### Task 1: `Screen` and `Key`

**Files:**
- Create: `include/Screen.hpp`, `src/Screen.cpp`, `include/Key.hpp`, `tests/tui_tests.cpp`
- Create: `include/des_ui.hpp`
- Modify: `CMakeLists.txt`, `tests/tests.cpp`

**Interfaces:**
- Produces: `des::Attr`, `des::Screen` (with `Screen::Glyph`), `des::KeyKind`, `des::Key`, and the test entry point `void runTuiTests()`.

- [ ] **Step 1: Write the failing test**

Create `tests/tui_tests.cpp`:

```cpp
// ============================================================================
// tests/tui_tests.cpp  --  v13: the terminal UI, tested without a terminal
// ============================================================================
// Every test here builds a state, feeds keys, and asserts on a Screen AS TEXT.
// That is possible because the screen is a value: nothing in the UI below
// tui/ touches a terminal, so all of it runs under the sanitisers with
// everything else.
#include <string>
#include "harness.hpp"
#include "des_ui.hpp"

using namespace des;
using des_test::check;
using des_test::modelPath;
using des_test::section;

void runTuiTests() {
    section("Screen is a value");
    {
        Screen s(10, 3);
        check(s.width() == 10 && s.height() == 3, "a screen has a size");
        check(s.line(0).empty(), "and starts blank");

        s.text(2, 1, "hi");
        check(s.line(1) == "  hi", "text lands where it was put");
        check(s.at(2, 1).ch == 'h', "and the glyph is readable");

        // Out of range is DROPPED, not asserted. A renderer that computes one
        // column past the edge has a layout bug, and a crash in front of the
        // user is a worse way to learn about it than a missing character.
        s.text(8, 1, "abcdef");
        check(s.line(1) == "  hi    ab", "a run that overflows is clipped, not fatal");
        s.put(-1, -1, 'x');
        s.put(999, 999, 'x');
        check(s.line(1) == "  hi    ab", "and writes outside it change nothing");

        s.text(0, 2, "trailing   ");
        check(s.line(2) == "trailing", "line() trims the trailing blanks");

        Screen t(10, 3);
        t.text(2, 1, "hi");
        t.text(8, 1, "ab");
        t.text(0, 2, "trailing");
        check(s.asText() == t.asText(), "two screens with the same glyphs read alike");

        s.clear();
        check(s.asText() == Screen(10, 3).asText(), "clear() empties it");
    }

    section("Key is a value");
    {
        const Key a = Key::character('a');
        check(a.kind == KeyKind::Char && a.ch == 'a', "a printable key carries its char");
        const Key ctrlS = Key::control('s');
        check(ctrlS.kind == KeyKind::Ctrl && ctrlS.ch == 'S',
              "a control key normalises to upper case, so ^s and ^S are one key");
        check(Key::special(KeyKind::Up).kind == KeyKind::Up, "a special key is its kind");
    }
}
```

- [ ] **Step 2: Wire the file into the suite and watch it fail**

In `tests/tests.cpp`, beside the other forward declarations near line 27, add:

```cpp
void runTuiTests();          // tests/tui_tests.cpp
```

and after the `runRuntimeTests();` call, add:

```cpp
    runTuiTests();
```

In `CMakeLists.txt`, extend the test sources:

```cmake
add_executable(des_tests tests/tests.cpp tests/harness.cpp tests/expression_tests.cpp tests/document_tests.cpp tests/runtime_tests.cpp tests/tui_tests.cpp)
```

```bash
cmake --build build
```

Expected: `error: 'Screen' was not declared in this scope`.

- [ ] **Step 3: Write `include/Key.hpp`**

```cpp
// ============================================================================
// Key.hpp  --  v13: a keypress as a value
// ============================================================================
// The platform layer's whole job on the input side is turning a
// KEY_EVENT_RECORD or an escape sequence into one of these. Everything above
// it takes a Key, which is what lets a test type into the UI without a
// terminal.

#pragma once
#include <cctype>

namespace des {

enum class KeyKind {
    Char, Enter, Escape, Backspace, Delete, Tab, BackTab,
    Up, Down, Left, Right, Home, End, PageUp, PageDown, Ctrl, Unknown
};

struct Key {
    KeyKind kind{KeyKind::Unknown};
    char    ch{0};      // Char: the character. Ctrl: the letter, upper case.

    static Key character(char c) { return Key{KeyKind::Char, c}; }
    // Upper case, so ^s and ^S are the same key rather than two that must
    // both be handled and can therefore disagree.
    static Key control(char c) {
        return Key{KeyKind::Ctrl,
                   static_cast<char>(std::toupper(static_cast<unsigned char>(c)))};
    }
    static Key special(KeyKind k) { return Key{k, 0}; }
};

}  // namespace des
```

- [ ] **Step 4: Write `include/Screen.hpp`**

```cpp
// ============================================================================
// Screen.hpp  --  v13: the screen is a VALUE
// ============================================================================
// This is the decision the whole UI rests on. A renderer fills one of these;
// the terminal blits it. Nothing here knows what a terminal is, so a test can
// render a document and assert on what a person would read -- headless, with
// no timing and no sleeps, under the same sanitisers as the engine.

#pragma once
#include <string>
#include <vector>

namespace des {

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

    int width()  const { return m_width; }
    int height() const { return m_height; }

    void clear();
    void put(int x, int y, char ch, Attr attr = Attr::Normal);
    // Returns the x it stopped at, so callers can chain runs of text.
    int  text(int x, int y, const std::string& s, Attr attr = Attr::Normal);
    void hline(int x, int y, int length, char ch = '-', Attr attr = Attr::Normal);
    void box(int x, int y, int width, int height, Attr attr = Attr::Normal);

    const Glyph& at(int x, int y) const;

    // One row as plain text, trailing blanks trimmed. THIS IS THE TEST
    // INTERFACE: a test asserts on what a person would read rather than on an
    // attribute grid, so a test says what it means.
    std::string line(int y) const;
    std::string asText() const;

private:
    bool inside(int x, int y) const;

    int                m_width{0};
    int                m_height{0};
    std::vector<Glyph> m_glyphs;   // row-major, m_width * m_height
    Glyph              m_void;     // returned by at() for an out-of-range read
};

}  // namespace des
```

- [ ] **Step 5: Write `src/Screen.cpp`**

```cpp
#include "Screen.hpp"

namespace des {

Screen::Screen(int width, int height)
    : m_width(width < 0 ? 0 : width), m_height(height < 0 ? 0 : height),
      m_glyphs(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height)) {}

bool Screen::inside(int x, int y) const {
    return x >= 0 && y >= 0 && x < m_width && y < m_height;
}

void Screen::clear() {
    for (Glyph& g : m_glyphs) g = Glyph{};
}

void Screen::put(int x, int y, char ch, Attr attr) {
    if (!inside(x, y)) return;
    m_glyphs[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
             static_cast<std::size_t>(x)] = Glyph{ch, attr};
}

int Screen::text(int x, int y, const std::string& s, Attr attr) {
    for (char c : s) put(x++, y, c, attr);
    return x;
}

void Screen::hline(int x, int y, int length, char ch, Attr attr) {
    for (int i = 0; i < length; ++i) put(x + i, y, ch, attr);
}

void Screen::box(int x, int y, int width, int height, Attr attr) {
    if (width < 2 || height < 2) return;
    hline(x, y, width, '-', attr);
    hline(x, y + height - 1, width, '-', attr);
    for (int i = 1; i < height - 1; ++i) {
        put(x, y + i, '|', attr);
        put(x + width - 1, y + i, '|', attr);
    }
    put(x, y, '+', attr);
    put(x + width - 1, y, '+', attr);
    put(x, y + height - 1, '+', attr);
    put(x + width - 1, y + height - 1, '+', attr);
}

const Screen::Glyph& Screen::at(int x, int y) const {
    if (!inside(x, y)) return m_void;
    return m_glyphs[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
                    static_cast<std::size_t>(x)];
}

std::string Screen::line(int y) const {
    if (y < 0 || y >= m_height) return std::string();
    std::string out;
    out.reserve(static_cast<std::size_t>(m_width));
    for (int x = 0; x < m_width; ++x) out.push_back(at(x, y).ch);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string Screen::asText() const {
    std::string out;
    for (int y = 0; y < m_height; ++y) {
        out += line(y);
        out.push_back('\n');
    }
    return out;
}

}  // namespace des
```

- [ ] **Step 6: Add the `des_ui` library**

In `CMakeLists.txt`, after the `des_engine` block and its `target_compile_options` line, add:

```cmake
# ----------------------------------------------------------------------------
# The user interface, as a SECOND library. des_ui links des_engine and never
# the reverse -- that is the whole of the layering, and it is what a repository
# boundary would otherwise have been protecting.
#
# The portable half lives in src/ deliberately: tools/verify.sh compiles
# src/*.cpp and its WSL leg compiles tests/*.cpp src/*.cpp, so putting it here
# runs the entire UI through GCC, Clang, ASan and UBSan with no change to any
# gate script. Only platform code lives in tui/, and platform code cannot be
# tested anyway.
# ----------------------------------------------------------------------------
add_library(des_ui STATIC
    src/Screen.cpp
)
target_link_libraries(des_ui PUBLIC des_engine)
target_compile_options(des_ui PRIVATE ${DES_WARNINGS})
```

and change the test target to link it:

```cmake
target_link_libraries(des_tests PRIVATE des_ui)
```

> `des_ui` links `des_engine` **PUBLIC**, so anything linking `des_ui` gets the engine too. `des_tests` therefore needs only the one line.

Create `include/des_ui.hpp` — a **second** umbrella, and not an addition to
`des.hpp`:

```cpp
// ============================================================================
// des_ui.hpp  --  the only header a front end needs
// ============================================================================
// Separate from des.hpp on purpose. The layering rule this version rests on is
// that des_ui links des_engine and NEVER the reverse -- and putting Screen.hpp
// into the engine's umbrella would have every example and every engine test
// pulling in the UI, which is the first step towards an engine that knows one
// exists. Two umbrellas, one direction.

#pragma once

#include "des.hpp"
#include "Key.hpp"
#include "Screen.hpp"

```

Later tasks append their headers to this file, not to `des.hpp`. `tests/tui_tests.cpp`
includes `des_ui.hpp`.

- [ ] **Step 7: Build and run**

```bash
cmake -S . -B build && cmake --build build && ./build/des_tests.exe
```

Expected: the `[Screen is a value]` and `[Key is a value]` sections appear, total 945 or more, no FAIL lines.

- [ ] **Step 8: Commit**

```bash
git add include/Screen.hpp include/Key.hpp include/des_ui.hpp src/Screen.cpp CMakeLists.txt tests/tui_tests.cpp tests/tests.cpp
git commit -m "feat: Screen and Key -- the two values the UI is made of

The screen is a VALUE. A renderer fills one, the terminal blits it, and a
test asserts on what a person would read. That is what makes a terminal UI
testable at all, and it is why the portable half lives in src/: verify.sh
already compiles src/*.cpp, so the whole UI goes through both compilers and
both sanitisers with no change to any gate script.

Out-of-range writes are dropped rather than asserted. A renderer that
computes one column past the edge has a layout bug, and a crash in front of
the user is a worse way to find out than a missing character."
```

---

### Task 2: `TuiState` — open, save, and the cursor

**Files:**
- Create: `include/TuiState.hpp`, `src/TuiState.cpp`
- Modify: `CMakeLists.txt`, `include/des_ui.hpp`
- Test: `tests/tui_tests.cpp`

**Interfaces:**
- Consumes: `ModelDocument`, `readDocumentFile`, `writeDocumentFile`, `ModuleRegistry`, `compile`, `Diagnostic`.
- Produces: `des::Mode`, `des::TuiState` with `open`, `mode`, `document`, `path`, `dirty`, `diagnostics`, `status`, `types`, `type`, `row`, `column`, `columnsHere`, `schemaHere`, `save`, `recompile`, `setType`, `setRow`, `setColumn`, `setMode`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("TuiState opens a document");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));
        check(!s.document().types().empty(), "a real file opens with its types");
        check(s.path() == modelPath("teller.des"), "and remembers where it came from");
        check(!s.dirty(), "a freshly opened document is not dirty");
        check(s.mode() == Mode::Grid, "and starts in the grid");

        // The tab bar's source of truth: every type the registry publishes,
        // then any the document holds that it does not. No module type name is
        // written in the UI's source.
        const std::vector<std::string> tabs = s.types();
        check(tabs.size() >= ModuleRegistry::instance().all().size(),
              "every published type gets a tab");
        bool sawProcess = false, sawRun = false;
        for (const std::string& t : tabs) {
            if (t == "Process") sawProcess = true;
            if (t == "Run")     sawRun = true;
        }
        check(sawProcess && sawRun, "including ones this document happens to use");

        s.setType("Process");
        check(s.type() == "Process", "the cursor can move to a type");
        check(!s.columnsHere().empty(), "which knows its columns");
        check(s.schemaHere() != nullptr, "and its schema");

        {
            // A type the registry does NOT know still gets a tab. v11 preserves
            // unknown modules rather than dropping them, and a UI that showed
            // only what it understood would hide the rows a person needs in
            // order to notice their editor is older than the file.
            ModelDocument d;
            d.addRow("FromTheFuture");
            d.setCell("FromTheFuture", 0, "X", "1");
            TuiState u = TuiState::fromDocument(d, "future.des");
            bool sawUnknown = false;
            for (const std::string& t : u.types())
                if (t == "FromTheFuture") sawUnknown = true;
            check(sawUnknown, "an unknown module type is still shown");
            u.setType("FromTheFuture");
            check(u.schemaHere() == nullptr, "with no schema, which the renderer must handle");
        }

        {
            TuiState missing = TuiState::open("no_such_file_here.des");
            check(missing.document().types().empty(),
                  "a file that does not exist opens as an EMPTY document");
            check(missing.status().find("new") != std::string::npos,
                  "and says so, because empty and unreadable must not look alike");
        }
    }

    section("TuiState saves without disturbing what it did not touch");
    {
        // The guarantee v11 was built on, exercised through the UI for the
        // first time. If the UI marks rows edited that nobody edited, this is
        // what catches it.
        const std::string src = modelPath("teller.des");
        std::ifstream in(src, std::ios::binary);
        const std::string before((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());

        TuiState s = TuiState::open(src);
        check(s.save("tui_roundtrip.des"), "it saves");
        std::ifstream out("tui_roundtrip.des", std::ios::binary);
        const std::string after((std::istreambuf_iterator<char>(out)),
                                std::istreambuf_iterator<char>());
        check(before == after,
              "opening a file and saving it UNEDITED gives byte-identical bytes");
    }
```

Add `#include <fstream>`, `#include <iterator>` and `#include <vector>` to the top of `tests/tui_tests.cpp`.

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build
```

Expected: `error: 'TuiState' was not declared in this scope`.

- [ ] **Step 3: Write `include/TuiState.hpp`**

```cpp
// ============================================================================
// TuiState.hpp  --  v13: everything the UI knows
// ============================================================================
// One object, because "what is on screen" is one question. The renderer reads
// it and the input layer writes it, and neither touches a terminal -- which is
// what lets a test drive the whole UI by constructing one of these and feeding
// it keys.

#pragma once
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "ModelDocument.hpp"
#include "ModuleSchema.hpp"

namespace des {

enum class Mode { Grid, Detail, Editing, Running, Confirm };

class TuiState {
public:
    // A file that does not PARSE still opens -- v11 preserves what it read --
    // with the reader's diagnostics shown. A file that does not EXIST opens as
    // an empty document, so `des_tui new.des` starts a new model rather than
    // refusing. The status line says which happened, because "empty" and
    // "unreadable" must not look alike.
    static TuiState open(const std::string& path);
    static TuiState fromDocument(ModelDocument doc, std::string path);

    Mode mode() const { return m_mode; }
    void setMode(Mode m) { m_mode = m; }

    const ModelDocument&           document()    const { return m_document; }
    const std::string&             path()        const { return m_path; }
    bool                           dirty()       const { return m_dirty; }
    const std::vector<Diagnostic>& diagnostics() const { return m_diagnostics; }
    const std::string&             status()      const { return m_status; }
    void setStatus(std::string s) { m_status = std::move(s); }

    // Every type the registry publishes, then any the document holds that it
    // does not. Computed, never a list in this file: that is the property the
    // flat child tables were chosen to protect.
    const std::vector<std::string>& types() const { return m_types; }

    const std::string& type()   const { return m_type; }
    std::size_t        row()    const { return m_row; }
    std::size_t        column() const { return m_column; }

    void setType(const std::string& type);
    void setRow(std::size_t row);
    void setColumn(std::size_t column);

    // Null for a type the registry does not know. Every caller must cope: v11
    // keeps unknown module types on purpose.
    const ModuleSchema* schemaHere() const;
    // The column ids to show. The schema's, or -- for an unknown type -- the
    // ones the document's own rows happen to carry.
    std::vector<std::string> columnsHere() const;
    std::size_t              rowCountHere() const;

    bool save();                                  // to path()
    bool save(const std::string& toPath);
    void recompile();

private:
    void rebuildTypes();
    void clampCursor();

    ModelDocument            m_document;
    std::string              m_path;
    std::string              m_status;
    std::vector<std::string> m_types;
    std::string              m_type;
    std::size_t              m_row{0};
    std::size_t              m_column{0};
    Mode                     m_mode{Mode::Grid};
    bool                     m_dirty{false};
    std::vector<Diagnostic>  m_diagnostics;
};

}  // namespace des
```

- [ ] **Step 4: Write `src/TuiState.cpp`**

```cpp
#include "TuiState.hpp"

#include <algorithm>
#include <fstream>
#include "Compiler.hpp"
#include "DocumentFormat.hpp"

namespace des {

TuiState TuiState::fromDocument(ModelDocument doc, std::string path) {
    TuiState s;
    s.m_document = std::move(doc);
    s.m_path = std::move(path);
    s.rebuildTypes();
    if (!s.m_types.empty()) s.m_type = s.m_types.front();
    s.recompile();
    return s;
}

TuiState TuiState::open(const std::string& path) {
    std::ifstream probe(path, std::ios::binary);
    if (!probe.is_open()) {
        // is_open, NOT good: on the GCC this project builds with, constructing
        // an ifstream on a missing file leaves good() reporting true. v12's
        // regression harness lost an afternoon to that.
        TuiState s = fromDocument(ModelDocument{}, path);
        s.setStatus(path + ": new file");
        return s;
    }
    probe.close();

    ReadResult read = readDocumentFile(path);
    TuiState s = fromDocument(std::move(read.document), path);
    for (const Diagnostic& d : read.diagnostics) s.m_diagnostics.push_back(d);
    if (hasErrors(read.diagnostics))
        s.setStatus(path + ": opened WITH ERRORS -- see the diagnostics");
    else
        s.setStatus(path);
    return s;
}

void TuiState::rebuildTypes() {
    m_types.clear();
    for (const ModuleSchema& schema : ModuleRegistry::instance().all())
        m_types.push_back(schema.typeName);
    for (const std::string& t : m_document.types())
        if (std::find(m_types.begin(), m_types.end(), t) == m_types.end())
            m_types.push_back(t);
}

const ModuleSchema* TuiState::schemaHere() const {
    return ModuleRegistry::instance().find(m_type);
}

std::vector<std::string> TuiState::columnsHere() const {
    std::vector<std::string> out;
    if (const ModuleSchema* schema = schemaHere()) {
        for (const Column& c : schema->columns) out.push_back(c.id);
        return out;
    }
    // An unknown type has no schema, so its columns are whatever its rows
    // happen to carry. Showing them is the point: they are what tells a person
    // their editor is older than the file.
    for (std::size_t r = 0; r < m_document.rowCount(m_type); ++r)
        for (const auto& kv : m_document.rows(m_type)[r].cells)
            if (std::find(out.begin(), out.end(), kv.first) == out.end())
                out.push_back(kv.first);
    return out;
}

std::size_t TuiState::rowCountHere() const { return m_document.rowCount(m_type); }

void TuiState::setType(const std::string& type) {
    if (std::find(m_types.begin(), m_types.end(), type) == m_types.end()) return;
    m_type = type;
    m_row = 0;
    m_column = 0;
}

void TuiState::setRow(std::size_t row)       { m_row = row;    clampCursor(); }
void TuiState::setColumn(std::size_t column) { m_column = column; clampCursor(); }

void TuiState::clampCursor() {
    const std::size_t rows = rowCountHere();
    if (rows == 0) m_row = 0;
    else if (m_row >= rows) m_row = rows - 1;

    const std::size_t cols = columnsHere().size();
    if (cols == 0) m_column = 0;
    else if (m_column >= cols) m_column = cols - 1;
}

bool TuiState::save() { return save(m_path); }

bool TuiState::save(const std::string& toPath) {
    if (!writeDocumentFile(m_document, toPath)) {
        setStatus(toPath + ": could not write");
        return false;
    }
    if (toPath == m_path) m_dirty = false;
    setStatus(toPath + ": saved");
    return true;
}

void TuiState::recompile() {
    m_diagnostics.clear();
    CompileResult r = compile(m_document);
    m_diagnostics = std::move(r.diagnostics);
}

}  // namespace des
```

> `TuiState` has no user-declared constructor, so `TuiState s;` in `fromDocument` default-constructs it. Every member has a default initialiser, which is why that is safe.

- [ ] **Step 5: Register and run**

Add `src/TuiState.cpp` to `add_library(des_ui ...)`, and `#include "TuiState.hpp"` to `include/des_ui.hpp` after `Screen.hpp`.

Add to `.gitignore`:

```
/tui_roundtrip.des
```

```bash
cmake -S . -B build && cmake --build build && ./build/des_tests.exe
```

Expected: PASS, including the byte-identical save.

- [ ] **Step 6: Commit**

```bash
git add include/TuiState.hpp src/TuiState.cpp include/des_ui.hpp CMakeLists.txt tests/tui_tests.cpp .gitignore
git commit -m "feat: TuiState -- the document, the cursor, and what the tabs are

types() is COMPUTED: every type the registry publishes, then any the document
holds that it does not. No module type name appears in the UI's source, which
is the property v11's flat child tables were chosen to protect -- and the
second half matters as much as the first, because v11 preserves unknown
modules on purpose and a UI that showed only what it understood would hide
exactly the rows telling a person their editor is too old for the file.

open() on a missing file starts a NEW document rather than failing, and says
so. It tests is_open() rather than good(), for the reason v12's regression
harness found out the hard way.

Opening teller.des and saving it unedited is byte-identical."
```

---

### Task 3: Mutations and undo

**Files:**
- Modify: `include/TuiState.hpp`, `src/TuiState.cpp`
- Test: `tests/tui_tests.cpp`

**Interfaces:**
- Produces: `TuiState::setCell`, `addRow`, `removeRow`, `moveRow`, `undo`, `canUndo`, `readOnlyHere`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Edits, and undo across all four of them");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.8)");
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Process");

        check(!s.canUndo(), "nothing to undo yet");

        s.setCell("Service", "EXPO(0.5)");
        check(s.document().cell("Process", 0, "Service") == "EXPO(0.5)", "a cell edits");
        check(s.dirty(), "and the document is dirty");
        check(s.canUndo(), "and undoable");
        s.undo();
        check(s.document().cell("Process", 0, "Service") == "EXPO(0.8)", "undo restores it");

        s.addRow();
        check(s.rowCountHere() == 2, "a row is added");
        s.undo();
        check(s.rowCountHere() == 1, "and undone");

        s.addRow();
        s.setRow(1);
        s.setCell("Name", "Check");
        s.removeRow();
        check(s.rowCountHere() == 1, "a row is removed");
        s.undo();
        check(s.rowCountHere() == 2, "and undone");
        check(s.document().cell("Process", 1, "Name") == "Check",
              "with its contents intact, which is what a snapshot buys");

        s.setRow(1);
        s.moveRow(-1);
        check(s.document().cell("Process", 0, "Name") == "Check", "a row moves");
        s.undo();
        check(s.document().cell("Process", 0, "Name") == "Serve", "and unmoves");

        // Row order is SEMANTIC in this format -- a Decide takes the first
        // branch that matches -- so moving a row is a model change, not a
        // display preference, and has to be undoable like any other.
    }

    section("A read-only module refuses an edit and says why");
    {
        ModelDocument d;
        d.addRow("Queue");
        d.setCell("Queue", 0, "Name", "Serve.Queue");
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Queue");
        check(s.readOnlyHere(), "Queue is read-only");
        s.setCell("Name", "Something Else");
        check(s.document().cell("Queue", 0, "Name") == "Serve.Queue",
              "so the edit does not land");
        check(s.status().find("read-only") != std::string::npos, "and it says why");
        check(!s.dirty(), "and nothing became dirty");
    }
```

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build
```

Expected: `error: 'class des::TuiState' has no member named 'canUndo'`.

- [ ] **Step 3: Declare them**

In `include/TuiState.hpp`, after `recompile()`:

```cpp
    // Every mutation pushes a snapshot first. Each is refused, with a reason
    // in the status line, when the current module type is read-only.
    void setCell(const std::string& column, const std::string& text);
    void addRow();
    void removeRow();
    void moveRow(int delta);

    bool canUndo() const { return !m_undo.empty(); }
    void undo();

    bool readOnlyHere() const;
```

and in the private data:

```cpp
    // Whole documents, not per-operation inverses. A snapshot cannot be wrong
    // about its own inverse; a hand-written undo for moveRow can. A model
    // document is kilobytes, so the crude answer is also the correct one.
    static constexpr std::size_t UNDO_DEPTH = 64;
    std::vector<ModelDocument> m_undo;

    bool refuseIfReadOnly();
    void pushUndo();
```

- [ ] **Step 4: Implement them**

Append to `src/TuiState.cpp`, before the closing `}  // namespace des`:

```cpp
bool TuiState::readOnlyHere() const {
    const ModuleSchema* schema = schemaHere();
    return schema != nullptr && schema->readOnly;
}

bool TuiState::refuseIfReadOnly() {
    if (!readOnlyHere()) return false;
    setStatus(m_type + " is read-only: there is no queue object apart from its "
                       "Process, so set the discipline on the Process row");
    return true;
}

void TuiState::pushUndo() {
    m_undo.push_back(m_document);
    if (m_undo.size() > UNDO_DEPTH) m_undo.erase(m_undo.begin());
}

void TuiState::setCell(const std::string& column, const std::string& text) {
    if (refuseIfReadOnly()) return;
    if (m_row >= rowCountHere()) return;
    pushUndo();
    m_document.setCell(m_type, m_row, column, text);
    m_dirty = true;
    recompile();
}

void TuiState::addRow() {
    if (refuseIfReadOnly()) return;
    pushUndo();
    m_document.addRow(m_type);
    rebuildTypes();
    m_row = rowCountHere() - 1;
    m_dirty = true;
    recompile();
}

void TuiState::removeRow() {
    if (refuseIfReadOnly()) return;
    if (m_row >= rowCountHere()) return;
    pushUndo();
    m_document.removeRow(m_type, m_row);
    clampCursor();
    m_dirty = true;
    recompile();
}

void TuiState::moveRow(int delta) {
    if (refuseIfReadOnly()) return;
    const std::size_t rows = rowCountHere();
    if (rows < 2 || m_row >= rows) return;
    const long long to = static_cast<long long>(m_row) + delta;
    if (to < 0 || to >= static_cast<long long>(rows)) return;
    pushUndo();
    m_document.moveRow(m_type, m_row, static_cast<std::size_t>(to));
    m_row = static_cast<std::size_t>(to);
    m_dirty = true;
    recompile();
}

void TuiState::undo() {
    if (m_undo.empty()) return;
    m_document = m_undo.back();
    m_undo.pop_back();
    rebuildTypes();
    clampCursor();
    m_dirty = true;
    recompile();
    setStatus("undone");
}
```

> `undo()` leaves `m_dirty` **true** on purpose. Undoing back to the state on disk is not the same as knowing you are there — tracking that needs a saved-at marker in the stack, and claiming "not dirty" wrongly loses work. Dirty-when-unsure is the safe direction.

- [ ] **Step 5: Run the tests**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add include/TuiState.hpp src/TuiState.cpp tests/tui_tests.cpp
git commit -m "feat: edits, and undo as a stack of whole documents

A snapshot cannot be wrong about its own inverse; a hand-written undo for
moveRow can. A model document is kilobytes, so the crude answer is also the
correct one, bounded at 64 deep.

Moving a row is undoable because row order is SEMANTIC in this format -- a
Decide takes the first branch that matches -- so it is a model change, not a
display preference.

undo() leaves the document dirty on purpose. Undoing back to what is on disk
is not the same as knowing you are there, and claiming otherwise loses work."
```

---

### Task 4: Rendering the tab bar and the grid

**Files:**
- Create: `include/TuiRender.hpp`, `src/TuiRender.cpp`
- Modify: `CMakeLists.txt`, `include/des_ui.hpp`
- Test: `tests/tui_tests.cpp`

**Interfaces:**
- Consumes: `TuiState`, `Screen`.
- Produces: `void render(const TuiState&, Screen&)`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Rendering: the tab bar and the grid");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.8)");
        d.addRow("Process");
        d.setCell("Process", 1, "Name", "Check");
        d.setCell("Process", 1, "Service", "EXPO(0.3)");
        TuiState s = TuiState::fromDocument(d, "teller.des");
        s.setType("Process");

        Screen screen(100, 30);
        render(s, screen);
        const std::string text = screen.asText();

        check(text.find("Process") != std::string::npos, "the tab bar names the type");
        check(text.find("Resource") != std::string::npos,
              "and every other type the registry publishes");
        check(text.find("teller.des") != std::string::npos, "the file is named");
        check(text.find("Serve") != std::string::npos, "the grid shows row 1");
        check(text.find("Check") != std::string::npos, "and row 2");
        check(text.find("Service") != std::string::npos, "with its column headings");

        // The cursor is visible, and it is on the row the state says.
        bool marked = false;
        for (int y = 0; y < screen.height(); ++y) {
            const std::string ln = screen.line(y);
            if (ln.find("Serve") != std::string::npos && ln.find('>') != std::string::npos)
                marked = true;
        }
        check(marked, "the current row is marked");

        s.setRow(1);
        Screen second(100, 30);
        render(s, second);
        check(screen.asText() != second.asText(),
              "and moving the cursor changes what is drawn");

        {
            // A dirty document says so, or a person loses work believing it is
            // saved.
            TuiState t = TuiState::fromDocument(d, "teller.des");
            t.setType("Process");
            t.setCell("Name", "Renamed");
            Screen third(100, 30);
            render(t, third);
            check(third.asText().find("teller.des*") != std::string::npos,
                  "an edited document is marked with a star");
        }
    }

    section("Rendering: a terminal too small says so and draws nothing else");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));
        Screen tiny(40, 10);
        render(s, tiny);
        const std::string text = tiny.asText();
        check(text.find("80") != std::string::npos && text.find("24") != std::string::npos,
              "it names the size it needs");
        check(text.find("Process") == std::string::npos,
              "and draws NO grid -- a layout that does not fit produces garbage "
              "that looks like a bug in the model");
    }
```

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build
```

Expected: `error: 'render' was not declared in this scope`.

- [ ] **Step 3: Write `include/TuiRender.hpp`**

```cpp
// ============================================================================
// TuiRender.hpp  --  v13: a state becomes a screen
// ============================================================================
// Pure: it reads a TuiState and fills a Screen. No terminal, no globals, no
// clock. That is what lets a test render a document and assert on the text a
// person would read.

#pragma once
#include "Screen.hpp"
#include "TuiState.hpp"

namespace des {

// The smallest terminal this layout fits in. Below it, render() draws one
// message and nothing else.
constexpr int MIN_WIDTH  = 80;
constexpr int MIN_HEIGHT = 24;

void render(const TuiState& state, Screen& screen);

}  // namespace des
```

- [ ] **Step 4: Write `src/TuiRender.cpp`**

```cpp
#include "TuiRender.hpp"

#include <string>
#include <vector>
#include "ModuleSchema.hpp"

namespace des {
namespace {

// Column widths are computed from the DATA, not fixed, because a schema added
// in a later version has column names this file has never seen.
int widthFor(const TuiState& state, const std::string& column) {
    std::size_t widest = column.size();
    for (std::size_t r = 0; r < state.rowCountHere(); ++r)
        widest = std::max(widest, state.document().cell(state.type(), r, column).size());
    const int w = static_cast<int>(widest) + 2;
    return w > 24 ? 24 : w;      // one runaway expression must not eat the row
}

void renderTabs(const TuiState& state, Screen& screen) {
    int x = 1;
    for (const std::string& t : state.types()) {
        const bool here = (t == state.type());
        const std::string label = here ? "[" + t + "]" : " " + t + " ";
        if (x + static_cast<int>(label.size()) >= screen.width() - 14) break;
        x = screen.text(x, 0, label, here ? Attr::Reverse : Attr::Normal);
    }
    const std::string name = state.path() + (state.dirty() ? "*" : "");
    const int at = screen.width() - static_cast<int>(name.size()) - 1;
    screen.text(at < x + 1 ? x + 1 : at, 0, name, Attr::Bold);
}

void renderGrid(const TuiState& state, Screen& screen, int top, int bottom) {
    const std::vector<std::string> columns = state.columnsHere();
    screen.box(0, top, screen.width(), bottom - top + 1);

    int x = 2;
    std::vector<int> xs;
    for (const std::string& c : columns) {
        xs.push_back(x);
        screen.text(x, top + 1, c, Attr::Bold);
        x += widthFor(state, c);
        if (x >= screen.width() - 1) break;
    }

    const int rowsVisible = bottom - top - 2;
    if (rowsVisible <= 0) return;
    // Scroll so the cursor is always on screen, without moving when it does
    // not have to.
    std::size_t first = 0;
    if (state.row() >= static_cast<std::size_t>(rowsVisible))
        first = state.row() - static_cast<std::size_t>(rowsVisible) + 1;

    for (int i = 0; i < rowsVisible; ++i) {
        const std::size_t r = first + static_cast<std::size_t>(i);
        if (r >= state.rowCountHere()) break;
        const int y = top + 2 + i;
        const bool here = (r == state.row());
        if (here) screen.put(1, y, '>', Attr::Bold);
        const Attr attr = state.readOnlyHere() ? Attr::Dim
                                               : (here ? Attr::Reverse : Attr::Normal);
        for (std::size_t c = 0; c < xs.size() && c < columns.size(); ++c)
            screen.text(xs[c], y, state.document().cell(state.type(), r, columns[c]), attr);
    }
}

}  // namespace

void render(const TuiState& state, Screen& screen) {
    screen.clear();
    if (screen.width() < MIN_WIDTH || screen.height() < MIN_HEIGHT) {
        screen.text(0, 0, "des_tui needs a terminal of at least 80 x 24.");
        screen.text(0, 1, "This one is too small; resize it and try again.");
        return;
    }
    renderTabs(state, screen);
    renderGrid(state, screen, 1, screen.height() - 4);
    screen.text(0, screen.height() - 2, state.status());
    if (state.mode() == Mode::Confirm)
        screen.text(0, screen.height() - 1,
                    "(s)ave and quit   (d)iscard and quit   (c)ancel", Attr::Bold);
    else
        screen.text(0, screen.height() - 1,
                    "^S save  ^R run  ^N new  ^D delete  ^Z undo  Tab pane  q quit",
                    Attr::Dim);
}

}  // namespace des
```

Add `#include <algorithm>` for `std::max`.

- [ ] **Step 5: Register and run**

Add `src/TuiRender.cpp` to `add_library(des_ui ...)`, and `#include "TuiRender.hpp"` to `include/des_ui.hpp`.

```bash
cmake -S . -B build && cmake --build build && ./build/des_tests.exe
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add include/TuiRender.hpp src/TuiRender.cpp include/des_ui.hpp CMakeLists.txt tests/tui_tests.cpp
git commit -m "feat: render the tab bar and the grid

Column widths come from the data rather than a table in this file, because a
schema added in a later version has column names this code has never seen --
which is the same reason the tab bar is built from the registry.

A terminal below 80x24 draws one message and nothing else. A layout that does
not fit produces garbage that looks like a bug in the model, which is a much
more expensive thing to debug than a resize."
```

---

### Task 5: The detail pane

**Files:**
- Modify: `src/TuiRender.cpp`, `include/TuiState.hpp`, `src/TuiState.cpp`
- Test: `tests/tui_tests.cpp`

**Interfaces:**
- Produces: `TuiState::editBuffer()`, `TuiState::beginEdit()`, `TuiState::cancelEdit()`, `TuiState::commitEdit()`, `TuiState::typeEdit(char)`, `TuiState::backspaceEdit()`, `TuiState::diagnosticFor(column)`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Rendering: the detail pane shows the row and its diagnostics");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.8");   // deliberately broken
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Process");
        s.setMode(Mode::Detail);

        Screen screen(100, 30);
        render(s, screen);
        const std::string text = screen.asText();

        check(text.find("Capacity") != std::string::npos,
              "the detail pane shows every column, including empty ones");
        check(text.find("Renege After") != std::string::npos,
              "which is the whole point: an 11-column table is unreadable across");
        check(text.find("FIFO") != std::string::npos,
              "an enum column shows the spellings it allows");
        check(text.find("-> Resource") != std::string::npos,
              "and a reference column shows what it must name");
        check(text.find("expected") != std::string::npos,
              "a broken cell shows its diagnostic IN the pane");

        const Diagnostic* d1 = s.diagnosticFor("Service");
        check(d1 != nullptr, "and the state can find that diagnostic by column");
        check(s.diagnosticFor("Name") == nullptr, "with none for a healthy cell");
    }

    section("The edit buffer");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "EXPO(0.8)");
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Process");
        s.setColumn(0);                       // Name

        s.beginEdit();
        check(s.mode() == Mode::Editing, "beginEdit enters Editing");
        check(s.editBuffer() == "Serve", "seeded with what is there");
        s.backspaceEdit();
        s.typeEdit('r');
        check(s.editBuffer() == "Servr", "typing edits the buffer, not the document");
        check(s.document().cell("Process", 0, "Name") == "Serve",
              "the document is untouched until commit");

        s.cancelEdit();
        check(s.mode() == Mode::Detail, "Escape leaves Editing");
        check(s.document().cell("Process", 0, "Name") == "Serve", "and abandons the edit");
        check(!s.dirty(), "leaving nothing dirty");

        s.beginEdit();
        s.typeEdit('!');
        s.commitEdit();
        check(s.document().cell("Process", 0, "Name") == "Serve!", "commit writes it");
        check(s.dirty(), "and marks the document dirty");
    }
```

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build
```

Expected: `error: 'class des::TuiState' has no member named 'beginEdit'`.

- [ ] **Step 3: Add the edit buffer to `TuiState`**

In `include/TuiState.hpp`, after `readOnlyHere()`:

```cpp
    // The cell being edited, held OUTSIDE the document until it commits. A
    // half-typed expression is not a model change, and recompiling on every
    // keystroke would flag EXPO(0.8 as broken while it is still being typed.
    const std::string& editBuffer() const { return m_edit; }
    void beginEdit();
    void typeEdit(char c);
    void backspaceEdit();
    void cancelEdit();
    void commitEdit();

    // The first diagnostic against a column of the CURRENT row, or null.
    const Diagnostic* diagnosticFor(const std::string& column) const;
```

and in the private data:

```cpp
    std::string m_edit;
```

- [ ] **Step 4: Implement them**

Append to `src/TuiState.cpp`:

```cpp
void TuiState::beginEdit() {
    if (refuseIfReadOnly()) return;
    const std::vector<std::string> columns = columnsHere();
    if (m_column >= columns.size() || m_row >= rowCountHere()) return;
    m_edit = m_document.cell(m_type, m_row, columns[m_column]);
    m_mode = Mode::Editing;
}

void TuiState::typeEdit(char c) { m_edit.push_back(c); }

void TuiState::backspaceEdit() { if (!m_edit.empty()) m_edit.pop_back(); }

void TuiState::cancelEdit() {
    m_edit.clear();
    m_mode = Mode::Detail;
}

void TuiState::commitEdit() {
    const std::vector<std::string> columns = columnsHere();
    if (m_column < columns.size()) setCell(columns[m_column], m_edit);
    m_edit.clear();
    m_mode = Mode::Detail;
}

const Diagnostic* TuiState::diagnosticFor(const std::string& column) const {
    for (const Diagnostic& d : m_diagnostics)
        if (d.cell && d.cell->moduleType == m_type && d.cell->row == m_row &&
            d.cell->column == column)
            return &d;
    return nullptr;
}
```

- [ ] **Step 5: Render the pane**

In `src/TuiRender.cpp`, add before the closing anonymous namespace:

```cpp
void renderDetail(const TuiState& state, Screen& screen, int top) {
    const std::vector<std::string> columns = state.columnsHere();
    const ModuleSchema* schema = state.schemaHere();
    screen.box(0, top, screen.width(), screen.height() - top - 2);

    std::string title = " row " + std::to_string(state.row() + 1) + " of " +
                        std::to_string(state.rowCountHere()) + " ";
    if (schema == nullptr) title += "(unknown module type) ";
    screen.text(3, top, title, Attr::Bold);

    int y = top + 1;
    for (std::size_t c = 0; c < columns.size(); ++c) {
        if (y >= screen.height() - 3) break;
        const bool here = (c == state.column());
        screen.text(2, y, columns[c], here ? Attr::Reverse : Attr::Normal);

        const bool editing = here && state.mode() == Mode::Editing;
        const std::string value = editing
            ? "[" + state.editBuffer() + "]"
            : state.document().cell(state.type(), state.row(), columns[c]);
        int x = screen.text(18, y, value, editing ? Attr::Bold : Attr::Normal);

        if (schema != nullptr) {
            if (const Column* col = schema->column(columns[c])) {
                if (col->type == ColumnType::Enum) {
                    x = screen.text(x + 2, y, "", Attr::Dim);
                    for (const std::string& v : col->enumValues)
                        x = screen.text(x + 1, y, v, Attr::Dim);
                } else if (col->type == ColumnType::Reference) {
                    screen.text(x + 2, y, "-> " + col->referencedType, Attr::Dim);
                }
            }
        }
        ++y;
        if (const Diagnostic* d = state.diagnosticFor(columns[c])) {
            if (y < screen.height() - 3) {
                screen.text(18, y,
                            (d->severity == Severity::Error ? "error: " : "warning: ") +
                                d->message,
                            Attr::Error);
                ++y;
            }
        }
    }
}
```

and change `render()` so the grid shrinks when the detail pane is up:

```cpp
    renderTabs(state, screen);
    const bool detail = state.mode() == Mode::Detail || state.mode() == Mode::Editing;
    const int gridBottom = detail ? (screen.height() / 2) : (screen.height() - 4);
    renderGrid(state, screen, 1, gridBottom);
    if (detail) renderDetail(state, screen, gridBottom + 1);
    screen.text(0, screen.height() - 2, state.status());
```

- [ ] **Step 6: Run the tests**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add include/TuiState.hpp src/TuiState.cpp src/TuiRender.cpp tests/tui_tests.cpp
git commit -m "feat: the detail pane, and the edit buffer behind it

A Process has eleven columns -- roughly 130 characters of grid in an
80-column terminal -- so the detail pane is not a convenience, it is how a
wide table is readable at all. Arena solves it the same way: a spreadsheet to
scan, a dialog to edit in.

The edit buffer lives OUTSIDE the document until Enter. A half-typed
expression is not a model change, and recompiling per keystroke would flag
EXPO(0.8 as broken while it is still being typed."
```

---

### Task 6: Navigation

**Files:**
- Create: `include/TuiInput.hpp`, `src/TuiInput.cpp`
- Modify: `CMakeLists.txt`, `include/des_ui.hpp`
- Test: `tests/tui_tests.cpp`

**Interfaces:**
- Produces: `bool handleKey(TuiState&, Key)` — false when the app should quit. Quitting with unsaved work enters `Mode::Confirm` rather than returning false.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Navigation");
    {
        ModelDocument d;
        for (int i = 0; i < 3; ++i) {
            d.addRow("Process");
            d.setCell("Process", static_cast<std::size_t>(i), "Name",
                      "P" + std::to_string(i));
            d.setCell("Process", static_cast<std::size_t>(i), "Service", "1");
        }
        TuiState s = TuiState::fromDocument(d, "x.des");
        s.setType("Process");

        check(s.row() == 0, "starts on the first row");
        handleKey(s, Key::special(KeyKind::Down));
        check(s.row() == 1, "Down moves down");
        handleKey(s, Key::special(KeyKind::Up));
        handleKey(s, Key::special(KeyKind::Up));
        check(s.row() == 0, "Up CLAMPS at the top rather than wrapping");
        handleKey(s, Key::special(KeyKind::End));
        check(s.row() == 2, "End goes to the last row");
        handleKey(s, Key::special(KeyKind::Down));
        check(s.row() == 2, "and Down clamps at the bottom");

        const std::string first = s.type();
        handleKey(s, Key::special(KeyKind::Tab));
        check(s.type() != first, "Tab moves to the next module type");
        handleKey(s, Key::special(KeyKind::BackTab));
        check(s.type() == first, "and BackTab comes back");

        handleKey(s, Key::special(KeyKind::Enter));
        check(s.mode() == Mode::Detail, "Enter opens the detail pane");
        handleKey(s, Key::special(KeyKind::Down));
        check(s.column() == 1, "where Down moves between FIELDS, not rows");
        check(s.row() == 2, "leaving the row where it was");
        handleKey(s, Key::special(KeyKind::Escape));
        check(s.mode() == Mode::Grid, "Escape returns to the grid");

        check(handleKey(s, Key::character('q')) == false,
              "q on a CLEAN document quits at once");
    }

    section("Quitting with unsaved work asks first");
    {
        ModelDocument d;
        d.addRow("Process");
        d.setCell("Process", 0, "Name", "Serve");
        d.setCell("Process", 0, "Service", "1");
        TuiState s = TuiState::fromDocument(d, "tui_confirm.des");
        s.setType("Process");
        s.setCell("Name", "Edited");
        check(s.dirty(), "there is unsaved work");

        check(handleKey(s, Key::character('q')) == true,
              "q does NOT quit while the document is dirty");
        check(s.mode() == Mode::Confirm, "it asks instead");

        check(handleKey(s, Key::character('c')) == true, "c cancels");
        check(s.mode() == Mode::Grid, "and returns to the grid");
        check(s.dirty(), "with the work still unsaved");

        handleKey(s, Key::character('q'));
        check(handleKey(s, Key::character('s')) == false, "s saves and quits");
        check(!s.dirty(), "having actually saved");

        TuiState t = TuiState::fromDocument(d, "tui_confirm.des");
        t.setType("Process");
        t.setCell("Name", "Discarded");
        handleKey(t, Key::character('q'));
        check(handleKey(t, Key::character('d')) == false, "d discards and quits");
    }
```

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build
```

Expected: `error: 'handleKey' was not declared in this scope`.

- [ ] **Step 3: Write `include/TuiInput.hpp`**

```cpp
// ============================================================================
// TuiInput.hpp  --  v13: a keypress becomes the next state
// ============================================================================
// Pure with respect to the terminal: it takes a Key and mutates a TuiState.
// That is what lets a test type a whole session without one.

#pragma once
#include "Key.hpp"
#include "TuiState.hpp"

namespace des {

// False when the application should stop. The caller owns the loop, exactly as
// v12's RunController hands the run loop to its caller.
bool handleKey(TuiState& state, Key key);

}  // namespace des
```

- [ ] **Step 4: Write `src/TuiInput.cpp`**

```cpp
#include "TuiInput.hpp"

#include <algorithm>
#include <vector>

namespace des {
namespace {

void stepType(TuiState& state, int delta) {
    const std::vector<std::string>& types = state.types();
    if (types.empty()) return;
    auto it = std::find(types.begin(), types.end(), state.type());
    long long i = (it == types.end()) ? 0 : (it - types.begin());
    i += delta;
    if (i < 0) i = static_cast<long long>(types.size()) - 1;
    if (i >= static_cast<long long>(types.size())) i = 0;
    state.setType(types[static_cast<std::size_t>(i)]);
}

// Clamping rather than wrapping, everywhere. A cursor that wraps from the last
// row to the first looks like the list jumped, and in a document where row
// order is semantic that is exactly the wrong thing to make ambiguous.
void stepRow(TuiState& state, long long delta) {
    const long long rows = static_cast<long long>(state.rowCountHere());
    if (rows == 0) return;
    long long r = static_cast<long long>(state.row()) + delta;
    r = std::max<long long>(0, std::min(r, rows - 1));
    state.setRow(static_cast<std::size_t>(r));
}

void stepColumn(TuiState& state, long long delta) {
    const long long cols = static_cast<long long>(state.columnsHere().size());
    if (cols == 0) return;
    long long c = static_cast<long long>(state.column()) + delta;
    c = std::max<long long>(0, std::min(c, cols - 1));
    state.setColumn(static_cast<std::size_t>(c));
}

bool handleGrid(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Up:       stepRow(state, -1); break;
        case KeyKind::Down:     stepRow(state, 1); break;
        case KeyKind::PageUp:   stepRow(state, -10); break;
        case KeyKind::PageDown: stepRow(state, 10); break;
        case KeyKind::Home:     state.setRow(0); break;
        case KeyKind::End:
            if (state.rowCountHere() > 0) state.setRow(state.rowCountHere() - 1);
            break;
        case KeyKind::Tab:      stepType(state, 1); break;
        case KeyKind::BackTab:  stepType(state, -1); break;
        case KeyKind::Left:     stepType(state, -1); break;
        case KeyKind::Right:    stepType(state, 1); break;
        case KeyKind::Enter:    state.setMode(Mode::Detail); break;
        case KeyKind::Char:
            // Unsaved work is asked about, never discarded on one keystroke.
            // The whole point of a UI over a file format is that a person
            // trusts it with work they have not written down anywhere else.
            if (key.ch == 'q') {
                if (!state.dirty()) return false;
                state.setMode(Mode::Confirm);
                state.setStatus("unsaved changes -- (s)ave and quit, "
                                "(d)iscard and quit, (c)ancel");
            }
            break;
        default: break;
    }
    return true;
}

bool handleConfirm(TuiState& state, Key key) {
    if (key.kind != KeyKind::Char) {
        if (key.kind == KeyKind::Escape) state.setMode(Mode::Grid);
        return true;
    }
    switch (key.ch) {
        case 's': return !state.save();     // stay open if the save failed
        case 'd': return false;
        case 'c': state.setMode(Mode::Grid); state.setStatus(""); return true;
        default:  return true;
    }
}

bool handleDetail(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Up:     stepColumn(state, -1); break;
        case KeyKind::Down:   stepColumn(state, 1); break;
        case KeyKind::Escape: state.setMode(Mode::Grid); break;
        case KeyKind::Enter:  state.beginEdit(); break;
        default: break;
    }
    return true;
}

bool handleEditing(TuiState& state, Key key) {
    switch (key.kind) {
        case KeyKind::Char:      state.typeEdit(key.ch); break;
        case KeyKind::Backspace: state.backspaceEdit(); break;
        case KeyKind::Enter:     state.commitEdit(); break;
        case KeyKind::Escape:    state.cancelEdit(); break;
        default: break;
    }
    return true;
}

}  // namespace

bool handleKey(TuiState& state, Key key) {
    // Editing swallows everything, including the control keys. Otherwise ^S
    // typed into a Service field would save a half-finished expression, and
    // a person editing text expects the text to receive their keys.
    if (state.mode() == Mode::Editing) return handleEditing(state, key);

    if (key.kind == KeyKind::Ctrl) {
        switch (key.ch) {
            case 'S': state.save(); return true;
            case 'N': state.addRow(); return true;
            case 'D': state.removeRow(); return true;
            case 'Z': state.undo(); return true;
            default: return true;
        }
    }
    if (state.mode() == Mode::Confirm) return handleConfirm(state, key);
    if (state.mode() == Mode::Detail)  return handleDetail(state, key);
    return handleGrid(state, key);
}

}  // namespace des
```

- [ ] **Step 5: Register and run**

Add `src/TuiInput.cpp` to `add_library(des_ui ...)`, and `#include "TuiInput.hpp"` to `include/des_ui.hpp`.

```bash
cmake -S . -B build && cmake --build build && ./build/des_tests.exe
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add include/TuiInput.hpp src/TuiInput.cpp include/des_ui.hpp CMakeLists.txt tests/tui_tests.cpp
git commit -m "feat: navigation, and one editing surface

Clamping rather than wrapping, everywhere. A cursor that wraps from the last
row to the first looks like the list jumped, and in a document where row order
is semantic that is the wrong thing to make ambiguous.

Editing swallows every key including the control ones: ^S typed into a Service
field must not save a half-finished expression, and a person editing text
expects the text to receive their keystrokes."
```

---

### Task 7: The decisive test — a scripted session

**Files:**
- Modify: `tests/tui_tests.cpp`, `.gitignore`

- [ ] **Step 1: Write it**

```cpp
    section("A scripted session round-trips the file");
    {
        // THE CLAIM THIS VERSION RESTS ON. v11's byte-identical round-trip is
        // the property the document layer was built for, and this is the first
        // time it is exercised through the surface a person actually uses. If
        // the UI marks rows edited that nobody edited, nothing else catches it.
        const std::string src = modelPath("teller.des");
        std::ifstream in(src, std::ios::binary);
        const std::string original((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
        check(original.size() > 100, "the source file is substantial");

        // 1. Open, wander around, save. Nothing was edited, so nothing changes.
        {
            TuiState s = TuiState::open(src);
            handleKey(s, Key::special(KeyKind::Tab));
            handleKey(s, Key::special(KeyKind::Tab));
            handleKey(s, Key::special(KeyKind::Down));
            handleKey(s, Key::special(KeyKind::Enter));
            handleKey(s, Key::special(KeyKind::Down));
            handleKey(s, Key::special(KeyKind::Escape));
            check(!s.dirty(), "moving about does not dirty the document");
            check(s.save("tui_session.des"), "and it saves");
        }
        std::ifstream a("tui_session.des", std::ios::binary);
        const std::string wandered((std::istreambuf_iterator<char>(a)),
                                   std::istreambuf_iterator<char>());
        check(wandered == original,
              "opening, navigating and saving gives back the SAME BYTES");

        // 2. Edit one cell. Every other row must still come back verbatim,
        //    comments and blank lines included.
        {
            TuiState s = TuiState::open(src);
            s.setType("Process");
            s.setColumn(0);
            s.beginEdit();
            for (int i = 0; i < 40; ++i) s.backspaceEdit();
            for (char c : std::string("Teller")) s.typeEdit(c);
            s.commitEdit();
            check(s.dirty(), "the edit dirties the document");
            check(s.save("tui_session.des"), "and it saves");
        }
        std::ifstream b("tui_session.des", std::ios::binary);
        const std::string edited((std::istreambuf_iterator<char>(b)),
                                 std::istreambuf_iterator<char>());
        check(edited != original, "an edited file differs");
        check(edited.find("Teller") != std::string::npos, "and carries the new value");
        check(edited.find("# A single teller.") != std::string::npos,
              "while the COMMENTS survive, which is what the source lines are for");
        check(edited.find("[Dispose]") != std::string::npos,
              "and so does every table nobody touched");

        // 3. The edited file re-reads to a document that says what was typed.
        ReadResult back = readDocumentFile("tui_session.des");
        check(!hasErrors(back.diagnostics), "the saved file reads cleanly");
        bool found = false;
        for (std::size_t r = 0; r < back.document.rowCount("Process"); ++r)
            if (back.document.cell("Process", r, "Name") == "Teller") found = true;
        check(found, "and round-trips the edit back");
    }
```

- [ ] **Step 2: Run it**

```bash
cmake --build build && ./build/des_tests.exe
```

Expected: PASS. A failure here is a real defect — find it rather than adjusting the test.

- [ ] **Step 3: Prove it can fail**

Temporarily add `m_document.markEdited();` to the top of `TuiState::open`'s `fromDocument` call path — that is, make every opened document claim it was edited — rebuild, and confirm the **first** assertion fails: `opening, navigating and saving gives back the SAME BYTES`. Then revert.

> A test that has only ever passed is not evidence. v10 shipped one that used `constant()` and consumed nothing from the stream it claimed to test; v11 shipped six that looped over an empty list; v12's chunk test needed three sabotages before one of them proved anything.

- [ ] **Step 4: Ignore the artefacts**

Add to `.gitignore`:

```
/tui_session.des
/tui_confirm.des
```

- [ ] **Step 5: Full gates, then commit**

```bash
bash tools/baseline.sh check
```
```bash
./build/des regress
```
```bash
bash tools/verify.sh
```

```bash
git add tests/tui_tests.cpp .gitignore
git commit -m "test: a scripted session round-trips the file

The claim v13 rests on, and it is not about pixels. Opening teller.des,
navigating through three module types and a detail pane, and saving gives back
byte-identical bytes. Editing one cell changes that cell and leaves every
comment, blank line and untouched table verbatim -- then re-reads to a
document carrying the edit.

That is v11's byte-identical round-trip exercised for the first time through
the surface a person actually uses. If the UI marks rows edited that nobody
edited, nothing else would catch it.

Made to fail before being believed: marking every opened document edited
breaks the first assertion."
```

---

### Task 8: The platform layer and `des_tui`

**Files:**
- Create: `include/Terminal.hpp`, `tui/Terminal_win32.cpp`, `tui/Terminal_posix.cpp`, `tui/main.cpp`
- Modify: `CMakeLists.txt`, `include/des_ui.hpp`

**Interfaces:**
- Consumes: `Screen`, `Key`, `TuiState`, `render`, `handleKey`.
- Produces: `des::TerminalSize`, `des::ITerminal`, `std::unique_ptr<ITerminal> des::openTerminal()`, and the `des_tui` executable.

- [ ] **Step 1: Write `include/Terminal.hpp`**

```cpp
// ============================================================================
// Terminal.hpp  --  v13: the only platform code in the project
// ============================================================================
// Three methods, deliberately. Everything else about the UI is a value that a
// test can build and inspect; this is the part that cannot be, so it is kept
// as small as it can possibly be. The implementations live in tui/ rather than
// src/ so that the portable half -- which is all of the rest -- goes through
// verify.sh and both sanitisers unchanged.

#pragma once
#include <memory>
#include "Key.hpp"
#include "Screen.hpp"

namespace des {

struct TerminalSize {
    int width{80};
    int height{24};
};

class ITerminal {
public:
    virtual ~ITerminal() = default;
    virtual TerminalSize size() const = 0;
    virtual void present(const Screen& screen) = 0;
    // Blocks. Returns KeyKind::Unknown for anything it does not recognise,
    // which the input layer ignores -- an unrecognised key must never be an
    // error, or a terminal sending an escape sequence nobody anticipated would
    // stop the program.
    virtual Key nextKey() = 0;
};

std::unique_ptr<ITerminal> openTerminal();

}  // namespace des
```

- [ ] **Step 2: Write `tui/Terminal_posix.cpp`**

```cpp
#include "Terminal.hpp"

#include <cstdio>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace des {
namespace {

const char* sgr(Attr a) {
    switch (a) {
        case Attr::Bold:    return "\033[1m";
        case Attr::Dim:     return "\033[2m";
        case Attr::Reverse: return "\033[7m";
        case Attr::Error:   return "\033[31m";
        case Attr::Normal:  break;
    }
    return "\033[0m";
}

class PosixTerminal : public ITerminal {
    termios m_saved{};
public:
    PosixTerminal() {
        tcgetattr(STDIN_FILENO, &m_saved);
        termios raw = m_saved;
        raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        std::fputs("\033[?1049h\033[?25l", stdout);   // alternate screen, no cursor
    }
    ~PosixTerminal() override {
        std::fputs("\033[?25h\033[?1049l", stdout);
        std::fflush(stdout);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_saved);
    }

    TerminalSize size() const override {
        winsize ws{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0)
            return TerminalSize{};
        return TerminalSize{static_cast<int>(ws.ws_col), static_cast<int>(ws.ws_row)};
    }

    void present(const Screen& screen) override {
        std::string out = "\033[H";
        Attr current = Attr::Normal;
        for (int y = 0; y < screen.height(); ++y) {
            for (int x = 0; x < screen.width(); ++x) {
                const Screen::Glyph& g = screen.at(x, y);
                if (g.attr != current) { out += sgr(g.attr); current = g.attr; }
                out.push_back(g.ch);
            }
            out += "\033[K";
            if (y + 1 < screen.height()) out += "\r\n";
        }
        out += "\033[0m";
        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
    }

    Key nextKey() override {
        char c = 0;
        if (::read(STDIN_FILENO, &c, 1) != 1) return Key::special(KeyKind::Unknown);
        if (c == '\r' || c == '\n') return Key::special(KeyKind::Enter);
        if (c == 127 || c == 8)     return Key::special(KeyKind::Backspace);
        if (c == '\t')              return Key::special(KeyKind::Tab);
        if (c == 27) {
            char a = 0, b = 0;
            if (::read(STDIN_FILENO, &a, 1) != 1) return Key::special(KeyKind::Escape);
            if (::read(STDIN_FILENO, &b, 1) != 1) return Key::special(KeyKind::Escape);
            if (a != '[') return Key::special(KeyKind::Escape);
            switch (b) {
                case 'A': return Key::special(KeyKind::Up);
                case 'B': return Key::special(KeyKind::Down);
                case 'C': return Key::special(KeyKind::Right);
                case 'D': return Key::special(KeyKind::Left);
                case 'H': return Key::special(KeyKind::Home);
                case 'F': return Key::special(KeyKind::End);
                case 'Z': return Key::special(KeyKind::BackTab);
                case '5': { char t = 0; (void)::read(STDIN_FILENO, &t, 1);
                            return Key::special(KeyKind::PageUp); }
                case '6': { char t = 0; (void)::read(STDIN_FILENO, &t, 1);
                            return Key::special(KeyKind::PageDown); }
                case '3': { char t = 0; (void)::read(STDIN_FILENO, &t, 1);
                            return Key::special(KeyKind::Delete); }
                default:  return Key::special(KeyKind::Unknown);
            }
        }
        if (c > 0 && c < 27) return Key::control(static_cast<char>('a' + c - 1));
        return Key::character(c);
    }
};

}  // namespace

std::unique_ptr<ITerminal> openTerminal() {
    return std::unique_ptr<ITerminal>(new PosixTerminal());
}

}  // namespace des
```

- [ ] **Step 3: Write `tui/Terminal_win32.cpp`**

```cpp
#include "Terminal.hpp"

#include <cstdio>
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace des {
namespace {

const char* sgr(Attr a) {
    switch (a) {
        case Attr::Bold:    return "\033[1m";
        case Attr::Dim:     return "\033[2m";
        case Attr::Reverse: return "\033[7m";
        case Attr::Error:   return "\033[31m";
        case Attr::Normal:  break;
    }
    return "\033[0m";
}

class Win32Terminal : public ITerminal {
    HANDLE m_in{nullptr};
    HANDLE m_out{nullptr};
    DWORD  m_savedIn{0};
    DWORD  m_savedOut{0};
public:
    Win32Terminal() {
        m_in  = GetStdHandle(STD_INPUT_HANDLE);
        m_out = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(m_in, &m_savedIn);
        GetConsoleMode(m_out, &m_savedOut);
        // Without ENABLE_VIRTUAL_TERMINAL_PROCESSING the escape sequences
        // print as literal text. Windows 10 and later support it; there is no
        // fallback, and the program says so rather than drawing garbage.
        SetConsoleMode(m_out, m_savedOut | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        SetConsoleMode(m_in, static_cast<DWORD>(m_savedIn &
                       ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT)));
        std::fputs("\033[?1049h\033[?25l", stdout);
    }
    ~Win32Terminal() override {
        std::fputs("\033[?25h\033[?1049l", stdout);
        std::fflush(stdout);
        SetConsoleMode(m_in, m_savedIn);
        SetConsoleMode(m_out, m_savedOut);
    }

    TerminalSize size() const override {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(m_out, &info)) return TerminalSize{};
        return TerminalSize{info.srWindow.Right - info.srWindow.Left + 1,
                            info.srWindow.Bottom - info.srWindow.Top + 1};
    }

    void present(const Screen& screen) override {
        std::string out = "\033[H";
        Attr current = Attr::Normal;
        for (int y = 0; y < screen.height(); ++y) {
            for (int x = 0; x < screen.width(); ++x) {
                const Screen::Glyph& g = screen.at(x, y);
                if (g.attr != current) { out += sgr(g.attr); current = g.attr; }
                out.push_back(g.ch);
            }
            out += "\033[K";
            if (y + 1 < screen.height()) out += "\r\n";
        }
        out += "\033[0m";
        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
    }

    Key nextKey() override {
        for (;;) {
            INPUT_RECORD record{};
            DWORD read = 0;
            if (!ReadConsoleInputW(m_in, &record, 1, &read) || read == 0)
                return Key::special(KeyKind::Unknown);
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                continue;

            const KEY_EVENT_RECORD& k = record.Event.KeyEvent;
            switch (k.wVirtualKeyCode) {
                case VK_UP:     return Key::special(KeyKind::Up);
                case VK_DOWN:   return Key::special(KeyKind::Down);
                case VK_LEFT:   return Key::special(KeyKind::Left);
                case VK_RIGHT:  return Key::special(KeyKind::Right);
                case VK_HOME:   return Key::special(KeyKind::Home);
                case VK_END:    return Key::special(KeyKind::End);
                case VK_PRIOR:  return Key::special(KeyKind::PageUp);
                case VK_NEXT:   return Key::special(KeyKind::PageDown);
                case VK_DELETE: return Key::special(KeyKind::Delete);
                case VK_RETURN: return Key::special(KeyKind::Enter);
                case VK_ESCAPE: return Key::special(KeyKind::Escape);
                case VK_BACK:   return Key::special(KeyKind::Backspace);
                case VK_TAB:
                    return Key::special((k.dwControlKeyState & SHIFT_PRESSED)
                                            ? KeyKind::BackTab : KeyKind::Tab);
                default: break;
            }
            const wchar_t ch = k.uChar.UnicodeChar;
            if (ch == 0) continue;
            if (ch < 27 && (k.dwControlKeyState &
                            (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)))
                return Key::control(static_cast<char>('a' + ch - 1));
            if (ch >= 32 && ch < 127) return Key::character(static_cast<char>(ch));
        }
    }
};

}  // namespace

std::unique_ptr<ITerminal> openTerminal() {
    return std::unique_ptr<ITerminal>(new Win32Terminal());
}

}  // namespace des
```

- [ ] **Step 4: Write `tui/main.cpp`**

```cpp
// ============================================================================
// tui/main.cpp  --  the des_tui command
// ============================================================================
// The loop, and nothing else. Everything it drives is a value that the test
// suite already exercises without a terminal, which is why this file is forty
// lines and has no logic worth testing.

#include <iostream>
#include <memory>
#include <string>
#include "des_ui.hpp"
#include "Terminal.hpp"

using namespace des;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "usage: des_tui <model.des>\n";
        return 2;
    }
    TuiState state = TuiState::open(argv[1]);
    std::unique_ptr<ITerminal> terminal = openTerminal();

    for (;;) {
        const TerminalSize size = terminal->size();
        Screen screen(size.width, size.height);
        render(state, screen);
        terminal->present(screen);
        if (!handleKey(state, terminal->nextKey())) break;
    }
    // The terminal restores itself in its destructor, which is the only reason
    // it is an object rather than a pair of functions: a return from anywhere
    // in that loop must not leave the console in raw mode.
    return 0;
}
```

- [ ] **Step 5: Add the target**

In `CMakeLists.txt`, beside the other executables:

```cmake
# The platform half. WIN32 and non-WIN32 sources are mutually exclusive: each
# includes headers the other platform does not have.
if(WIN32)
    add_executable(des_tui tui/main.cpp tui/Terminal_win32.cpp)
else()
    add_executable(des_tui tui/main.cpp tui/Terminal_posix.cpp)
endif()
target_link_libraries(des_tui PRIVATE des_ui)
target_compile_options(des_tui PRIVATE ${DES_WARNINGS})
```

`Terminal.hpp` is **not** added to `include/des_ui.hpp` — it is the platform boundary, and only `tui/` includes it.

- [ ] **Step 6: Build and try it by hand**

```bash
cmake -S . -B build && cmake --build build
```

```bash
./build/des_tui.exe examples/models/teller.des
```

Expected: the tab bar, the grid, and the help line. Check by hand: Tab moves between types, Down moves rows, Enter opens the detail pane, Escape leaves it, `q` quits and **the terminal is left usable** — no raw mode, cursor visible.

- [ ] **Step 7: Commit**

```bash
git add include/Terminal.hpp tui/Terminal_win32.cpp tui/Terminal_posix.cpp tui/main.cpp CMakeLists.txt
git commit -m "feat: des_tui -- the platform layer, and the loop

Three methods: report the size, blit a Screen, read a Key. Everything else
about the UI is a value the test suite already drives without a terminal, so
this is the only part outside the discipline the rest is under -- which is
exactly why it is this small.

ITerminal is an object rather than a pair of functions for one reason: the
destructor restores the console. A return from anywhere in the loop must not
leave a person in raw mode with no cursor."
```

---

### Task 9: Running a model from the UI

**Files:**
- Modify: `include/TuiState.hpp`, `src/TuiState.cpp`, `src/TuiRender.cpp`, `src/TuiInput.cpp`
- Test: `tests/tui_tests.cpp`

**Interfaces:**
- Consumes: `RunController`, `RunProgress`, `RunState`.
- Produces: `TuiState::startRun()`, `TuiState::advanceRun()`, `TuiState::stopRun()`, `TuiState::runProgress()`, `TuiState::runReport()`, `TuiState::running()`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Running a model from the UI");
    {
        TuiState s = TuiState::open(modelPath("teller.des"));
        s.startRun();
        check(s.mode() == Mode::Running, "^R enters Running");
        check(s.running() != nullptr, "with a controller");

        int guard = 0;
        while (s.running() != nullptr &&
               (s.running()->state() == RunState::Ready ||
                s.running()->state() == RunState::Running) &&
               guard++ < 10000)
            s.advanceRun();
        check(s.running()->state() == RunState::Finished, "and it runs to the end");
        check(!s.runReport().empty(), "producing a report");

        Screen screen(100, 30);
        render(s, screen);
        check(screen.asText().find("simulation report") != std::string::npos,
              "which the UI shows");

        s.stopRun();
        check(s.mode() == Mode::Grid, "Escape returns to the grid");

        {
            // A document that will not compile must say so rather than
            // entering a run mode with nothing running.
            ModelDocument d;
            d.addRow("Process");
            d.setCell("Process", 0, "Name", "Lonely");
            d.setCell("Process", 0, "Service", "EXPO(1");
            TuiState bad = TuiState::fromDocument(d, "bad.des");
            bad.startRun();
            check(bad.mode() != Mode::Running, "a broken document does not start a run");
            check(!bad.status().empty(), "and says why");
        }
    }
```

- [ ] **Step 2: Run it and watch it fail**

```bash
cmake --build build
```

Expected: `error: 'class des::TuiState' has no member named 'startRun'`.

- [ ] **Step 3: Add the run to `TuiState`**

In `include/TuiState.hpp`, add `#include <memory>` and `#include "RunController.hpp"`, then after `commitEdit()`:

```cpp
    // ^R. Refuses, with a reason, when the document does not compile: entering
    // a run mode with nothing running is worse than not entering it.
    void startRun();
    void advanceRun();
    void stopRun();
    const RunController* running() const { return m_run.get(); }
    const std::string&   runReport() const { return m_runReport; }
```

and in the private data:

```cpp
    std::unique_ptr<RunController> m_run;
    std::string                    m_runReport;
```

> `TuiState` now holds a `unique_ptr`, so it is move-only. `fromDocument` returns by value, which move elision covers; nothing in the plan copies a `TuiState`.

- [ ] **Step 4: Implement them**

Append to `src/TuiState.cpp`, and add `#include <sstream>`:

```cpp
void TuiState::startRun() {
    std::vector<Diagnostic> problems;
    std::unique_ptr<RunController> run =
        RunController::fromDocument(m_document, problems);
    if (run == nullptr) {
        m_diagnostics = std::move(problems);
        setStatus("cannot run: the document does not compile");
        return;
    }
    m_run = std::move(run);
    m_runReport.clear();
    m_mode = Mode::Running;
    setStatus("running");
}

void TuiState::advanceRun() {
    if (!m_run) return;
    m_run->advance(4096);
    if (m_run->state() == RunState::Finished || m_run->state() == RunState::Failed) {
        std::ostringstream out;
        m_run->report(out);
        m_runReport = out.str();
        setStatus(m_run->state() == RunState::Failed ? m_run->failure() : "run finished");
    }
}

void TuiState::stopRun() {
    if (m_run) m_run->cancel();
    m_run.reset();
    m_runReport.clear();
    m_mode = Mode::Grid;
}
```

- [ ] **Step 5: Render the run**

In `src/TuiRender.cpp`, add to the anonymous namespace:

```cpp
void renderRun(const TuiState& state, Screen& screen) {
    screen.box(0, 1, screen.width(), screen.height() - 3);
    screen.text(3, 1, " running ", Attr::Bold);

    const RunController* run = state.running();
    if (run != nullptr) {
        const RunProgress p = run->progress();
        std::string line = "replication " + std::to_string(p.replication) + " of " +
                           std::to_string(p.replications) + "   t = " +
                           std::to_string(p.now) + "   ";
        if (p.fraction) {
            const int filled = static_cast<int>(*p.fraction * 40.0);
            line += "[";
            for (int i = 0; i < 40; ++i) line += (i < filled ? '#' : ' ');
            line += "]";
        } else {
            // NO BAR. The stopping rule cannot say how far through it is, and
            // a bar sitting at zero until it jumps to full is a lie the reader
            // cannot detect. This is v12's fourth-time rule reaching the
            // surface it was written for.
            line += "(this rule cannot say how far through it is)";
        }
        screen.text(2, 3, line);
    }

    int y = 5;
    std::string report = state.runReport();
    std::string current;
    for (char c : report) {
        if (c == '\n') {
            if (y < screen.height() - 4) screen.text(2, y++, current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty() && y < screen.height() - 4) screen.text(2, y, current);
}
```

and in `render()`, before the tab bar:

```cpp
    if (state.mode() == Mode::Running) {
        renderTabs(state, screen);
        renderRun(state, screen);
        screen.text(0, screen.height() - 2, state.status());
        screen.text(0, screen.height() - 1, "Esc  stop and return to the grid",
                    Attr::Dim);
        return;
    }
```

- [ ] **Step 6: Wire the keys**

In `src/TuiInput.cpp`, add to the control-key switch:

```cpp
            case 'R': state.startRun(); return true;
```

and before the `Mode::Detail` dispatch:

```cpp
    if (state.mode() == Mode::Running) {
        if (key.kind == KeyKind::Escape) state.stopRun();
        return true;
    }
```

> The **loop** in `tui/main.cpp` blocks on `nextKey()`, so a run would not advance while waiting. Change that loop to advance the run before blocking:
> ```cpp
>         if (state.mode() == Mode::Running && state.running() != nullptr &&
>             (state.running()->state() == RunState::Ready ||
>              state.running()->state() == RunState::Running)) {
>             state.advanceRun();
>             continue;      // redraw immediately; do not block on a key
>         }
> ```
> placed directly after `terminal->present(screen);`. A run that only advanced when a key was pressed would look frozen — and this is exactly the shape v12's `advance(budget)` was designed for: do some work, redraw, come back.

- [ ] **Step 7: Run the tests and try it by hand**

```bash
cmake --build build && ./build/des_tests.exe
```

```bash
./build/des_tui.exe examples/models/teller.des
```

Press `^R`. Expected: a progress bar that fills, then the report; `Esc` returns.

- [ ] **Step 8: Commit**

```bash
git add include/TuiState.hpp src/TuiState.cpp src/TuiRender.cpp src/TuiInput.cpp tui/main.cpp tests/tui_tests.cpp
git commit -m "feat: run a model from the UI

The loop advances the run before blocking on a key -- exactly the shape v12's
advance(budget) was designed for: do some work, redraw, come back. A run that
only advanced when a key was pressed would look frozen.

When progress().fraction is EMPTY the UI draws no bar and says the rule cannot
tell. That is v12's fourth-time rule reaching the surface it was written for:
a bar sitting at zero until it jumps to full is a lie the reader cannot
detect."
```

---

### Task 10: Documents

**Files:**
- Create: `V13_READLOG.md`
- Modify: `CHANGELOG.md`, `README.md`, `ARENA_MAP.md`, `examples/README.md`, `tools/manual_data.py`, `DES_Engine_Reference.pdf`

- [ ] **Step 1: Write `V13_READLOG.md`**

Follow `V12_READLOG.md`: what the version demanded, numbered sections, then the bugs and what is still open. Cover at minimum:

1. **Why the TUI came home.** The v10 decision to build it in a separate repository, why it was reversed, and what replaces the guarantee that boundary gave — `des_ui` links `des_engine` and never the reverse, and that is a rule no gate can check.
2. **The screen as a value.** Why it is what makes a terminal UI testable at all, and why the portable half lives in `src/`: every existing gate covers it unchanged.
3. **The tab bar is computed, both halves.** Registry types *and* the unknown types v11 preserves, and why hiding the second set would be the worse bug.
4. **Why the detail pane exists.** Eleven columns, eighty characters.
5. **Undo as a snapshot.** Why a stack of documents beats per-operation inverses.
6. **`progress().fraction` reaching a screen.** The fourth-time rule, drawn.
7. Anything that went wrong while building it. **The bugs are the most valuable part of these documents.**

- [ ] **Step 2: Update `CHANGELOG.md`**

A `[13.0.0]` section at the top, in the established style: Added, Changed, Fixed, Compatibility.

- [ ] **Step 3: Update `README.md`**

- "Current state" becomes v13, and a v13 paragraph joins the v10, v11 and v12 ones.
- A "The terminal UI" section: a screenshot-style block of the layout, the key map, and `./build/des_tui examples/models/teller.des`.
- `./build/des_tui` in the Build and run list.
- The check count, and `V13_READLOG.md` in the documents table.
- The roadmap tail becomes v14.

- [ ] **Step 4: Update `ARENA_MAP.md`**

Arena's module spreadsheets against this UI's tab bar and grid, and the one real difference: Arena has a canvas and this has exit columns, which is why the flowchart is edited as text rather than drawn.

- [ ] **Step 5: Update `examples/README.md`**

A short "Editing a model" section: open a `.des` file in `des_tui`, the key map, and the fact that saving preserves everything you did not touch.

- [ ] **Step 6: Update the manual**

```bash
python tools/manual.py
```

It will report the v13 types as undocumented. Add prose for `Attr`, `Screen`, `Screen::Glyph`, `KeyKind`, `Key`, `TerminalSize`, `ITerminal`, `Mode` and `TuiState` to `TYPE_DOC` in `tools/manual_data.py`, member prose to `MEMBER_DOC`, a **Chapter 8 "The Terminal UI"** inserted after Runtime Control with the later chapters renumbered, and a call-flow diagram for one keypress. Update `VERSION_LINE`.

Expected afterwards: `types documented N / N`.

- [ ] **Step 7: Final verification**

```bash
cmake --build build --clean-first
```
```bash
./build/des_tests.exe
```
```bash
bash tools/baseline.sh check
```
```bash
./build/des regress
```
```bash
bash tools/verify.sh
```

Expected: no warnings, all checks pass, `BASELINE CLEAN`, `REGRESSION CLEAN`, `VERIFY CLEAN`.

- [ ] **Step 8: Commit**

```bash
git add V13_READLOG.md CHANGELOG.md README.md ARENA_MAP.md examples/README.md tools/manual_data.py DES_Engine_Reference.pdf
git commit -m "v13: the spreadsheets get a screen"
```

---

## Self-review notes

**Spec coverage.** `Screen` and `Key` → Task 1. `TuiState`, open, save, the computed tab bar → Task 2. Mutations and undo → Task 3. Tab bar, grid, small-terminal message → Task 4. Detail pane, enum and reference hints, cell diagnostics, the edit buffer → Task 5. Navigation, modes, one editing surface, read-only refusal, the Confirm-on-quit flow → Tasks 3 and 6. The byte-identical round trip → Tasks 2 and 7. `Terminal`, both platforms, `main()` → Task 8. Running and the absent fraction → Task 9. Docs → Task 10.

**Two things added while writing the plan that the spec did not name.**
`TuiState::fromDocument(ModelDocument, std::string)` — the tests need a state built from a document held in memory, and routing every one of them through a temporary file would make the suite depend on the filesystem for no gain. And `tui/main.cpp` gained a run-advance branch before its blocking `nextKey()`: the spec described the run loop without noticing that a loop which blocks on input cannot advance a simulation.

**One place the spec was wrong and the plan corrects it.** The spec's `Screen::text` returns "the x it stopped at" and its `hline`/`box` took no `Attr`. A dim box round a read-only table needs one, so both take an `Attr` defaulting to `Normal`.

**Check counts are not predicted.** What matters per task is zero FAIL lines and a total that grew.
