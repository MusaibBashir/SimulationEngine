# v16 readlog — the ride-hailing model

v16 had a concrete target. Group 9's mid-semester Arena model is a three-zone
ride-hailing city where riders and cars are both moving entities. Matching
pairs a rider with a car in the same zone, and cars drive to wherever their
trip ends. The target: build it in this engine, from the same build guide,
and get the same answers.

`examples/models/ride_hailing.des` is that model. This file covers what it
needed, the decisions made along the way, and what the result is worth.

## What the model needed

| The guide uses | Before v16 | Now |
|---|---|---|
| Create → Assign, Decide, Delay, Dispose, `TNOW`, `&&`, DISC/TRIA/UNIF/EXPO | yes | — |
| **Match**, Based on Attribute, two lanes | no | `[Match]` + `[MatchLane]` |
| **Schedule**, Type Arrival, driving a Create | no | `[Schedule]` + `[ScheduleRow]`, `Create.Schedule` |
| **Route → Station**, joined by name | no | `[Route]`, `[Station]` |
| **Variable** `TravelTime`, 3 × 3 | scalars only | `Rows`, `Columns`, `Name(i, j)` |
| Entities per Arrival = `Num_CV` | a whole number | an expression |
| Record: Count into a named counter, Time Interval, Expression | Count / Attribute / TimeInSystem, under the block's name | all five, into a `Statistic` several Records share |
| Tally and counter results with half-widths | only the five system-wide numbers | a statistics table in both reports |

The Process Analyzer (§8.5) was left out on request.

## Decisions

**Match lanes are blocks.** Arena draws a Match with numbered input arrows.
The text format has no arrows, only names. Each `[MatchLane]` row is
therefore a block with its own name, which other blocks route to and which
the report lists. `Next = Riders_Waiting` says more than "input 1". The Match
itself cannot be routed to, and trying to is refused with the names of its
lanes.

**Which rider a car takes.** A car arriving when several zones have riders
matches only its own zone. Within a zone, it takes the rider who has waited
longest. If several *values* could ever match at once, the value whose
earliest waiter has waited longest goes first.
It is written down because it is a modelling decision; nothing in the build
guide settles it.

**Max Arrivals counts arrivals, not entities.** The fleet is created with
Per Arrival 80 and Max Arrivals 1. In Arena that is 80 cars. Here it was one,
because Max Arrivals was compared with the number of *entities* made. It now
counts arrival events, as Arena's does. No example or regression model used
more than one entity per arrival, so nothing already captured changed. The
tests say so rather than hope so.

**A loop into a Match lane is not a routing loop.** The loop check exists for
entities that can never leave: a Process loop fed by endless arrivals. Cars
return to the Match lane forever, and that is the model. The check stops at a
Match lane. A Route has no `Next` (its Destination is where it goes), so the
check and the visit ratios also stop at a Route instead of following a
teleport as though it were a wire.

**Schedules use thinning.** The obvious method draws `EXPO(1 / rate(now))`.
It is wrong exactly where this model needs it right: a gap drawn at 7:59 at
the quiet rate carries straight over the 8am spike. Thinning draws at the peak
rate and keeps each candidate with probability `rate(t) / peak`. It is exact
for any schedule, zero-rate hours included. A scheduled Create's first event
only *starts* the stream, so no replication gets a free arrival at time zero.

**Units are converted once.** `[Run] Time Units` (default Minutes) is the
model's clock. A schedule's rows are in its own units and are converted at
compile time. Models without a schedule never read the new field.

**Named statistics are opt-in by construction.** A Record written in a
document always has a Statistic (its own name by default). A Record built
through the C++ API has none. The new report table therefore appears for
documents with Records, Matches or Routes, and every C++ example prints
exactly what it did before.

## The result

100 replications of a 480-minute shift with a 60-minute warm-up, quarter
scale (10 contract and 40 freelance cars), next to the report's Arena numbers:

| | this engine | Arena (report) |
|---|---|---|
| Wait, Suburb (min) | 33.31 ± 4.93 | 36.74 ± 6.03 |
| Wait, Downtown (min) | 0.00 | 0.00 |
| Wait, Airport (min) | 1.54 ± 0.58 | 1.05 ± 0.60 |
| Service level | 0.867 ± 0.012 | 0.875 ± 0.014 |
| Trips per shift | 143.1 ± 2.8 | 140.4 ± 3.3 |
| Riders never served | 18.5 ± 2.3 | 17.8 ± 2.4 |
| Idle vehicles | 44.3 ± 0.13 | 44.8 ± 0.13 |
| Rider queue wait (min) | 11.3 ± 1.5 | 12.2 ± 1.9 |
| Vehicle queue wait (min) | 101.4 ± 2.2 | 106.6 ± 2.5 |

Every interval overlaps the report's. The two engines draw different random
numbers, so agreement to the decimal was never on offer. What should
reproduce is the finding, and it does: the Suburb starves while Downtown
waits for nothing, and the fleet is idle 89% of the time. The study takes
about five seconds.

## Found along the way

- **A study of a model with no Process read past the end of a list.** The
  summary's utilisation column came from `stationAt(0)`, and the ride-hailing
  model has no Process. It reports 0 there now.
- **A time limit stopped one event late.** `stopAt(45)` finished with the
  clock at 50, because the arrival at 50 was processed before the rule was
  checked -- a run of Length 480 measured 480 plus however long it was to
  the next event. This predated v16. It was fixed on its own branch
  (`claude/pensive-volta-usgu5f`), which put the deadline on the event list
  as `EndSimulation` -- scheduled for the first time since v1 -- and checked
  the result against a model worked by hand rather than against a baseline.
  It was merged into v16 and the gates re-captured deliberately: twelve
  examples, `teller.expected` and `ride_hailing.expected` changed, each by a
  run length of Length + epsilon becoming Length, and nothing else. The table
  above is after the fix; before it, each replication of this model ran up to
  a few minutes past 480 and the Suburb wait read 33.57.

  A model that runs out of work before its Length still ends where the work
  ended, as before. Arena would run on to the Length and divide every time
  average by it; that is a different change, and still open.
- **An enum with one value is refused by the registry's own test.** Schedule's
  Type offers only Arrival in this version, so it is a text field that the
  compiler checks. `Capacity` gets its own message saying it is not supported
  yet.

## Still open

- Whether a model that runs out of work early should run on to its Length,
  as Arena's does.
- Capacity schedules, with Ignore, Wait and Preempt.
- Arena Sets (resource, entity type, counter, tally, queue).
- Writing to array elements.
- The Process Analyzer: several runs of one model with different Variable
  values, compared side by side. The Runs tab is the natural place for it.
- Derived results such as utilisation = (fleet − idle) / fleet, which the
  report computes by hand.

## v16.1 — the Tables tab

A 3 × 3 table typed as `Initial Value = 4, 14, 22,  14, 5, 28,  22, 28, 3`
works, but it is not how anyone thinks of a table, and eight schedule rows
of three lines each made the Model text mostly schedule. Variables, arrays,
matrices and schedules now have a tab of their own, edited as grids.

**Where they live.** Still in the one `.des` file, so a model stays one file
to send and the engine, `des check`, `des run` and the regression harness read
the format they always have. Opening a file takes every `[Variable]`,
`[Schedule]` and `[ScheduleRow]` out of the text; saving writes the text and
then a section after `# ---- tables: edited on the Tables tab ----`, in one
fixed layout. The alternative -- folding the blocks inside the editor -- would
have left the file in its original order, at the price of hidden lines in
every piece of caret, selection and line-number code. Splitting keeps the
Model tab's line numbers the ones in the file, because the tables come after.
The cost is stated: the first save moves tables to the end, and comments
inside them are lost, except the one directly above each, which becomes the
table's note. A file that never had a table is saved byte for byte as typed.

**Pasting from Excel.** Windows Terminal handles Ctrl+V itself and types the
clipboard into the program: digits, a Tab between cells, an Enter between
rows. So the grid's keys are the paste format -- Enter keeps a cell and moves
down to the column the row started in, Tab keeps it and moves right -- and a
column copied from a spreadsheet lands as it looked there, growing a schedule
as it goes. `^V` still pastes the program's own clipboard, parsed as a block.

**Found along the way.** Deleting a table that a Create still read produced
no error until the run, because the compiler checked expressions' syntax and
never their names -- only `validate()`, at the start of a run, did. On a tab
where the table and its user are on different screens that silence is worse
than before, so the compiler now reports unknown names too. Their position is
an offset inside an expression, not a file line, so they carry none rather
than a marker on the wrong line. No baseline or regression output changed.

## v16.2 — the flow as a graph

The Flow tab had been a list of blocks and their exits -- accurate, and no
help at all in seeing a model's shape. Graphviz now lays the model out and
the tab draws the result in characters.

**Graphviz cannot draw ASCII, so it does not.** `dot -Tplain` gives every
node's centre and size and every edge's B-spline control points, in inches.
drawAscii() maps an inch to 12 columns and 6 rows -- which is what the DOT asks
a 10-point Courier to take, so a block's name fits the box dot sized for it --
draws each edge by sampling its spline, then the edge labels, then the boxes
over both, so no line runs through a name. Two things only looking found: a
slope stepped one cell at a time read as noise (`|--/--`), so direction is
judged over five cells; and a 45-degree line crosses two columns a row, so only
the first cell of each row gets the slash. And the fleet's loop back into the
Match lane pulled every Create into the middle of the picture until
`newrank=true` let `rank=source` hold them on the left.

**Running it.** dot runs once per change of the model, not per frame. On
Windows it runs through CreateProcess on a hidden console of its own: going
through cmd.exe on the program's own console -- which std::system does -- can
reset the raw keyboard mode the terminal UI depends on.

**Getting it installed.** Graphviz is not on the laptops this is sent to. The
installer offers to install it with winget when it is missing; Graphviz's own
installer then asks for administrator permission. If that is refused, or the
laptop is offline, or the zip was used instead, the Flow tab shows the list and
says why, and the README gives the one command to run by hand.
