DES Simulator  @VERSION@
========================

A discrete-event simulation tool in the spirit of Arena's Basic Process
template. You write a model as text, run it, and read the results. It needs
nothing else installed: no compiler, no libraries, no internet connection.

Requires Windows 10 or 11 (64-bit).


STARTING IT
-----------

Double-click DES-Simulator.exe.

It opens a blank model called untitled.des, kept in

    Documents\DES Models\

and opens that same file again next time, so your work is where you left it.

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

The examples folder has four finished models to open and take apart.


KEYS
----

  F1            every key, on screen
  Ctrl+G        explain the field the cursor is on
  Ctrl+L        list the values a field allows (Arena's drop-down)
  Ctrl+J        jump to the first error; errors are marked E beside the line
  Ctrl+T        write a working model into an empty file
  Ctrl+R        run the model
  Ctrl+W        save the results to a file beside the model
  Ctrl+S        save the model
  Ctrl+Z Ctrl+Y undo, redo
  Ctrl+X Ctrl+C Ctrl+V   cut, copy, paste (Shift+arrows select)
  Tab           move between the module list and the text
  Ctrl+Q        quit (it asks first if you have unsaved changes)

The tabs along the top -- Model, Flow, Runs, Results -- can be clicked, or
reached with Ctrl+B, Ctrl+F, Ctrl+U and Ctrl+E.


IF SOMETHING GOES WRONG
-----------------------

If the program ever stops unexpectedly with unsaved changes, it writes your
model to a file with ".recovered" added to its name, next to the original, and
tells you where before the window closes.
