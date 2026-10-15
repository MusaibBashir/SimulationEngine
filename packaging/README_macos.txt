DES Simulator  @VERSION@
========================

A discrete-event simulation tool in the spirit of Arena's Basic Process
template. You write a model as text, run it, and read the results. It needs
nothing else installed: no compiler, no libraries, no internet connection.

Requires macOS 11 (Big Sur) or later, on either an Apple Silicon or an Intel
Mac.


STARTING IT
-----------

Double-click "DES Simulator.command". It opens in Terminal.

It opens a blank model called untitled.des, kept in

    Documents/DES Models/

and opens that same file again next time, so your work is where you left it.

To open some other model from Terminal:

    "/path/to/DES-Simulator/des-simulator" model.des

The Terminal window has to be at least 80 columns by 24 lines. If it is
smaller the program says so; make the window bigger.


THE FIRST TIME macOS WARNS YOU
------------------------------

Because this program is not notarised by Apple, macOS may say it "cannot be
opened because Apple cannot check it for malicious software". To allow it:

  1. Try to open "DES Simulator.command" once, and dismiss the warning.
  2. Open System Settings > Privacy & Security, scroll down, and click
     "Open Anyway" next to the message about DES Simulator.
  3. Do the same for "des-simulator" if macOS asks about it too.

Or, in Terminal, remove the download mark from the whole folder in one go:

    xattr -dr com.apple.quarantine /path/to/DES-Simulator

This happens once.


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

The examples folder has finished models to open and take apart.


KEYS
----

The keys are Control, not Command.

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

On a Mac keyboard F1 may need the fn key. If a key does nothing, check
Terminal > Settings > Profiles > Keyboard.

The tabs along the top -- Model, Flow, Runs, Results -- can be clicked, or
reached with Ctrl+B, Ctrl+F, Ctrl+U and Ctrl+E.


IF SOMETHING GOES WRONG
-----------------------

If the program ever stops unexpectedly with unsaved changes, it writes your
model to a file with ".recovered" added to its name, next to the original, and
tells you where.
