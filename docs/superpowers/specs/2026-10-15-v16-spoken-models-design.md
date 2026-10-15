# v16 — a model you can describe out loud

**Status:** proposal, not yet approved. Revised 2026-10-15: designed for a
small, cheap model (Llama on Groq) rather than a frontier one.
**Date:** 2026-10-15

---

## The goal

Somebody presses a key, says *"customers arrive every two minutes on average,
two tellers, service takes about three minutes, run it for an eight-hour day"*,
and a working `.des` model appears in the editor — compiled, with the
assumptions it rests on listed, undoable with `^Z`. They bring their own key
for a cheap hosted model, or point it at a model running on their own machine.

## The principle: the model fills slots, our code builds the model

A frontier model can be handed the whole `.des` grammar and asked to write a
file. A small model cannot be trusted to: it will invent a column, misspell a
function, forget a `Next`, and every repair round costs another call. So the
split is the other way round:

| The LLM does | Our code does |
|---|---|
| Read a sentence, pick out *facts* into a small JSON shape | Units, numbers, distributions, defaults |
| Choose from a closed list of distribution words | Turn the facts into a `ModelDocument` |
| Answer "what is missing?" | Wiring: names, `Next` fields, the `Dispose` |
| | Compile, stability check, report |

The LLM never writes `.des` text. **Every model that reaches the buffer was
written by C++ we test**, so it is syntactically valid by construction; the
only question left is whether the *facts* were heard right, and that is what
the person checks.

This is also what makes it cheap: a 300-token system prompt and a 200-token
JSON answer, not a 10,000-token grammar and a whole file.

## The pipeline

```
speech ──► text ──► [1 pre-parse] ──► [2 LLM extract] ──► [3 validate IR]
                                                              │
        buffer ◄── [6 diff + apply] ◄── [5 compile] ◄── [4 build document]
                                            │
                              diagnostics ──┴──► ask the person, not the LLM
```

### 1. Pre-parse (deterministic, no LLM)

A small tokenizer marks up what code can already read reliably — and these are
exactly the things a small model gets wrong:

- **Numbers and number words:** `two` → 2, `a dozen` → 12, `one and a half` → 1.5.
- **Durations with units:** `every 2 minutes`, `90 seconds`, `an eight-hour day`
  → a value plus a unit, normalised to the model's time unit.
- **Rates vs. times:** `30 per hour` is an interarrival *of* 2 minutes. Code
  does that inversion, never the LLM.

The marked-up text goes to the LLM with the numbers already labelled, so its
job is attaching them to the right slot, not parsing them.

### 2. Extract into an intermediate representation (one LLM call)

The LLM is asked for JSON matching a deliberately small schema — the
**scenario IR** — at temperature 0, using the provider's JSON mode / structured
output:

```json
{
  "time_unit": "minutes",
  "run_length": {"value": 8, "unit": "hours"},
  "replications": 1,
  "arrivals": {"pattern": "random", "mean": 2, "unit": "minutes", "limit": null},
  "stations": [
    {"name": "Teller", "servers": 2,
     "service": {"pattern": "random", "mean": 3, "unit": "minutes"},
     "queue": "first come first served"}
  ],
  "routing": [],
  "assumptions": []
}
```

- `pattern` is a **closed vocabulary**: `fixed`, `random` (exponential),
  `between` (uniform, with `min`/`max`), `around` (triangular or normal, with a
  spread), `exactly-as-measured` (lognormal from mean and sd). Each maps to one
  distribution in code. The LLM chooses a word, never writes `EXPO(...)`.
- `routing` covers branching — `{"from": "Inspect", "to": "Rework",
  "probability": 0.1}` — which our code turns into `Decide` + `DecideBranch`.
- Anything the sentence does not say is **left null**, not guessed. The prompt
  says so, and step 3 enforces it.

The IR covers the Arena Basic Process cases people actually describe aloud:
Create → Process/Delay chains, shared resources, probabilistic branches,
balking limits, run length and replications. Assign, Batch, Separate and
Record stay in the editor; the generator says so rather than half-supporting
them.

### 3. Validate the IR (C++)

Parsed with the project's own JSON reader and checked field by field: types,
ranges (servers ≥ 1, probabilities in [0, 1] and summing to ≤ 1 per station),
references (`routing.to` names a station). A bad field is **re-asked for that
field alone** — *"Station Teller: service time is missing; the sentence was:
…"* — which is a much easier question for a small model than "fix the file".
One retry per field, then it becomes a question for the person (step 5).

### 4. Build the document (C++, no LLM)

`ScenarioBuilder` turns the IR into a `ModelDocument` through the same API the
TUI uses — `[Run]`, `[Create]`, one `[Process]` per station, `Decide` and
`DecideBranch` for routing, a `[Dispose]`, all `Next` fields wired, names
de-duplicated. Every IR default (`queue` → FIFO, missing `replications` → 1)
is applied here and added to `assumptions`, so the person sees it.

### 5. Compile, check, and ask

`compile()` and the offered-load check run as for any model. When something is
still wrong — a station whose utilisation exceeds 1, a null nobody filled — the
answer is a **question for the person**, shown in the Ask panel:

> *Teller would be busy 150% of the time (arrivals every 2 min, service 3 min,
> 1 server). How many tellers are there?*

That is slot filling: the reply goes back through steps 1–4 merged into the
existing IR. It is better engineering than a repair loop *and* cheaper — a
human knows the answer; an LLM would only guess one.

### 6. Edits, as operations

With a model already open, *"add an inspection after Cut, 10% go to rework"*
is extracted into **operations** on the IR — `add_station`, `set`,
`add_route`, `remove` — not into a new file. Our code applies them, rebuilds,
and the TUI shows a line diff; `Enter` applies it as one undo step. A small
model can emit `{"op": "add_station", "after": "Cut", ...}` reliably; it cannot
reliably re-emit a whole file unchanged except for one part.

## The provider: anything OpenAI-compatible

Groq, OpenRouter, Together, and local servers (Ollama, llama.cpp's
`llama-server`, LM Studio) all speak the same **OpenAI-compatible
`/chat/completions`** request. So the setting is three values, not a
provider integration:

```
base_url = https://api.groq.com/openai/v1
model    = llama-3.3-70b-versatile
api_key  = (from DES_LLM_API_KEY, or the ^K prompt)
```

- **Groq** is the default suggestion: fast enough that the round trip feels
  like part of typing, and cheap enough not to matter per model. A 70B Llama
  for extraction; an 8B one is worth trying once the eval set (below) can
  measure whether it holds up.
- **Local** (`base_url = http://localhost:11434/v1`, no key) means free and
  fully offline, for anybody willing to run Ollama. The design costs nothing
  extra to support it, and it keeps the README's "no internet needed" true for
  people who want that.
- Model names change on hosted providers; they are a setting, never compiled
  in, and the README points at the provider's model list.

### HTTPS without a dependency

**Spawn the system `curl`**, which ships with Windows 10 (1803+) / 11 and every
macOS, so the package still needs nothing installed. Headers and body go on
curl's stdin (`curl --config -`), so the key never appears on a command line.
The child is polled without blocking, so the TUI keeps drawing and `Escape`
cancels.

### Layering

A third library, **`des_ai`**, links `des_engine`, never the reverse. The
network is an interface, like the terminal:

```cpp
class ITransport {   // one HTTPS POST; poll() never blocks
public:
    virtual ~ITransport() = default;
    virtual void start(const std::string& url, const Headers&, const std::string& body) = 0;
    virtual TransportState poll() = 0;
    virtual std::string response() const = 0;
    virtual void cancel() = 0;
};
```

`CurlTransport` is the real one; `RecordedTransport` replays saved responses,
so everything above runs in `des_tests` under the sanitisers with no network.
New portable pieces: `Json` (writer + reader), `PreParse`, `ScenarioIR`,
`ScenarioBuilder`, `ModelGenerator`. Only the pre-parser and builder carry
real logic, and both are pure functions — the easiest code in the project to
test.

## Voice

**Tier A — the operating system's dictation (ship first, no code).** Windows
voice typing (`Win+H`) and macOS Dictation type into a console like any other
window. The person dictates into the `^A` Ask prompt and presses `Enter`.

**Tier B — push-to-talk in the program.** Hold a key, record, release:

- Capture with **miniaudio** (one public-domain header, vendored; WASAPI on
  Windows, Core Audio on macOS — nothing to install).
- Transcribe with **Whisper through the same provider**: Groq serves Whisper on
  its OpenAI-compatible `/audio/transcriptions` endpoint, so it is **the same
  key and the same transport**, a multipart upload instead of JSON. Locally,
  whisper.cpp's server speaks the same endpoint.

Either way the transcript lands in the Ask prompt **editable, never sent
unread**: speech recognition confuses "two" and "to", "fifteen" and "fifty",
and a misheard rate gives a model that compiles and is wrong.

## The key

- `DES_LLM_API_KEY` if set; otherwise `^K` asks (masked) and stores it in a
  per-user config with owner-only permissions (`%APPDATA%\DES Simulator\config`
  / `~/Library/Application Support/DES Simulator/config`).
- Never written into a `.des`, results, `.recovered` file or log; the status
  bar shows `gsk_…a1b2`. One test per output path checks the key is absent
  from the bytes written.
- First use asks once before anything is sent to a hosted provider.

## The engineering that makes a small model enough: an eval set

`tests/scenarios/` holds descriptions, each with the IR it should produce:

```
tests/scenarios/teller-two-servers.txt     "customers arrive every two minutes…"
tests/scenarios/teller-two-servers.ir.json the expected IR
```

- **Offline, in `des_tests`:** the pre-parser, validator and builder run on
  every case with the LLM step replaced by the expected IR. Always on.
- **Live, by hand:** `des eval-llm --model llama-3.1-8b-instant` sends every
  description to the configured provider and scores the IR field by field.
  This is how a cheaper model is chosen, how a prompt change is judged, and
  how a regression is noticed — with a number, not an impression. It costs
  cents per run on Groq and nothing locally.

Start with ~30 cases drawn from the coursework and `examples/models/`, plus the
awkward ones: rates instead of times, number words, "about", missing facts,
two stations sharing a resource.

## What gets built, in order

Each step is usable and gated on its own (`des_tests`, `verify.sh`, baselines
unchanged).

1. **`ScenarioIR` + `ScenarioBuilder`** — IR in, `ModelDocument` out. No
   network, no LLM; useful immediately as `des new scenario.json`.
2. **`PreParse`** with its own tests, and the offline half of the eval set.
3. **`Json`**, **`ModelGenerator`** against `ITransport`, tested with recorded
   responses (good extraction, missing field, bad JSON, HTTP 401, 429, curl
   missing).
4. **`CurlTransport`** + **`des generate "…" -o model.des`** and
   **`des eval-llm`** — first usable from a terminal; pick the default model
   here, by measurement.
5. **TUI: `^A` Ask (with clarifying questions), `^K` key, async with `Escape`,
   edit operations with diff preview.** README section on OS dictation.
   *Tier A done.*
6. *(optional)* **Tier B** push-to-talk with Whisper.

## Decisions this needs from you

- **IR scope:** Create/Process/Delay/Decide/shared resources/run setup only,
  with Batch/Separate/Assign/Record left to the editor? Recommended: yes.
- **Default provider:** Groq with a 70B Llama, switching to 8B if the eval set
  says it holds up. Local Ollama documented as the free, offline option.
- **Tier B now or later?** Recommended: later — Tier A gets voice working
  with zero code.
