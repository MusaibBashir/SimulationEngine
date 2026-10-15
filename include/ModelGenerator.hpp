// ============================================================================
// ModelGenerator.hpp  --  v17: a sentence becomes a model, through a cheap LLM
// ============================================================================
// describe -> pre-parse -> ONE call to the language model for a Scenario ->
// validate -> build -> compile. The model is asked to fill in a small JSON form
// and nothing else (Scenario.hpp); the .des file is written by our own code.
//
// The only retry is for a MALFORMED answer, and it says exactly which fields
// were wrong. A MISSING fact is never sent back to the model, because it would
// only guess -- it becomes a Question for the person, and their answer comes
// back in through `previous`, merged by the model into the same form.
//
// ANY OpenAI-compatible endpoint: Groq by default, but OpenRouter, Together,
// and local Ollama or llama.cpp servers all accept the same request. The
// provider is three settings, not an integration.

#pragma once
#include <map>
#include <string>
#include <vector>
#include "Json.hpp"
#include "ModelDocument.hpp"
#include "Scenario.hpp"

namespace des {

struct HttpResponse {
    bool        ok{false};       // a response arrived at all
    int         status{0};       // HTTP status, when ok
    std::string body;
    std::string error;           // why not, when !ok
};

// One HTTPS POST. An interface so the whole pipeline runs in the test suite
// against recorded replies, with no network -- the same reason the terminal is
// one. The real one is CurlTransport.
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual HttpResponse post(const std::string& url,
                              const std::vector<std::string>& headers,
                              const std::string& body) = 0;
};

struct LlmConfig {
    std::string baseUrl{"https://api.groq.com/openai/v1"};
    // Qwen 3.8 27B on Groq. A Groq PREVIEW model -- for evaluation, and
    // liable to be withdrawn -- so it is a setting, never a constant
    // anything else depends on.
    std::string model{"qwen/qwen3.8-27b"};
    std::string apiKey;          // empty for a local server that needs none

    // For reasoning models. Groq REFUSES JSON mode with a reasoning model
    // unless reasoning_format is "hidden" or "parsed" (HTTP 400), and refuses
    // the parameter on a model that does not reason -- so it is sent only
    // when set here, or when the model is one known to need it (Qwen 3).
    // reasoning_effort "none" turns the thinking off: faster and cheaper,
    // worth trying once there is an eval set to say what it costs in quality.
    std::string reasoningFormat;    // "hidden" | "parsed" | empty: automatic
    std::string reasoningEffort;    // "none" | "low" | "medium" | "high" | empty: not sent

    // DES_LLM_BASE_URL, DES_LLM_MODEL, and DES_LLM_API_KEY or GROQ_API_KEY.
    static LlmConfig fromEnvironment();

    // The settings file first, then the environment over it: a variable set
    // for one session is a deliberate override of what was saved.
    static LlmConfig load();

    // "api.groq.com", for telling a person where their words are going.
    std::string host() const;
    // Groq (or any hosted provider) without a key is a request bound to fail;
    // a local server needs none.
    bool needsKey() const;
};

// Where the settings live -- per user, never beside the model, so a key cannot
// be emailed along with a .des file:
//   Windows  %APPDATA%\DES Simulator\llm.conf
//   macOS    ~/Library/Application Support/DES Simulator/llm.conf
//   other    $XDG_CONFIG_HOME/des-simulator/llm.conf, or ~/.config/...
std::string llmConfigPath();

// `key = value` lines: api_key, base_url, model, reasoning_format,
// reasoning_effort. Unknown keys and # comments
// are ignored, so a newer version's file does not break an older program.
LlmConfig   parseLlmConfig(const std::string& text, LlmConfig base = LlmConfig{});
std::string llmConfigText(const LlmConfig& config);

// The settings file's text with its api_key set: only that line changes in an
// existing file; a new one names the key alone, with the model and server as
// comments, so later defaults still apply. Separate from the save so a test
// can check it without touching the real file.
std::string withApiKey(const std::string& existingText, const std::string& key);

// Writes the key into the settings file, keeping whatever else is there.
// Created readable by this user alone. False, with the reason, on failure.
bool saveApiKey(const std::string& key, std::string& error);

// "gsk_...a1b2": enough to tell two keys apart, never enough to use one.
std::string maskKey(const std::string& key);

struct GenerateResult {
    bool                     ok{false};     // a model was built with no questions left
    std::string              error;         // transport, HTTP or unreadable answer
    JsonValue                scenarioJson;  // the form as filled in: feed back as `previous`
    Scenario                 scenario;
    ModelDocument            document;
    std::vector<std::string> assumptions;
    std::vector<Question>    questions;     // for the person; ok is false while any remain
    std::vector<std::string> notes;         // what the pre-parser worked out
    int                      calls{0};      // requests made, for the person paying
};

// `previous` is the form from an earlier round, or null for a new model. The
// description is then an answer or a change: "there are 3 tellers".
GenerateResult generateModel(const std::string& description, const JsonValue& previous,
                             const LlmConfig& config, ITransport& transport);

// Exposed for tests and for `des generate --show-prompt`. Byte-identical on
// every call, so a provider that caches prompt prefixes can.
const std::string& systemPrompt();
// The reasoning_format the request will carry: the setting, else "hidden" for
// a Qwen 3 model, else nothing.
std::string        effectiveReasoningFormat(const LlmConfig& config);
std::string        chatRequestBody(const LlmConfig& config,
                                   const std::vector<std::pair<std::string, std::string>>& messages);

}  // namespace des
