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
    check(contains(screen, "describe the model -- sent to api.groq.com, llama-3.3-70b-versatile"),
          "the panel says where the words go, and to which model");
    check(contains(screen, "Win+H") && contains(screen, "Dictation"),
          "...and how to speak instead of typing, on both systems");
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

void testWaiting() {
    section("v17: waiting, and stopping waiting");
    Fake fake;
    fake.hold = true;
    TuiState s = withKey(fake, "keep me\n");
    handleKey(s, Key::function(2));
    typeText(s, "two tellers");
    handleKey(s, Key::special(KeyKind::Enter));
    check(s.askBusy() && fake.held.size() == 1, "the request is out");
    check(contains(screenText(s), "asking llama-3.3-70b-versatile"), "the panel says it is waiting");
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
    check(logHas(s, AskLine::Kind::Info, "gsk_…wxyz saved"), "the log names the key only by its ends");
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
    testWaiting();
    testKey();
}
