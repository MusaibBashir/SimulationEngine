#include "ModelGenerator.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include "Compiler.hpp"
#include "PreParse.hpp"
#include "ScenarioBuilder.hpp"

#ifndef _WIN32
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <unistd.h>
#endif

namespace des {

namespace {

std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v == nullptr ? std::string() : std::string(v);
}

using Messages = std::vector<std::pair<std::string, std::string>>;

// A reasoning model's <think>...</think>, should a server pass it through in
// the content. Its braces would otherwise be the first "{" the JSON search
// finds.
std::string withoutThinking(std::string text) {
    for (std::size_t open = text.find("<think>"); open != std::string::npos; open = text.find("<think>")) {
        const std::size_t close = text.find("</think>", open);
        text.erase(open, close == std::string::npos ? std::string::npos : close + 8 - open);
    }
    return text;
}

// The reply's text, or an explanation of why there is none.
bool replyText(const HttpResponse& r, std::string& text, std::string& error) {
    if (!r.ok) { error = r.error.empty() ? "the request did not complete" : r.error; return false; }
    if (r.status == 401 || r.status == 403) {
        error = "the provider refused the API key (HTTP " + std::to_string(r.status) +
                ") -- check DES_LLM_API_KEY";
        return false;
    }
    if (r.status == 429) {
        error = "the provider is rate-limiting this key (HTTP 429) -- wait a moment and try again";
        return false;
    }
    const JsonParse p = parseJson(r.body);
    if (r.status != 200) {
        const std::string why = p.ok ? p.value["error"]["message"].asString() : std::string();
        error = "the provider answered HTTP " + std::to_string(r.status) +
                (why.empty() ? std::string() : ": " + why);
        // A 404 from a chat endpoint is nearly always the model name. Say
        // where it is set, since that is the one thing the person can change.
        if (r.status == 404)
            error += " -- the model is set by `model =` in " + llmConfigPath() +
                     " (or DES_LLM_MODEL)";
        return false;
    }
    if (!p.ok) { error = "the provider's reply was not JSON: " + p.error; return false; }
    const JsonValue& choices = p.value["choices"];
    if (!choices.isArray() || choices.items().empty()) { error = "the reply had no choices"; return false; }
    const JsonValue& choice = choices.items().front();
    if (choice["finish_reason"].isString() && choice["finish_reason"].asString() == "length") {
        error = "the reply was cut off before it finished";
        return false;
    }
    const JsonValue& content = choice["message"]["content"];
    if (!content.isString() || content.asString().empty()) { error = "the reply was empty"; return false; }
    text = withoutThinking(content.asString());
    if (text.find_first_not_of(" \t\r\n") == std::string::npos) {
        error = "the reply held only the model's thinking, no answer";
        return false;
    }
    return true;
}

// A short answer to "how many servers does X have?" -- "3", "take 4 servers",
// "there are 2 machines" -- is filled in by code. A language model given "3"
// and a form once sent back a form with no arrivals and no steps; arithmetic
// on one field cannot go wrong that way, and costs nothing.
bool fillServersDirectly(JsonValue& form, const Question& q, const std::string& answer) {
    const std::string tail = ".servers";
    if (q.path.size() <= tail.size() || q.path.compare(q.path.size() - tail.size(), tail.size(), tail) != 0)
        return false;
    const std::size_t open = q.path.find('['), close = q.path.find(']');
    if (open == std::string::npos || close == std::string::npos || close < open) return false;
    const std::string list = q.path.substr(0, open);
    const std::size_t index = static_cast<std::size_t>(std::atoi(q.path.substr(open + 1, close - open - 1).c_str()));

    // Exactly one whole number, a short reply, and no time unit: "3 minutes"
    // is a change to a time, not a count, and goes to the model.
    const std::string text = preParse(answer).text;
    int numbers = 0, words = 0, value = 0;
    bool fraction = false;
    std::string word;
    auto endWord = [&] {
        if (word.empty()) return;
        ++words;
        std::string w = word;
        for (char& c : w) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        TimeUnit unused{};
        if (parseTimeUnit(w, unused)) numbers = 99;
        word.clear();
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (std::isdigit(c)) {
            endWord();
            std::size_t j = i;
            while (j < text.size() && (std::isdigit(static_cast<unsigned char>(text[j])) || text[j] == '.')) ++j;
            const std::string n = text.substr(i, j - i);
            fraction = fraction || n.find('.') != std::string::npos;
            value = std::atoi(n.c_str());
            ++numbers;
            ++words;
            i = j - 1;
        } else if (std::isalpha(c)) {
            word += static_cast<char>(c);
        } else {
            if (c == '%') numbers = 99;
            endWord();
        }
    }
    endWord();
    if (numbers != 1 || fraction || value < 1 || words > 6) return false;

    const JsonValue& items = form[list];
    if (!items.isArray() || index >= items.items().size() || !items.items()[index].isObject()) return false;
    JsonValue rebuilt = JsonValue::array();
    for (std::size_t i = 0; i < items.items().size(); ++i) {
        JsonValue item = items.items()[i];
        if (i == index) item.set("servers", JsonValue::number(value));
        rebuilt.push(item);
    }
    form.set(list, rebuilt);
    return true;
}

std::string issueList(const std::vector<ScenarioIssue>& issues) {
    std::string s;
    for (const ScenarioIssue& i : issues)
        s += "- " + (i.path.empty() ? std::string("(whole answer)") : i.path) + ": " + i.message + "\n";
    return s;
}

}  // namespace

LlmConfig LlmConfig::fromEnvironment() {
    LlmConfig c;
    if (!env("DES_LLM_BASE_URL").empty()) c.baseUrl = env("DES_LLM_BASE_URL");
    if (!env("DES_LLM_MODEL").empty())    c.model   = env("DES_LLM_MODEL");
    c.apiKey = env("DES_LLM_API_KEY");
    if (c.apiKey.empty()) c.apiKey = env("GROQ_API_KEY");
    while (!c.baseUrl.empty() && c.baseUrl.back() == '/') c.baseUrl.pop_back();
    return c;
}

LlmConfig LlmConfig::load() {
    LlmConfig c;
    std::ifstream in(llmConfigPath(), std::ios::binary);
    if (in.is_open())
        c = parseLlmConfig(std::string((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>()));
    const LlmConfig e = fromEnvironment();
    if (!env("DES_LLM_BASE_URL").empty()) c.baseUrl = e.baseUrl;
    if (!env("DES_LLM_MODEL").empty())    c.model   = e.model;
    if (!e.apiKey.empty())                c.apiKey  = e.apiKey;
    return c;
}

std::string LlmConfig::host() const {
    std::string h = baseUrl;
    const std::size_t scheme = h.find("://");
    if (scheme != std::string::npos) h = h.substr(scheme + 3);
    const std::size_t slash = h.find('/');
    return slash == std::string::npos ? h : h.substr(0, slash);
}

bool LlmConfig::needsKey() const {
    if (!apiKey.empty()) return false;
    const std::string h = host();
    return h.find("localhost") == std::string::npos && h.find("127.0.0.1") == std::string::npos &&
           h.find("[::1]") == std::string::npos;
}

std::string llmConfigPath() {
    namespace fs = std::filesystem;
    fs::path dir;
#ifdef _WIN32
    const std::string appdata = env("APPDATA");
    dir = appdata.empty() ? fs::path(".") : fs::path(appdata) / "DES Simulator";
#elif defined(__APPLE__)
    const std::string home = env("HOME");
    dir = fs::path(home.empty() ? "." : home) / "Library" / "Application Support" / "DES Simulator";
#else
    const std::string xdg = env("XDG_CONFIG_HOME");
    const std::string home = env("HOME");
    dir = !xdg.empty() ? fs::path(xdg) / "des-simulator"
                       : fs::path(home.empty() ? "." : home) / ".config" / "des-simulator";
#endif
    return (dir / "llm.conf").string();
}

namespace {
const char* const kRetiredDefaultModel = "llama-3.3-70b-versatile";
}

LlmConfig parseLlmConfig(const std::string& text, LlmConfig c) {
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;
        const std::size_t eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
        auto trim = [](std::string v) {
            const std::size_t b = v.find_first_not_of(" \t\r");
            if (b == std::string::npos) return std::string();
            return v.substr(b, v.find_last_not_of(" \t\r") - b + 1);
        };
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key == "api_key")                         c.apiKey = value;
        else if (key == "base_url" && !value.empty()) c.baseUrl = value;
        // The first build wrote its own default into every settings file it
        // saved a key to, so "llama-3.3-70b-versatile" there is almost never a
        // choice anybody made -- and Groq has since withdrawn it, answering
        // 404. Read as "no model chosen", so the current default applies.
        else if (key == "model" && !value.empty() && value != kRetiredDefaultModel) c.model = value;
        else if (key == "reasoning_format")           c.reasoningFormat = value;
        else if (key == "reasoning_effort")           c.reasoningEffort = value;
    }
    while (!c.baseUrl.empty() && c.baseUrl.back() == '/') c.baseUrl.pop_back();
    return c;
}

std::string llmConfigText(const LlmConfig& c) {
    return "# DES Simulator: the language model behind F2 (describe a model).\n"
           "# Any OpenAI-compatible server works; for Ollama on this machine use\n"
           "#   base_url = http://localhost:11434/v1   and no api_key.\n"
           "base_url = " + c.baseUrl + "\n"
           "model = " + c.model + "\n"
           "api_key = " + c.apiKey + "\n"
           "# For a reasoning model: reasoning_effort = none turns thinking off.\n"
           "reasoning_format = " + c.reasoningFormat + "\n"
           "reasoning_effort = " + c.reasoningEffort + "\n";
}

std::string withApiKey(const std::string& existing, const std::string& key) {
    // A new file names only the key. The model and server stay commented
    // out, so a later version's default reaches this person -- the first
    // build wrote every setting out and froze its default model in place.
    if (existing.find_first_not_of(" \t\r\n") == std::string::npos) {
        const LlmConfig d;
        return "# DES Simulator: the language model behind F2 (describe a model).\n"
               "# Uncomment a line to change it. Any OpenAI-compatible server works;\n"
               "# for Ollama on this machine: base_url = http://localhost:11434/v1, no key.\n"
               "# base_url = " + d.baseUrl + "\n"
               "# model = " + d.model + "\n"
               "# reasoning_effort = none      (a reasoning model, thinking off)\n"
               "api_key = " + key + "\n";
    }
    // Otherwise only the api_key line changes. Every other line -- comments,
    // choices, keys a newer version added -- is kept as it was, except the
    // retired default model, which is commented out with the reason.
    std::string out;
    bool replaced = false;
    std::size_t start = 0;
    while (start < existing.size()) {
        std::size_t end = existing.find('\n', start);
        if (end == std::string::npos) end = existing.size();
        std::string line = existing.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t eq = line.find('=');
        const std::size_t b = line.find_first_not_of(" \t");
        std::string name;
        std::string value;
        if (eq != std::string::npos && b != std::string::npos && line[b] != '#') {
            name = line.substr(b, eq - b);
            name.erase(name.find_last_not_of(" \t") + 1);
            value = line.substr(eq + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
        }
        if (name == "api_key") {
            if (!replaced) out += "api_key = " + key + "\n";
            replaced = true;
        } else if (name == "model" && value == kRetiredDefaultModel) {
            out += "# model = " + value + "   (withdrawn by Groq; the current default is used)\n";
        } else {
            out += line + "\n";
        }
    }
    if (!replaced) out += "api_key = " + key + "\n";
    return out;
}

bool saveApiKey(const std::string& key, std::string& error) {
    namespace fs = std::filesystem;
    const fs::path path = llmConfigPath();
    std::string existing;
    {
        std::ifstream in(path, std::ios::binary);
        if (in.is_open())
            existing.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    const std::string text = withApiKey(existing, key);
#ifdef _WIN32
    // %APPDATA% is inside the user's own profile, which other accounts cannot
    // read; that is the protection on Windows.
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    if (!out) { error = "could not write " + path.string(); return false; }
#else
    // Write a fresh 0600 file and move it into place, so the key is never in a
    // file anybody else could read, not even for a moment.
    const std::string tmp = path.string() + ".tmp";
    ::unlink(tmp.c_str());
    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) { error = "could not write " + tmp; return false; }
    const bool wrote = ::write(fd, text.data(), text.size()) == static_cast<ssize_t>(text.size());
    if (::close(fd) != 0 || !wrote || std::rename(tmp.c_str(), path.c_str()) != 0) {
        ::unlink(tmp.c_str());
        error = "could not write " + path.string();
        return false;
    }
#endif
    return true;
}

std::string maskKey(const std::string& key) {
    // ASCII dots, not an ellipsis: a Windows console not set to UTF-8 drew
    // "…" as "ÔÇª" in the middle of the key.
    if (key.size() <= 8) return key.empty() ? "(none)" : "...";
    const std::size_t us = key.find('_');
    const std::string prefix = (us != std::string::npos && us < 6) ? key.substr(0, us + 1) : key.substr(0, 3);
    return prefix + "..." + key.substr(key.size() - 4);
}

const std::string& systemPrompt() {
    static const std::string prompt =
        "You turn a spoken description of a queueing system into a JSON form for a "
        "discrete-event simulator. You only fill in the form; you never write the "
        "model itself.\n\n"
        "Rules:\n"
        "- Use only facts the description states. Anything not stated is null. A "
        "missing number is fine; an invented one is wrong.\n"
        "- Numbers in the description have already been normalised to digits, and "
        "rates have their time-between-arrivals written beside them in brackets: use "
        "that time.\n"
        "- How a time varies: use \"fixed\" ONLY when the description says exactly, "
        "always, or on the dot. A single number with no such word -- \"every 2 "
        "minutes\", \"takes 3 minutes\", \"30 an hour\" -- is \"random\" with that "
        "mean, because real arrivals and services vary.\n"
        "- After a step with branches, set that step's \"next\" to where the REST "
        "go. \"20% need an X-ray, then everyone goes to discharge\": the branch goes "
        "to X-ray, the step's next is Discharge, and X-ray's next is Discharge.\n"
        "- A step is something each arrival goes through: being served (process) or "
        "just spending time (delay). Name steps with the person's own short words.\n"
        "- Use \"resources\" only when the description says several steps share the "
        "same staff or machines.\n"
        "- If a current form is given, return the WHOLE form again with the new "
        "information merged in, keeping everything else as it was.\n"
        "- Reply with the JSON object only.\n\n" +
        scenarioSchemaText() +
        "\nExample. Description: \"Patients arrive about every 6 minutes on average. "
        "Registration takes between 2 and 4 minutes with one clerk, then they see one "
        "of 3 doctors for about 15 minutes. 1 in 10 needs an X-ray, which takes 20 "
        "minutes exactly. Simulate a 10 hour day.\"\n"
        "Answer: {\"time_unit\":\"minutes\",\"run_length\":{\"value\":10,\"unit\":\"hours\"},"
        "\"warm_up\":null,\"replications\":null,"
        "\"arrivals\":[{\"name\":\"Patients\",\"every\":{\"pattern\":\"random\",\"mean\":6,"
        "\"unit\":\"minutes\"},\"limit\":null,\"group_size\":null,\"first_step\":null}],"
        "\"resources\":[],"
        "\"steps\":[{\"name\":\"Registration\",\"kind\":\"process\",\"servers\":1,\"resource\":null,"
        "\"time\":{\"pattern\":\"between\",\"min\":2,\"max\":4,\"unit\":\"minutes\"},\"queue\":null,"
        "\"balk_at\":null,\"next\":null},"
        "{\"name\":\"Doctor\",\"kind\":\"process\",\"servers\":3,\"resource\":null,"
        "\"time\":{\"pattern\":\"around\",\"mean\":15,\"min\":null,\"max\":null,\"unit\":\"minutes\"},"
        "\"queue\":null,\"balk_at\":null,\"next\":\"exit\"},"
        "{\"name\":\"X-ray\",\"kind\":\"process\",\"servers\":null,\"resource\":null,"
        "\"time\":{\"pattern\":\"fixed\",\"mean\":20,\"unit\":\"minutes\"},\"queue\":null,"
        "\"balk_at\":null,\"next\":\"exit\"}],"
        "\"branches\":[{\"from\":\"Doctor\",\"to\":\"X-ray\",\"probability\":0.1}]}\n";
    return prompt;
}

std::string effectiveReasoningFormat(const LlmConfig& config) {
    if (!config.reasoningFormat.empty()) return config.reasoningFormat;
    std::string m = config.model;
    for (char& c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return m.find("qwen3") != std::string::npos ? "hidden" : std::string();
}

std::string chatRequestBody(const LlmConfig& config, const Messages& messages) {
    JsonValue body = JsonValue::object();
    body.set("model", JsonValue::string(config.model));
    // Zero: the same sentence should give the same model, and a form-filling
    // task has no use for creativity.
    body.set("temperature", JsonValue::number(0));
    // Room for a reasoning model's hidden thinking as well as the answer: the
    // thinking counts against this, and running out mid-thought is a reply
    // cut off before the JSON starts.
    body.set("max_tokens", JsonValue::number(8192));
    const std::string reasoning = effectiveReasoningFormat(config);
    if (!reasoning.empty()) body.set("reasoning_format", JsonValue::string(reasoning));
    if (!config.reasoningEffort.empty())
        body.set("reasoning_effort", JsonValue::string(config.reasoningEffort));
    JsonValue format = JsonValue::object();
    format.set("type", JsonValue::string("json_object"));
    body.set("response_format", format);
    JsonValue list = JsonValue::array();
    for (const auto& m : messages) {
        JsonValue msg = JsonValue::object();
        msg.set("role", JsonValue::string(m.first));
        msg.set("content", JsonValue::string(m.second));
        list.push(msg);
    }
    body.set("messages", list);
    return writeJson(body);
}

// Questions, assumptions, build and compile: the same whichever way the form
// was filled in.
GenerateResult& finish(GenerateResult& out, const ScenarioRead& read);

GenerateResult generateModel(const std::string& description, const JsonValue& previous,
                             const LlmConfig& config, ITransport& transport) {
    GenerateResult out;
    const PreParsed pre = preParse(description);
    out.notes = pre.notes;

    // What the person was last asked, worked out from the form itself, so the
    // CLI's --from and the editor agree without passing questions around.
    std::vector<Question> pending;
    if (previous.isObject()) {
        const ScenarioRead before = readScenario(previous);
        pending = before.questions;
        if (pending.empty() && before.invalid.empty()) pending = loadQuestions(before.scenario);
    }

    ScenarioRead read;
    if (!pending.empty()) {
        JsonValue filled = previous;
        if (fillServersDirectly(filled, pending.front(), description)) {
            read = readScenario(filled);
            out.scenarioJson = filled;
            out.notes.push_back("Took that as the answer to: " + pending.front().text.substr(
                                    0, pending.front().text.find('.') + 1));
            return finish(out, read);
        }
    }

    Messages messages;
    messages.emplace_back("system", systemPrompt());
    std::string user;
    if (previous.isObject()) {
        user += "Current form:\n" + writeJson(previous) + "\n\n";
        if (!pending.empty()) {
            user += "The person was asked:\n";
            for (const Question& q : pending) user += "- " + q.text + "  (fills " + q.path + ")\n";
            user += "\nTheir answer, or a change:\n";
        } else {
            user += "New information or change:\n";
        }
    } else {
        user += "Description:\n";
    }
    user += pre.text;
    messages.emplace_back("user", user);

    std::vector<std::string> headers = {"Content-Type: application/json"};
    if (!config.apiKey.empty()) headers.push_back("Authorization: Bearer " + config.apiKey);
    const std::string url = config.baseUrl + "/chat/completions";

    // Two attempts: the answer, and one correction naming the bad fields.
    for (int attempt = 0; attempt < 2; ++attempt) {
        ++out.calls;
        std::string text;
        if (!replyText(transport.post(url, headers, chatRequestBody(config, messages)), text, out.error))
            return out;
        const JsonParse parsed = parseJsonObjectIn(text);
        if (!parsed.ok) {
            read = ScenarioRead{};
            read.invalid.push_back({"", "not a JSON object: " + parsed.error});
        } else {
            // Laid over the earlier form, so a reply holding only what changed
            // -- which this model sends however it is told -- loses nothing.
            const JsonValue form = mergeScenarioJson(previous, parsed.value);
            read = readScenario(form);
            out.scenarioJson = form;
        }
        if (read.invalid.empty()) break;
        messages.emplace_back("assistant", text);
        messages.emplace_back("user", "These fields are wrong:\n" + issueList(read.invalid) +
                                          "Return the whole corrected JSON object.");
    }
    if (!read.invalid.empty()) {
        out.error = "the language model's answer still had mistakes after a correction:\n" +
                    issueList(read.invalid);
        return out;
    }

    return finish(out, read);
}

GenerateResult& finish(GenerateResult& out, const ScenarioRead& read) {
    out.scenario  = read.scenario;
    out.questions = read.questions;
    if (out.questions.empty()) {
        const std::vector<Question> load = loadQuestions(read.scenario);
        out.questions.insert(out.questions.end(), load.begin(), load.end());
    }

    BuiltScenario built = buildScenario(read.scenario);
    out.assumptions = read.adjustments;
    out.assumptions.insert(out.assumptions.end(), built.assumptions.begin(), built.assumptions.end());
    out.document    = std::move(built.document);
    if (!out.questions.empty()) return out;

    // The builder writes only what it knows how to write, so this should never
    // fire. When it does, it is a bug in the builder, and saying so beats
    // handing over a model that does not compile.
    const CompileResult compiled = compile(out.document);
    if (hasErrors(compiled.diagnostics)) {
        out.error = "the generated model did not compile (a bug in the scenario builder):\n" +
                    formatDiagnostics(compiled.diagnostics);
        return out;
    }
    out.ok = true;
    return out;
}

}  // namespace des
