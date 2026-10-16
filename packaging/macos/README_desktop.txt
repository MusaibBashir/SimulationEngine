DES Simulator  @VERSION@  --  desktop app for macOS
==================================================

Requires macOS 12 (Monterey) or later, on an Apple Silicon or an Intel Mac.

INSTALLING
----------

Drag "DES Simulator" onto the Applications folder in this window. The
examples folder has finished models to open from File > Open.

THE FIRST TIME macOS WARNS YOU
------------------------------

The app is not notarised by Apple, so the first time macOS may say it
"cannot be opened because Apple cannot check it for malicious software".

  1. Open it once and dismiss the warning.
  2. Open System Settings > Privacy & Security, scroll down, and click
     "Open Anyway" next to the message about DES Simulator.

Or, in Terminal:

    xattr -dr com.apple.quarantine "/Applications/DES Simulator.app"

This happens once.

THE MICROPHONE
--------------

Click the microphone in the Describe panel (or press F3) to say what the
model should do. The first time, macOS asks whether DES Simulator may use the
microphone; allow it. If you said no, change it in System Settings >
Privacy & Security > Microphone.

Describing a model sends your words to Groq and needs a free API key from
console.groq.com/keys -- Model > Settings. Everything else works offline.
