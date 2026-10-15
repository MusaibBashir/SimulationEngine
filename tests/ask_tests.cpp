// ============================================================================
// ask_tests.cpp  --  v17: F2, describe a model, driven by keys like any other
// part of the UI. The language model is a function installed here, and the
// background thread is replaced by one that runs the work when told to, so
// "still waiting" is a state a test can stand in.
// ============================================================================

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>
#include "harness.hpp"
#include "des_ui.hpp"
#include "ScenarioBuilder.hpp"

#ifndef _WIN32
  #include <sys/stat.h>
#endif

using namespace des;
using des_test::check;
using des_test::section;
using des_test::skip;

namespace {

const char* kTeller = R"({"time_unit": "minutes", "run_length": {"value": 8, "unit": "hours"},
  "arrivals": [{"name": "Customers", "every": {"pattern": "random", "mean": 2, "unit": "minutes"}}],
  "steps": [{"name": "Teller", "servers": 2, "time": {"pattern": "random", "mean": 3, "unit": "minutes"}}]})";

GenerateResult built(const char* scenario) {
    GenerateResult r;
    r.scenarioJson = parseJson(scenario).value;
    const ScenarioRead read = readScenario(r.scenarioJson);
    r.scenario = read.scenario;
    r.questions = read.questions;
    BuiltScenario b = buildScenario(read.scenario);
    r.document = std::move(b.document);
    r.assumptions = b.assumptions;
    r.notes = {"30 an hour = one every 2 minutes"};
    r.ok = read.questions.empty();
    r.calls = 1;
    return r;
}

// Everything the panel sends out, and a way to hold the work back.
struct Fake {
    std::vector<std::string> texts;
    std::vector<JsonValue>   previous;
    std::vector<std::string> keys;        // the key each request carried
    std::vector<std::string> saved;       // keys saveKey was given
    std::function<GenerateResult()> answer = [] { return built(kTeller); };
    std::vector<std::function<void()>> held;
    bool hold{false};

    AskHooks hooks() {
        AskHooks h;
        h.generate = [this](const std::string& t, const JsonValue& p, const LlmConfig& c) {
            texts.push_back(t);
            previous.push_back(p);
            keys.push_back(c.apiKey);
            return answer();
        };
        h.saveKey = [this](const std::string& k, std::string&) { saved.push_back(k); return true; };
        h.launch = [this](std::function<void()> work) {
            if (hold) held.push_back(std::move(work));
            else      work();
        };
        return h;
    }
};

void typeText(TuiState& s, const std::string& text) {
    for (const char c : text) handleKey(s, Key::character(c));
}

std::string screenText(const TuiState& s, int w = 90, int h = 28) {
    Screen screen(w, h);
    render(s, screen);
    std::string all;
    for (int y = 0; y < h; ++y) all += screen.line(y) + "\n";
    return all;
}

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

bool logHas(const TuiState& s, AskLine::Kind kind, const std::string& needle) {
    for (const AskLine& l : s.askLog())
        if (l.kind == kind && contains(l.text, needle)) return true;
    return false;
}

TuiState withKey(Fake& fake, const std::string& text = "") {
    TuiState s = TuiState::fromText(text, "ask.des");
    LlmConfig c;
    c.apiKey = "gsk_alreadyhere0123456789";
    s.setLlmConfig(c);
    s.setAskHooks(fake.hooks());
    return s;
}

void setEnv(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void testOpening() {
    section("v17: F2 opens the Ask panel");
    Fake fake;
    TuiState s = withKey(fake);
    handleKey(s, Key::function(2));
    check(s.overlay() == Overlay::Ask, "F2 opens it");
    const std::string screen = screenText(s);
    check(contains(screen, "describe the model -- sent to api.groq.com, qwen/qwen3.8-27b"),
          "the panel says where the words go, and to which model");
    check(contains(screen, "^T to speak it instead"), "...and how to speak instead of typing");
    check(contains(screen, "Enter sends") && contains(screen, "^K API key"), "the hint line has its keys");
    handleKey(s, Key::function(2));
    check(s.overlay() == Overlay::None, "F2 again closes it");

    TuiState t = withKey(fake);
    const Layout L = layoutFor(t, 90, 28);
    check(contains(screenText(t), "Ask F2"), "the top bar has an Ask button");
    handleKey(t, Key::mouse(MouseButton::Left, L.askStart + 1, L.barRow), 90, 28);
    check(t.overlay() == Overlay::Ask, "...and clicking it opens the panel, for a Mac keyboard's sake");

    TuiState narrow = withKey(fake);
    const Layout N = layoutFor(narrow, 80, 24);
    Screen sc(80, 24);
    render(narrow, sc);
    check(sc.line(0).find("Ask F2") == static_cast<std::size_t>(N.askStart + 1),
          "at 80 columns the button is not drawn over by the file name");

    handleKey(narrow, Key::function(1));
    check(contains(screenText(narrow, 80, 24), "F2"), "the key map lists F2");
}

void testDescribe() {
    section("v17: a description becomes the model, as one edit");
    Fake fake;
    TuiState s = withKey(fake, "# what was here before\n");
    handleKey(s, Key::function(2));
    typeText(s, "customers come thirty an hour, two tellers, 3 minutes each");
    handleKey(s, Key::special(KeyKind::Enter));
    check(fake.texts.size() == 1 && fake.texts[0] == "customers come thirty an hour, two tellers, 3 minutes each",
          "Enter sends exactly what was typed");
    check(fake.keys.size() == 1 && fake.keys[0] == "gsk_alreadyhere0123456789", "...with the configured key");
    check(s.askBusy(), "until the answer is collected, it is waiting");
    s.pollAsk();
    check(!s.askBusy(), "pollAsk collects it");
    check(contains(s.buffer().text(), "[Create]") && contains(s.buffer().text(), "Interarrival = EXPO(2)"),
          "the model replaces the text");
    check(!contains(s.buffer().text(), "what was here before"), "...all of it");
    check(!hasErrors(s.diagnostics()), "and it compiles in the editor");
    check(logHas(s, AskLine::Kind::Understood, "one every 2 minutes"), "what was understood is shown");
    check(logHas(s, AskLine::Kind::Assumed, "exponential"), "every assumption is shown");
    check(s.overlay() == Overlay::Ask, "the panel stays open, to read and to change it");
    check(contains(screenText(s), "assumed: "), "...and draws them");

    handleKey(s, Key::special(KeyKind::Enter));
    check(s.overlay() == Overlay::None, "Enter on an empty line closes it");
    handleKey(s, Key::control('z'));
    check(s.buffer().text() == "# what was here before\n", "one ^Z puts back exactly what was there");
}

void testQuestions() {
    section("v17: missing facts are asked, and the answer continues the form");
    Fake fake;
    fake.answer = [] {
        GenerateResult r = built(R"({"arrivals": [{"every": {"pattern": "random", "mean": 2}}],
                                     "steps": [{"name": "Wash", "time": null}]})");
        return r;
    };
    TuiState s = withKey(fake);
    handleKey(s, Key::function(2));
    typeText(s, "cars every 2 minutes get washed");
    handleKey(s, Key::special(KeyKind::Enter));
    s.pollAsk();
    check(logHas(s, AskLine::Kind::Question, "How long does Wash take"), "the question is in the panel");
    check(s.buffer().text().empty(), "and nothing is written while something is missing");
    check(contains(screenText(s), "? How long does Wash take"), "...drawn with a ?");

    fake.answer = [] { return built(kTeller); };
    typeText(s, "about 4 minutes");
    handleKey(s, Key::special(KeyKind::Enter));
    check(fake.previous.size() == 2 && fake.previous[1].isObject() &&
              fake.previous[1]["steps"].items()[0]["name"].asString() == "Wash",
          "the answer is sent with the form so far");
    s.pollAsk();
    check(contains(s.buffer().text(), "[Process]"), "and finishes the model");

    // A hand edit since then is replaced by the next change -- said first.
    handleKey(s, Key::special(KeyKind::Escape));
    handleKey(s, Key::control('b'));
    typeText(s, "# mine");
    handleKey(s, Key::function(2));
    typeText(s, "add a third teller");
    handleKey(s, Key::special(KeyKind::Enter));
    check(logHas(s, AskLine::Kind::Info, "edited by hand"), "a hand edit is warned about before it is replaced");
    s.pollAsk();
}

void testNewModelKeepsSettings() {
    section("v17: a new or reopened model keeps F2 working");
    Fake fake;
    TuiState s = withKey(fake, "old\n");
    std::vector<std::string> opened;
    s.setOpener([&opened](const std::string& f) { opened.push_back(f); return true; });

    // ^N and a name: the whole state is rebuilt for the new model.
    handleKey(s, Key::control('n'));
    typeText(s, "fresh_model_for_ask_test");
    handleKey(s, Key::special(KeyKind::Enter));
    check(s.buffer().text().empty() && contains(s.path(), "fresh_model_for_ask_test"),
          "^N made a new, empty model");
    check(s.llmConfig().apiKey == "gsk_alreadyhere0123456789", "...and the key survived it");
    handleKey(s, Key::function(2));
    typeText(s, "two tellers");
    handleKey(s, Key::special(KeyKind::Enter));
    check(fake.texts.size() == 1 && fake.keys[0] == "gsk_alreadyhere0123456789",
          "F2 still sends after ^N -- it once said \"not available in this build\"");
    check(!logHas(s, AskLine::Kind::Error, "not available"), "...with no such error");
    s.pollAsk();
    check(contains(s.buffer().text(), "[Create]"), "and the model lands in the new file");

    TuiState t = withKey(fake);
    t.setOpener([&opened](const std::string& f) { opened.push_back(f); return true; });
    t.newBlankModel("another.des");
    check(t.llmConfig().apiKey == "gsk_alreadyhere0123456789", "newBlankModel keeps the key");
    t.setClipboard("kept");
    t.newBlankModel("third.des");
    check(t.clipboard() == "kept", "...and the clipboard, which belongs to the session too");
}

// A microphone that records what the test says it heard.
class FakeMic : public IRecorder {
public:
    double secs{0.0};
    std::size_t samples{16000};    // one second
    bool fail{false};
    bool* stopped{nullptr};
    bool start(std::string& error) override {
        if (fail) error = "could not open the microphone (no device)";
        return !fail;
    }
    std::vector<std::int16_t> stop() override {
        if (stopped) *stopped = true;
        return std::vector<std::int16_t>(samples, 100);
    }
    double seconds() const override { return secs; }
    float level() const override { return 0.5f; }
};

void testTalking() {
    section("v17: speaking into it");
    Fake fake;
    std::vector<std::size_t> wavSizes;
    FakeMic* mic = nullptr;
    bool micFails = false;
    std::size_t micSamples = 16000;
    AskHooks h = fake.hooks();
    h.openMicrophone = [&mic, &micFails, &micSamples](std::string&) {
        auto m = std::unique_ptr<FakeMic>(new FakeMic());
        m->fail = micFails;
        m->samples = micSamples;
        mic = m.get();
        return std::unique_ptr<IRecorder>(std::move(m));
    };
    std::string heard = "patients arrive every 5 minutes";
    h.transcribe = [&wavSizes, &heard](const std::string& wav, const LlmConfig&) {
        wavSizes.push_back(wav.size());
        TranscriptResult r;
        r.ok = true;
        r.text = heard;
        return r;
    };

    TuiState s = TuiState::fromText("", "talk.des");
    LlmConfig c;
    c.apiKey = "gsk_alreadyhere0123456789";
    s.setLlmConfig(c);
    s.setAskHooks(h);

    handleKey(s, Key::function(3));
    check(s.overlay() == Overlay::Ask && s.recording(), "F3 opens the panel and starts recording");
    mic->secs = 4.0;
    std::string screen = screenText(s);
    check(contains(screen, "* recording 0:04") && contains(screen, "[#"), "the panel shows the time and a level meter");
    check(contains(screen, "^T or Enter  stop and transcribe"), "...and how to stop");
    typeText(s, "x");
    check(s.askDraft().empty(), "typing while recording does nothing");
    handleKey(s, Key::control('t'));
    check(!s.recording() && s.transcribing(), "^T stops it and sends it for transcription");
    check(wavSizes.size() == 1 && wavSizes[0] == 44 + 32000, "one second at 16 kHz, as a WAV file");
    check(contains(screenText(s), "turning speech into text with whisper-large-v3-turbo"), "the panel says so");
    s.pollAsk();
    check(!s.askBusy() && s.askDraft() == "patients arrive every 5 minutes",
          "the transcript lands in the box -- not sent");
    check(fake.texts.empty(), "nothing goes to the language model until Enter");

    heard = "two tellers";
    handleKey(s, Key::control('t'));
    handleKey(s, Key::special(KeyKind::Enter));
    check(!s.recording(), "Enter also stops a recording");
    s.pollAsk();
    check(s.askDraft() == "patients arrive every 5 minutes two tellers", "a second recording adds to the box");
    handleKey(s, Key::special(KeyKind::Enter));
    check(fake.texts.size() == 1 && fake.texts[0] == "patients arrive every 5 minutes two tellers",
          "Enter then sends the words as checked");
    s.pollAsk();

    bool stopped = false;
    handleKey(s, Key::control('t'));
    mic->stopped = &stopped;
    handleKey(s, Key::special(KeyKind::Escape));
    check(!s.recording() && !s.transcribing() && stopped && wavSizes.size() == 2,
          "Esc throws a recording away: the microphone stops and nothing is sent");

    micSamples = 2000;
    handleKey(s, Key::control('t'));
    handleKey(s, Key::control('t'));
    check(!s.transcribing() && wavSizes.size() == 2 && logHas(s, AskLine::Kind::Info, "too short"),
          "a recording under half a second is not sent");
    micSamples = 16000;

    handleKey(s, Key::control('t'));
    mic->secs = MAX_TALK_SECONDS + 1;
    s.pollAsk();
    check(!s.recording() && wavSizes.size() == 3 && logHas(s, AskLine::Kind::Info, "two minutes"),
          "a recording stops itself at two minutes");
    s.pollAsk();

    micFails = true;
    handleKey(s, Key::control('t'));
    check(!s.recording() && logHas(s, AskLine::Kind::Error, "could not open the microphone"),
          "no microphone is an error in the panel, not a crash");
    micFails = false;

    fake.hold = true;
    handleKey(s, Key::control('t'));
    h.transcribe = nullptr;
    handleKey(s, Key::control('t'));
    check(s.transcribing() && fake.held.size() == 1, "transcription waits in the background");
    handleKey(s, Key::special(KeyKind::Escape));
    check(!s.transcribing(), "Esc stops waiting for it");
    fake.held[0]();
    const std::string before = s.askDraft();
    s.pollAsk();
    check(s.askDraft() == before, "a transcript nobody is waiting for changes nothing");

    // With no key, the key comes first and the recording starts once it is in.
    Fake fresh;
    TuiState k = TuiState::fromText("", "talk2.des");
    AskHooks kh = fresh.hooks();
    kh.openMicrophone = h.openMicrophone;
    kh.transcribe = [](const std::string&, const LlmConfig&) { TranscriptResult r; r.ok = true; return r; };
    k.setAskHooks(kh);
    handleKey(k, Key::function(3));
    check(k.enteringKey() && !k.recording(), "talking with no key asks for the key first");
    typeText(k, "gsk_typed0123456789wxyz");
    handleKey(k, Key::special(KeyKind::Enter));
    check(k.recording(), "...and records once it is entered");
    handleKey(k, Key::control('t'));
    k.pollAsk();
    check(logHas(k, AskLine::Kind::Info, "Nothing was heard"), "an empty transcript says nothing was heard");

    TuiState none = withKey(fake);
    AskHooks nh = Fake().hooks();
    none.setAskHooks(nh);
    handleKey(none, Key::function(3));
    check(logHas(none, AskLine::Kind::Error, "Win+H"), "a build with no microphone says what to use instead");
}

void testWaiting() {
    section("v17: waiting, and stopping waiting");
    Fake fake;
    fake.hold = true;
    TuiState s = withKey(fake, "keep me\n");
    handleKey(s, Key::function(2));
    typeText(s, "two tellers");
    handleKey(s, Key::special(KeyKind::Enter));
    check(s.askBusy() && fake.held.size() == 1, "the request is out");
    check(contains(screenText(s), "asking qwen/qwen3.8-27b"), "the panel says it is waiting");
    check(contains(screenText(s), "Esc  stop waiting"), "...and how to stop");
    typeText(s, "x");
    check(s.askDraft().empty(), "typing while waiting does not queue up words");
    handleKey(s, Key::special(KeyKind::Escape));
    check(!s.askBusy() && s.overlay() == Overlay::Ask, "Esc stops waiting and leaves the panel open");
    fake.held[0]();     // the answer arrives late
    s.pollAsk();
    check(s.buffer().text() == "keep me\n", "an answer nobody is waiting for changes nothing");

    fake.hold = false;
    fake.answer = [] { GenerateResult r; r.error = "the provider refused the API key (HTTP 401)"; return r; };
    typeText(s, "two tellers");
    handleKey(s, Key::special(KeyKind::Enter));
    s.pollAsk();
    check(logHas(s, AskLine::Kind::Error, "refused the API key"), "an error is shown in the panel");
    check(s.buffer().text() == "keep me\n", "...and changes nothing");
}

void testKey() {
    section("v17: the API key");
    Fake fake;
    TuiState s = TuiState::fromText("", "ask.des");
    s.setAskHooks(fake.hooks());
    check(s.llmConfig().needsKey(), "Groq with no key needs one");
    handleKey(s, Key::function(2));
    check(contains(s.status(), "^K"), "opening with no key says how to give one");
    typeText(s, "a teller");
    handleKey(s, Key::special(KeyKind::Enter));
    check(s.enteringKey() && fake.texts.empty(), "sending with no key asks for the key first");
    check(logHas(s, AskLine::Kind::Info, "console.groq.com/keys"), "...and says where to get one");
    typeText(s, "gsk_secret0123456789wxyz");
    const std::string shown = screenText(s);
    check(contains(shown, "API key: ********************wxyz") && !contains(shown, "secret"),
          "the key is drawn as stars and its last four");
    handleKey(s, Key::special(KeyKind::Enter));
    check(fake.saved.size() == 1 && fake.saved[0] == "gsk_secret0123456789wxyz", "Enter saves it");
    check(fake.texts.size() == 1 && fake.texts[0] == "a teller" && fake.keys[0] == "gsk_secret0123456789wxyz",
          "...and the waiting description goes out with it");
    check(logHas(s, AskLine::Kind::Info, "gsk_...wxyz saved"), "the log names the key only by its ends");
    s.pollAsk();
    for (const AskLine& l : s.askLog())
        check(!contains(l.text, "secret"), "no line of the conversation holds the key");

    handleKey(s, Key::control('k'));
    check(s.enteringKey(), "^K in the panel changes the key");
    handleKey(s, Key::special(KeyKind::Escape));
    check(!s.enteringKey() && s.llmConfig().apiKey == "gsk_secret0123456789wxyz",
          "Esc leaves the key as it was");

    LlmConfig local;
    local.baseUrl = "http://localhost:11434/v1";
    check(!local.needsKey() && local.host() == "localhost:11434", "a local server needs no key");

    const LlmConfig parsed = parseLlmConfig("# comment\nbase_url = http://x/v1/\nmodel = m\napi_key = k\nfuture = 1\n");
    check(parsed.baseUrl == "http://x/v1" && parsed.model == "m" && parsed.apiKey == "k",
          "the settings file is read, an unknown key ignored");
    check(parseLlmConfig(llmConfigText(parsed)).apiKey == "k", "...and written back the same");

    // Saving a key changes the key and nothing else.
    const std::string fresh = withApiKey("", "gsk_new");
    check(parseLlmConfig(fresh).apiKey == "gsk_new" && parseLlmConfig(fresh).model == LlmConfig{}.model,
          "a new settings file names the key and leaves the model to the default");
    check(contains(fresh, "# model = "), "...showing the model as a comment, to change by hand");
    const std::string chosen = withApiKey("# mine\nmodel = some/other-model\napi_key = old\nfuture = 3\n", "gsk_new");
    check(chosen == "# mine\nmodel = some/other-model\napi_key = gsk_new\nfuture = 3\n",
          "in an existing file only the api_key line changes -- a chosen model is kept");

    // The first build froze its default model into the file when it saved a
    // key; Groq has since withdrawn that model.
    const std::string firstBuild = "base_url = https://api.groq.com/openai/v1\nmodel = llama-3.3-70b-versatile\n"
                                   "api_key = gsk_old\nreasoning_format = \nreasoning_effort = \n";
    check(parseLlmConfig(firstBuild).model == "qwen/qwen3.8-27b" && parseLlmConfig(firstBuild).apiKey == "gsk_old",
          "a file the first build wrote uses the current default model, and keeps its key");
    const std::string resaved = withApiKey(firstBuild, "gsk_new");
    check(contains(resaved, "# model = llama-3.3-70b-versatile") && parseLlmConfig(resaved).model == "qwen/qwen3.8-27b",
          "saving a key again comments that model out, saying why");

    // saveApiKey writes into the user's settings folder: pointed at a scratch
    // one here, so the test never touches a real key.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "des_ask_settings";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
#ifdef _WIN32
    setEnv("APPDATA", dir.string());
#elif defined(__APPLE__)
    const char* oldHome = std::getenv("HOME");
    const std::string keepHome = oldHome ? oldHome : "";
    setEnv("HOME", dir.string());
#else
    setEnv("XDG_CONFIG_HOME", dir.string());
#endif
    std::string error;
    check(saveApiKey("gsk_saved0123456789", error), "the key is saved: " + error);
    check(LlmConfig::load().apiKey == "gsk_saved0123456789" ||
              std::getenv("DES_LLM_API_KEY") != nullptr || std::getenv("GROQ_API_KEY") != nullptr,
          "...and loaded back");
#ifndef _WIN32
    struct stat st{};
    check(::stat(llmConfigPath().c_str(), &st) == 0 && (st.st_mode & 0777) == 0600,
          "the settings file is readable by this user alone");
#else
    skip(1, "file permission bits are POSIX");
#endif
#if defined(__APPLE__)
    setEnv("HOME", keepHome);
#endif
    std::filesystem::remove_all(dir, ec);
}

}  // namespace

void runAskTests() {
    testOpening();
    testDescribe();
    testQuestions();
    testNewModelKeepsSettings();
    testTalking();
    testWaiting();
    testKey();
}
