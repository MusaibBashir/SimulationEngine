DES Simulator  @VERSION@
========================

A discrete-event simulation tool in the spirit of Arena's Basic Process
template. You write a model as text, run it, and read the results. It needs
nothing else installed: no compiler, no libraries, and -- apart from F2 --
no internet connection.

Requires Windows 10 or 11 (64-bit).


STARTING IT
-----------

Double-click DES-Simulator.exe.

It opens a blank model. The first time you save it (Ctrl+S) it asks for a
name, and keeps it in

    Documents\DES Models\

Ctrl+N starts another new model, asking for its name first.

To open some other model, drag its .des file onto DES-Simulator.exe. If you
used the installer, double-clicking a .des file opens it directly.

The window has to be at least 80 columns by 24 lines. If it is smaller the
program says so; make the window bigger.


THE FIRST TIME WINDOWS WARNS YOU
--------------------------------

Because this program is not signed with a paid certificate, Windows may show
"Windows protected your PC". Click "More info", then "Run anyway". This
happens once. Some antivirus programs are cautious about new unsigned programs
in the same way.


YOUR FIRST MODEL
----------------

Press Ctrl+T in the blank model. It writes a small working model: customers
arrive, wait for one server, are served, and leave. Press Ctrl+R to run it,
and the results appear. Then change a number and run it again.

To build your own, pick a module in the list on the left (Create, Process,
Dispose, ...) and press Enter, or click it. Its fields appear in the text,
blank, for you to fill in. Lines starting with # are comments.

There is no drawing canvas. A CONNECTION between two modules is the name of
the next module, typed into a "Next" field.

The examples folder has finished models to open and take apart, including
ride_hailing.des: a three-zone ride-hailing city with riders and cars matched
by zone, hourly arrival schedules, and cars driving to where their trips end.


KEYS
----

  F1            every key, on screen
  Ctrl+G        explain the field the cursor is on
  Ctrl+L        list the values a field allows (Arena's drop-down)
  Ctrl+J        jump to the first error; errors are marked E beside the line
  Ctrl+T        write a working model into an empty file
  F2            describe a model in words, or speak it (see below)
  Ctrl+R        run the model
  Ctrl+N        a new model (it asks for the name)
  Ctrl+D        the Tables tab: variables, arrays, matrices, schedules
  Ctrl+W        save the results to a file beside the model
  Ctrl+S        save the model
  Ctrl+Z Ctrl+Y undo, redo
  Ctrl+X Ctrl+C Ctrl+V   cut, copy, paste (Shift+arrows select)
  Tab           move between the module list and the text
  Ctrl+Q        quit (it asks first if you have unsaved changes)

The tabs along the top -- Model, Tables, Flow, Runs, Results -- can be
clicked, or reached with Ctrl+B, Ctrl+D, Ctrl+F, Ctrl+U and Ctrl+E.

The Flow tab draws the model as a graph using Graphviz. The installer offers
to install Graphviz for you; if you skipped that, or used the zip, install it
once with this command in a terminal, then restart the program:

    winget install Graphviz.Graphviz

Without it the Flow tab shows the same wiring as a list. On the Flow tab, the
Left and Right arrows move across a graph too wide for the window -- the bar
along the bottom shows which part is in view, and can be clicked -- Up and
Down choose a block, L switches between the graph and the list, and P saves
the graph as a picture in an "images" folder next to the model and opens it.

Double-clicking a model opens the program maximized.

On the Tables tab, numbers such as a fleet size, a travel-time table or an
hourly arrival schedule are edited as a grid. Pick a kind on the left and
press Enter to make one; the model uses it by its name. A block copied from
Excel can be pasted straight into the grid.

On the Runs tab: A adds a run, Delete removes one, and Tab moves into the
chosen run's settings, where Enter changes a setting.


DESCRIBING A MODEL INSTEAD OF WRITING IT
----------------------------------------

Press F2, or click "Ask F2" at the top. Say what happens -- press Win+H to
speak instead of typing -- for example:

    customers come thirty an hour, two tellers, about 3 minutes each

and press Enter. A language model fills in what you said, and the program
writes the model for you. It lists every number it had to assume, asks when
something is missing, and Ctrl+Z takes the whole model back.

This one feature uses the internet: your words are sent to Groq, and the
first time it asks for a free API key from console.groq.com/keys (Ctrl+K in
the panel changes it). The key is saved for you alone, never in a model.
Everything else works with no connection at all.


IF SOMETHING GOES WRONG
-----------------------

If the program ever stops unexpectedly with unsaved changes, it writes your
model to a file with ".recovered" added to its name, next to the original, and
tells you where before the window closes.
