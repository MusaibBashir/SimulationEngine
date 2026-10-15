#include "ModelGenerator.hpp"

#include <cstdlib>
#include "Compiler.hpp"
#include "PreParse.hpp"
#include "ScenarioBuilder.hpp"

namespace des {

namespace {

std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v == nullptr ? std::string() : std::string(v);
}

using Messages = std::vector<std::pair<std::string, std::string>>;

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
    text = content.asString();
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

std::string maskKey(const std::string& key) {
    if (key.size() <= 8) return key.empty() ? "(none)" : "…";
    const std::size_t us = key.find('_');
    const std::string prefix = (us != std::string::npos && us < 6) ? key.substr(0, us + 1) : key.substr(0, 3);
    return prefix + "…" + key.substr(key.size() - 4);
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

std::string chatRequestBody(const LlmConfig& config, const Messages& messages) {
    JsonValue body = JsonValue::object();
    body.set("model", JsonValue::string(config.model));
    // Zero: the same sentence should give the same model, and a form-filling
    // task has no use for creativity.
    body.set("temperature", JsonValue::number(0));
    body.set("max_tokens", JsonValue::number(2048));
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

GenerateResult generateModel(const std::string& description, const JsonValue& previous,
                             const LlmConfig& config, ITransport& transport) {
    GenerateResult out;
    const PreParsed pre = preParse(description);
    out.notes = pre.notes;

    Messages messages;
    messages.emplace_back("system", systemPrompt());
    std::string user;
    if (previous.isObject())
        user += "Current form:\n" + writeJson(previous) + "\n\nNew information or change:\n";
    else
        user += "Description:\n";
    user += pre.text;
    messages.emplace_back("user", user);

    std::vector<std::string> headers = {"Content-Type: application/json"};
    if (!config.apiKey.empty()) headers.push_back("Authorization: Bearer " + config.apiKey);
    const std::string url = config.baseUrl + "/chat/completions";

    // Two attempts: the answer, and one correction naming the bad fields.
    ScenarioRead read;
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
            read = readScenario(parsed.value);
            out.scenarioJson = parsed.value;
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

    out.scenario  = read.scenario;
    out.questions = read.questions;
    if (out.questions.empty()) {
        const std::vector<Question> load = loadQuestions(read.scenario);
        out.questions.insert(out.questions.end(), load.begin(), load.end());
    }

    BuiltScenario built = buildScenario(read.scenario);
    out.assumptions = built.assumptions;
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
