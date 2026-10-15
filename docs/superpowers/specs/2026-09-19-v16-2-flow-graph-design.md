# v16.2 — the Flow tab as a graph

**Goal.** The Flow tab draws the model as a graph laid out by Graphviz, in
ASCII, and `P` saves it as a PNG beside the model and opens it.

## Pieces

- `FlowGraph` (des_ui, no screen, no processes):
  - `dotFor(const ModelDocument&)` — DOT, `rankdir=LR`. One node per flowchart
    block and Match lane (from `flowOf`), shaped by type: Create `house`,
    Decide `diamond`, Dispose `oval`, Route `cds`, Station `circle`, Match
    lanes in a `cluster` with the Match's name, everything else `box`. Edges
    from every exit; Decide branches labelled with the condition / `p=` /
    `else`; Route → Station dashed. Names are quoted.
  - `parsePlain(text)` — reads `dot -Tplain`: graph size, nodes (name, centre,
    size, label), edges (tail, head, control points, optional label position).
  - `drawAscii(layout, highlight)` — lines of text. 10 columns and 4 rows per
    inch; boxes `+-+ | |` with the label centred; edges sampled along the
    B-spline and drawn with `- | / \`, an arrowhead `> < v ^` at the head;
    labels at their positions; boxes drawn last so edges never cross them. The
    highlighted node's box uses `#` for its corners and edges.
- `Graphviz` (des_ui, runs processes):
  - `findDot()` — `DES_DOT` environment variable, then `dot` on PATH, then
    `C:\Program Files\Graphviz\bin\dot.exe` (Windows) / `/usr/bin/dot`.
  - `runDot(dotPath, dotText, format, outPath, error)` — writes the DOT to a
    temp file and runs dot through the system shell; false with the reason.
- TUI: the Flow tab shows the graph; its layout is cached by DOT text, so dot
  runs once per change of the model, not per frame. Up/Down/Enter keep their
  meaning (step through blocks, jump to the text); the view scrolls to keep the
  chosen block on screen; `L` toggles the list view. When dot is not found or
  fails, the list view is shown with the reason on the status line.
- `P` on the Flow tab: `dot -Tpng` to `<model's folder>/images/<model>-flow.png`
  (folder created), then opened with the default viewer (Windows
  `ShellExecute "open"`, POSIX `xdg-open`). Status line gives the path.
- Installer: when Graphviz is not found at install time, a ticked task
  "Install Graphviz (draws the Flow graph; needs internet)" runs
  `winget install --id Graphviz.Graphviz -e --silent --accept-package-agreements
  --accept-source-agreements` after install. Failure is not fatal. The
  shipped README says how to install it by hand.

## Tests

`dotFor` on ride_hailing (shapes, labels, dashed routes, the Match cluster,
quoting); `parsePlain` + `drawAscii` on a stored `-Tplain` fixture (boxes at
the right places, an arrowhead per edge, labels present, highlight); when dot
is found, a real round trip and a PNG written with the PNG signature;
otherwise those checks are reported as skipped. The TUI falls back to the list
when dot is missing (forced with an invalid `DES_DOT`).
