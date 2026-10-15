# v16 — a model you can describe out loud

**Status:** proposal, not yet approved.
**Date:** 2026-10-15

---

## The goal

Somebody presses a key, says *"customers arrive every two minutes on average,
two tellers, service takes about three minutes, run it for an eight-hour day"*,
and a working `.des` model appears in the editor — compiled, with any
assumptions the generator made listed beside it, and undoable with `^Z` like
any other edit. They bring their own Anthropic API key.

## Why this is cheaper here than it looks

v15 already did the hard part: **the model is the text.** A language model is
good at writing text, and this project has three things most targets for
"generate it with AI" do not:

1. **A self-describing schema.** Since v14 every module and every column
   carries a line saying what it is for, in `ModuleRegistry` — not in the UI.
   That is the system prompt, generated, so it cannot drift from the compiler.
2. **Diagnostics at the cell.** `compile()` says *which* row and column is
   wrong and why. That is exactly the feedback a repair loop needs.
3. **A stability check.** The engine already refuses a model whose offered
   load exceeds capacity. A generated model that would never reach steady
   state is caught by the same gate a hand-written one is.

So generation is a loop: *describe → generate text → compile → feed the
diagnostics back → repeat (at most 3 rounds) → hand the buffer the result.*

## Layering

A third library, **`des_ai`**, links `des_engine` and never the reverse —
the same rule as `des_ui`. The engine stays network-free and offline; a
person who never sets a key never sees any of this.

```
des_engine  <--  des_ai  <--  des (CLI)     des generate ...
                    ^
des_ui  <-----------+------  des_tui        ^A  "Ask"
```

Inside `des_ai`, the network is an interface, the way the terminal is:

```cpp
class ITransport {                 // one HTTPS POST, JSON in, JSON out
public:
    virtual ~ITransport() = default;
    virtual void start(const std::string& url, const Headers&, const std::string& body) = 0;
    virtual TransportState poll() = 0;   // Pending / Done / Failed -- never blocks
    virtual std::string response() const = 0;
    virtual void cancel() = 0;
};
```

The real implementation lives in `tui/`-style platform code; the test suite
uses a `RecordedTransport` that replays saved responses, so the repair loop,
the JSON handling and every error path run in `des_tests` with no network,
under the sanitisers, like everything else.

### HTTPS without a dependency

**Spawn the system `curl`.** It ships with Windows 10 (1803+) and 11 at
`C:\Windows\System32\curl.exe`, and with every macOS. That keeps the package's
promise — no DLLs, nothing to install — and avoids linking WinHTTP on one side
and NSURLSession (Objective-C) on the other.

- The request body goes on curl's **stdin**, and so do the headers, via
  `curl --config -`. The API key therefore never appears on a command line,
  where any other process can read it.
- `poll()` checks the child process without blocking, so the TUI keeps
  redrawing and `Escape` cancels — the same `keyPending()` shape v12 gave the
  run loop.

The one new piece of portable code is a **small JSON writer/reader**
(`src/Json.cpp`): enough to build a Messages request and read `content`,
`stop_reason` and `usage` from the reply. No library; tested like `Lexer`.

## The API call

- **Endpoint:** `POST https://api.anthropic.com/v1/messages`, headers
  `x-api-key`, `anthropic-version: 2023-06-01`, `content-type: application/json`.
- **Model:** `claude-opus-5-5` by default, with `output_config.effort` set to
  `medium` explicitly. A setting can switch it to `claude-sonnet-5-5` or
  `claude-haiku-5-5` for people who want it cheaper; that is their call, so it
  is a setting, not a default.
- **Thinking:** adaptive (the default on this model; nothing to send).
- **System prompt, cached.** Built once from:
  - `ModuleRegistry::instance().all()` — every module, column, description,
    enum value and reference rule;
  - the expression language's function list (`EXPO`, `UNIF`, `NQ`, ...);
  - the four shipped `examples/models/*.des` as worked examples.

  It is identical byte for byte on every request, so it carries a
  `cache_control: {type: "ephemeral"}` breakpoint and repeat requests read it
  from cache rather than paying for it again. A `des schema --prompt` command
  prints it, so it can be read and diffed in review.
- **Structured output** via `output_config.format`, a JSON schema of:

  ```json
  { "model": "<the complete .des file>",
    "assumptions": ["service time taken as EXPO(3)", "..."] }
  ```

  The assumptions are shown to the person — *"I assumed exponential service"*
  is exactly what an Arena user would want to check before trusting a number.
- **Repair rounds** append the compiler's diagnostics as the next user turn
  (`Process row 2, Service (col 6): error: unknown function EXP`) and ask for
  the whole file again. The history only ever grows — never edited — which is
  what keeps the cache and the model's earlier reasoning valid. Three rounds,
  then stop and show what is left: a model with one error and a pointer to it
  is still useful, and an unbounded loop spends somebody else's money.
- **Check `stop_reason` before reading `content`:** `max_tokens` → retry with
  room, `refusal` → say so plainly, never paste a half answer into the buffer.
- **Editing, not only creating.** With text already in the buffer, the request
  sends the current file plus the instruction (*"add a second inspection
  station after Cut"*) and asks for the whole revised file. The TUI shows a
  line diff and `Enter` applies it as one undoable edit.

## The API key

- Read from `ANTHROPIC_API_KEY` if set.
- Otherwise `^K` in the TUI asks for it (input masked) and stores it in a
  per-user config file, created with owner-only permissions:
  - Windows: `%APPDATA%\DES Simulator\config`
  - macOS: `~/Library/Application Support/DES Simulator/config`
- It is **never** written into a `.des` file, a results file, a `.recovered`
  file, or a log, and the status bar shows only `sk-ant-…a1b2`. One unit test
  per output path checks the key is absent from the bytes written.
- **First use asks once**: *"Your description and the current model will be
  sent to Anthropic to generate the model. Continue?"* — because until now the
  README has promised "no internet connection", and that stays true for
  everybody who does not opt in.

## Voice

The Messages API takes text, images and documents, not audio, so speech has to
become text first. Two tiers, the first costing nothing:

**Tier A — the operating system's dictation (ship first).** Windows 11 voice
typing (`Win+H`) and macOS Dictation (press `Fn` twice / the mic key) both type
into whatever has focus — including a console or Terminal window. The TUI gets
a one-line **Ask** prompt (`^A`), the person dictates into it, presses
`Enter`. No audio code, no second API key, works offline for the speech part
on both platforms, and it is already as good as the OS's own recogniser. This
is a README section and a prompt line, not a subsystem.

**Tier B — push-to-talk inside the program (later, if Tier A is not enough).**
Hold a key, record, release, transcribe:

- Capture with **miniaudio** — one public-domain header, vendored into
  `third_party/`, using WASAPI on Windows and Core Audio on macOS, so the
  package still needs nothing installed. (macOS will ask for microphone
  permission the first time; the README says so.)
- Transcribe with either a **cloud speech-to-text API** (a second key, from a
  provider that accepts audio), or **whisper.cpp** locally with a small model
  shipped beside the exe — offline and private, at the cost of ~75–150 MB in
  the download.
- The transcript lands in the same Ask prompt as Tier A, editable before it is
  sent. Speech recognition gets numbers wrong ("two" vs "to", "4 min" vs
  "forty"), and a model built on a misheard rate is wrong in a way no compiler
  catches — so nothing is ever sent unread.

## What gets built, in order

Each step is usable on its own and gated like the rest of the project
(`des_tests`, `verify.sh`, baselines unchanged).

1. **`des schema --prompt`** — the generated reference. Offline, testable, and
   useful on its own as documentation.
2. **`src/Json.cpp`** — writer and reader, with tests.
3. **`des_ai`: `ModelGenerator`** — request building, the repair loop, the
   key handling, all against `ITransport`; tested with `RecordedTransport`
   fixtures (good model, model needing one repair, refusal, `max_tokens`,
   HTTP 401, HTTP 429, curl missing).
4. **`CurlTransport`** + **`des generate "description" -o model.des`** — the
   first thing a person can actually use, from a terminal.
5. **TUI: `^A` Ask prompt, `^K` key prompt, async generation with `Escape`
   to cancel, diff preview, apply as one undo step.** README sections for
   Windows voice typing and macOS Dictation. — *this is Tier A, done.*
6. *(optional)* **Tier B push-to-talk.**

## Decisions this proposal needs from you

- **Tier A first, Tier B only if needed?** Recommended: yes.
- **For Tier B, cloud STT or local whisper.cpp?** Local keeps the "works
  offline" story and a single API key; cloud keeps the download small.
- **Default model:** `claude-opus-5-5` (best results) vs. a cheaper default.
  Since each user pays with their own key, the proposal defaults to quality
  and makes the cheaper models a setting.
