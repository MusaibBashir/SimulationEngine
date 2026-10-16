# v17 readlog — describe a model out loud

v17 had two targets. First, someone who has never seen a `.des` file should
be able to say what happens and get a model that runs. Second, the program
should open as a window on Windows and on a Mac, not only in a terminal.

The constraint shaped everything: it had to be **cheap**. A frontier model
could probably write `.des` text directly. Paying for one on every
description was ruled out, so the work went into engineering around a small
model: Qwen 3.8 27B on Groq, at a fraction of a cent per model built.

## What was built

| Piece | Where |
|---|---|
| A form the language model fills in, instead of writing `.des` | `include/Scenario.hpp`, `src/Scenario.cpp` |
| A pre-parser that does the arithmetic first | `include/PreParse.hpp` |
| A builder that writes the model and lists every default | `include/ScenarioBuilder.hpp` |
| The pipeline, the settings file, the provider call | `include/ModelGenerator.hpp`, `src/CurlTransport.cpp` |
| Speech: microphone, WAV, Whisper | `include/Recorder.hpp`, `include/Speech.hpp`, `tui/Microphone.cpp` |
| `F2` / `F3` in the terminal UI | `src/TuiState.cpp` (the Ask overlay) |
| `des generate`, `des new` on the command line | `cli/` |
| The desktop app, Qt 6 Widgets | `gui/` |
| Release and desktop builds by CI | `.github/workflows/release.yml`, `desktop.yml` |

Scope, agreed at the start: arrival streams, processing steps with their own
or shared servers, delays, chance branches, and run setup. Assign, Batch,
Separate, Record, Match and the rest stay in the editor.

## Decisions

**The language model never writes `.des` text.** It fills in a Scenario: about
twenty fields covering time unit, run setup, arrivals, resources, steps and
branches. `readScenario()` checks every field. A malformed one goes back to
the model once, by its path (`steps[1].time`), and a missing one becomes a
question for the person. `buildScenario()` then writes every block, `Next` and
default, and the result is compiled before anyone sees it. Whatever comes back
compiles, because this project's own code wrote it.

**Durations are words, not distributions.** The form asks for a `Pattern`
(Fixed, Random, Between, Around, Measured) rather than for EXPO or TRIA. A
small model picks between words a person would use far more reliably than it
picks a distribution name, and the mapping from word to distribution lives in
`expressionFor()`, where it can be tested.

**Every default is said out loud.** If the person didn't say how long the run
is, the builder picks a day and lists it under *Assumed*. The same goes for a
fixed time the person called "exactly", or a queue rule nobody mentioned. A
default nobody can see is a modelling decision nobody made.

**Overloaded servers are a question, not a model.** `offeredLoads()` solves
the traffic equations over the form, including branches and rework loops. A
step or shared resource at or above 100% would never reach steady state, and
only the person knows the real server count, so it is asked: "X-ray would be
140% busy with 1 server. How many are there? 2 would keep up."

**Answers merge, they don't replace.** An answer goes back with the form so
far. A small model sends back only what it changed, however firmly it is told
otherwise, so `mergeScenarioJson()` lays the reply over the earlier form:
rows match by name, and a single unnamed row matches by position. A bare
number answering a server question is filled in by code, with no call at
all.

**The provider is three settings.** Base URL, model and key, in a per-user
`llm.conf` (never beside a model, so a key cannot be emailed with a `.des`
file). Groq is the default, but any OpenAI-compatible server works, Ollama
included. Saving one setting rewrites that one line, so later defaults still
apply. The first test build wrote every setting and froze a model Groq later
withdrew.

**HTTPS through the system's curl.** curl ships with Windows 10/11 and every
macOS, so nothing new is installed and nothing new is linked. The key and
body go in owner-only temporary files, never on a command line. On Windows
curl is started with `CreateProcess` and no console, because `_popen` from a
windowed program flashed a console up for every request.

**Speech is read before it is sent.** The recording goes to Whisper
(`whisper-large-v3-turbo`) on the same Groq account with the same key. The
transcript lands in the box, not straight in the pipeline: speech recognition
hears "fifteen" for "fifty", and a model built on a misheard rate compiles
and is wrong. Capture is miniaudio, one public-domain header (WASAPI on
Windows, Core Audio on a Mac), behind `IRecorder` so the suite drives the
whole talk-and-transcribe flow with a fake.

**The desktop app is a second face, not a second program.** `gui/` links the
same libraries as the terminal UI and keeps its rule: the text is the model,
and every view is redrawn from it after each edit. The run setup in the
toolbar rewrites lines of the `[Run]` block, the way the Runs tab's form
does. It uses Qt's Fusion style with one palette, so Windows and macOS look
the same. Qt's `slots` macro collides with the engine's `Schedule::slots()`,
so the GUI is built with `QT_NO_KEYWORDS`.

## Learned from testing on Qwen 3.8 27B

Every one of these was a wrong model on a real run. Each is now handled in
code rather than by asking the model more firmly:

- **"Thirty an hour, the teller takes 3 minutes"** became fixed gaps and fixed
  service. A bare number now means random with that mean, the rate hint says
  "on average", and a fixed time is listed as an assumption.
- **"About 15 minutes"** became exponential. The pre-parser now marks it
  `around` beside the words before the model reads them.
- **"After the doctor, 20% need an X-ray"**, with nothing said about the other
  80%, sent everyone to X-ray (140% busy). A branch whose target is also where
  the rest would fall through now sends the rest past it, and says so.
- **Answering "3"** to a server question came back as an empty form. Counts are
  filled in by code now, and other answers are merged.
- **The 404 for `llama-3.3-70b-versatile`.** The first build's settings file
  froze the default model, and Groq withdrew that model. It is ignored now,
  and a 404 says where the model name is set.

## Found along the way

- **A lone `Esc` on macOS and Linux hung** until the next key: the terminal
  waited, blocking, for the rest of an escape sequence. It now waits 50 ms.
  This was noticed when a recording's clock froze on Esc.
- **`^N` and opening a file dropped the session's hooks**: afterwards F2 said
  "not available in this build", and the Flow tab's picture opener had been
  gone since 16.2. `replaceWith()` now keeps them.
- **The macOS packaging check refused a good universal binary**:
  `otool -L` prints a heading per architecture, and the second heading was
  read as a library.
- **Windows offscreen screenshots in CI drew every letter as a box.** Qt's
  offscreen platform keeps its own font list and finds none on Windows. The
  real app was unaffected.
- **Newer Xcode SDKs no longer ship AGL**, which Qt 6.8's CMake files still
  link. Nothing here uses AGL, so it is filtered out of the link.
- **The reference manual's header reader** treated the `//` in
  `"https://..."` as a comment, lost `LlmConfig`'s closing brace, and listed
  the free functions after it as members. It now skips string literals.

## Packages

- **Terminal UI**: the Windows zip and installer and the universal macOS
  zip and dmg, as before. They now include F2 and F3.
- **Desktop app, Windows**: MSVC with Qt 6.8, gathered by `windeployqt` (Qt
  as separate DLLs, as its LGPL licence expects), zipped.
- **Desktop app, macOS 12+**: one universal binary, bundled by `macdeployqt`
  into `DES Simulator.app`, ad-hoc signed, on a dmg with an Applications
  shortcut. `Info.plist` carries `NSMicrophoneUsageDescription`; without it
  macOS ends the app the moment the microphone opens.

A `v17.0.0` tag builds all of them, terminal and desktop, and publishes them
with the changelog section as notes.

## What it is worth

The pipeline is tested end to end against recorded replies, with no network:
1943 checks in all. The models it built in testing (a bank, a clinic with
triage, doctor and X-ray, a shared-nurse ward) ran and gave sensible
utilisations once the fixes above were in.

Still to try: real microphones and real Whisper accuracy, on the laptops
this is sent to. There is no evaluation set yet. One would show what
`reasoning_effort: none` costs in quality against what it saves.
