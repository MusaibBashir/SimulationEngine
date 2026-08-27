# v10 Expression Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a model's conditions, assignment values and duration fields **text** rather than compiled C++, so a spreadsheet cell can hold `defects > 2` or `EXPO(0.8)`.

**Architecture:** A new expression layer sits *below* the engine core. It defines `IModelState` — a narrow interface for the live state expressions need — which `SimulationSystem` implements, exactly as `NodeContext` inverted the dependency for nodes in v6. A recursive-descent parser with a precedence climb produces an `IExpression` AST. Distributions are grammar functions owning the existing `IDistribution` objects, so nothing is reimplemented. Every existing C++ API survives as a thin adapter over the new representation.

**Tech Stack:** C++17, CMake, no external dependencies. The hand-rolled test harness in `tests/`.

## Global Constraints

- **C++17.** No newer features. No external dependencies — the project deliberately has none.
- **All 293 existing checks must pass, unchanged.** Not adapted, not deleted. If a test needs changing, stop and raise it.
- **The 14 gated examples must stay byte-identical to v9.** Baselines were captured before this work began and are committed in `tests/baseline/`.
- **The verification gate is `bash tools/verify.sh`** and it must print `VERIFY CLEAN`.
  It runs three checks; run it, do not hand-roll the commands.
  - GCC 14.2 `-Wall -Wextra -Wpedantic` — zero warnings.
  - Clang 19.1 `-Wall -Wextra -Wpedantic` — zero warnings.
  - MSVC `/fsanitize=address` — the full suite passes under AddressSanitizer.
- **UBSan is unavailable on this machine and must not be claimed.** MinGW ships
  no `libubsan`, MinGW Clang has no runtime for the windows-gnu target, and MSVC
  has no UBSan at all. A `-fsanitize=undefined` command here fails to LINK — it
  does not silently pass. Do not add one back.
- **The `g++` on PATH is MinGW GCC 6.3 and cannot compile this code** (no
  `<variant>`, no `<optional>`). `tools/verify.sh` finds the WinLibs GCC 14 /
  Clang 19 itself and refuses any compiler older than GCC 7.
- **Headers declare, sources define.** One-line getters stay inline. Every `.cpp` includes its own header first.
- **`unique_ptr` = ownership, raw pointer = observation.** Every polymorphic base gets a virtual destructor.
- **Every object holding run state needs `reset()`**, and `initialise()` must call it.
- **Sources are LISTED in `CMakeLists.txt`, never globbed** (except `examples/`).
- **User errors are `Diagnostic` values; programmer errors are thrown `ModelError`.** Never mix these.
- Namespace is `des` throughout. New public headers are added to `include/des.hpp`.
- **The byte-identical gate is `bash tools/baseline.sh check`** and it must print
  `BASELINE CLEAN`. Baselines are already captured from v9 — **never re-capture**;
  re-capturing after a change is how a gate silently stops being a gate.
- **`12_shared_resources` is EXCLUDED from that gate** — it produces a different
  result on every run despite a fixed seed, a v9 bug predating this work. The
  script prints the exclusion on every run. Do not "fix" it as part of v10, and
  do not add anything else to the exclusion list.
- **Build with CMake + Visual Studio**: `cmake --build build --config Debug`.
  Binaries land in `build/Debug/` and examples in `build/examples/Debug/` — not
  the Unix `build/` layout.
- **Git Bash mangles MSVC-style flags** (`/fsanitize=address` becomes a path).
  Use PowerShell for `cl`/CMake flag arguments, or the provided scripts.

## File Structure

**New headers/sources** (each added to `CMakeLists.txt` `add_library(des_engine ...)` and to `include/des.hpp`):

| File | Responsibility |
|---|---|
| `include/Value.hpp` / `src/Value.cpp` | `Value` variant, numeric coercion, formatting |
| `include/Diagnostic.hpp` | `SourceSpan`, `Severity`, `Diagnostic` — header-only |
| `include/Lexer.hpp` / `src/Lexer.cpp` | Tokeniser carrying positions |
| `include/EvalContext.hpp` / `src/EvalContext.cpp` | `IModelState`, `EvalContext`, `ValidationContext` |
| `include/Expression.hpp` / `src/Expression.cpp` | `IExpression` and its node kinds |
| `include/Functions.hpp` / `src/Functions.cpp` | Built-in registry: name → arity + builder |
| `include/Parser.hpp` / `src/Parser.cpp` | Recursive descent, precedence climb, error recovery |
| `include/VariableStore.hpp` / `src/VariableStore.cpp` | Global variables, initial values, reset, statistics |

**New test files:**

| File | Responsibility |
|---|---|
| `tests/harness.hpp` / `tests/harness.cpp` | The shared `check` / `checkClose` / `section` harness, extracted |
| `tests/expression_tests.cpp` | Everything v10 adds |

**Modified:** `include/Nodes.hpp`, `src/Nodes.cpp`, `include/Station.hpp`, `src/Station.cpp`, `include/Create.hpp`, `src/Create.cpp`, `include/Model.hpp`, `src/Model.cpp`, `include/SimulationSystem.hpp`, `src/SimulationSystem.cpp`, `include/Build.hpp`, `include/des.hpp`, `CMakeLists.txt`, `tests/tests.cpp`.

**Ordering rationale.** The AST and evaluator land before the parser, so evaluation is proven on hand-built trees and a parser bug can never be mistaken for an evaluator bug. `Decide` is wired first because it has the smallest blast radius — its `std::function` path is self-contained — and the byte-identical-trace test starts working from that point on. `Station` and `Create` come last, once the adapter pattern is proven.

---

### Task 1: Baselines and the test harness split

Nothing here changes behaviour. It creates the safety net every later task is checked against, and makes room for ~600 lines of new tests without pushing `tests/tests.cpp` past 2,200 lines.

**Files:**
- Create: `tests/harness.hpp`, `tests/harness.cpp`, `tests/expression_tests.cpp`
- Create: `tests/baseline/` (committed trace baselines)
- Modify: `tests/tests.cpp` (lines 24–43: remove the harness, include it instead)
- Modify: `CMakeLists.txt:65` (add the new test sources)

**Interfaces:**
- Consumes: nothing.
- Produces: `des_test::check(bool, const std::string&)`, `des_test::checkClose(double got, double want, double tol, const std::string&)`, `des_test::section(const char*)`, `des_test::g_checks`, `des_test::g_failures`, and `void runExpressionTests();`

- [ ] **Step 1: Capture the v9 trace baselines**

Build v9 as it stands and record what every example prints. These files are the byte-identical gate for Tasks 8–12.

```bash
# ALREADY DONE before this plan started -- tests/baseline/ holds 14 captured
# outputs and tools/baseline.sh is committed. Verify, do not recapture:
bash tools/baseline.sh check
```

Expected: 15 `.txt` files, non-empty.

> If example binaries are named differently, list `build/` and adjust the glob. Do not proceed until 15 baselines exist.

- [ ] **Step 2: Extract the harness**

Create `tests/harness.hpp`:

```cpp
// ============================================================================
// tests/harness.hpp  --  the twenty-line test harness, shared
// ============================================================================
// Extracted in v10. tests.cpp was 1650 lines and the expression layer adds
// another 600; one file holding both would be the largest file in the project
// by a factor of three. Still no external framework -- see the note at the top
// of tests.cpp for why.

#pragma once
#include <string>

namespace des_test {

extern int g_checks;
extern int g_failures;

void check(bool condition, const std::string& what);
void checkClose(double got, double want, double tol, const std::string& what);
void section(const char* name);

}  // namespace des_test
```

Create `tests/harness.cpp`:

```cpp
#include "harness.hpp"
#include <cmath>
#include <iostream>

namespace des_test {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "\n";
    }
}

void checkClose(double got, double want, double tol, const std::string& what) {
    ++g_checks;
    if (std::fabs(got - want) > tol) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "  (got " << got << ", want " << want << ")\n";
    }
}

void section(const char* name) { std::cout << "[" << name << "]\n"; }

}  // namespace des_test
```

- [ ] **Step 3: Point tests.cpp at the harness**

In `tests/tests.cpp`, delete the `int g_checks`, `int g_failures`, `check`, `checkClose` and `section` definitions (lines ~25–43, inside the anonymous namespace). Add after the existing includes:

```cpp
#include "harness.hpp"
using des_test::check;
using des_test::checkClose;
using des_test::section;
```

At the end of `main`, before the summary line, add the new suite and read the counters from the harness:

```cpp
    runExpressionTests();

    std::cout << "\n" << (des_test::g_checks - des_test::g_failures)
              << " / " << des_test::g_checks << " checks passed\n";
    if (des_test::g_failures > 0) std::cout << des_test::g_failures << " FAILURES\n";
    return des_test::g_failures == 0 ? 0 : 1;
```

Declare it near the top of `tests.cpp`:

```cpp
void runExpressionTests();   // tests/expression_tests.cpp
```

- [ ] **Step 4: Create the empty expression suite**

Create `tests/expression_tests.cpp`:

```cpp
// ============================================================================
// tests/expression_tests.cpp  --  v10: the expression layer
// ============================================================================
#include "harness.hpp"
#include "des.hpp"

using namespace des;
using des_test::check;
using des_test::checkClose;
using des_test::section;

void runExpressionTests() {
    section("Expression layer");
    check(true, "suite is wired up");
}
```

- [ ] **Step 5: Add the test sources to CMake**

In `CMakeLists.txt`, change the `des_tests` executable to include the new files:

```cmake
add_executable(des_tests tests/tests.cpp tests/harness.cpp tests/expression_tests.cpp)
```

- [ ] **Step 6: Verify the count is unchanged plus one**

```bash
cmake --build build && ./build/des_tests | tail -3
```

Expected: `294 / 294 checks passed` — the original 293 plus the one placeholder check. **If the number is anything other than 294, the extraction dropped a test. Stop and find it.**

- [ ] **Step 7: Commit**

```bash
git add tests/ CMakeLists.txt
git commit -m "test: extract the harness, add the v10 suite, capture v9 trace baselines"
```

---

### Task 2: Value and Diagnostic

The two leaf types everything else is written in terms of. No dependencies on anything in the project.

**Files:**
- Create: `include/Diagnostic.hpp`, `include/Value.hpp`, `src/Value.cpp`
- Modify: `CMakeLists.txt` (add `src/Value.cpp`), `include/des.hpp`
- Test: `tests/expression_tests.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `struct des::SourceSpan { std::size_t offset, length; }`
  - `enum class des::Severity { Error, Warning }`
  - `struct des::Diagnostic { Severity severity; SourceSpan span; std::string message; }`
  - `using des::Value = std::variant<double, std::string>;`
  - `bool des::isNumber(const Value&)`, `bool des::isText(const Value&)`
  - `double des::asNumber(const Value&)` — throws `ExpressionError` on a string
  - `const std::string& des::asText(const Value&)` — throws `ExpressionError` on a number
  - `bool des::truthy(const Value&)` — non-zero is true; a string is an error
  - `std::string des::formatValue(const Value&)`
  - `class des::ExpressionError : public ModelError`

- [ ] **Step 1: Write the failing test**

Append to `runExpressionTests()` in `tests/expression_tests.cpp`:

```cpp
    section("Value");
    {
        Value n = 3.5;
        Value t = std::string("Ball");
        check(isNumber(n) && !isText(n), "a double Value is a number");
        check(isText(t) && !isNumber(t), "a string Value is text");
        checkClose(asNumber(n), 3.5, 1e-12, "asNumber round-trips");
        check(asText(t) == "Ball", "asText round-trips");

        check(truthy(Value(1.0)), "non-zero is true");
        check(!truthy(Value(0.0)), "zero is false");
        check(truthy(Value(-2.0)), "negative non-zero is true");

        // A string in a numeric position is an ERROR, never a coerced 0.
        bool threw = false;
        try { asNumber(t); } catch (const ExpressionError&) { threw = true; }
        check(threw, "asNumber on text throws rather than coercing");

        threw = false;
        try { truthy(t); } catch (const ExpressionError&) { threw = true; }
        check(threw, "truthy on text throws rather than coercing");

        check(formatValue(Value(2.0)) == "2", "whole numbers format without a decimal point");
        check(formatValue(t) == "Ball", "text formats as itself");
    }
```

- [ ] **Step 2: Run it and confirm it fails**

```bash
cmake --build build 2>&1 | head -20
```

Expected: compile error — `'Value' was not declared in this scope`.

- [ ] **Step 3: Write `include/Diagnostic.hpp`**

```cpp
// ============================================================================
// Diagnostic.hpp  --  v10: a user's mistake is DATA, not an exception
// ============================================================================
// ModelError throws because a PROGRAMMER wired the model wrong: there is one
// mistake, it is a bug, and stopping is the right response.
//
// A malformed expression is different. It is a person typing into a cell, and
// v11's spreadsheet must show EVERY bad cell at once -- not the first one and
// then nothing. An exception can only carry one failure and unwinds past the
// rest, so the parser returns a LIST instead and keeps going.

#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace des {

// A range within the text that was parsed. Offsets are relative to the start of
// the expression string -- which in v11 is the contents of one spreadsheet cell.
// The parser therefore never needs to know what a row or a column is; v11 adds
// that identity around the span.
struct SourceSpan {
    std::size_t offset{0};
    std::size_t length{0};
};

enum class Severity { Error, Warning };

struct Diagnostic {
    Severity    severity{Severity::Error};
    SourceSpan  span;
    std::string message;
};

// True if any diagnostic in the list is an Error (Warnings alone are fine).
bool hasErrors(const std::vector<Diagnostic>& diagnostics);

// "col 7: expected ')'" -- one line per diagnostic, for tests and the CLI.
std::string formatDiagnostics(const std::vector<Diagnostic>& diagnostics);

}  // namespace des
```

Put the two function bodies in `src/Value.cpp` (they are three lines each and do not justify a file of their own).

- [ ] **Step 4: Write `include/Value.hpp`**

```cpp
// ============================================================================
// Value.hpp  --  v10: what an expression evaluates to
// ============================================================================
// A variant of double and string, and the string half exists for exactly ONE
// reason: Entity.Type == "Ball". Nothing else in the language needs text.
//
// BOOLEANS ARE DOUBLES, 0 and 1, the way Arena treats them. That is not
// laziness -- it is what makes a condition field and a value field the same
// kind of expression, which is the whole point of having one grammar. A
// separate bool type would fork every operator.
//
// There is NO coercion between the two halves. A string in a numeric position
// throws rather than reading as zero, because "a default return value is a
// place for a bug to hide" and a silent 0 in a duration field is a service time
// of zero that nobody notices.

#pragma once
#include <string>
#include <variant>
#include "ModelError.hpp"

namespace des {

using Value = std::variant<double, std::string>;

// Thrown when an expression cannot do its job AT RUN TIME -- a string in
// arithmetic, division by zero, an attribute read with no entity. Derives from
// ModelError because by then there is no cell to point at, and continuing would
// produce confident nonsense. Compare Diagnostic, which is for user errors
// caught BEFORE a run.
class ExpressionError : public ModelError {
public:
    explicit ExpressionError(const std::string& what) : ModelError(what) {}
};

inline bool isNumber(const Value& v) { return std::holds_alternative<double>(v); }
inline bool isText(const Value& v)   { return std::holds_alternative<std::string>(v); }

double             asNumber(const Value& v);
const std::string& asText(const Value& v);
bool               truthy(const Value& v);
std::string        formatValue(const Value& v);

}  // namespace des
```

- [ ] **Step 5: Write `src/Value.cpp`**

```cpp
#include "Value.hpp"
#include <cmath>
#include <sstream>
#include "Diagnostic.hpp"

namespace des {

double asNumber(const Value& v) {
    if (const double* d = std::get_if<double>(&v)) return *d;
    throw ExpressionError("expected a number, got the text '" + std::get<std::string>(v) + "'");
}

const std::string& asText(const Value& v) {
    if (const std::string* s = std::get_if<std::string>(&v)) return *s;
    throw ExpressionError("expected text, got a number");
}

bool truthy(const Value& v) { return asNumber(v) != 0.0; }

std::string formatValue(const Value& v) {
    if (const std::string* s = std::get_if<std::string>(&v)) return *s;
    const double d = std::get<double>(v);
    std::ostringstream out;
    // A whole number prints as "2", not "2.000000" -- these strings end up in
    // trace lines and error messages that get read by a person.
    if (d == std::floor(d) && std::fabs(d) < 1e15) out << static_cast<long long>(d);
    else                                           out << d;
    return out.str();
}

bool hasErrors(const std::vector<Diagnostic>& diagnostics) {
    for (const Diagnostic& d : diagnostics)
        if (d.severity == Severity::Error) return true;
    return false;
}

std::string formatDiagnostics(const std::vector<Diagnostic>& diagnostics) {
    std::ostringstream out;
    for (const Diagnostic& d : diagnostics) {
        out << "col " << (d.span.offset + 1) << ": " << d.message << "\n";
    }
    return out.str();
}

}  // namespace des
```

- [ ] **Step 6: Wire into the build**

Add `src/Value.cpp` to `add_library(des_engine ...)` in `CMakeLists.txt`, immediately after `src/Entity.cpp`. Add to `include/des.hpp`, after `#include "ModelError.hpp"`:

```cpp
#include "Diagnostic.hpp"
#include "Value.hpp"
```

- [ ] **Step 7: Run the tests**

```bash
cmake --build build && ./build/des_tests | tail -3
```

Expected: `304 / 304 checks passed` (294 + 10 new).

- [ ] **Step 8: Commit**

```bash
git add include/Value.hpp include/Diagnostic.hpp src/Value.cpp include/des.hpp CMakeLists.txt tests/expression_tests.cpp
git commit -m "feat: Value and Diagnostic -- the leaf types of the expression layer"
```

---

### Task 3: The lexer

**Files:**
- Create: `include/Lexer.hpp`, `src/Lexer.cpp`
- Modify: `CMakeLists.txt`, `include/des.hpp`
- Test: `tests/expression_tests.cpp`

**Interfaces:**
- Consumes: `SourceSpan`, `Diagnostic` (Task 2).
- Produces:
  - `enum class des::TokenKind { Number, Text, Identifier, Plus, Minus, Star, Slash, Percent, Caret, Bang, Less, LessEqual, Greater, GreaterEqual, EqualEqual, BangEqual, AndAnd, OrOr, LParen, RParen, Comma, End, Invalid }`
  - `struct des::Token { TokenKind kind; SourceSpan span; double number; std::string text; }`
  - `struct des::LexResult { std::vector<Token> tokens; std::vector<Diagnostic> diagnostics; }`
  - `LexResult des::tokenise(const std::string& source)`

- [ ] **Step 1: Write the failing test**

Append to `runExpressionTests()`:

```cpp
    section("Lexer");
    {
        LexResult r = tokenise("1 + 2.5 * x");
        check(!hasErrors(r.diagnostics), "a clean expression lexes without diagnostics");
        check(r.tokens.size() == 6, "five tokens plus End");
        check(r.tokens[0].kind == TokenKind::Number, "first token is a number");
        checkClose(r.tokens[0].number, 1.0, 1e-12, "first number value");
        check(r.tokens[1].kind == TokenKind::Plus, "operator token");
        checkClose(r.tokens[2].number, 2.5, 1e-12, "decimal number");
        check(r.tokens[4].kind == TokenKind::Identifier && r.tokens[4].text == "x", "identifier");
        check(r.tokens[5].kind == TokenKind::End, "End terminates the stream");

        // A DOT IS AN IDENTIFIER CHARACTER. Entity.Type is ONE token.
        LexResult dotted = tokenise("Entity.Type");
        check(dotted.tokens.size() == 2, "a dotted name is a single token");
        check(dotted.tokens[0].text == "Entity.Type", "dot is part of the identifier");

        // Two-character operators must not lex as two one-character ones.
        LexResult ops = tokenise("a >= b && c != d");
        check(ops.tokens[1].kind == TokenKind::GreaterEqual, ">= is one token");
        check(ops.tokens[3].kind == TokenKind::AndAnd, "&& is one token");
        check(ops.tokens[5].kind == TokenKind::BangEqual, "!= is one token");

        LexResult str = tokenise("Entity.Type == \"Ball\"");
        check(str.tokens[2].kind == TokenKind::Text && str.tokens[2].text == "Ball",
              "a quoted string lexes as Text without its quotes");

        // Spans are what v11 points a spreadsheet cursor at.
        LexResult sp = tokenise("ab + c");
        check(sp.tokens[0].span.offset == 0 && sp.tokens[0].span.length == 2, "span of 'ab'");
        check(sp.tokens[2].span.offset == 5 && sp.tokens[2].span.length == 1, "span of 'c'");

        LexResult bad = tokenise("1 @ 2");
        check(hasErrors(bad.diagnostics), "an unknown character is a diagnostic");
        check(bad.diagnostics[0].span.offset == 2, "the diagnostic points at the '@'");

        LexResult unterminated = tokenise("\"oops");
        check(hasErrors(unterminated.diagnostics), "an unterminated string is a diagnostic");
    }
```

- [ ] **Step 2: Run it and confirm it fails**

```bash
cmake --build build 2>&1 | head -20
```

Expected: `'tokenise' was not declared in this scope`.

- [ ] **Step 3: Write `include/Lexer.hpp`**

```cpp
// ============================================================================
// Lexer.hpp  --  v10: text into tokens, each carrying where it came from
// ============================================================================
// Every token carries a SourceSpan, and that is the point of writing a lexer at
// all rather than pattern-matching strings. v11 puts a cursor in a spreadsheet
// cell on the exact character that is wrong; it can only do that if position
// survives from here all the way to the diagnostic.
//
// ONE DELIBERATE ODDITY: a dot is an IDENTIFIER character, not an operator. So
// `Entity.Type` is a single token and there is no member-access node in the
// grammar. That buys Arena's dotted names for free. The cost is that a stray
// dot lands inside an identifier and is reported as an unknown NAME rather than
// as a syntax error -- which is the better of the two messages, because dotted
// names are far more common than mistyped operators.

#pragma once
#include <string>
#include <vector>
#include "Diagnostic.hpp"

namespace des {

enum class TokenKind {
    Number, Text, Identifier,
    Plus, Minus, Star, Slash, Percent, Caret, Bang,
    Less, LessEqual, Greater, GreaterEqual, EqualEqual, BangEqual,
    AndAnd, OrOr,
    LParen, RParen, Comma,
    End,
    Invalid
};

struct Token {
    TokenKind   kind{TokenKind::Invalid};
    SourceSpan  span;
    double      number{0.0};   // Number only
    std::string text;          // Identifier and Text only
};

struct LexResult {
    std::vector<Token>      tokens;        // always ends with an End token
    std::vector<Diagnostic> diagnostics;
};

// Never throws. An unknown character produces a diagnostic and is SKIPPED, so
// one call reports every bad character rather than the first.
LexResult tokenise(const std::string& source);

const char* describe(TokenKind kind);   // "')'", "a number" -- for messages

}  // namespace des
```

- [ ] **Step 4: Write `src/Lexer.cpp`**

```cpp
#include "Lexer.hpp"
#include <cctype>
#include <cstdlib>

namespace des {
namespace {

bool isIdentifierStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentifierPart(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
}

}  // namespace

const char* describe(TokenKind kind) {
    switch (kind) {
        case TokenKind::Number:       return "a number";
        case TokenKind::Text:         return "a quoted string";
        case TokenKind::Identifier:   return "a name";
        case TokenKind::Plus:         return "'+'";
        case TokenKind::Minus:        return "'-'";
        case TokenKind::Star:         return "'*'";
        case TokenKind::Slash:        return "'/'";
        case TokenKind::Percent:      return "'%'";
        case TokenKind::Caret:        return "'^'";
        case TokenKind::Bang:         return "'!'";
        case TokenKind::Less:         return "'<'";
        case TokenKind::LessEqual:    return "'<='";
        case TokenKind::Greater:      return "'>'";
        case TokenKind::GreaterEqual: return "'>='";
        case TokenKind::EqualEqual:   return "'=='";
        case TokenKind::BangEqual:    return "'!='";
        case TokenKind::AndAnd:       return "'&&'";
        case TokenKind::OrOr:         return "'||'";
        case TokenKind::LParen:       return "'('";
        case TokenKind::RParen:       return "')'";
        case TokenKind::Comma:        return "','";
        case TokenKind::End:          return "the end of the expression";
        case TokenKind::Invalid:      return "an unrecognised character";
    }
    return "something unexpected";
}

LexResult tokenise(const std::string& source) {
    LexResult result;
    std::size_t i = 0;
    const std::size_t n = source.size();

    auto push = [&](TokenKind kind, std::size_t start, std::size_t length) {
        Token t;
        t.kind = kind;
        t.span = SourceSpan{start, length};
        result.tokens.push_back(std::move(t));
    };

    while (i < n) {
        const char c = source[i];

        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }

        // --- numbers ---
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(source[i + 1])))) {
            const std::size_t start = i;
            const char* begin = source.c_str() + i;
            char* end = nullptr;
            const double value = std::strtod(begin, &end);
            i += static_cast<std::size_t>(end - begin);
            Token t;
            t.kind = TokenKind::Number;
            t.span = SourceSpan{start, i - start};
            t.number = value;
            result.tokens.push_back(std::move(t));
            continue;
        }

        // --- identifiers (dots included -- see the header) ---
        if (isIdentifierStart(c)) {
            const std::size_t start = i;
            while (i < n && isIdentifierPart(source[i])) ++i;
            Token t;
            t.kind = TokenKind::Identifier;
            t.span = SourceSpan{start, i - start};
            t.text = source.substr(start, i - start);
            result.tokens.push_back(std::move(t));
            continue;
        }

        // --- quoted text ---
        if (c == '"') {
            const std::size_t start = i;
            ++i;
            std::string text;
            bool closed = false;
            while (i < n) {
                if (source[i] == '"') { closed = true; ++i; break; }
                text.push_back(source[i]);
                ++i;
            }
            if (!closed) {
                result.diagnostics.push_back(
                    Diagnostic{Severity::Error, SourceSpan{start, i - start},
                               "unterminated string: no closing '\"'"});
            }
            Token t;
            t.kind = TokenKind::Text;
            t.span = SourceSpan{start, i - start};
            t.text = std::move(text);
            result.tokens.push_back(std::move(t));
            continue;
        }

        // --- two-character operators, checked BEFORE the one-character ones ---
        if (i + 1 < n) {
            const char d = source[i + 1];
            TokenKind two = TokenKind::Invalid;
            if      (c == '<' && d == '=') two = TokenKind::LessEqual;
            else if (c == '>' && d == '=') two = TokenKind::GreaterEqual;
            else if (c == '=' && d == '=') two = TokenKind::EqualEqual;
            else if (c == '!' && d == '=') two = TokenKind::BangEqual;
            else if (c == '&' && d == '&') two = TokenKind::AndAnd;
            else if (c == '|' && d == '|') two = TokenKind::OrOr;
            if (two != TokenKind::Invalid) { push(two, i, 2); i += 2; continue; }
        }

        // --- one-character operators ---
        TokenKind one = TokenKind::Invalid;
        switch (c) {
            case '+': one = TokenKind::Plus;    break;
            case '-': one = TokenKind::Minus;   break;
            case '*': one = TokenKind::Star;    break;
            case '/': one = TokenKind::Slash;   break;
            case '%': one = TokenKind::Percent; break;
            case '^': one = TokenKind::Caret;   break;
            case '!': one = TokenKind::Bang;    break;
            case '<': one = TokenKind::Less;    break;
            case '>': one = TokenKind::Greater; break;
            case '(': one = TokenKind::LParen;  break;
            case ')': one = TokenKind::RParen;  break;
            case ',': one = TokenKind::Comma;   break;
            default: break;
        }
        if (one != TokenKind::Invalid) { push(one, i, 1); ++i; continue; }

        // A single '=' is worth its own message: it is the commonest mistake a
        // person makes writing a condition, and "unrecognised character" would
        // not tell them what to do about it.
        if (c == '=') {
            result.diagnostics.push_back(
                Diagnostic{Severity::Error, SourceSpan{i, 1},
                           "use '==' to compare; a single '=' is not an operator here"});
            ++i;
            continue;
        }

        // Unknown: report it and SKIP, so one call reports every bad character.
        result.diagnostics.push_back(
            Diagnostic{Severity::Error, SourceSpan{i, 1},
                       std::string("unexpected character '") + c + "'"});
        ++i;
    }

    push(TokenKind::End, n, 0);
    return result;
}

}  // namespace des
```

- [ ] **Step 5: Wire into the build**

Add `src/Lexer.cpp` to `CMakeLists.txt` after `src/Value.cpp`, and `#include "Lexer.hpp"` to `include/des.hpp` after `Value.hpp`.

- [ ] **Step 6: Run the tests**

```bash
cmake --build build && ./build/des_tests | tail -3
```

Expected: `320 / 320 checks passed` (304 + 16 new). No FAIL lines.

- [ ] **Step 7: Commit**

```bash
git add include/Lexer.hpp src/Lexer.cpp include/des.hpp CMakeLists.txt tests/expression_tests.cpp
git commit -m "feat: the lexer -- tokens that remember where they came from"
```

---

### Task 4: The AST and the evaluation context

The AST and evaluator land **before** the parser, deliberately. Evaluation is proven on hand-built trees, so when the parser arrives a wrong answer can only be a parser bug — the two are never suspects at the same time.

**Files:**
- Create: `include/EvalContext.hpp`, `src/EvalContext.cpp`, `include/Expression.hpp`, `src/Expression.cpp`
- Create (stub, replaced in Task 6): `include/VariableStore.hpp`, `src/VariableStore.cpp`
- Modify: `CMakeLists.txt`, `include/des.hpp`
- Test: `tests/expression_tests.cpp`

**Interfaces:**
- Consumes: `Value`, `Diagnostic` (Task 2).
- Produces:
  - `class des::IModelState` — pure virtual `queueLength(const std::string&)`, `resourceBusy(const std::string&)`, `resourceCapacity(const std::string&)`, `numberInSystem()`, `now()`
  - `class des::EvalContext` — ctor `(const Entity*, VariableStore*, const IModelState*, RandomStream*)`
  - `enum class des::FieldContext { HasEntity, NoEntity }`
  - `struct des::ValidationContext { FieldContext field; const VariableStore* variables; std::vector<std::string> attributeNames; }`
  - `class des::IExpression` — `evaluate(EvalContext&) const`, `validate(const ValidationContext&, std::vector<Diagnostic>&) const`, `meanIfKnown() const`, `describe() const`, `span() const`, `clone() const`, `useStream(RandomStream*)`
  - `using des::ExpressionPtr = std::unique_ptr<IExpression>;`
  - Node kinds: `LiteralExpression`, `NameExpression`, `UnaryExpression`, `BinaryExpression`, `LambdaExpression`, `DistributionExpression`
  - `enum class des::UnaryOp { Negate, Not }`
  - `enum class des::BinaryOp { Add, Subtract, Multiply, Divide, Modulo, Power, Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual, And, Or }`

- [ ] **Step 1: Write the failing test**

Append to `runExpressionTests()`:

```cpp
    section("AST and evaluation");
    {
        // Hand-built trees only -- there is no parser yet, on purpose.
        auto lit = [](double v) {
            return std::make_unique<LiteralExpression>(Value(v), SourceSpan{0, 0});
        };
        Entity e(1, 0.0);
        e.setAttribute("defects", 3.0);
        EvalContext ctx(&e, nullptr, nullptr, nullptr);

        BinaryExpression add(BinaryOp::Add, lit(2.0), lit(3.0), SourceSpan{0, 0});
        checkClose(asNumber(add.evaluate(ctx)), 5.0, 1e-12, "2 + 3");

        BinaryExpression gt(BinaryOp::Greater, lit(3.0), lit(2.0), SourceSpan{0, 0});
        checkClose(asNumber(gt.evaluate(ctx)), 1.0, 1e-12, "a true comparison is 1.0");

        UnaryExpression neg(UnaryOp::Negate, lit(4.0), SourceSpan{0, 0});
        checkClose(asNumber(neg.evaluate(ctx)), -4.0, 1e-12, "unary minus");

        NameExpression name("defects", SourceSpan{0, 7});
        checkClose(asNumber(name.evaluate(ctx)), 3.0, 1e-12, "a name reads an entity attribute");

        // meanIfKnown: a literal knows its own mean, arithmetic does not.
        check(lit(5.0)->meanIfKnown().has_value(), "a literal has a known mean");
        checkClose(*lit(5.0)->meanIfKnown(), 5.0, 1e-12, "a literal's mean is itself");
        BinaryExpression sum(BinaryOp::Add, lit(1.0), lit(2.0), SourceSpan{0, 0});
        check(!sum.meanIfKnown().has_value(), "arithmetic does not claim a known mean");

        // Division by zero is a RUN-TIME error, not an inf that spreads silently.
        BinaryExpression div(BinaryOp::Divide, lit(1.0), lit(0.0), SourceSpan{0, 0});
        bool threw = false;
        try { div.evaluate(ctx); } catch (const ExpressionError&) { threw = true; }
        check(threw, "division by zero throws rather than producing inf");

        // Short-circuit: the right operand must NOT be evaluated when the left
        // decides the answer. Without this, `Count > 0 && Total / Count > 5`
        // cannot be written at all.
        {
            BinaryExpression safe(BinaryOp::And,
                std::make_unique<BinaryExpression>(BinaryOp::Greater, lit(0.0), lit(1.0),
                                                   SourceSpan{0, 0}),
                std::make_unique<BinaryExpression>(BinaryOp::Divide, lit(1.0), lit(0.0),
                                                   SourceSpan{0, 0}),
                SourceSpan{0, 0});
            bool blewUp = false;
            try { checkClose(asNumber(safe.evaluate(ctx)), 0.0, 1e-12, "&& short-circuits to 0"); }
            catch (const ExpressionError&) { blewUp = true; }
            check(!blewUp, "&& does not evaluate its right operand when the left is false");
        }

        // An attribute reference in a field with no entity is caught BEFORE the run.
        ValidationContext vc;
        vc.field = FieldContext::NoEntity;
        vc.attributeNames = {"defects"};
        std::vector<Diagnostic> diags;
        name.validate(vc, diags);
        check(hasErrors(diags), "attribute reference with no entity is a validation error");
        check(diags[0].span.offset == 0 && diags[0].span.length == 7,
              "the diagnostic spans the name");

        // The same name in a field that HAS an entity is fine.
        std::vector<Diagnostic> ok;
        vc.field = FieldContext::HasEntity;
        name.validate(vc, ok);
        check(!hasErrors(ok), "attribute reference with an entity validates");

        // An unknown name is an error in either context.
        NameExpression unknown("nosuch", SourceSpan{0, 6});
        std::vector<Diagnostic> u;
        unknown.validate(vc, u);
        check(hasErrors(u), "an unresolvable name is a validation error");

        // clone() is deep -- v11 copies expressions when a cell is duplicated.
        auto copy = add.clone();
        checkClose(asNumber(copy->evaluate(ctx)), 5.0, 1e-12, "clone evaluates identically");
    }
```

- [ ] **Step 2: Run it and confirm it fails**

```bash
cmake --build build 2>&1 | head -20
```

Expected: `'LiteralExpression' was not declared in this scope`.

- [ ] **Step 3: Write the VariableStore stub**

`VariableStore` is not designed until Task 6, but `EvalContext` needs to name it. Create the minimum, and say so in the commit so it is never mistaken for finished work.

`include/VariableStore.hpp`:

```cpp
// ============================================================================
// VariableStore.hpp  --  v10 STUB. Task 6 replaces this file wholesale.
// ============================================================================
#pragma once
#include <string>

namespace des {

class VariableStore {
public:
    bool   has(const std::string& name) const;
    double get(const std::string& name) const;
};

}  // namespace des
```

`src/VariableStore.cpp`:

```cpp
#include "VariableStore.hpp"

namespace des {

bool   VariableStore::has(const std::string&) const { return false; }
double VariableStore::get(const std::string&) const { return 0.0; }

}  // namespace des
```

- [ ] **Step 4: Write `include/EvalContext.hpp`**

```cpp
// ============================================================================
// EvalContext.hpp  --  v10: the only view of the engine an expression gets
// ============================================================================
// THE DEPENDENCY PROBLEM, AND WHY THE ANSWER IS ALREADY IN THIS PROJECT.
//
// DecideNode must hold an expression, so the engine core depends on the
// expression layer. But `NQ(Teller)` needs live model state, which would point
// the dependency straight back up -- a cycle, and the expression layer would
// have to include Model.hpp.
//
// v6 solved exactly this shape for nodes. NodeContext is a deliberately narrow
// facade: a node may do six things and cannot touch the FEL, the statistics or
// the clock. The same move works here. The expression layer DEFINES IModelState
// -- the four questions an expression may ask about the running system -- and
// SimulationSystem IMPLEMENTS it. The expression layer never includes Model.hpp.
//
// That is the general fix whenever "this abstraction needs my internals" comes
// up: publish a role-specific interface, not the whole class. Second time in
// this project, same reasoning, same result.

#pragma once
#include <string>
#include <vector>
#include "Common.hpp"

namespace des {

class Entity;
class RandomStream;
class VariableStore;

// Implemented by SimulationSystem. Four questions and a clock -- that is the
// whole surface an expression is allowed.
class IModelState {
public:
    virtual ~IModelState() = default;
    virtual double  queueLength(const std::string& blockName) const = 0;
    virtual double  resourceBusy(const std::string& name) const = 0;
    virtual double  resourceCapacity(const std::string& name) const = 0;
    virtual double  numberInSystem() const = 0;
    virtual SimTime now() const = 0;
};

// What an expression may read WHILE THE RUN IS HAPPENING.
//
// Every member may be null, and that is meaningful rather than sloppy: a Create
// block's interarrival expression is evaluated with NO entity, because there
// isn't one yet. Reading an attribute through a null entity throws -- but
// validate() catches almost all of those before the run ever starts.
class EvalContext {
private:
    const Entity*      m_entity;
    VariableStore*     m_variables;
    const IModelState* m_state;
    RandomStream*      m_rng;

public:
    EvalContext(const Entity* entity, VariableStore* variables,
                const IModelState* state, RandomStream* rng)
        : m_entity(entity), m_variables(variables), m_state(state), m_rng(rng) {}

    const Entity*      entity() const    { return m_entity; }
    VariableStore*     variables() const { return m_variables; }
    const IModelState* state() const     { return m_state; }
    RandomStream*      rng() const       { return m_rng; }

    // Each throws ExpressionError naming what was missing, rather than
    // returning a plausible default.
    double             attribute(const std::string& name) const;
    const std::string& entityType() const;
    RandomStream&      requireRng() const;
    const IModelState& requireState() const;
};

// Whether the field being validated has an entity to read from at all.
enum class FieldContext { HasEntity, NoEntity };

// What a name is ALLOWED to resolve to, checked before the run.
struct ValidationContext {
    FieldContext             field{FieldContext::HasEntity};
    const VariableStore*     variables{nullptr};
    std::vector<std::string> attributeNames;

    bool isVariable(const std::string& name) const;
    bool isAttribute(const std::string& name) const;
};

}  // namespace des
```

- [ ] **Step 5: Write `src/EvalContext.cpp`**

```cpp
#include "EvalContext.hpp"
#include <algorithm>
#include "Entity.hpp"
#include "RandomStream.hpp"
#include "Value.hpp"
#include "VariableStore.hpp"

namespace des {

double EvalContext::attribute(const std::string& name) const {
    if (m_entity == nullptr)
        throw ExpressionError("'" + name + "' is an entity attribute, but this field is "
                              "evaluated with no entity");
    return m_entity->attribute(name);
}

const std::string& EvalContext::entityType() const {
    if (m_entity == nullptr)
        throw ExpressionError("Entity.Type read in a field with no entity");
    return m_entity->type();
}

RandomStream& EvalContext::requireRng() const {
    if (m_rng == nullptr)
        throw ExpressionError("this expression samples, but no random stream was supplied");
    return *m_rng;
}

const IModelState& EvalContext::requireState() const {
    if (m_state == nullptr)
        throw ExpressionError("this expression reads model state, but no model state "
                              "was supplied");
    return *m_state;
}

bool ValidationContext::isVariable(const std::string& name) const {
    return variables != nullptr && variables->has(name);
}

bool ValidationContext::isAttribute(const std::string& name) const {
    return std::find(attributeNames.begin(), attributeNames.end(), name) != attributeNames.end();
}

}  // namespace des
```

- [ ] **Step 6: Write `include/Expression.hpp`**

```cpp
// ============================================================================
// Expression.hpp  --  v10: the AST a model's text becomes
// ============================================================================
// Five things every node can do, and the last three are the ones that matter
// for what comes after v10:
//
//   evaluate()     the obvious one
//   validate()     report every problem BEFORE the run, into a list
//   meanIfKnown()  say whether a mean can be computed at all
//   clone()        because v11 copies cells, and v9's Model already clones
//   useStream()    give this sampling site its own random stream
//
// meanIfKnown IS THE INTERESTING ONE. Model::offeredLoad() calls dist->mean()
// to check stability before a run. An arbitrary expression has no knowable
// mean -- EXPO(MeanTime) depends on a variable that changes during the run. The
// honest answer is nullopt, and the report then says "could not verify" and
// names the block.
//
// That is VisitRatios::exact a second time: this project's habit is to say it
// does not know rather than to guess, because a guessed stability check is a
// check that passes when it should not.

#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "Distribution.hpp"
#include "EvalContext.hpp"
#include "Value.hpp"

namespace des {

class Entity;

enum class UnaryOp  { Negate, Not };
enum class BinaryOp {
    Add, Subtract, Multiply, Divide, Modulo, Power,
    Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual,
    And, Or
};

const char* spelling(UnaryOp op);
const char* spelling(BinaryOp op);

class IExpression {
protected:
    SourceSpan m_span;
public:
    explicit IExpression(SourceSpan span) : m_span(span) {}
    virtual ~IExpression() = default;
    IExpression(const IExpression&) = delete;
    IExpression& operator=(const IExpression&) = delete;

    SourceSpan span() const { return m_span; }

    virtual Value evaluate(EvalContext& ctx) const = 0;

    // Append every problem found; do NOT throw and do NOT stop at the first.
    virtual void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const = 0;

    // nullopt means "not computable in advance", NOT "zero".
    virtual std::optional<double> meanIfKnown() const { return std::nullopt; }

    virtual std::string describe() const = 0;
    virtual std::unique_ptr<IExpression> clone() const = 0;

    // Give every sampling site below this node its own stream. A no-op for
    // nodes that do not sample. This is what finally separates a Delay's draws
    // from a Decide's: they shared a stream because they shared a code path.
    virtual void useStream(RandomStream* s);
};

using ExpressionPtr = std::unique_ptr<IExpression>;

// --- a number or a quoted string -------------------------------------------
class LiteralExpression : public IExpression {
    Value m_value;
public:
    LiteralExpression(Value v, SourceSpan span) : IExpression(span), m_value(std::move(v)) {}
    Value evaluate(EvalContext&) const override { return m_value; }
    void validate(const ValidationContext&, std::vector<Diagnostic>&) const override {}
    std::optional<double> meanIfKnown() const override;
    std::string describe() const override { return formatValue(m_value); }
    ExpressionPtr clone() const override;
};

// --- a bare name: attribute, variable, Entity.Type, or TNOW -----------------
//
// RESOLVED LATE, NEVER BY THE PARSER. The parser emits this node holding a
// string; what the string IS gets decided by validate() and evaluate(). That is
// what stops the parser going stale when v11 adds a Variable data module -- it
// has no name tables to update.
class NameExpression : public IExpression {
    std::string m_name;
public:
    NameExpression(std::string name, SourceSpan span)
        : IExpression(span), m_name(std::move(name)) {}
    const std::string& name() const { return m_name; }
    Value evaluate(EvalContext& ctx) const override;
    void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const override;
    std::string describe() const override { return m_name; }
    ExpressionPtr clone() const override;
};

class UnaryExpression : public IExpression {
    UnaryOp       m_op;
    ExpressionPtr m_operand;
public:
    UnaryExpression(UnaryOp op, ExpressionPtr operand, SourceSpan span)
        : IExpression(span), m_op(op), m_operand(std::move(operand)) {}
    Value evaluate(EvalContext& ctx) const override;
    void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const override;
    std::optional<double> meanIfKnown() const override;
    std::string describe() const override;
    ExpressionPtr clone() const override;
    void useStream(RandomStream* s) override;
};

class BinaryExpression : public IExpression {
    BinaryOp      m_op;
    ExpressionPtr m_left;
    ExpressionPtr m_right;
public:
    BinaryExpression(BinaryOp op, ExpressionPtr left, ExpressionPtr right, SourceSpan span)
        : IExpression(span), m_op(op), m_left(std::move(left)), m_right(std::move(right)) {}
    Value evaluate(EvalContext& ctx) const override;
    void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const override;
    std::string describe() const override;
    ExpressionPtr clone() const override;
    void useStream(RandomStream* s) override;
};

// --- the two ADAPTERS that keep every existing API alive -------------------
//
// These are why v10 breaks nothing. The old C++ API does not become a second
// code path kept alive in parallel -- it becomes a CONSTRUCTOR for a node in
// the one and only representation.

// Wraps a std::function<bool(const Entity&)>, so DecideNode's existing
// Condition overload still compiles and still behaves identically.
class LambdaExpression : public IExpression {
public:
    using Predicate = std::function<bool(const Entity&)>;
private:
    Predicate m_predicate;
public:
    explicit LambdaExpression(Predicate p)
        : IExpression(SourceSpan{0, 0}), m_predicate(std::move(p)) {}
    Value evaluate(EvalContext& ctx) const override;
    void validate(const ValidationContext&, std::vector<Diagnostic>&) const override {}
    std::string describe() const override { return "<C++ predicate>"; }
    ExpressionPtr clone() const override;
};

// Wraps an IDistribution, so station(..., exponential(0.8)) still compiles.
// Also what a parsed EXPO(0.8) with CONSTANT arguments becomes.
class DistributionExpression : public IExpression {
    std::unique_ptr<IDistribution> m_distribution;
public:
    explicit DistributionExpression(std::unique_ptr<IDistribution> d,
                                    SourceSpan span = SourceSpan{0, 0})
        : IExpression(span), m_distribution(std::move(d)) {}
    Value evaluate(EvalContext& ctx) const override;
    void validate(const ValidationContext&, std::vector<Diagnostic>&) const override {}
    std::optional<double> meanIfKnown() const override;
    std::string describe() const override { return m_distribution->describe(); }
    ExpressionPtr clone() const override;
    void useStream(RandomStream* s) override;
    void reset() { m_distribution->reset(); }
};

}  // namespace des
```

- [ ] **Step 7: Write `src/Expression.cpp`**

```cpp
#include "Expression.hpp"
#include <cmath>
#include "Entity.hpp"
#include "RandomStream.hpp"
#include "VariableStore.hpp"

namespace des {

void IExpression::useStream(RandomStream*) {}

const char* spelling(UnaryOp op) {
    switch (op) { case UnaryOp::Negate: return "-"; case UnaryOp::Not: return "!"; }
    return "?";
}

const char* spelling(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";       case BinaryOp::Subtract: return "-";
        case BinaryOp::Multiply: return "*";  case BinaryOp::Divide: return "/";
        case BinaryOp::Modulo: return "%";    case BinaryOp::Power: return "^";
        case BinaryOp::Less: return "<";      case BinaryOp::LessEqual: return "<=";
        case BinaryOp::Greater: return ">";   case BinaryOp::GreaterEqual: return ">=";
        case BinaryOp::Equal: return "==";    case BinaryOp::NotEqual: return "!=";
        case BinaryOp::And: return "&&";      case BinaryOp::Or: return "||";
    }
    return "?";
}

// --- LiteralExpression ---
std::optional<double> LiteralExpression::meanIfKnown() const {
    if (isNumber(m_value)) return asNumber(m_value);
    return std::nullopt;
}
ExpressionPtr LiteralExpression::clone() const {
    return std::make_unique<LiteralExpression>(m_value, m_span);
}

// --- NameExpression ---
// Resolution order is fixed and total: the special names, then variables, then
// attributes. A variable colliding with an attribute is rejected at DECLARATION
// (see VariableStore), so this order can never silently pick the wrong one of
// two live candidates.
Value NameExpression::evaluate(EvalContext& ctx) const {
    if (m_name == "TNOW")        return static_cast<double>(ctx.requireState().now());
    if (m_name == "Entity.Type") return ctx.entityType();
    if (ctx.variables() != nullptr && ctx.variables()->has(m_name))
        return ctx.variables()->get(m_name);
    return ctx.attribute(m_name);
}

void NameExpression::validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const {
    if (m_name == "TNOW") return;

    if (m_name == "Entity.Type") {
        if (vc.field == FieldContext::NoEntity)
            out.push_back(Diagnostic{Severity::Error, m_span,
                "'Entity.Type' needs an entity, but this field is evaluated without one"});
        return;
    }

    if (vc.isVariable(m_name)) return;

    if (vc.isAttribute(m_name)) {
        if (vc.field == FieldContext::NoEntity)
            out.push_back(Diagnostic{Severity::Error, m_span,
                "'" + m_name + "' is an entity attribute, but this field is evaluated "
                "without an entity"});
        return;
    }

    out.push_back(Diagnostic{Severity::Error, m_span,
        "unknown name '" + m_name + "' -- not a declared variable or attribute"});
}

ExpressionPtr NameExpression::clone() const {
    return std::make_unique<NameExpression>(m_name, m_span);
}

// --- UnaryExpression ---
Value UnaryExpression::evaluate(EvalContext& ctx) const {
    const double v = asNumber(m_operand->evaluate(ctx));
    switch (m_op) {
        case UnaryOp::Negate: return -v;
        case UnaryOp::Not:    return (v == 0.0) ? 1.0 : 0.0;
    }
    throw ExpressionError("unreachable unary operator");
}
void UnaryExpression::validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const {
    m_operand->validate(vc, out);
}
std::optional<double> UnaryExpression::meanIfKnown() const {
    if (m_op != UnaryOp::Negate) return std::nullopt;
    if (auto inner = m_operand->meanIfKnown()) return -*inner;
    return std::nullopt;
}
std::string UnaryExpression::describe() const {
    return std::string(spelling(m_op)) + m_operand->describe();
}
ExpressionPtr UnaryExpression::clone() const {
    return std::make_unique<UnaryExpression>(m_op, m_operand->clone(), m_span);
}
void UnaryExpression::useStream(RandomStream* s) { m_operand->useStream(s); }

// --- BinaryExpression ---
Value BinaryExpression::evaluate(EvalContext& ctx) const {
    // && and || SHORT-CIRCUIT. That is not an optimisation: it is what lets
    // `Count > 0 && Total / Count > 5` be written at all.
    if (m_op == BinaryOp::And) {
        if (!truthy(m_left->evaluate(ctx))) return 0.0;
        return truthy(m_right->evaluate(ctx)) ? 1.0 : 0.0;
    }
    if (m_op == BinaryOp::Or) {
        if (truthy(m_left->evaluate(ctx))) return 1.0;
        return truthy(m_right->evaluate(ctx)) ? 1.0 : 0.0;
    }

    const Value l = m_left->evaluate(ctx);
    const Value r = m_right->evaluate(ctx);

    // Text compares only for equality -- the one thing strings are for.
    if (isText(l) || isText(r)) {
        if (m_op == BinaryOp::Equal || m_op == BinaryOp::NotEqual) {
            if (isText(l) != isText(r))
                throw ExpressionError("cannot compare text with a number");
            const bool same = (asText(l) == asText(r));
            return ((m_op == BinaryOp::Equal) == same) ? 1.0 : 0.0;
        }
        throw ExpressionError(std::string("'") + spelling(m_op) +
                              "' cannot be applied to text");
    }

    const double a = asNumber(l);
    const double b = asNumber(r);
    switch (m_op) {
        case BinaryOp::Add:      return a + b;
        case BinaryOp::Subtract: return a - b;
        case BinaryOp::Multiply: return a * b;
        case BinaryOp::Divide:
            if (b == 0.0) throw ExpressionError("division by zero");
            return a / b;
        case BinaryOp::Modulo:
            if (b == 0.0) throw ExpressionError("modulo by zero");
            return std::fmod(a, b);
        case BinaryOp::Power:        return std::pow(a, b);
        case BinaryOp::Less:         return a <  b ? 1.0 : 0.0;
        case BinaryOp::LessEqual:    return a <= b ? 1.0 : 0.0;
        case BinaryOp::Greater:      return a >  b ? 1.0 : 0.0;
        case BinaryOp::GreaterEqual: return a >= b ? 1.0 : 0.0;
        case BinaryOp::Equal:        return a == b ? 1.0 : 0.0;
        case BinaryOp::NotEqual:     return a != b ? 1.0 : 0.0;
        case BinaryOp::And: case BinaryOp::Or: break;   // handled above
    }
    throw ExpressionError("unreachable binary operator");
}

void BinaryExpression::validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const {
    m_left->validate(vc, out);
    m_right->validate(vc, out);
}
std::string BinaryExpression::describe() const {
    return "(" + m_left->describe() + " " + spelling(m_op) + " " + m_right->describe() + ")";
}
ExpressionPtr BinaryExpression::clone() const {
    return std::make_unique<BinaryExpression>(m_op, m_left->clone(), m_right->clone(), m_span);
}
void BinaryExpression::useStream(RandomStream* s) {
    m_left->useStream(s);
    m_right->useStream(s);
}

// --- LambdaExpression ---
Value LambdaExpression::evaluate(EvalContext& ctx) const {
    if (ctx.entity() == nullptr)
        throw ExpressionError("a C++ predicate was evaluated with no entity");
    return m_predicate(*ctx.entity()) ? 1.0 : 0.0;
}
ExpressionPtr LambdaExpression::clone() const {
    return std::make_unique<LambdaExpression>(m_predicate);
}

// --- DistributionExpression ---
Value DistributionExpression::evaluate(EvalContext& ctx) const {
    return static_cast<double>(m_distribution->draw(ctx.requireRng()));
}
std::optional<double> DistributionExpression::meanIfKnown() const {
    return static_cast<double>(m_distribution->mean());
}
ExpressionPtr DistributionExpression::clone() const {
    return std::make_unique<DistributionExpression>(m_distribution->clone(), m_span);
}
void DistributionExpression::useStream(RandomStream* s) { m_distribution->useStream(s); }

}  // namespace des
```

- [ ] **Step 8: Wire into the build**

Add `src/EvalContext.cpp`, `src/Expression.cpp`, `src/VariableStore.cpp` to `add_library(des_engine ...)` in `CMakeLists.txt`, after `src/Lexer.cpp`. In `include/des.hpp`, add after `#include "Distribution.hpp"` (order matters — `Expression.hpp` includes it):

```cpp
#include "EvalContext.hpp"
#include "VariableStore.hpp"
#include "Expression.hpp"
```

- [ ] **Step 9: Run the tests**

```bash
cmake --build build && ./build/des_tests | tail -3
```

Expected: `336 / 336 checks passed` (320 + 16 new). No FAIL lines.

- [ ] **Step 10: Commit**

```bash
git add include/EvalContext.hpp src/EvalContext.cpp include/Expression.hpp src/Expression.cpp include/VariableStore.hpp src/VariableStore.cpp include/des.hpp CMakeLists.txt tests/expression_tests.cpp
git commit -m "feat: the expression AST and EvalContext

IModelState inverts the dependency the way NodeContext did in v6: the
expression layer defines what it needs, the engine implements it, and the
expression layer never includes Model.hpp.

VariableStore is a deliberate stub here; Task 6 replaces it wholesale."
```

---

### Task 5: The parser and the function registry

Tasks 5 and 6 land **together** in one commit: the parser calls `buildCall` and the registry needs the parser's argument vectors, so neither compiles alone. Write the parser, then the registry, then build once.

**Files:**
- Create: `include/Parser.hpp`, `src/Parser.cpp`, `include/Functions.hpp`, `src/Functions.cpp`
- Modify: `CMakeLists.txt`, `include/des.hpp`
- Test: `tests/expression_tests.cpp`

**Interfaces:**
- Consumes: `Token`, `LexResult`, `describe(TokenKind)` (Task 3); all AST node kinds (Task 4).
- Produces:
  - `struct des::ParseResult { ExpressionPtr expr; std::vector<Diagnostic> diagnostics; }`
  - `ParseResult des::parseExpression(const std::string& source)`
  - `ExpressionPtr des::expr(const std::string& source)` — throws `ModelError` on any diagnostic
  - `ExpressionPtr des::buildCall(const std::string& name, std::vector<ExpressionPtr> args, const std::vector<SourceSpan>& argSpans, SourceSpan nameSpan, std::vector<Diagnostic>& out)`
  - `bool des::isBuiltinFunction(const std::string& name)`

- [ ] **Step 1: Write the failing test**

Append to `runExpressionTests()`:

```cpp
    section("Parser");
    {
        Entity e(1, 0.0);
        e.setAttribute("defects", 3.0);
        EvalContext ctx(&e, nullptr, nullptr, nullptr);

        auto value = [&](const char* s) {
            ParseResult r = parseExpression(s);
            check(!hasErrors(r.diagnostics), std::string("parses cleanly: ") + s);
            return asNumber(r.expr->evaluate(ctx));
        };

        // Precedence and associativity, hand-computed.
        checkClose(value("1 + 2 * 3"),      7.0, 1e-12, "* binds tighter than +");
        checkClose(value("(1 + 2) * 3"),    9.0, 1e-12, "parentheses override");
        checkClose(value("2 ^ 3 ^ 2"),    512.0, 1e-12, "^ is RIGHT-associative: 2^(3^2)");
        checkClose(value("10 - 3 - 2"),     5.0, 1e-12, "- is LEFT-associative");
        checkClose(value("-2 ^ 2"),        -4.0, 1e-12, "unary minus binds looser than ^");
        checkClose(value("1 + 2 > 2"),      1.0, 1e-12, "arithmetic binds tighter than comparison");
        checkClose(value("1 > 2 || 3 > 2"), 1.0, 1e-12, "|| is loosest");
        checkClose(value("0 && 1 || 1"),    1.0, 1e-12, "&& binds tighter than ||");
        checkClose(value("!0"),             1.0, 1e-12, "logical not");
        checkClose(value("7 % 3"),          1.0, 1e-12, "modulo");
        checkClose(value("defects > 2"),    1.0, 1e-12, "a name in a comparison");

        // Maths functions.
        checkClose(value("MIN(3, 1, 2)"),   1.0, 1e-12, "MIN is variadic");
        checkClose(value("MAX(3, 1, 2)"),   3.0, 1e-12, "MAX is variadic");
        checkClose(value("ABS(-4)"),        4.0, 1e-12, "ABS");
        checkClose(value("ROUND(2.6)"),     3.0, 1e-12, "ROUND");
        checkClose(value("TRUNC(2.6)"),     2.0, 1e-12, "TRUNC");
        checkClose(value("SQRT(9)"),        3.0, 1e-12, "SQRT");
        checkClose(value("MOD(7, 3)"),      1.0, 1e-12, "MOD");
        checkClose(value("LN(EXP(1))"),     1.0, 1e-9,  "LN and EXP are inverses");

        // Error POSITIONS, character-exact. "an error occurred" is not enough
        // for v11: the spreadsheet puts a cursor on this offset.
        {
            ParseResult r = parseExpression("EXPO(0.8");
            check(hasErrors(r.diagnostics), "unclosed paren is an error");
            check(r.diagnostics[0].message.find("expected ')'") != std::string::npos,
                  "the message names what was expected");
            check(r.diagnostics[0].span.offset == 8, "diagnostic points at the end of input");
        }
        {
            ParseResult r = parseExpression("1 + ");
            check(hasErrors(r.diagnostics), "a dangling operator is an error");
            check(r.diagnostics[0].span.offset == 4, "diagnostic at the missing operand");
        }
        {
            ParseResult r = parseExpression("EXPOO(1)");
            check(hasErrors(r.diagnostics), "an unknown function is an error");
            check(r.diagnostics[0].message.find("EXPOO") != std::string::npos,
                  "the message names the function");
        }
        {
            ParseResult r = parseExpression("EXPO(1, 2)");
            check(hasErrors(r.diagnostics), "wrong arity is an error");
            check(r.diagnostics[0].message.find("1 argument") != std::string::npos,
                  "the message says how many arguments were expected");
        }
        {
            ParseResult r = parseExpression("a = 1");
            check(hasErrors(r.diagnostics), "a single '=' is an error");
            check(r.diagnostics[0].message.find("'=='") != std::string::npos,
                  "the message suggests '=='");
        }

        // RECOVERY: one parse reports MANY problems, because v11 must show
        // every bad cell at once rather than the first.
        {
            ParseResult r = parseExpression("1 @ 2 $ 3");
            check(r.diagnostics.size() >= 2, "one parse reports more than one problem");
        }

        // Text comparison.
        {
            ParseResult r = parseExpression("Entity.Type == \"Ball\"");
            check(!hasErrors(r.diagnostics), "a string comparison parses");
        }

        // expr() is the C++-side convenience: a bad expression there IS a
        // programmer error, so it throws rather than returning diagnostics.
        {
            bool threw = false;
            try { expr("1 +"); } catch (const ModelError&) { threw = true; }
            check(threw, "expr() throws on a malformed expression");
            check(expr("2 + 2") != nullptr, "expr() returns a usable tree");
        }
    }
```

- [ ] **Step 2: Run it and confirm it fails**

```bash
cmake --build build 2>&1 | head -20
```

Expected: `'parseExpression' was not declared in this scope`.

- [ ] **Step 3: Write `include/Functions.hpp`**

```cpp
// ============================================================================
// Functions.hpp  --  v10: the built-in call table
// ============================================================================
// THREE FAMILIES, AND THE FIRST ONE IS THE INTERESTING DESIGN DECISION.
//
//   distributions   EXPO UNIF TRIA NORM LOGN WEIB ERLA POIS DISC CONS
//   model state     NQ NR MR WIP        (TNOW is a bare NAME, not a call)
//   maths           MIN MAX ABS ROUND TRUNC SQRT LN EXP MOD
//
// THERE IS NO SEPARATE "DISTRIBUTION FIELD" IN THIS ENGINE ANY MORE. Arena's
// Delay cell holds an expression: 5 is constant, TRIA(1,2,3) samples each time,
// SetupTime*2 computes. v10 adopts that exactly, so a distribution is simply a
// function that samples. Nothing is reimplemented -- the call OWNS one of the
// twelve IDistribution objects that already exist.
//
// EXPO is the exponential DISTRIBUTION; EXP is e^x. That collision is Arena's
// and is kept deliberately: a model written against Arena's names should read
// the same here.
//
// Empirical and Deterministic have no Arena spelling and stay programmatic-only.

#pragma once
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "Expression.hpp"

namespace des {

bool isBuiltinFunction(const std::string& name);

// Build the AST node for a call. Reports unknown names and wrong arity into
// `out` and returns a placeholder rather than throwing, so the parser can keep
// going and report every problem in the expression.
ExpressionPtr buildCall(const std::string& name,
                        std::vector<ExpressionPtr> args,
                        const std::vector<SourceSpan>& argSpans,
                        SourceSpan nameSpan,
                        std::vector<Diagnostic>& out);

}  // namespace des
```

- [ ] **Step 4: Write `src/Functions.cpp`**

Three node kinds and a dispatch table.

```cpp
#include "Functions.hpp"
#include <cmath>
#include <unordered_map>
#include "Build.hpp"
#include "RandomStream.hpp"

namespace des {
namespace {

// --- maths -----------------------------------------------------------------
enum class MathFn { Min, Max, Abs, Round, Trunc, Sqrt, Ln, Exp, Mod };

class MathCall : public IExpression {
    MathFn                     m_fn;
    std::string                m_name;
    std::vector<ExpressionPtr> m_args;
public:
    MathCall(MathFn fn, std::string name, std::vector<ExpressionPtr> args, SourceSpan span)
        : IExpression(span), m_fn(fn), m_name(std::move(name)), m_args(std::move(args)) {}

    Value evaluate(EvalContext& ctx) const override {
        std::vector<double> v;
        v.reserve(m_args.size());
        for (const ExpressionPtr& a : m_args) v.push_back(asNumber(a->evaluate(ctx)));
        switch (m_fn) {
            case MathFn::Min: { double r = v[0]; for (double x : v) if (x < r) r = x; return r; }
            case MathFn::Max: { double r = v[0]; for (double x : v) if (x > r) r = x; return r; }
            case MathFn::Abs:   return std::fabs(v[0]);
            case MathFn::Round: return std::floor(v[0] + 0.5);
            case MathFn::Trunc: return std::trunc(v[0]);
            case MathFn::Sqrt:
                if (v[0] < 0.0) throw ExpressionError("SQRT of a negative number");
                return std::sqrt(v[0]);
            case MathFn::Ln:
                if (v[0] <= 0.0) throw ExpressionError("LN of a non-positive number");
                return std::log(v[0]);
            case MathFn::Exp: return std::exp(v[0]);
            case MathFn::Mod:
                if (v[1] == 0.0) throw ExpressionError("MOD by zero");
                return std::fmod(v[0], v[1]);
        }
        throw ExpressionError("unreachable maths function");
    }
    void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const override {
        for (const ExpressionPtr& a : m_args) a->validate(vc, out);
    }
    std::string describe() const override {
        std::string s = m_name + "(";
        for (std::size_t i = 0; i < m_args.size(); ++i) {
            if (i) s += ", ";
            s += m_args[i]->describe();
        }
        return s + ")";
    }
    ExpressionPtr clone() const override {
        std::vector<ExpressionPtr> copies;
        copies.reserve(m_args.size());
        for (const ExpressionPtr& a : m_args) copies.push_back(a->clone());
        return std::make_unique<MathCall>(m_fn, m_name, std::move(copies), m_span);
    }
    void useStream(RandomStream* s) override {
        for (const ExpressionPtr& a : m_args) a->useStream(s);
    }
};

// --- model state -----------------------------------------------------------
enum class StateFn { NQ, NR, MR, WIP };

// THE ONE PLACE AN ARGUMENT IS NOT EVALUATED. NQ(Teller) means "the queue at
// the block called Teller" -- so `Teller` must NOT be resolved as a variable.
// The parser hands us a NameExpression and we capture its TEXT.
class StateCall : public IExpression {
    StateFn     m_fn;
    std::string m_target;
public:
    StateCall(StateFn fn, std::string target, SourceSpan span)
        : IExpression(span), m_fn(fn), m_target(std::move(target)) {}

    Value evaluate(EvalContext& ctx) const override {
        const IModelState& s = ctx.requireState();
        switch (m_fn) {
            case StateFn::NQ:  return s.queueLength(m_target);
            case StateFn::NR:  return s.resourceBusy(m_target);
            case StateFn::MR:  return s.resourceCapacity(m_target);
            case StateFn::WIP: return s.numberInSystem();
        }
        throw ExpressionError("unreachable state function");
    }
    void validate(const ValidationContext&, std::vector<Diagnostic>&) const override {}
    std::string describe() const override {
        const char* n = m_fn == StateFn::NQ ? "NQ" : m_fn == StateFn::NR ? "NR"
                      : m_fn == StateFn::MR ? "MR" : "WIP";
        return std::string(n) + "(" + m_target + ")";
    }
    ExpressionPtr clone() const override {
        return std::make_unique<StateCall>(m_fn, m_target, m_span);
    }
};

// --- distributions ---------------------------------------------------------
//
// CONSTANT arguments  -> the IDistribution is built ONCE, and its mean is known.
// COMPUTED arguments  -> built per evaluation, because the parameter may differ
//                        each time, and the mean is NOT knowable in advance.
//
// Either way this node owns the stream pointer and applies it to whichever
// instance it makes, so useStream() works in both cases.
using DistBuilder = std::unique_ptr<IDistribution> (*)(const std::vector<double>&);

class DistributionCall : public IExpression {
    std::string                    m_name;
    DistBuilder                    m_build;
    std::vector<ExpressionPtr>     m_args;
    std::unique_ptr<IDistribution> m_fixed;    // non-null when args are constant
    RandomStream*                  m_stream{nullptr};
public:
    DistributionCall(std::string name, DistBuilder build,
                     std::vector<ExpressionPtr> args, SourceSpan span)
        : IExpression(span), m_name(std::move(name)), m_build(build), m_args(std::move(args)) {
        std::vector<double> constants;
        bool allConstant = true;
        for (const ExpressionPtr& a : m_args) {
            if (auto m = a->meanIfKnown()) constants.push_back(*m);
            else { allConstant = false; break; }
        }
        if (allConstant) m_fixed = m_build(constants);
    }

    Value evaluate(EvalContext& ctx) const override {
        RandomStream& rng = ctx.requireRng();
        if (m_fixed) return static_cast<double>(m_fixed->draw(rng));
        std::vector<double> v;
        v.reserve(m_args.size());
        for (const ExpressionPtr& a : m_args) v.push_back(asNumber(a->evaluate(ctx)));
        std::unique_ptr<IDistribution> d = m_build(v);
        if (m_stream) d->useStream(m_stream);
        return static_cast<double>(d->draw(rng));
    }
    void validate(const ValidationContext& vc, std::vector<Diagnostic>& out) const override {
        for (const ExpressionPtr& a : m_args) a->validate(vc, out);
    }
    // The whole point of meanIfKnown: constant arguments give a mean the
    // stability check can use; computed ones honestly cannot.
    std::optional<double> meanIfKnown() const override {
        if (m_fixed) return static_cast<double>(m_fixed->mean());
        return std::nullopt;
    }
    std::string describe() const override {
        std::string s = m_name + "(";
        for (std::size_t i = 0; i < m_args.size(); ++i) {
            if (i) s += ", ";
            s += m_args[i]->describe();
        }
        return s + ")";
    }
    ExpressionPtr clone() const override {
        std::vector<ExpressionPtr> copies;
        copies.reserve(m_args.size());
        for (const ExpressionPtr& a : m_args) copies.push_back(a->clone());
        auto c = std::make_unique<DistributionCall>(m_name, m_build, std::move(copies), m_span);
        c->useStream(m_stream);
        return c;
    }
    void useStream(RandomStream* s) override {
        m_stream = s;
        if (m_fixed) m_fixed->useStream(s);
    }
};

// --- the table -------------------------------------------------------------
// arity of -1 means variadic (at least one argument).
struct Entry {
    int         arity;
    MathFn      math{MathFn::Abs};
    StateFn     state{StateFn::WIP};
    DistBuilder dist{nullptr};
    enum class Kind { Math, State, Dist } kind{Kind::Math};
};

std::unique_ptr<IDistribution> buildExpo(const std::vector<double>& a) { return exponential(a[0]); }
std::unique_ptr<IDistribution> buildCons(const std::vector<double>& a) { return constant(a[0]); }
std::unique_ptr<IDistribution> buildUnif(const std::vector<double>& a) { return uniform(a[0], a[1]); }
std::unique_ptr<IDistribution> buildTria(const std::vector<double>& a) { return triangular(a[0], a[1], a[2]); }
std::unique_ptr<IDistribution> buildNorm(const std::vector<double>& a) { return normal(a[0], a[1]); }
std::unique_ptr<IDistribution> buildLogn(const std::vector<double>& a) { return lognormalFrom(a[0], a[1]); }
std::unique_ptr<IDistribution> buildWeib(const std::vector<double>& a) { return weibull(a[0], a[1]); }
std::unique_ptr<IDistribution> buildErla(const std::vector<double>& a) { return erlang(a[0], static_cast<int>(a[1])); }
std::unique_ptr<IDistribution> buildPois(const std::vector<double>& a) { return poisson(a[0]); }

// DISC(p1, v1, p2, v2, ...) -- Arena's spelling: cumulative probability first.
std::unique_ptr<IDistribution> buildDisc(const std::vector<double>& a) {
    std::vector<SimTime> values;
    std::vector<double>  cumulative;
    for (std::size_t i = 0; i + 1 < a.size(); i += 2) {
        cumulative.push_back(a[i]);
        values.push_back(a[i + 1]);
    }
    return std::make_unique<Discrete>(values, cumulative);
}

const std::unordered_map<std::string, Entry>& table() {
    static const std::unordered_map<std::string, Entry> t = {
        {"EXPO", {1, {}, {}, &buildExpo, Entry::Kind::Dist}},
        {"CONS", {1, {}, {}, &buildCons, Entry::Kind::Dist}},
        {"UNIF", {2, {}, {}, &buildUnif, Entry::Kind::Dist}},
        {"TRIA", {3, {}, {}, &buildTria, Entry::Kind::Dist}},
        {"NORM", {2, {}, {}, &buildNorm, Entry::Kind::Dist}},
        {"LOGN", {2, {}, {}, &buildLogn, Entry::Kind::Dist}},
        {"WEIB", {2, {}, {}, &buildWeib, Entry::Kind::Dist}},
        {"ERLA", {2, {}, {}, &buildErla, Entry::Kind::Dist}},
        {"POIS", {1, {}, {}, &buildPois, Entry::Kind::Dist}},
        {"DISC", {-1, {}, {}, &buildDisc, Entry::Kind::Dist}},

        {"NQ",  {1, {}, StateFn::NQ,  nullptr, Entry::Kind::State}},
        {"NR",  {1, {}, StateFn::NR,  nullptr, Entry::Kind::State}},
        {"MR",  {1, {}, StateFn::MR,  nullptr, Entry::Kind::State}},
        {"WIP", {0, {}, StateFn::WIP, nullptr, Entry::Kind::State}},

        {"MIN",   {-1, MathFn::Min,   {}, nullptr, Entry::Kind::Math}},
        {"MAX",   {-1, MathFn::Max,   {}, nullptr, Entry::Kind::Math}},
        {"ABS",   {1,  MathFn::Abs,   {}, nullptr, Entry::Kind::Math}},
        {"ROUND", {1,  MathFn::Round, {}, nullptr, Entry::Kind::Math}},
        {"TRUNC", {1,  MathFn::Trunc, {}, nullptr, Entry::Kind::Math}},
        {"SQRT",  {1,  MathFn::Sqrt,  {}, nullptr, Entry::Kind::Math}},
        {"LN",    {1,  MathFn::Ln,    {}, nullptr, Entry::Kind::Math}},
        {"EXP",   {1,  MathFn::Exp,   {}, nullptr, Entry::Kind::Math}},
        {"MOD",   {2,  MathFn::Mod,   {}, nullptr, Entry::Kind::Math}},
    };
    return t;
}

ExpressionPtr placeholder(SourceSpan span) {
    return std::make_unique<LiteralExpression>(Value(0.0), span);
}

}  // namespace

bool isBuiltinFunction(const std::string& name) { return table().count(name) != 0; }

ExpressionPtr buildCall(const std::string& name,
                        std::vector<ExpressionPtr> args,
                        const std::vector<SourceSpan>& argSpans,
                        SourceSpan nameSpan,
                        std::vector<Diagnostic>& out) {
    const auto it = table().find(name);
    if (it == table().end()) {
        out.push_back(Diagnostic{Severity::Error, nameSpan,
            "unknown function '" + name + "'"});
        return placeholder(nameSpan);
    }
    const Entry& entry = it->second;

    const int given = static_cast<int>(args.size());
    if (entry.arity >= 0 && given != entry.arity) {
        out.push_back(Diagnostic{Severity::Error, nameSpan,
            name + " expects " + std::to_string(entry.arity) +
            (entry.arity == 1 ? " argument, got " : " arguments, got ") +
            std::to_string(given)});
        return placeholder(nameSpan);
    }
    if (entry.arity < 0 && given == 0) {
        out.push_back(Diagnostic{Severity::Error, nameSpan,
            name + " expects at least one argument"});
        return placeholder(nameSpan);
    }
    if (name == "DISC" && given % 2 != 0) {
        out.push_back(Diagnostic{Severity::Error, nameSpan,
            "DISC expects probability/value pairs, so an even number of arguments"});
        return placeholder(nameSpan);
    }

    switch (entry.kind) {
        case Entry::Kind::Math:
            return std::make_unique<MathCall>(entry.math, name, std::move(args), nameSpan);

        case Entry::Kind::State: {
            if (entry.state == StateFn::WIP)
                return std::make_unique<StateCall>(entry.state, "", nameSpan);
            // The one special case: capture the argument's TEXT, do not evaluate it.
            const auto* asName = dynamic_cast<const NameExpression*>(args[0].get());
            if (asName != nullptr)
                return std::make_unique<StateCall>(entry.state, asName->name(), nameSpan);
            if (auto lit = args[0]->meanIfKnown(); !lit.has_value()) {
                // A quoted string: LiteralExpression holding text. describe()
                // gives it back unquoted.
                return std::make_unique<StateCall>(entry.state, args[0]->describe(), nameSpan);
            }
            out.push_back(Diagnostic{Severity::Error,
                argSpans.empty() ? nameSpan : argSpans[0],
                name + " takes the NAME of a block, not a computed value"});
            return placeholder(nameSpan);
        }

        case Entry::Kind::Dist:
            return std::make_unique<DistributionCall>(name, entry.dist, std::move(args), nameSpan);
    }
    return placeholder(nameSpan);
}

}  // namespace des
```

> Check `include/Distribution.hpp` for `Discrete`'s exact constructor and for `lognormalFrom` / `erlang` signatures in `include/Build.hpp` before writing the builders. Match what is there; do not change those classes.

- [ ] **Step 5: Write `include/Parser.hpp`**

```cpp
// ============================================================================
// Parser.hpp  --  v10: tokens into a tree, reporting everything wrong
// ============================================================================
// Recursive descent with a precedence climb. The shape is ordinary; two things
// about it are not.
//
// IT DOES NOT THROW. A malformed expression is a person typing in a cell, and
// v11's spreadsheet must show every bad cell at once. So the parser RECOVERS:
// on an error it records a diagnostic, skips to something it can resume at, and
// keeps going. One call, many problems.
//
// IT RESOLVES NOTHING. An identifier becomes a NameExpression holding a string.
// Whether that string is an attribute, a variable or TNOW is decided later, by
// validate() and evaluate(). The parser has no name tables, so v11 adding a
// Variable data module cannot make it stale.

#pragma once
#include <string>
#include <vector>
#include "Diagnostic.hpp"
#include "Expression.hpp"

namespace des {

struct ParseResult {
    ExpressionPtr           expr;          // never null; a placeholder on error
    std::vector<Diagnostic> diagnostics;
};

ParseResult parseExpression(const std::string& source);

// For C++ model code, where a bad expression is a PROGRAMMER error and there is
// no cell to point at. Throws ModelError listing every diagnostic.
ExpressionPtr expr(const std::string& source);

}  // namespace des
```

- [ ] **Step 6: Write `src/Parser.cpp`**

```cpp
#include "Parser.hpp"
#include <utility>
#include "Functions.hpp"
#include "Lexer.hpp"
#include "ModelError.hpp"

namespace des {
namespace {

class Parser {
private:
    std::vector<Token>       m_tokens;
    std::size_t              m_at{0};
    std::vector<Diagnostic>* m_diagnostics;

    const Token& peek() const  { return m_tokens[m_at]; }
    bool at(TokenKind k) const { return peek().kind == k; }

    const Token& advance() {
        const Token& t = m_tokens[m_at];
        if (m_at + 1 < m_tokens.size()) ++m_at;
        return t;
    }
    bool match(TokenKind k) {
        if (!at(k)) return false;
        advance();
        return true;
    }

    void error(const std::string& message) { error(message, peek().span); }
    void error(const std::string& message, SourceSpan span) {
        // One diagnostic per position. Without this a recovery loop can emit
        // the same complaint repeatedly and bury the real first cause.
        if (!m_diagnostics->empty() && m_diagnostics->back().span.offset == span.offset) return;
        m_diagnostics->push_back(Diagnostic{Severity::Error, span, message});
    }

    // A stand-in so the tree stays well-formed after an error and parsing can
    // continue. Never evaluated: hasErrors() gates that.
    ExpressionPtr recover() {
        return std::make_unique<LiteralExpression>(Value(0.0), peek().span);
    }

    ExpressionPtr parseOr() {
        ExpressionPtr left = parseAnd();
        while (at(TokenKind::OrOr)) {
            const SourceSpan span = advance().span;
            left = std::make_unique<BinaryExpression>(BinaryOp::Or, std::move(left),
                                                      parseAnd(), span);
        }
        return left;
    }

    ExpressionPtr parseAnd() {
        ExpressionPtr left = parseComparison();
        while (at(TokenKind::AndAnd)) {
            const SourceSpan span = advance().span;
            left = std::make_unique<BinaryExpression>(BinaryOp::And, std::move(left),
                                                      parseComparison(), span);
        }
        return left;
    }

    ExpressionPtr parseComparison() {
        ExpressionPtr left = parseAdditive();
        for (;;) {
            BinaryOp op;
            switch (peek().kind) {
                case TokenKind::Less:         op = BinaryOp::Less;         break;
                case TokenKind::LessEqual:    op = BinaryOp::LessEqual;    break;
                case TokenKind::Greater:      op = BinaryOp::Greater;      break;
                case TokenKind::GreaterEqual: op = BinaryOp::GreaterEqual; break;
                case TokenKind::EqualEqual:   op = BinaryOp::Equal;        break;
                case TokenKind::BangEqual:    op = BinaryOp::NotEqual;     break;
                default: return left;
            }
            const SourceSpan span = advance().span;
            left = std::make_unique<BinaryExpression>(op, std::move(left), parseAdditive(), span);
        }
    }

    ExpressionPtr parseAdditive() {
        ExpressionPtr left = parseMultiplicative();
        for (;;) {
            BinaryOp op;
            if      (at(TokenKind::Plus))  op = BinaryOp::Add;
            else if (at(TokenKind::Minus)) op = BinaryOp::Subtract;
            else return left;
            const SourceSpan span = advance().span;
            left = std::make_unique<BinaryExpression>(op, std::move(left),
                                                      parseMultiplicative(), span);
        }
    }

    ExpressionPtr parseMultiplicative() {
        ExpressionPtr left = parseUnary();
        for (;;) {
            BinaryOp op;
            if      (at(TokenKind::Star))    op = BinaryOp::Multiply;
            else if (at(TokenKind::Slash))   op = BinaryOp::Divide;
            else if (at(TokenKind::Percent)) op = BinaryOp::Modulo;
            else return left;
            const SourceSpan span = advance().span;
            left = std::make_unique<BinaryExpression>(op, std::move(left), parseUnary(), span);
        }
    }

    // Unary binds LOOSER than ^, so -2^2 is -(2^2) = -4, as in every maths
    // convention and in Arena. Getting this backwards is the classic
    // precedence-climb bug, and the test asserts it explicitly.
    ExpressionPtr parseUnary() {
        if (at(TokenKind::Minus) || at(TokenKind::Bang)) {
            const bool negate = at(TokenKind::Minus);
            const SourceSpan span = advance().span;
            return std::make_unique<UnaryExpression>(
                negate ? UnaryOp::Negate : UnaryOp::Not, parseUnary(), span);
        }
        return parsePower();
    }

    ExpressionPtr parsePower() {
        ExpressionPtr base = parsePrimary();
        if (at(TokenKind::Caret)) {
            const SourceSpan span = advance().span;
            // Right-associative: recurse into parseUnary, not parsePower.
            return std::make_unique<BinaryExpression>(BinaryOp::Power, std::move(base),
                                                      parseUnary(), span);
        }
        return base;
    }

    ExpressionPtr parsePrimary() {
        if (at(TokenKind::Number)) {
            const Token t = advance();
            return std::make_unique<LiteralExpression>(Value(t.number), t.span);
        }
        if (at(TokenKind::Text)) {
            const Token t = advance();
            return std::make_unique<LiteralExpression>(Value(t.text), t.span);
        }
        if (at(TokenKind::LParen)) {
            advance();
            ExpressionPtr inner = parseOr();
            if (!match(TokenKind::RParen)) error("expected ')'");
            return inner;
        }
        if (at(TokenKind::Identifier)) {
            const Token name = advance();
            if (at(TokenKind::LParen)) return parseCall(name);
            return std::make_unique<NameExpression>(name.text, name.span);
        }

        error(std::string("expected a value, got ") + describe(peek().kind));
        if (!at(TokenKind::End)) advance();   // skip it so we make progress
        return recover();
    }

    ExpressionPtr parseCall(const Token& name) {
        advance();                                  // consume '('
        std::vector<ExpressionPtr> args;
        std::vector<SourceSpan>    argSpans;
        if (!at(TokenKind::RParen)) {
            do {
                argSpans.push_back(peek().span);
                args.push_back(parseOr());
            } while (match(TokenKind::Comma));
        }
        if (!match(TokenKind::RParen)) error("expected ')'");
        return buildCall(name.text, std::move(args), argSpans, name.span, *m_diagnostics);
    }

public:
    Parser(std::vector<Token> tokens, std::vector<Diagnostic>* diagnostics)
        : m_tokens(std::move(tokens)), m_diagnostics(diagnostics) {}

    ExpressionPtr run() {
        ExpressionPtr e = parseOr();
        if (!at(TokenKind::End))
            error(std::string("unexpected ") + describe(peek().kind) +
                  " after the end of the expression");
        return e;
    }
};

}  // namespace

ParseResult parseExpression(const std::string& source) {
    ParseResult result;
    LexResult lexed = tokenise(source);
    result.diagnostics = std::move(lexed.diagnostics);
    Parser parser(std::move(lexed.tokens), &result.diagnostics);
    result.expr = parser.run();
    return result;
}

ExpressionPtr expr(const std::string& source) {
    ParseResult r = parseExpression(source);
    if (hasErrors(r.diagnostics))
        throw ModelError("cannot parse expression \"" + source + "\":\n" +
                         formatDiagnostics(r.diagnostics));
    return std::move(r.expr);
}

}  // namespace des
```

- [ ] **Step 7: Wire into the build**

Add `src/Functions.cpp` and `src/Parser.cpp` to `CMakeLists.txt` after `src/Expression.cpp`. In `include/des.hpp`, add after `#include "Expression.hpp"`:

```cpp
#include "Functions.hpp"
#include "Parser.hpp"
```

- [ ] **Step 8: Run the tests**

```bash
cmake --build build && ./build/des_tests | tail -3
```

Expected: `374 / 374 checks passed` (336 + 38 new). No FAIL lines.

- [ ] **Step 9: Check the sanitisers**

```bash
bash tools/verify.sh
```

Expected: `ASAN CLEAN`.

- [ ] **Step 10: Commit**

```bash
git add include/Parser.hpp src/Parser.cpp include/Functions.hpp src/Functions.cpp include/des.hpp CMakeLists.txt tests/expression_tests.cpp
git commit -m "feat: the parser and the built-in function registry

Distributions are grammar functions owning the existing IDistribution
objects -- there is no separate distribution field any more. Constant
arguments build the distribution once and keep a knowable mean; computed
ones build per draw and report nullopt."
```

---

### Task 6: VariableStore

Replaces the Task 4 stub wholesale.

**Files:**
- Rewrite: `include/VariableStore.hpp`, `src/VariableStore.cpp`
- Test: `tests/expression_tests.cpp`

**Interfaces:**
- Consumes: `Statistics` (existing), `ModelError`.
- Produces:
  - `void VariableStore::declare(const std::string& name, double initialValue)` — throws `ModelError` on a duplicate or on a collision with a known attribute name
  - `void VariableStore::noteAttributeNames(std::vector<std::string>)` — the set `declare` checks against
  - `bool has(const std::string&) const`, `double get(const std::string&) const`
  - `void set(const std::string& name, double value, SimTime now)` — throws on an undeclared name
  - `void reset()`, `void resetStatistics(SimTime now)`, `void updateIntegrals(SimTime now)`
  - `double timeAverage(const std::string& name) const`
  - `std::vector<std::string> names() const`

- [ ] **Step 1: Write the failing test**

```cpp
    section("Variables");
    {
        VariableStore v;
        v.declare("WIPCount", 0.0);
        check(v.has("WIPCount"), "a declared variable is present");
        check(!v.has("nosuch"), "an undeclared variable is absent");
        checkClose(v.get("WIPCount"), 0.0, 1e-12, "initial value");

        v.set("WIPCount", 5.0, 0.0);
        checkClose(v.get("WIPCount"), 5.0, 1e-12, "set round-trips");

        // Configuration survives reset; run state does not.
        v.reset();
        checkClose(v.get("WIPCount"), 0.0, 1e-12, "reset restores the initial value");
        check(v.has("WIPCount"), "reset keeps the declaration");

        // Duplicates are refused.
        bool threw = false;
        try { v.declare("WIPCount", 1.0); } catch (const ModelError&) { threw = true; }
        check(threw, "a duplicate declaration throws");

        // ONE NAMESPACE: a variable may not shadow an attribute. The
        // alternative is a resolution order, and a resolution order means one
        // of the two reads silently wrong.
        VariableStore w;
        w.noteAttributeNames({"priority"});
        threw = false;
        try { w.declare("priority", 0.0); } catch (const ModelError&) { threw = true; }
        check(threw, "a variable colliding with an attribute is refused");

        // Undeclared writes are refused rather than auto-declaring.
        threw = false;
        try { w.set("nosuch", 1.0, 0.0); } catch (const ModelError&) { threw = true; }
        check(threw, "setting an undeclared variable throws");

        // Time-persistent average, hand-computed:
        // value 0 for [0,2), 10 for [2,4)  ->  time average over [0,4) = 5.
        VariableStore t;
        t.declare("Level", 0.0);
        t.resetStatistics(0.0);
        t.set("Level", 10.0, 2.0);
        t.updateIntegrals(4.0);
        checkClose(t.timeAverage("Level"), 5.0, 1e-9, "time-persistent average, hand-computed");

        // Warm-up: statistics restart, the VALUE does not.
        t.resetStatistics(4.0);
        checkClose(t.get("Level"), 10.0, 1e-12, "resetStatistics leaves the value alone");
        t.updateIntegrals(6.0);
        checkClose(t.timeAverage("Level"), 10.0, 1e-9, "average measures only since the warm-up");

        check(v.names().size() == 1, "names() lists the declarations");
    }
```

- [ ] **Step 2: Run it and confirm it fails**

```bash
cmake --build build 2>&1 | head -20
```

Expected: `'class des::VariableStore' has no member named 'declare'`.

- [ ] **Step 3: Rewrite `include/VariableStore.hpp`**

```cpp
// ============================================================================
// VariableStore.hpp  --  v10: Arena's Variable data module
// ============================================================================
// Global, mutable, numeric, shared by every block. An entity ATTRIBUTE travels
// with one entity; a VARIABLE belongs to the system. "How many are in the shop
// right now", "what shift are we on", "how many have failed inspection today"
// are all variables, and none of them can be said with attributes.
//
// NUMERIC ONLY, as Arena's are, even though Value can hold a string. Strings
// exist in the language to compare an entity's TYPE, not to be stored.
//
// ONE NAMESPACE WITH ATTRIBUTES. Declaring a variable named `priority` when an
// attribute of that name exists is a hard error. The alternative is a
// resolution order, and a resolution order means one of the two reads silently
// wrong -- which is the failure this project's own rules name twice: "a default
// return value is a place for a bug to hide", and "a statistic that is silently
// zero is worse than one that is missing".
//
// TIME-PERSISTENT, NOT TALLY. A variable's average is weighted by how long it
// held each value, so the integral must be closed BEFORE the value changes --
// the same ordering rule the engine's run loop lives by.

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "Common.hpp"

namespace des {

class VariableStore {
private:
    struct Variable {
        double  initial{0.0};
        double  current{0.0};
        double  area{0.0};          // integral of value over measured time
        SimTime lastChange{0.0};    // when `current` was last integrated to
    };

    std::unordered_map<std::string, Variable> m_variables;
    std::vector<std::string>                  m_order;          // declaration order
    std::vector<std::string>                  m_attributeNames;
    SimTime                                   m_measuringSince{0.0};
    SimTime                                   m_lastUpdate{0.0};

public:
    // Throws ModelError on a duplicate, or on a collision with a known
    // attribute name.
    void declare(const std::string& name, double initialValue);

    // The attribute names `declare` checks against. Set before declaring.
    void noteAttributeNames(std::vector<std::string> names);

    bool   has(const std::string& name) const;
    double get(const std::string& name) const;

    // Closes the integral at `now` BEFORE changing the value. Throws
    // ModelError if the name was never declared -- auto-declaring on write is
    // how a typo becomes a second variable nobody notices.
    void set(const std::string& name, double value, SimTime now);

    // Back to t=0. Configuration survives, run state does not.
    void reset();

    // Warm-up removal: discard what has been measured, keep the VALUES.
    void resetStatistics(SimTime now);

    // Close every integral up to `now`. The engine calls this alongside its
    // other integrals, before the clock moves.
    void updateIntegrals(SimTime now);

    double timeAverage(const std::string& name) const;

    std::vector<std::string> names() const { return m_order; }
    std::size_t              count() const { return m_order.size(); }
};

}  // namespace des
```

- [ ] **Step 4: Rewrite `src/VariableStore.cpp`**

```cpp
#include "VariableStore.hpp"
#include <algorithm>
#include "ModelError.hpp"

namespace des {

void VariableStore::noteAttributeNames(std::vector<std::string> names) {
    m_attributeNames = std::move(names);
}

void VariableStore::declare(const std::string& name, double initialValue) {
    if (m_variables.count(name) != 0)
        throw ModelError("variable '" + name + "' is declared twice");

    if (std::find(m_attributeNames.begin(), m_attributeNames.end(), name) !=
        m_attributeNames.end())
        throw ModelError("'" + name + "' is already an entity attribute; a variable may not "
                         "share its name, because then one of the two would read wrong "
                         "with no error");

    Variable v;
    v.initial = initialValue;
    v.current = initialValue;
    m_variables.emplace(name, v);
    m_order.push_back(name);
}

bool VariableStore::has(const std::string& name) const {
    return m_variables.count(name) != 0;
}

double VariableStore::get(const std::string& name) const {
    const auto it = m_variables.find(name);
    if (it == m_variables.end())
        throw ModelError("no variable named '" + name + "'");
    return it->second.current;
}

void VariableStore::set(const std::string& name, double value, SimTime now) {
    const auto it = m_variables.find(name);
    if (it == m_variables.end())
        throw ModelError("no variable named '" + name + "' -- declare it before assigning to it");

    // CLOSE THE INTERVAL FIRST, then change the value. Reversed, the new value
    // would be credited with time it was never held, and every time average
    // would be quietly wrong with no error -- the same ordering rule as the
    // engine's run loop.
    Variable& v = it->second;
    v.area += v.current * (now - v.lastChange);
    v.lastChange = now;
    v.current = value;
}

void VariableStore::reset() {
    for (auto& pair : m_variables) {
        Variable& v = pair.second;
        v.current = v.initial;
        v.area = 0.0;
        v.lastChange = 0.0;
    }
    m_measuringSince = 0.0;
    m_lastUpdate = 0.0;
}

void VariableStore::resetStatistics(SimTime now) {
    for (auto& pair : m_variables) {
        Variable& v = pair.second;
        v.area = 0.0;
        v.lastChange = now;      // the VALUE is untouched -- only the measurement restarts
    }
    m_measuringSince = now;
    m_lastUpdate = now;
}

void VariableStore::updateIntegrals(SimTime now) {
    for (auto& pair : m_variables) {
        Variable& v = pair.second;
        v.area += v.current * (now - v.lastChange);
        v.lastChange = now;
    }
    m_lastUpdate = now;
}

double VariableStore::timeAverage(const std::string& name) const {
    const auto it = m_variables.find(name);
    if (it == m_variables.end())
        throw ModelError("no variable named '" + name + "'");
    const SimTime measured = m_lastUpdate - m_measuringSince;
    if (measured <= 0.0) return it->second.current;
    return it->second.area / measured;
}

}  // namespace des
```

- [ ] **Step 5: Run the tests**

```bash
cmake --build build && ./build/des_tests | tail -3
```

Expected: `387 / 387 checks passed` (374 + 13 new).

- [ ] **Step 6: Commit**

```bash
git add include/VariableStore.hpp src/VariableStore.cpp tests/expression_tests.cpp
git commit -m "feat: VariableStore -- Arena's Variable data module

One namespace with attributes: a colliding declaration is refused rather
than resolved by precedence, because a resolution order means one of the
two reads silently wrong."
```

---

### Task 7: Wire variables and model state into the engine

The first task that touches the engine core. Nothing user-visible changes yet — this makes `NQ(Teller)`, `TNOW` and variables *reachable* from a running model.

**Files:**
- Modify: `include/Model.hpp`, `src/Model.cpp` (variable declarations)
- Modify: `include/SimulationSystem.hpp`, `src/SimulationSystem.cpp` (implement `IModelState`, own the store, reset it, integrate it)
- Modify: `include/Node.hpp`, `src/Node.cpp` (`NodeContext` hands out an `EvalContext`)
- Test: `tests/expression_tests.cpp`

**Interfaces:**
- Consumes: `IModelState`, `EvalContext`, `VariableStore` (Tasks 4, 6).
- Produces:
  - `Model& Model::variable(const std::string& name, double initialValue = 0.0)`
  - `VariableStore& Model::variables()`, `const VariableStore& Model::variables() const`
  - `SimulationSystem` publicly derives from `IModelState` and implements all five methods
  - `EvalContext NodeContext::evaluationContext(const Entity* e)` — the **only** way a node builds one
  - `double SimulationSystem::variableAverage(const std::string&) const`

- [ ] **Step 1: Read what you are about to change**

```bash
sed -n '1,120p' include/SimulationSystem.hpp
grep -n "refreshState\|updateAllIntegrals\|resetStatistics\|initialise\|reset()" src/SimulationSystem.cpp | head -40
grep -n "queueLength\|unitsBusy\|numberInSystem\|WIP" src/SimulationSystem.cpp | head -20
```

You need to know: where `updateAllIntegrals` is called from `run()`, what `refreshState()` sums over (v9 taught it to count batch queues too), and how `initialise()` resets things. **Do not restructure any of it.** The ordering inside `run()` — close integrals, then move the clock, then change state — is not negotiable, and `VariableStore::updateIntegrals` must be called in the *close the integrals* step, not after the clock moves.

- [ ] **Step 2: Write the failing test**

```cpp
    section("Variables in a running model");
    {
        SimulationSystem sim(12345u);
        sim.model().variable("Served", 0.0)
                   .arrivals(constant(1.0))
                   .station("Teller", 1, FIFO, constant(0.5))
                   .entryAt("Teller");
        sim.stopAt(10.0);
        sim.initialise();

        // Declared variables survive initialise() at their initial value.
        checkClose(sim.model().variables().get("Served"), 0.0, 1e-12,
                   "a variable starts at its initial value");

        sim.run();

        // IModelState is answerable from the engine.
        const IModelState& state = sim;
        checkClose(state.now(), sim.clock().now(), 1e-12, "IModelState::now is the clock");
        check(state.queueLength("Teller") >= 0.0, "queueLength answers for a real block");
        check(state.resourceCapacity("Teller") == 1.0, "resourceCapacity answers");

        // An unknown block name is an ERROR, not a silent zero. A typo'd block
        // name in NQ() must not read as "the queue is empty".
        bool threw = false;
        try { state.queueLength("Tellr"); } catch (const ModelError&) { threw = true; }
        check(threw, "queueLength on an unknown block throws rather than reading 0");

        // A second run starts clean.
        sim.initialise();
        checkClose(sim.model().variables().get("Served"), 0.0, 1e-12,
                   "initialise() resets variables");
    }
```

> `sim.clock().now()` — confirm the accessor's spelling in `include/SimulationSystem.hpp` and match it. If the clock is not exposed, compare against `state.now()` being non-zero instead.

- [ ] **Step 3: Add variables to `Model`**

In `include/Model.hpp`, add a `VariableStore m_variables;` member and:

```cpp
    // v10: Arena's Variable data module. Global, numeric, shared by every
    // block -- the thing an entity attribute cannot say, because an attribute
    // travels with one entity and this belongs to the system.
    Model& variable(const std::string& name, double initialValue = 0.0);
    VariableStore&       variables()       { return m_variables; }
    const VariableStore& variables() const { return m_variables; }
```

In `src/Model.cpp`:

```cpp
Model& Model::variable(const std::string& name, double initialValue) {
    // Feed the store the attribute names first, so a collision is caught HERE
    // rather than becoming a silent shadow at evaluation time.
    std::vector<std::string> attributeNames;
    for (const ArrivalAttribute& a : m_arrivalAttributes) attributeNames.push_back(a.name);
    m_variables.noteAttributeNames(attributeNames);
    m_variables.declare(name, initialValue);
    return *this;
}
```

Add `m_variables.reset();` to `Model::reset()`.

- [ ] **Step 4: Make `SimulationSystem` an `IModelState`**

In `include/SimulationSystem.hpp`, change the class declaration to `class SimulationSystem : public IModelState`, and declare the five overrides plus one accessor:

```cpp
    // v10: the narrow interface the expression layer asked for. Note what is
    // NOT here -- the FEL, the entity table, the statistics objects. An
    // expression may ask four questions about the running system and no more.
    double  queueLength(const std::string& blockName) const override;
    double  resourceBusy(const std::string& name) const override;
    double  resourceCapacity(const std::string& name) const override;
    double  numberInSystem() const override;
    SimTime now() const override;

    double variableAverage(const std::string& name) const;
```

In `src/SimulationSystem.cpp`, implement them against the model. Each throws `ModelError` when the name is unknown:

```cpp
double SimulationSystem::queueLength(const std::string& blockName) const {
    // A typo'd block name must not read as "the queue is empty". That is the
    // exact shape of the v9 bug where WIP was silently 0.0 while 32 balls were
    // waiting -- a value that is quietly zero gets copied into an answer.
    if (const Station* s = m_model.station(blockName))
        return static_cast<double>(s->queueSize());
    throw ModelError("NQ(" + blockName + "): no block named '" + blockName + "'");
}

double SimulationSystem::resourceBusy(const std::string& name) const {
    if (const Resource* r = m_model.resourceNamed(name)) return r->unitsBusy();
    if (const Station* s = m_model.station(name))        return s->resource().unitsBusy();
    throw ModelError("NR(" + name + "): no resource or block named '" + name + "'");
}

double SimulationSystem::resourceCapacity(const std::string& name) const {
    if (const Resource* r = m_model.resourceNamed(name)) return r->capacity();
    if (const Station* s = m_model.station(name))        return s->resource().capacity();
    throw ModelError("MR(" + name + "): no resource or block named '" + name + "'");
}

double SimulationSystem::numberInSystem() const { return m_state.numberInSystem(); }

SimTime SimulationSystem::now() const { return m_clock.now(); }

double SimulationSystem::variableAverage(const std::string& name) const {
    return m_model.variables().timeAverage(name);
}
```

> Member names (`m_model`, `m_state`, `m_clock`) and accessors (`queueSize()`, `resource()`, `numberInSystem()`) must be checked against the real headers. Use what is there.

- [ ] **Step 5: Integrate and reset the variables**

Two calls, both in places that already exist:

1. In `initialise()`, alongside the other `reset()` calls: `m_model.variables().reset();` then, after the warm-up is known, ensure `resetStatistics` is called for variables wherever the other `resetStatistics(now)` calls happen.
2. In `updateAllIntegrals(SimTime t)` — **the step that closes the interval that just ended, before the clock moves** — add `m_model.variables().updateIntegrals(t);`.

> Getting (2) in the wrong place is the failure mode the run-loop comment in `README.md` warns about: every time-average goes quietly wrong with no error. Put it beside the existing integral updates, not after `clock.advanceTo`.

- [ ] **Step 6: Let nodes build an EvalContext**

In `include/Node.hpp`, add to `NodeContext`:

```cpp
    // v10: the only way a node evaluates an expression. Nodes never construct
    // an EvalContext themselves -- this is what keeps the variable store and
    // the model-state implementation out of every node's reach.
    EvalContext evaluationContext(const Entity* e);
```

In `src/Node.cpp`:

```cpp
EvalContext NodeContext::evaluationContext(const Entity* e) {
    return EvalContext(e, &m_sim.model().variables(), &m_sim, &m_sim.rng());
}
```

> Confirm `SimulationSystem::rng()` exists and is non-const. If it is private, `NodeContext` is already a friend or has an accessor — reuse whichever route `NodeContext::rng()` already takes.

- [ ] **Step 7: Run the tests**

```bash
cmake --build build && ./build/des_tests | tail -3
```

Expected: `394 / 394 checks passed` (387 + 7 new), **and all 293 original checks still among them**.

- [ ] **Step 8: Verify the traces are still byte-identical**

```bash
bash tools/baseline.sh check
```

Expected: no `DIFFERS` lines. **If any example differs, stop.** Nothing in this task should change behaviour, and a diff means an integral moved.

- [ ] **Step 9: Commit**

```bash
git add include/Model.hpp src/Model.cpp include/SimulationSystem.hpp src/SimulationSystem.cpp include/Node.hpp src/Node.cpp tests/expression_tests.cpp
git commit -m "feat: variables and IModelState wired into the engine

SimulationSystem implements the interface the expression layer declared.
An unknown block name in NQ() throws rather than reading zero -- the v9
silently-zero-WIP bug is the reason."
```

---

### Task 8: Decide takes an expression

The first block converted, and chosen first because its blast radius is the smallest: `DecideNode`'s condition is self-contained. From here on, the byte-identical trace test is meaningful.

**Files:**
- Modify: `include/Nodes.hpp`, `src/Nodes.cpp` (`DecideNode`)
- Modify: `include/Model.hpp`, `src/Model.cpp` (string overloads)
- Test: `tests/expression_tests.cpp`

**Interfaces:**
- Consumes: `ExpressionPtr`, `LambdaExpression`, `expr()` (Tasks 4, 5).
- Produces:
  - `DecideNode::Branch` gains `ExpressionPtr condition` **replacing** `Condition condition`
  - `DecideNode::addBranch(ExpressionPtr, INode*)` alongside the existing `addBranch(Condition, INode*)`
  - `Model& Model::decideByCondition(const std::string& name, const std::string& conditionText)`
  - `Model& Model::branch(const std::string& decideName, const std::string& conditionText, const std::string& to)`

- [ ] **Step 1: Write the failing test**

```cpp
    section("Decide by a text condition");
    {
        // Text and lambda must agree exactly -- same seed, same routing.
        auto countExpress = [](bool useText) {
            SimulationSystem sim(4242u);
            Model& m = sim.model();
            m.attribute("priority", uniform(0.0, 4.0))
             .arrivals(exponential(1.0));
            if (useText) m.decideByCondition("Sort", "priority > 2");
            else         m.decideByCondition("Sort",
                            [](const Entity& e){ return e.attribute("priority") > 2; });
            m.station("Express", 1, FIFO, exponential(0.3))
             .station("Normal",  1, FIFO, exponential(0.3))
             .routeTrue("Sort", "Express")
             .route("Sort", "Normal")
             .entryAt("Sort");
            sim.stopAt(200.0).initialise();
            sim.run();
            return sim.model().nodeAs<DecideNode>("Sort").tookTrue();
        };
        check(countExpress(true) == countExpress(false),
              "a text condition routes IDENTICALLY to the equivalent lambda");
        check(countExpress(true) > 0, "the condition actually fired");

        // A malformed condition is a programmer error on the C++ path.
        {
            SimulationSystem sim(1u);
            bool threw = false;
            try { sim.model().decideByCondition("Bad", "priority >"); }
            catch (const ModelError&) { threw = true; }
            check(threw, "a malformed condition text throws on the C++ API");
        }

        // A condition reading model state.
        {
            SimulationSystem sim(7u);
            sim.model().arrivals(constant(1.0))
                       .station("Busy", 1, FIFO, constant(2.0))
                       .decideByCondition("Full", "NQ(Busy) > 2")
                       .station("Overflow", 1, FIFO, constant(0.1))
                       .routeTrue("Full", "Overflow")
                       .route("Full", "Busy")
                       .entryAt("Full");
            sim.stopAt(50.0).initialise();
            sim.run();
            check(sim.model().nodeAs<DecideNode>("Full").tookTrue() > 0,
                  "NQ() in a condition sees the queue actually filling");
        }
    }
```

- [ ] **Step 2: Run it and confirm it fails**

Expected: no `decideByCondition` overload taking a `const char*`/`std::string`.

- [ ] **Step 3: Change `DecideNode::Branch`**

In `include/Nodes.hpp`, replace `Condition condition;` with `ExpressionPtr condition;`. Keep the `using Condition = std::function<bool(const Entity&)>;` alias — the old overload still takes it.

Add the new overload beside the existing one:

```cpp
    DecideNode& addBranch(double probability, INode* target);
    DecideNode& addBranch(Condition condition, INode* target);      // wraps in a LambdaExpression
    DecideNode& addBranch(ExpressionPtr condition, INode* target);  // v10
```

> `Branch` now holds a `unique_ptr`, so it is move-only. `std::vector<Branch>` is fine, but any code doing `Branch b = m_branches[i];` will stop compiling. Change those to references. Do **not** add a copy constructor.

- [ ] **Step 4: Implement in `src/Nodes.cpp`**

```cpp
DecideNode& DecideNode::addBranch(Condition condition, INode* target) {
    // The old API becomes a CONSTRUCTOR for the new representation, not a
    // second code path kept alive beside it. There is one evaluation route.
    return addBranch(std::make_unique<LambdaExpression>(std::move(condition)), target);
}

DecideNode& DecideNode::addBranch(ExpressionPtr condition, INode* target) {
    if (m_byChance)
        throw ModelError("Decide '" + m_name + "' is a by-chance block; it cannot take a "
                         "condition branch. A Decide is all-chance or all-condition.");
    Branch b;
    b.probability = -1.0;
    b.condition = std::move(condition);
    b.target = target;
    m_branches.push_back(std::move(b));
    return *this;
}
```

In `enter()`, replace the predicate call with an evaluation. Everything else about the block — order of branches, first match wins, fall-through counting — stays exactly as it is:

```cpp
        EvalContext ectx = ctx.evaluationContext(e);
        for (Branch& b : m_branches) {
            if (truthy(b.condition->evaluate(ectx))) {
                ++b.taken;
                ctx.route(e, b.target);
                return;
            }
        }
```

- [ ] **Step 5: Add the `Model` string overloads**

In `include/Model.hpp` and `src/Model.cpp`:

```cpp
Model& Model::decideByCondition(const std::string& name, const std::string& conditionText) {
    // expr() throws on a bad expression: on the C++ API a malformed condition
    // IS a programmer error and there is no cell to point at. v11's compile()
    // uses parseExpression() instead and collects diagnostics.
    return decideByCondition(name, expr(conditionText));
}

Model& Model::branch(const std::string& decideName, const std::string& conditionText,
                     const std::string& to) {
    return branch(decideName, expr(conditionText), to);
}
```

> `decideByCondition(name, ExpressionPtr)` and `branch(name, ExpressionPtr, to)` need adding too, mirroring the existing `Condition` versions exactly. Watch for overload ambiguity: `decideByCondition("X", "text")` with both a `Condition` and a `std::string` overload can be ambiguous for a string literal. If it is, take `const char*` explicitly on the text overload.

- [ ] **Step 6: Run the tests and the trace check**

```bash
cmake --build build && ./build/des_tests | tail -3
bash tools/baseline.sh check
```

Expected: `399 / 399 checks passed`, no `DIFFERS` lines.

- [ ] **Step 7: Commit**

```bash
git add include/Nodes.hpp src/Nodes.cpp include/Model.hpp src/Model.cpp tests/expression_tests.cpp
git commit -m "feat: Decide conditions are expressions

The std::function overload survives as a LambdaExpression adapter, so
there is one evaluation path rather than two kept in parallel."
```

---

### Task 9: Assign gains target kinds and expressions

**Files:**
- Modify: `include/Nodes.hpp`, `src/Nodes.cpp` (`AssignNode`)
- Modify: `include/Model.hpp`, `src/Model.cpp`
- Test: `tests/expression_tests.cpp`

**Interfaces:**
- Produces:
  - `enum class des::AssignTarget { Attribute, Variable, EntityType }`
  - `AssignNode::Rule { AssignTarget target; std::string name; ExpressionPtr value; }`
  - `AssignNode& set(AssignTarget, const std::string& name, ExpressionPtr value)`
  - `Model& Model::assign(const std::string& block, const std::string& attribute, const std::string& valueText)`
  - `Model& Model::assignVariable(const std::string& block, const std::string& variable, const std::string& valueText)`
  - `Model& Model::assignEntityType(const std::string& block, const std::string& typeText)`

- [ ] **Step 1: Write the failing test**

```cpp
    section("Assign");
    {
        // WIP = WIP + 1 -- an expression with a global on BOTH sides. This is
        // the thing attributes cannot express, and the reason variables exist.
        SimulationSystem sim(99u);
        sim.model().variable("Counted", 0.0)
                   .arrivals(constant(1.0))
                   .assignVariable("Count", "Counted", "Counted + 1")
                   .station("Work", 1, FIFO, constant(0.5))
                   .route("Count", "Work")
                   .entryAt("Count");
        sim.stopAt(10.5).initialise();
        sim.run();
        checkClose(sim.model().variables().get("Counted"), 11.0, 1e-12,
                   "the variable counted every arrival");

        // An attribute assignment from an expression over another attribute.
        SimulationSystem s2(5u);
        s2.model().attribute("base", constant(2.0))
                  .arrivals(constant(1.0))
                  .assign("Double", "doubled", "base * 3")
                  .record("Seen")
                  .recordAttribute("Seen", "doubled")
                  .route("Double", "Seen")
                  .entryAt("Double");
        s2.stopAt(5.0).initialise();
        s2.run();
        checkClose(s2.model().nodeAs<RecordNode>("Seen").average(), 6.0, 1e-9,
                   "the assigned attribute is base * 3");

        // Assigning a string to a variable is refused -- variables are numeric.
        {
            SimulationSystem s3(1u);
            s3.model().variable("V", 0.0).arrivals(constant(1.0));
            bool threw = false;
            try { s3.model().assignVariable("A", "V", "\"text\""); s3.initialise(); s3.run(); }
            catch (const ModelError&) { threw = true; }
            check(threw, "a string assigned to a numeric variable is refused");
        }
    }
```

> `RecordNode::average()` — check the real accessor name in `include/Nodes.hpp` and use it. If Record exposes the tally differently, assert on whatever it does expose.

- [ ] **Step 2: Run it and confirm it fails**

Expected: `'class des::Model' has no member named 'assignVariable'`.

- [ ] **Step 3: Change `AssignNode`**

In `include/Nodes.hpp`:

```cpp
// v10: Arena's Assign sets an attribute, a VARIABLE, or the entity's TYPE.
// Through v9 this block could only write attributes, which is why "how many
// have we served" could not be said at all.
enum class AssignTarget { Attribute, Variable, EntityType };

class AssignNode : public INode {
public:
    struct Rule {
        AssignTarget  target{AssignTarget::Attribute};
        std::string   name;      // empty for EntityType
        ExpressionPtr value;
    };
    ...
    AssignNode& set(const std::string& attribute, std::unique_ptr<IDistribution> value); // old
    AssignNode& set(AssignTarget target, const std::string& name, ExpressionPtr value);  // v10
```

- [ ] **Step 4: Implement in `src/Nodes.cpp`**

```cpp
AssignNode& AssignNode::set(const std::string& attribute, std::unique_ptr<IDistribution> value) {
    // Old API -> new representation. One code path.
    return set(AssignTarget::Attribute, attribute,
               std::make_unique<DistributionExpression>(std::move(value)));
}

AssignNode& AssignNode::set(AssignTarget target, const std::string& name, ExpressionPtr value) {
    Rule r;
    r.target = target;
    r.name = name;
    r.value = std::move(value);
    m_rules.push_back(std::move(r));
    return *this;
}

void AssignNode::enter(NodeContext& ctx, Entity* e) {
    EvalContext ectx = ctx.evaluationContext(e);
    for (const Rule& r : m_rules) {
        const Value v = r.value->evaluate(ectx);
        switch (r.target) {
            case AssignTarget::Attribute:
                e->setAttribute(r.name, asNumber(v));
                break;
            case AssignTarget::Variable:
                // A variable is numeric: asNumber throws on text rather than
                // storing something that later reads as zero.
                ctx.variables().set(r.name, asNumber(v), ctx.now());
                break;
            case AssignTarget::EntityType:
                e->setType(isText(v) ? asText(v) : formatValue(v));
                break;
        }
    }
    ++m_count;
    ctx.route(e, m_next);
}
```

> This needs `NodeContext::variables()` returning `VariableStore&`. Add it beside `evaluationContext` in Task 7's pattern. Also confirm `Entity::setType` exists; if entity type is immutable, drop `AssignTarget::EntityType` from this task, note it in the readlog, and remove its test.

- [ ] **Step 5: Add the `Model` overloads**

```cpp
Model& Model::assign(const std::string& block, const std::string& attribute,
                     const std::string& valueText) {
    assignBlock(block).set(AssignTarget::Attribute, attribute, expr(valueText));
    return *this;
}
Model& Model::assignVariable(const std::string& block, const std::string& variable,
                             const std::string& valueText) {
    assignBlock(block).set(AssignTarget::Variable, variable, expr(valueText));
    return *this;
}
Model& Model::assignEntityType(const std::string& block, const std::string& typeText) {
    assignBlock(block).set(AssignTarget::EntityType, "", expr(typeText));
    return *this;
}
```

where `assignBlock(name)` is the existing "find-or-create the AssignNode" helper inside `Model::assign` — extract it to a private method rather than repeating it three times.

- [ ] **Step 6: Run the tests and the trace check**

```bash
cmake --build build && ./build/des_tests | tail -3
bash tools/baseline.sh check
```

Expected: `403 / 403 checks passed`, no `DIFFERS` lines.

- [ ] **Step 7: Commit**

```bash
git add include/Nodes.hpp src/Nodes.cpp include/Model.hpp src/Model.cpp include/Node.hpp src/Node.cpp tests/expression_tests.cpp
git commit -m "feat: Assign writes attributes, variables or entity type, from expressions

WIP = WIP + 1 is now sayable -- an expression with a global on both
sides, which is what attributes structurally cannot express."
```

---

### Task 10: Durations become expressions, and every call site gets its own stream

The largest task, and the one that finishes the collapse: after this there is no "distribution field" left in the engine.

**Files:**
- Modify: `include/Station.hpp`, `src/Station.cpp` (service time)
- Modify: `include/Nodes.hpp`, `src/Nodes.cpp` (`DelayNode` duration; `SeparateNode`/`BatchNode` untouched)
- Modify: `include/Create.hpp`, `src/Create.cpp` (interarrival)
- Modify: `include/Model.hpp`, `src/Model.cpp` (string overloads for `station`, `stationUsing`, `delay`, `source`, `arrivals`, `renegeAfter`)
- Test: `tests/expression_tests.cpp`

**Interfaces:**
- Produces: string-taking overloads of `station`, `stationUsing`, `delay`, `arrivals`, `source`, `renegeAfter`; `Station::serviceExpression()`, `DelayNode::durationExpression()`, `CreateNode::interarrivalExpression()` returning `const IExpression&`.

- [ ] **Step 1: Write the failing test**

```cpp
    section("Durations as expressions");
    {
        // EXPO(0.8) parsed must give the IDENTICAL sequence to exponential(0.8)
        // constructed. Same seed, same draws -- the distributions are the same
        // objects underneath.
        auto wait = [](bool useText) {
            SimulationSystem sim(31337u);
            Model& m = sim.model();
            if (useText) m.arrivals("EXPO(1.0)").station("T", 1, FIFO, "EXPO(0.8)");
            else         m.arrivals(exponential(1.0)).station("T", 1, FIFO, exponential(0.8));
            m.entryAt("T");
            sim.stopAt(500.0).initialise();
            sim.run();
            return sim.model().station("T")->stats().averageWaitingTime();
        };
        checkClose(wait(true), wait(false), 1e-12,
                   "a parsed EXPO draws IDENTICALLY to a constructed one");

        // A service time that depends on the entity -- impossible before v10.
        {
            SimulationSystem sim(11u);
            sim.model().attribute("size", constant(3.0))
                       .arrivals(constant(5.0))
                       .station("Cut", 1, FIFO, "size * 0.5")
                       .entryAt("Cut");
            sim.stopAt(50.0).initialise();
            sim.run();
            checkClose(sim.model().station("Cut")->stats().averageServiceTime(), 1.5, 1e-9,
                       "service time computed from an entity attribute");
        }

        // An interarrival expression has NO entity: an attribute there must be
        // caught before the run, not throw halfway through it.
        {
            SimulationSystem sim(1u);
            bool threw = false;
            try {
                sim.model().attribute("size", constant(1.0))
                           .arrivals("size * 2")
                           .station("X", 1, FIFO, constant(1.0))
                           .entryAt("X");
                sim.initialise();
            } catch (const ModelError&) { threw = true; }
            check(threw, "an attribute in an interarrival field is refused at initialise()");
        }

        // v9 open item #5: two blocks' draws are now independent, because they
        // are separate AST nodes rather than one shared code path.
        {
            auto serviceAt = [](double delayDuration) {
                SimulationSystem sim(777u);
                sim.model().arrivals("EXPO(1.0)")
                           .delay("Move", constant(delayDuration))
                           .station("T", 1, FIFO, "EXPO(0.8)")
                           .route("Move", "T")
                           .entryAt("Move");
                sim.stopAt(300.0).initialise();
                sim.run();
                return sim.model().station("T")->stats().averageServiceTime();
            };
            checkClose(serviceAt(1.0), serviceAt(2.0), 1e-12,
                       "changing a Delay's duration does not shift the service stream");
        }
    }
```

> `averageServiceTime()` — confirm the accessor on `Statistics`. If absent, assert on `averageWaitingTime()` or utilisation instead; the point of the last block is only that the two numbers are *equal*.

- [ ] **Step 2: Run it and confirm it fails**

Expected: no `station` overload taking a string for the service argument.

- [ ] **Step 3: Convert the three blocks**

In each of `Station`, `DelayNode`, `CreateNode`: replace the `std::unique_ptr<IDistribution> m_service` (or `m_duration`, `m_interarrival`) member with `ExpressionPtr`, and replace the `->draw(rng)` call with:

```cpp
    EvalContext ectx = ctx.evaluationContext(e);      // e is nullptr in CreateNode
    const SimTime duration = static_cast<SimTime>(asNumber(m_service->evaluate(ectx)));
```

Keep every existing constructor: each wraps its `IDistribution` in a `DistributionExpression`.

**`CreateNode` is the one to be careful with.** Its interarrival is evaluated with **no entity**, so `evaluationContext(nullptr)`. Preserve v9's ordering exactly: it schedules its *next* arrival **before** routing the current entity, so the stream cannot depend on what happens downstream.

- [ ] **Step 4: Validate before the run**

In `Model::validate()`, walk every block and call `validate()` on its expression with the right `FieldContext`:

```cpp
    ValidationContext vc;
    vc.variables = &m_variables;
    for (const ArrivalAttribute& a : m_arrivalAttributes) vc.attributeNames.push_back(a.name);
    // Attributes written by an Assign block count as declared too.
    for (const std::unique_ptr<INode>& n : m_nodes)
        if (const auto* a = dynamic_cast<const AssignNode*>(n.get()))
            for (const AssignNode::Rule& r : a->rules())
                if (r.target == AssignTarget::Attribute) vc.attributeNames.push_back(r.name);

    std::vector<Diagnostic> problems;
    // A Create's interarrival has NO entity; every other field has one.
    for (const std::unique_ptr<INode>& n : m_nodes) n->validateExpressions(vc, problems);
    if (hasErrors(problems))
        throw ModelError("this model has expression errors:\n" + formatDiagnostics(problems));
```

Add `virtual void INode::validateExpressions(const ValidationContext&, std::vector<Diagnostic>&) const {}` to `include/Node.hpp`, overridden by the blocks that hold expressions. `CreateNode`'s override sets `vc.field = FieldContext::NoEntity` on its own copy before descending.

> This is the v11 hook: `compile()` will call the same `validate()` and turn each `Diagnostic` into a row/column reference. Keep the diagnostics *returned as a list* internally even though `validate()` throws at the end.

- [ ] **Step 5: Give each block its own stream**

Wherever v8's per-distribution stream assignment happens (`grep -n "useStream" src/*.cpp`), call `expression->useStream(...)` instead. `IExpression::useStream` recurses into every sampling site beneath it, so a block with two distribution calls gets both on its stream.

- [ ] **Step 6: Add the `Model` string overloads**

```cpp
Model& Model::station(const std::string& name, int capacity,
                      QueueDiscipline discipline, const std::string& serviceText) {
    return station(name, capacity, discipline, expr(serviceText));
}
```

…and the same shape for `stationUsing`, `delay`, `arrivals`, `source`, `renegeAfter`. Each is one line delegating to an `ExpressionPtr` overload.

- [ ] **Step 7: Run everything**

```bash
cmake --build build && ./build/des_tests | tail -3
bash tools/baseline.sh check
bash tools/verify.sh
```

Expected: `410 / 410 checks passed`, no `DIFFERS`, `ASAN CLEAN`.

- [ ] **Step 8: Commit**

```bash
git add include/ src/ tests/expression_tests.cpp
git commit -m "feat: service, delay and interarrival fields are expressions

There is no distribution field left in this engine. Every duration is an
expression, and a distribution is a function in the grammar that samples
-- which also gives every call site its own stream, finishing v9 item 5."
```

---

### Task 11: The stability check learns to say it does not know

**Files:**
- Modify: `include/Model.hpp`, `src/Model.cpp` (`offeredLoad`, `VisitRatios`)
- Modify: `src/SimulationSystem.cpp` (the report line)
- Test: `tests/expression_tests.cpp`

**Interfaces:**
- Produces: `double INode::loadPerVisit() const` unchanged in signature, plus `bool INode::loadIsKnown() const`; `Model::StabilityReport { bool checked; std::vector<std::string> unverifiable; }`

- [ ] **Step 1: Write the failing test**

```cpp
    section("Stability check with unknowable means");
    {
        // A constant-argument distribution keeps a knowable mean, so the
        // overload refusal still fires exactly as it did in v9.
        {
            SimulationSystem sim(1u);
            bool threw = false;
            try {
                sim.model().arrivals("EXPO(1.0)")
                           .station("Slow", 1, FIFO, "EXPO(2.0)")
                           .entryAt("Slow");
                sim.initialise();
            } catch (const ModelError&) { threw = true; }
            check(threw, "rho >= 1 is still refused when the mean is knowable");
        }

        // A computed service time has NO knowable mean. The engine must say
        // so -- not pass silently, and not refuse a model it cannot judge.
        {
            SimulationSystem sim(2u);
            sim.model().variable("Rate", 2.0)
                       .arrivals("EXPO(1.0)")
                       .station("Var", 1, FIFO, "EXPO(Rate)")
                       .entryAt("Var");
            sim.initialise();                       // must NOT throw
            Model::StabilityReport r = sim.model().stability();
            check(!r.checked, "a computed mean makes the check inconclusive");
            check(r.unverifiable.size() == 1 && r.unverifiable[0] == "Var",
                  "the report names the block it could not verify");
        }

        // meanIfKnown, all four rows of the spec's table.
        check(expr("5")->meanIfKnown().has_value(),           "a bare number has a known mean");
        checkClose(*expr("EXPO(0.8)")->meanIfKnown(), 0.8, 1e-12, "EXPO(0.8) mean is 0.8");
        check(!expr("NQ(X) * 2")->meanIfKnown().has_value(),  "model state has no known mean");
    }
```

- [ ] **Step 2: Run it and confirm it fails**

Expected: `'StabilityReport' is not a member of 'des::Model'`.

- [ ] **Step 3: Implement**

`INode::loadPerVisit()` currently returns `0.0` for "not checkable", which the header already admits means "not checkable, not definitely fine". Make that explicit:

```cpp
    // v10: an expression's mean may be UNKNOWABLE -- EXPO(Rate) depends on a
    // variable that changes during the run. Returning 0 would let an unstable
    // model through a check that looked like it passed. false means "ask me
    // nothing further about this block".
    virtual bool loadIsKnown() const { return true; }
```

`Station::loadIsKnown()` returns `m_service->meanIfKnown().has_value()`.

In `Model`:

```cpp
struct StabilityReport {
    bool                     checked{true};   // false if any block was unverifiable
    std::vector<std::string> unverifiable;    // block names
    double                   maxUtilisation{0.0};
};
StabilityReport stability() const;
```

`validate()` calls `stability()` and:
- refuses (as v9 did) when a **known** ρ ≥ 1 and `allowOverload()` is off;
- **never refuses** on an unverifiable block — it cannot judge, so refusing would reject good models;
- records the names.

In the report, print alongside the existing `VisitRatios::exact` line:

```
*** STABILITY NOT VERIFIED for: Var
*** These blocks have service times whose mean cannot be computed in
*** advance, so the offered-load check could not be applied to them.
```

- [ ] **Step 4: Run the tests and the trace check**

```bash
cmake --build build && ./build/des_tests | tail -3
bash tools/baseline.sh check
```

Expected: `416 / 416 checks passed`, no `DIFFERS` lines — no existing example uses a computed mean, so none should gain the new warning.

- [ ] **Step 5: Commit**

```bash
git add include/ src/ tests/expression_tests.cpp
git commit -m "feat: the stability check reports what it could not verify

VisitRatios::exact a second time: say you do not know rather than guess,
because a guessed check is a check that passes when it should not."
```

---

### Task 12: The decisive test — every example, both ways

Everything up to here proves the pieces. This proves the **whole**.

**Files:**
- Create: `examples/16_expressions.cpp`
- Modify: `tests/expression_tests.cpp`
- Test: the trace comparison itself

- [ ] **Step 1: Re-express the examples' lambdas and distributions as text**

For each of `examples/01`–`15`, find every `exponential(...)`, `triangular(...)`, `constant(...)`, `uniform(...)` argument and every `[](const Entity& e){...}` condition. Add — **do not replace** — a second build of the same model using text, and assert the two agree.

Add to `tests/expression_tests.cpp` one function per example that has a condition or a distribution, of this shape:

```cpp
    section("Text and code agree, example by example");
    {
        // The argument this project already makes with trace diffing: if the
        // text path and the code path agree EVENT FOR EVENT, the text path is
        // correct. A near-miss average would not be evidence; an identical
        // trace is.
        auto runBoth = [](bool useText, const char* tracePath) {
            SimulationSystem sim(20250827u);
            Model& m = sim.model();
            if (useText) {
                m.arrivals("EXPO(1.0)")
                 .station("Teller", 2, FIFO, "TRIA(0.5, 1.0, 2.5)")
                 .decideByCondition("Recheck", "NQ(Teller) > 3")
                 .station("Extra", 1, FIFO, "EXPO(0.4)")
                 .routeTrue("Recheck", "Extra")
                 .route("Teller", "Recheck")
                 .entryAt("Teller");
            } else {
                m.arrivals(exponential(1.0))
                 .station("Teller", 2, FIFO, triangular(0.5, 1.0, 2.5))
                 .decideByCondition("Recheck", ExpressionPtr(nullptr))   // see below
                 .station("Extra", 1, FIFO, exponential(0.4))
                 .routeTrue("Recheck", "Extra")
                 .route("Teller", "Recheck")
                 .entryAt("Teller");
            }
            sim.enableTrace(tracePath, TraceLevel::Events);
            sim.stopAt(200.0).initialise();
            sim.run();
        };
        runBoth(true,  "trace_text.md");
        runBoth(false, "trace_code.md");
        check(filesAreIdentical("trace_text.md", "trace_code.md"),
              "the text-built model traces IDENTICALLY to the code-built one");
    }
```

> The `NQ(Teller) > 3` condition has no lambda equivalent before v10 (a lambda cannot see the queue). For the code side, use a lambda over an **attribute** and give the text side the same attribute condition — the point is that the two *paths* agree, so both sides must express the same predicate. Pick a condition both can state.

Add a small helper next to the harness:

```cpp
bool filesAreIdentical(const std::string& a, const std::string& b) {
    std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
    if (!fa || !fb) return false;
    return std::string(std::istreambuf_iterator<char>(fa), {}) ==
           std::string(std::istreambuf_iterator<char>(fb), {});
}
```

- [ ] **Step 2: Write `examples/16_expressions.cpp`**

A runnable program in the style of the other fifteen, showing what v10 bought: a variable counting throughput, a service time computed from an attribute, a Decide reading `NQ()`, and a duration written as `TRIA(1,2,3)`. Follow the header-comment style of `examples/11_flowchart_line.cpp`.

- [ ] **Step 3: Run everything**

```bash
cmake --build build && ./build/des_tests | tail -3
./build/example_16_expressions | head -30
bash tools/baseline.sh check
bash tools/verify.sh
```

Expected: all checks pass, example 16 runs, **no `DIFFERS` for examples 01–15**, `ASAN CLEAN`.

- [ ] **Step 4: Commit**

```bash
git add examples/16_expressions.cpp tests/expression_tests.cpp
git commit -m "test: the text path and the code path trace identically

Fifteen examples, event for event. A near-miss average would not be
evidence; an identical trace is."
```

---

### Task 13: The documents

This project's versions are arguments, and the readlog is where the argument lives. Do not skip this.

**Files:**
- Create: `V10_READLOG.md`
- Modify: `CHANGELOG.md`, `README.md`, `ARENA_MAP.md`, `examples/README.md`

- [ ] **Step 1: Write `V10_READLOG.md`**

Follow the shape of `V9_READLOG.md`: what the version demanded, then numbered sections, then "The answers" and "Verified". Cover at minimum:

1. **Why a parser, and why it subsumed distributions.** There is no distribution field any more. A duration cell holds an expression; `EXPO(0.8)` is a function call. Arena has always worked this way and it collapses a whole category.
2. **`EvalContext` is `NodeContext` again.** Second time the same dependency problem appeared and the same fix worked: publish a role-specific interface rather than widening a class. Worth recording that the *pattern* transferred, not just the code.
3. **`meanIfKnown` and saying you don't know.** Third time this project has chosen "report that it cannot tell" over a plausible default — after `VisitRatios::exact` and the v9 silently-zero WIP. Note that a guessed stability check is worse than no check.
4. **Errors as data, not exceptions.** Why the parser recovers, and why `expr()` throws while `parseExpression()` does not — the C++ caller is a programmer, the v11 caller is a person typing in a cell.
5. **One namespace for variables and attributes.** Why a resolution order was refused.
6. **v9 item #5 solved by construction.** Delay and Decide draws shared a stream because they shared a code path. Separate AST nodes, separate streams, no feature written for it.
7. **The adapters.** How 293 tests survived a change this large: the old API became a constructor for the new representation, never a second code path.
8. Anything that went wrong while building it. **The bugs are the most valuable part of these documents** — v9's readlog is worth reading because of section 7, not section 1.

- [ ] **Step 2: Update `CHANGELOG.md`**

Add a `v10` section in the established style, at the top.

- [ ] **Step 3: Update `README.md`**

- Change the "Current state" paragraph to v10 and mention expressions and variables.
- Add an expressions section after "Writing a model", with the grammar and the function table.
- Update the check count.
- In **"Known / still open"**, remove item #5 from the v9 list (streams for every block — now solved) and note that the remaining items are unchanged.
- Add `V10_READLOG.md` to the documents table.
- Add `Variable` to the theory table.

- [ ] **Step 4: Update `ARENA_MAP.md`**

- Add rows for **Variable** (data module), `Assign` to a Variable, and the expression syntax in Delay/Service fields.
- Add a row mapping Arena's expression functions to this engine's.
- In **"Three differences worth knowing"**, add a fourth if one emerged, and note that `Empirical`/`Deterministic` have no Arena spelling.

- [ ] **Step 5: Update `examples/README.md`**

Add example 16 to the table, and add an expressions section to the API reference with the grammar, the function list, and the gotchas: `EXPO` vs `EXP`; a variable may not share a name with an attribute; `NQ()` takes a block name, not a value; an interarrival field has no entity.

- [ ] **Step 6: Final verification**

```bash
rm -rf build && cmake -S . -B build && cmake --build build 2>&1 | grep -i "warning" | head
./build/des_tests | tail -3
cd build && ctest --output-on-failure | tail -5
```

Expected: **no warnings**, all checks pass, ctest green.

- [ ] **Step 7: Commit**

```bash
git add V10_READLOG.md CHANGELOG.md README.md ARENA_MAP.md examples/README.md
git commit -m "v10: the model stops needing a compiler"
```

---

## Self-review notes

**Spec coverage.** Grammar → Task 5. `Value` → Task 2. Distributions as calls → Task 5. Constant vs computed arguments → Task 5. `meanIfKnown` and the ρ check → Tasks 4 and 11. Variables → Task 6, wired in Task 7. One namespace → Task 6. Errors as data → Tasks 2 and 5. Static validation and `FieldContext` → Tasks 4 and 10. Nothing existing breaks → the adapters in Task 4, exercised in Tasks 8–10, gated by the trace check in every task from 7 onward. Per-node streams → Task 10. The decisive test → Task 12. Docs → Task 13.

**Two things this plan does not fully specify, deliberately.** Tasks 7, 9 and 10 touch `SimulationSystem`, `Station`, `Create` and `Nodes`, and each begins by telling the implementer to read the real code and match its member and accessor names rather than trusting names written here. Fabricating exact line edits for files at this depth would be worse than useless — it would look authoritative and be wrong. The *requirements* in those tasks are exact; the *spellings* must be checked.

**The check counts** (`294`, `304`, `320`, …) are cumulative predictions based on the tests written in each task. If a count is off by a few because a test was split differently, that is fine. If it is off because a test **failed**, it is not.
