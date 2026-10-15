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
    std::string model{"llama-3.3-70b-versatile"};
    std::string apiKey;          // empty for a local server that needs none

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

// `key = value` lines: api_key, base_url, model. Unknown keys and # comments
// are ignored, so a newer version's file does not break an older program.
LlmConfig   parseLlmConfig(const std::string& text, LlmConfig base = LlmConfig{});
std::string llmConfigText(const LlmConfig& config);

// Writes the key into the settings file, keeping whatever else is there.
// Created readable by this user alone. False, with the reason, on failure.
bool saveApiKey(const std::string& key, std::string& error);

// "gsk_…a1b2": enough to tell two keys apart, never enough to use one.
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
std::string        chatRequestBody(const LlmConfig& config,
                                   const std::vector<std::pair<std::string, std::string>>& messages);

}  // namespace des
