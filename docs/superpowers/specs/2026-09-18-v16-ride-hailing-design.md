# v16 — build the Group 9 ride-hailing model

**Goal.** Everything needed to build the Group 9 mid-semester Arena model
(*Multi-Zone Ride-Hailing Dispatch & Driver Repositioning*, build guide §0–§8)
in this engine, plus two TUI requests. Process Analyzer (§8.5) is out of scope.

**Out of scope, deferred to a later version:** Arena Sets (all kinds), capacity
schedules and the Ignore/Wait/Preempt rules, an `[Attribute]` declaration module
(attributes already exist: an Assign creates one, and an unset one reads 0),
writing to array elements.

**Invariant.** A model that uses none of the new modules or columns compiles,
runs and reports byte-identically to v15.2. `tools/baseline.sh check` and
`des regress` stay clean without re-capturing anything.

---

## 1. TUI

### 1.1 Runs tab: a list and a form

- Left pane: the runs. Up/Down selects; Enter or ^R runs the selected one.
  **A** or **Insert** appends a new `[Run]` block (this moves off ^N).
  **Delete** asks "Delete run *Name*?" and, on yes, removes that `[Run]` block
  from the text (with the comment lines directly above it).
- Right pane: the selected run's fields, one row each — Name, Length, Warm-up,
  Replications, Base Seed, Stop When Drained, Max Entities, Separate Streams,
  Antithetic, Time Units (§3). An unset field shows its default, dimmed.
- Tab / Shift-Tab moves focus between list and form. In the form, Up/Down picks a
  field; Enter edits it in place (Enter commits, Esc cancels); Boolean and Enum
  fields open the existing picker. `?` shows the field's help line.
- A committed edit rewrites that one `key = value` line of the `[Run]` block in
  the text buffer (inserting it if absent, removing it if set back to blank).
  Nothing else in the file moves: comments and alignment survive. It is one
  undo step. An invalid value (letters in Replications) is refused with the same
  message the compiler would give, and the text is not changed.
- Mouse: clicking a run selects it; clicking a field selects it; double-click
  edits.

### 1.2 New file and first save

- **^N** (any view): if the buffer is dirty, a Confirm asks
  *Save / Discard / Cancel*. Then a one-line prompt: `New model name:`.
- Name resolution (a pure, tested function):
  - a bare name (no `/` or `\`) goes in the directory of `defaultModelPath()`
    (`Documents\DES Models` on Windows);
  - a name with a directory part is used as given (relative to the working
    directory);
  - `.des` is appended when there is no extension;
  - empty or all-blank input is refused with a status message.
- If the resolved file exists: Confirm "*x.des* exists — open it instead?"
  Yes opens it; No returns to the prompt.
- Otherwise a blank model is opened at that path, unsaved; the window title and
  status line change to the new name.
- Launching with **no argument** still opens the scratch model at
  `defaultModelPath()`, but if that file did not exist the state is *unnamed*.
  The first ^S on an unnamed model opens the same prompt instead of writing
  `untitled.des`; after a successful save it is named.
- The prompt is a new `Overlay::Prompt` (single-line text input, caret, Home/End,
  Backspace/Delete, Enter, Esc) reused by both paths.

## 2. Match

```
[Match]
Name      = Match_Rider_Driver
Type      = Attribute            # Any | Attribute   (default Any)
Attribute = myZone               # required when Type = Attribute

[MatchLane]                      # child table; row order = lane order
Match = Match_Rider_Driver
Name  = Riders_Waiting           # entry point other blocks route to
Next  = Calc_SLA                 # exit for this lane's matched entity
```

- A Match has 2–5 lanes (its `[MatchLane]` rows); fewer or more is an error.
- A lane `Name` is a routable block name (unique among all block names); a lane
  `Next` is an ordinary exit (blank = leaves the system).
- On every arrival into a lane:
  - **Any:** if every lane is non-empty, the head of each lane leaves.
  - **Attribute:** find values *v* such that every lane holds an entity whose
    `Attribute` equals *v*. If several, choose the *v* whose earliest waiter
    (across all lanes) has waited longest. From each lane take the
    longest-waiting entity with that value.
  - All matched entities leave at the same instant, lane order 1..n, each via its
    lane's `Next`.
- Statistics per lane: time-average and maximum number waiting, average wait,
  number still waiting at the end of the run (after warm-up reset like every
  other statistic).
- Flow tab: lanes show as `Match_Rider_Driver ▸ Riders_Waiting`.

## 3. Arrival schedules and base time units

```
[Run]
Time Units = Minutes             # new; Seconds | Minutes | Hours | Days; default Minutes

[Schedule]
Name       = Sched_Suburb
Type       = Arrival             # the only type in v16
Time Units = Hours               # unit of Duration; Value is arrivals per this unit

[ScheduleRow]                    # child table; row order = time order
Schedule = Sched_Suburb
Value    = 15
Duration = 1

[Create]
Schedule = Sched_Suburb          # new column
```

- A Create with `Schedule` set generates a non-stationary Poisson process by
  thinning: candidates at the schedule's peak rate, each kept with probability
  rate(t) / peak. After the last row the schedule repeats from the first.
  `Per Arrival`, `Max Arrivals` and `First At` apply as usual.
- Conversion happens once at compile: rate and duration are converted from the
  schedule's units to the Run's `Time Units`. The Run's unit is read only when a
  schedule exists, so it cannot change any existing model.
- Errors: a Create with both `Interarrival` and `Schedule`, or neither; a
  negative Value or Duration; a schedule whose Durations sum to 0; `Schedule`
  naming something that is not a `[Schedule]`. Warning: a Schedule no Create uses.

## 4. Route and Station

```
[Station]
Name = Zone1_Station
Next = SetZone_Downtown

[Route]
Name        = Trip_to_Downtown
Route Time  = TravelTime(myZone, 1) * UNIF(0.85, 1.15)
Destination = Zone1_Station
```

- Station: zero-time pass-through to `Next`. Any block may route to it.
- Route: holds the entity for `Route Time` (base units), then delivers it to
  `Destination`. A Route has no `Next`. `Destination` must name a `[Station]`
  (error otherwise, with a closest-name suggestion).
- Statistics per Route: entities that travelled, time-average number in transit.
- Warning: a Station that no block routes to.
- Flow tab: Route ⇢ Station drawn as a dashed link.
- Arena's separate block name / station name is collapsed into one name.

## 5. Variables, Create, Record, and the report

### 5.1 Array variables

```
[Variable]
Name          = TravelTime
Rows          = 3               # new, optional (default: scalar)
Columns       = 3               # new, optional; needs Rows
Initial Value = 4, 14, 22, 14, 5, 28, 22, 28, 3   # row-major
```

- `Name(i)` reads a 1-D array, `Name(i, j)` a 2-D one; indices are 1-based and
  rounded to the nearest integer. Out of range stops the run with a message
  naming the variable, the index and the size.
- Wrong number of initial values (neither 1 — fill all — nor rows×columns) is a
  compile error. Using an array without an index, or a scalar with one, is a
  compile error.
- Assign to an array element is a compile error in v16 ("not yet supported").

### 5.2 Create: Per Arrival is an expression

Evaluated at each arrival (so `Num_CV` works); the result is truncated to an
integer; negative stops the run with a message. Existing integer values parse as
expressions and behave identically.

### 5.3 Record

```
[Record]
Name      = Wait_Suburb
What      = TimeInterval         # Count | TimeInterval | Expression | Attribute | TimeInSystem
Attribute = tStart               # TimeInterval: records TNOW - tStart
Value     = metSLA               # Count: amount added (default 1); Expression: value tallied
Statistic = Wait_Suburb          # new; default = the block's Name
```

- Several Records may share one `Statistic`; mixing a counter (Count) and a tally
  (everything else) under one name is a compile error.
- `Attribute` and `TimeInSystem` keep their v15 behaviour exactly.

### 5.4 Cross-replication statistics table

After the existing summary, one more table: for each tally (mean of per-rep
averages), counter (per-rep totals), Match lane (average number waiting, average
wait, number waiting at end) and Route (average number in transit): the mean and
95% half-width across replications. With one replication the same table prints
without half-widths. It is printed only when the model has at least one of these
statistics, so every existing report is unchanged.

### 5.5 The example model

`examples/models/ride_hailing.des`: the assembled Group 9 model with the §8.1,
8.2, 8.3, 8.4 and 8.6 upgrades, commented by section and owner. Added to the
regression set with checks for: it compiles; conservation of entities (created
= disposed + waiting in Match lanes + in transit + in Delays); mean Suburb wait
> mean Downtown wait over the regression's replications. Exact agreement with the
report's Arena numbers is not claimed (different random streams).

## 6. Everywhere else

- Module schemas, help text and palette entries for Match, MatchLane, Schedule,
  ScheduleRow, Route and Station; new columns on Run, Create, Variable, Record.
- The DES Engine Reference (manual_data.py → PDF), CHANGELOG, README, and a
  V16 readlog.
- Gates: suite, `tools/verify.sh`, baseline check, `des regress`,
  `tools/package.sh`, manual completeness.
