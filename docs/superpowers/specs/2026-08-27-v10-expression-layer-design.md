# v10 — the expression layer

**Status:** design approved, not yet implemented.
**Date:** 2026-08-27

---

## The arc this belongs to

Three versions carry the engine from "a C++ library you write models in" to "a
library a terminal front-end can drive". The front-end itself — `des-tui` — is a
**separate repository** that links this one. This document specifies **v10 only**;
v11 and v12 get their own specs, written when v10 is done and its details are
known rather than guessed.

| Version | Theme | One-line summary |
|---|---|---|
| **v10** | The expression layer | A model's conditions, values and durations become **text**, not compiled code. |
| v11 | The model as a document | Module schemas, a `ModelDocument` of typed cells, `compile()` → `Model` \| diagnostics, a file format. |
| v12 | Runtime control | A run a front-end can start, watch, pause and cancel; a regression harness. |

Decisions already taken that constrain v10:

- The TUI is a **separate repo**, and the engine keeps versioning after it
  exists. New engine versions update the TUI, not the other way round.
- Compatibility is **source-level, not ABI**. Schemas are discovered at runtime
  so new module types render in an unmodified front-end; the two are rebuilt
  together. **Model files are the durable artifact** and migrate forward forever.
- Data modules in scope across the arc: **Variable, Entity, Queue, Resource,
  Expression**. Set and Schedule are deliberately deferred — they change
  simulation semantics, and they are far easier to build *through* a spreadsheet
  than alongside one.

## Why this version exists

Today a model is built by calling C++ methods, and the two most interesting
fields hold **code**:

```cpp
m.decideByCondition("Inspect", [](const Entity& e){ return e.attribute("defects") > 2; });
m.station("Teller", 1, FIFO, exponential(0.8));
```

A person typing into a spreadsheet cell can produce `defects > 2` and
`EXPO(0.8)`. Nothing in the engine can turn those strings into behaviour. Every
later feature is blocked behind that one fact:

- A spreadsheet can only edit a model that exists **as data**.
- A front-end can only build a model **without invoking a compiler**.
- Arena **Variables** are read and written by expressions (`WIP = WIP + 1`), so
  they cannot be added meaningfully before expressions exist.
- Serialising a model is currently impossible: `std::function` and
  `unique_ptr<IDistribution>` members are why config-file input has sat unbuilt
  since v5.

v10 removes that block, and nothing else.

## Architecture

Four layers, each depending only downward.

```
  ┌─────────────────────────────────────────────────────────┐
  │  des-tui        SEPARATE REPO, links libdes             │
  └─────────────────────────────────────────────────────────┘
  ═══════════════════════ repo boundary ═══════════════════════
  ┌─────────────────────────────────────────────────────────┐
  │  v12  Runtime control                                   │
  ├─────────────────────────────────────────────────────────┤
  │  v11  Document layer                                    │
  ├─────────────────────────────────────────────────────────┤
  │  v1–v9  Engine core                     (grows in v10)  │
  ├─────────────────────────────────────────────────────────┤
  │  v10  Expression layer                                  │
  │       Lexer · Parser · IExpression · Value              │
  │       EvalContext · VariableStore · Diagnostic          │
  └─────────────────────────────────────────────────────────┘
```

### Why the expression layer sits *below* the core

`DecideNode` must hold an expression, so the core depends on the expression
layer. But `NQ(Teller)` needs live model state, which would point the dependency
back up.

The fix is the one this project already made in v6. **`EvalContext` is to
expressions what `NodeContext` is to nodes:** the expression layer *defines* a
narrow interface for the state it needs, and the engine *implements* it. The
expression layer never includes `Model.hpp`.

```cpp
// Defined in the expression layer. Implemented by SimulationSystem.
class IModelState {
public:
    virtual ~IModelState() = default;
    virtual double queueLength(const std::string& name) const = 0;
    virtual double resourceBusy(const std::string& name) const = 0;
    virtual double resourceCapacity(const std::string& name) const = 0;
    virtual double numberInSystem() const = 0;
    virtual SimTime now() const = 0;
};
```

Second use of the same pattern, in the same project, for the same reason:
publish a role-specific interface rather than widening a class.

## The grammar

One grammar covers **conditions**, **assignment right-hand sides**, and **every
duration field**.

```
expression     := or
or             := and          { "||" and }
and            := comparison   { "&&" comparison }
comparison     := additive     { ("=="|"!="|"<"|"<="|">"|">=") additive }
additive       := multiplicative { ("+"|"-") multiplicative }
multiplicative := unary        { ("*"|"/"|"%") unary }
unary          := ("-"|"!") unary | power
power          := primary [ "^" unary ]              // right-associative
primary        := number | string | identifier | call | "(" expression ")"
call           := identifier "(" [ expression { "," expression } ] ")"
identifier     := letter { letter | digit | "_" | "." }
```

Recursive descent with a precedence climb. C-style operators only; Arena's
`.LT.`/`.EQ.` spellings are a token-table alias if they are ever wanted, and are
**not** in scope for v10.

**A dot is an identifier character, not an operator.** `Entity.Type` is one
token, and so is a future `Teller.Queue`. This buys Arena's dotted names with no
grammar rule and no member-access node; the cost is that a stray dot lands inside
an identifier and is reported as an unknown name rather than as a syntax error.
That is the better error of the two, because the dotted form is far more common
than a mistyped operator.

**Identifiers are resolved late, never by the parser.** The parser emits a
`NameReference` node holding a string. What that string *is* — an entity
attribute, a global variable, or a built-in constant — is decided by
`validate()` against a `ValidationContext`, and read by `evaluate()` against an
`EvalContext`. The parser has no name tables and therefore cannot go stale when
v11 adds a Variable data module.

`TNOW` is a **bare identifier**, not a call: it is written `TNOW`, not `TNOW()`,
and resolves to the clock. It is the only built-in constant in v10.

### Values

```cpp
using Value = std::variant<double, std::string>;
```

Strings exist for exactly one reason: `Entity.Type == "Ball"`. Arithmetic on a
string produces a diagnostic, never a coerced number.

**Booleans are doubles**, `0` and `1`, as Arena treats them. `&&` and `||` need
no separate type, and a condition field and a value field are the same kind of
expression.

## Distributions are function calls

There is **no separate distribution field**. Arena's Delay cell holds an
expression: `5` is constant, `TRIA(1,2,3)` samples each time, `SetupTime * 2`
computes. v10 adopts that exactly.

A `DistributionCall` AST node **owns an `IDistribution`** and calls `draw()` when
evaluated. Nothing is reimplemented — `IDistribution` already has `draw()`,
`mean()`, `clone()` and `useStream()`.

Grammar functions, matching the twelve distributions already implemented:

| Category | Functions |
|---|---|
| Distributions | `EXPO` `UNIF` `TRIA` `NORM` `LOGN` `WEIB` `ERLA` `POIS` `DISC` `CONS` |
| Model state | `NQ(name)` `NR(name)` `MR(name)` `WIP()`, and the bare constant `TNOW` |
| Maths | `MIN` `MAX` `ABS` `ROUND` `TRUNC` `SQRT` `LN` `EXP` `MOD` |

`EXPO` is the exponential *distribution*; `EXP` is e^x. That collision is
Arena's and is kept, because a model written against Arena's names should read
the same here.

**The state functions take a bare block name, not a value.** `NQ(Teller)` means
*the queue at the block called Teller*, so `Teller` must not be resolved as a
variable. These four functions are the one place the parser special-cases an
argument: a lone `NameReference` argument is captured as a **literal name
string** rather than evaluated. Writing `NQ("Teller")` is accepted and
identical; writing `NQ(x + 1)` is a diagnostic, because there is no such thing as
a computed block reference in v10.

`Empirical` and `Deterministic` have no Arena spelling and stay
programmatic-only in v10.

### Constant vs computed arguments

- **Constant arguments** (`EXPO(0.8)`): the `IDistribution` is built **once** at
  parse time. Cheap, and its `mean()` is known.
- **Computed arguments** (`EXPO(MeanTime)`): the distribution is built **per
  evaluation**, because the parameter may differ each time. Its mean is not
  knowable in advance.

Either way the AST node owns the stream pointer and applies it to whichever
instance it builds, so `useStream()` keeps working in both cases.

### A v9 open item falls out for free

Item #5 on the v9 list — *"streams for every block, finishing what v8 started;
Delay durations and Decide draws still share the common stream"* — is solved by
construction. Every distribution call site is now a **distinct AST node**, so
giving each its own stream is a per-node `useStream()` rather than a code change.
They shared a stream because they shared a code path; they no longer do.

## The stability check, honestly

`Model::offeredLoad()` calls `dist->mean()`. An arbitrary expression has no
knowable mean, so:

```cpp
virtual std::optional<double> meanIfKnown() const;
```

| Expression | Result |
|---|---|
| `5` | `5.0` |
| `EXPO(0.8)` | `0.8` |
| `EXPO(MeanTime)` | `nullopt` |
| `NQ(Teller) * 2` | `nullopt` |

When it is `nullopt` the ρ check reports **"could not verify"** and names the
block. It does not guess, and it does not silently pass.

This is `VisitRatios::exact` a second time — the same "say you don't know"
decision, appearing in the same report. The two are reported together.

## Variables

`VariableStore`: named, mutable, global, `double`-valued.

Variables are **numeric only**, as Arena's are, even though `Value` can hold a
string. Assigning a string-valued expression to a variable is a diagnostic at
validation time. Strings exist in the language to compare an entity's type, not
to be stored.

- Declared with an initial value.
- `reset()` restores initial values — the project's rule that every object
  holding run state has a `reset()` that `initialise()` calls.
- Time-persistent statistics collected per variable, so Arena's variable
  averages are reportable, and honouring the warm-up via `resetStatistics(now)`.
- **Scalars only.** Arena's 1-D and 2-D variable arrays are noted as future work
  and are not in v10.

### One namespace

Declaring a variable whose name collides with an entity attribute is a **hard
error**.

The alternative is a resolution order, and a resolution order means one of the
two reads silently wrong. That is the failure mode this project's own design
rules name twice — *"a default return value is a place for a bug to hide"* and
*"a statistic that is silently zero is worse than one that is missing"*.

## Errors are data, not exceptions

`ModelError` throws because a **programmer** wired the model wrong. A malformed
expression is a **user** typing in a cell, and v11 must show every bad cell at
once, not the first one.

```cpp
struct SourceSpan  { std::size_t offset, length; };
enum class Severity { Error, Warning };
struct Diagnostic  { Severity severity; SourceSpan span; std::string message; };

struct ParseResult {
    std::unique_ptr<IExpression> expr;          // null only if unrecoverable
    std::vector<Diagnostic>      diagnostics;
};
```

- The parser **recovers and continues** after an error, so one call reports many.
- Spans are **offsets within the cell text**. v11 adds row and column identity
  around them; the parser never learns what a spreadsheet is.
- Message style is fixed here and asserted in tests: `expected ')'`,
  `unknown function 'EXPOO'`, `EXPO expects 1 argument, got 2`.

### Static validation, before any run

```cpp
enum class FieldContext { HasEntity, NoEntity };
std::vector<Diagnostic> IExpression::validate(const ValidationContext&) const;
```

Catches unresolved names, wrong arity, and **an attribute reference in a field
with no entity** — a Create block's interarrival time has no entity to read from,
and that must be an error at build time rather than a throw mid-run.

### Runtime failures

Division by zero, a string in arithmetic, an attribute read with no entity that
static validation could not see: **throw `ExpressionError`** (deriving from
`ModelError`). There is no cell to point at during a run, and continuing would
produce confident nonsense. *A function that cannot do its job must assert, not
return a plausible value.*

## Nothing existing breaks

This is a **constraint**, not an aspiration.

| Existing API | After v10 |
|---|---|
| `DecideNode::Condition` (`std::function`) | Kept — wrapped in a `LambdaExpression` adapter |
| `station(name, cap, FIFO, exponential(0.8))` | Kept — wrapped in a `DistributionExpression` |
| `AssignNode::Rule { name, unique_ptr<IDistribution> }` | Becomes `{ target kind, name, unique_ptr<IExpression> }`; old overload constructs one |

There is **one** evaluation path. The old API becomes a thin constructor over the
new representation — it is not a second implementation kept alive in parallel.

**`Assign` gains a target kind**: `Attribute`, `Variable`, or `EntityType`. The
existing single-argument form means `Attribute`.

## Files

**New**

| File | Contents |
|---|---|
| `Value.hpp/.cpp` | `Value`, coercion, formatting |
| `Diagnostic.hpp` | `SourceSpan`, `Severity`, `Diagnostic` |
| `Lexer.hpp/.cpp` | Tokeniser with positions |
| `Expression.hpp/.cpp` | `IExpression` and its node kinds |
| `Parser.hpp/.cpp` | Recursive descent, precedence climb, error recovery |
| `Functions.hpp/.cpp` | Built-in registry: name → arity, builder |
| `EvalContext.hpp/.cpp` | `IModelState`, `EvalContext`, `ValidationContext` |
| `VariableStore.hpp/.cpp` | Variables, initial values, reset, statistics |

Estimated ~1,500 lines against the current 7,100.

**Modified**

`Nodes.hpp/.cpp` (Decide, Assign, Delay) · `Station` (service expression) ·
`Create` (interarrival expression) · `Model` (`variable()`, string overloads) ·
`SimulationSystem` (implements `IModelState`; resets the variable store) ·
`Build.hpp` (an `expr("...")` helper) · `des.hpp`.

## How it is proven

Added to the existing 293 checks:

1. **Precedence and associativity** — a table of expressions with hand-computed
   values, including right-associative `^` and unary minus binding.
2. **Error positions asserted character-exact** — `EXPO(0.8` reports `expected
   ')'` at the right offset. Not just "an error occurred".
3. **Error recovery** — one parse of a multiply-broken expression returns
   multiple diagnostics.
4. **Evaluation** against hand-computed values, including string comparison and
   the diagnostics for string arithmetic.
5. **Seeded distribution calls** reproduce the existing draws exactly — `EXPO(0.8)`
   parsed must give the identical sequence to `exponential(0.8)` constructed.
6. **`meanIfKnown`** on all four rows of the table above, and the ρ check
   reporting "could not verify" rather than passing.
7. **Variables** — reset across replications, time-persistent averages against a
   hand-worked case, warm-up honoured, name-collision rejected.
8. **Static validation** — an attribute reference in a Create's interarrival
   field is an error before the run, not a throw during it.
9. **Per-node streams** — two blocks with their own streams stay independent when
   one block's parameter changes.

### The decisive test

**Take every lambda condition and every distribution argument in `examples/` and
re-express it as a string, then assert the resulting trace is byte-identical to
the compiled version.**

That is the argument this project already makes with trace diffing: if the text
path and the code path agree event for event across fifteen programs, the text
path is correct. A near-miss average would not be evidence; an identical trace
is.

### Gates

- All **293 existing checks pass unchanged**.
- Examples 01–15 produce **byte-identical traces** to v9.
- Clean under `-Wall -Wextra -Wpedantic` and ASan/UBSan.

## Explicitly not in v10

| Deferred | Why |
|---|---|
| `ModelDocument`, schemas, file format | v11. v10 is the machinery those need. |
| The TUI | Separate repo, after v12. |
| Variable arrays (1-D, 2-D) | YAGNI until a model needs one. |
| Set and Schedule data modules | Change simulation semantics; easier to build through a spreadsheet. |
| Arena `.LT.`/`.EQ.` operator spellings | Token-table alias, no design content. |
| `Empirical` / `Deterministic` in the grammar | No Arena spelling; stay programmatic. |
| Preemption, batch means, distribution fitting | Unrelated to this arc; still on the v9 list. |
