DES Simulator  @VERSION@
========================

A discrete-event simulation tool in the spirit of Arena's Basic Process
template. You write a model as text, run it, and read the results. It needs
nothing else installed: no compiler, no libraries, and -- apart from F2 --
no internet connection.

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
  F2            describe a model in words, or speak it (see below)
  F3            the same, starting by recording what you say
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


DESCRIBING A MODEL INSTEAD OF WRITING IT
----------------------------------------

Press F2 (fn+F2 on most Mac keyboards), or click "Ask F2" at the top. Say
what happens -- press the Dictation key, or Fn twice, to speak instead of
typing -- for example:

    customers come thirty an hour, two tellers, about 3 minutes each

Or press Ctrl+T in that box, speak, and press Ctrl+T again: what you said
appears in the box to check before sending. (The first time, allow the
program to use the microphone if asked.)

Then press Enter. A language model fills in what you said, and the program
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
tells you where.
