# v16.1 — the Tables tab

**Goal.** Variables, arrays, matrices and schedules are edited on their own tab,
as tables, and the Model tab's text holds only the rest of the model. A model
uses a table by its name, as it does now.

## 1. Storage

- `TableStore` (TUI side) holds an ordered list of `Table`:
  - `kind`: Variable | Array | Matrix | Schedule
  - `name`, `note` (optional comment)
  - Variable: one value. Array: `size` values. Matrix: `rows` x `columns`
    values, row-major. Schedule: `timeUnits` (Seconds/Minutes/Hours/Days,
    default Hours), `type` (Arrival) and rows of (Value, Duration).
  - Values are held as TEXT per cell, always a valid number (the editor refuses
    anything else), so what was typed is what is written back.
- **Open:** `splitTables(text)` removes every `[Variable]`, `[Schedule]` and
  `[ScheduleRow]` record from the text, with the comment lines directly above
  each (which become the note), and the marker line if present. What is left is
  the Model buffer. Unknown columns on those records are dropped. A
  `[ScheduleRow]` whose Schedule names no `[Schedule]` becomes a schedule of
  that name.
- **Save:** model text (trailing blank lines trimmed), a blank line, the marker
  `# ---- tables: edited on the Tables tab ----`, then each table in canonical
  form (below). No tables: no marker. Saving twice is byte-identical.
- **Canonical form:**
  - Variable: `[Variable]` / `Name` / `Initial Value`
  - Array: `[Variable]` / `Name` / `Rows = n` / `Initial Value = a, b, c`
  - Matrix: `[Variable]` / `Name` / `Rows` / `Columns` / `Initial Value`, rows
    separated by two spaces after the comma
  - Schedule: `[Schedule]` / `Name` / `Type = Arrival` / `Time Units`, then one
    `[ScheduleRow]` per row (`Schedule`, `Value`, `Duration`)
  - A `Variable` with `Rows` and `Columns = 1` is read back as an Array.
- **Compile / run:** the document is read from `joined()` = model text + the
  tables section, so Model line numbers are unchanged. The engine and the
  `.des` format do not change.
- The Model palette no longer offers Variable, Schedule, ScheduleRow. A table
  block typed into the Model text still compiles and moves to the Tables tab
  the next time the file is opened.

## 2. The tab

- Tab bar: Model ^B, **Tables ^D**, Flow ^F, Runs ^U, Results ^E.
- Three panels: kinds (left), editor (middle), list of tables (right).
- **Kinds:** Enter/click creates `<Kind>N` (first free N) with default shape
  (Variable 0; Array size 3; Matrix 2x2; Schedule 1 row, Hours), opens it with
  the Name field selected.
- **Editor:** header fields (Name; Array Size; Matrix Rows, Columns; Schedule
  Rows, Time Units; Variable Value), then the grid. Arrows move; typing starts
  an edit; Enter keeps, Esc drops; Tab/Shift-Tab next/previous field or cell;
  Ins adds a row after (Array, Schedule); Del on a row removes it (Array,
  Schedule); ^V pastes a block (tab, comma, semicolon or whitespace separated;
  one row per line) from the selected cell, growing the grid. Shrinking a size
  that would drop non-zero values asks first. Time Units opens a picker.
  "used by:" lists blocks that name the table.
- **List:** every table, kind and shape; selecting shows it; Del deletes after
  asking, naming users.
- Tab/Shift-Tab (outside an edit) cycle panels; ^Z/^Y undo/redo table edits
  (own stack); `?` explains; mouse selects in all panels, a second click on a
  cell edits it.

## 3. Rules

- A cell refuses a non-number; Name refuses empty, invalid identifier or a
  duplicate; sizes must be whole numbers 1..1000.
- Compiler diagnostics whose line falls in the tables section are attributed to
  a table (and cell where the diagnostic names a column); the tab label shows
  the error count; ^J goes to the first error on either tab.
- Rename asks whether to update references in the Model text (`Schedule =`,
  `Per Arrival =`, and whole-word uses in expression fields) as one undo step.
- ^R runs the joined model from any tab; ^S saves both; dirty/quit/rescue cover
  table edits.

## Out of scope

Editing tables from the CLI; per-cell comments; tables other than the four
kinds.
