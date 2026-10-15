#include "Speech.hpp"

namespace des {

namespace {

void put16(std::string& s, std::uint16_t v) {
    s += static_cast<char>(v & 0xFF);
    s += static_cast<char>((v >> 8) & 0xFF);
}

void put32(std::string& s, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) s += static_cast<char>((v >> (8 * i)) & 0xFF);
}

std::string trim(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

}  // namespace

std::string wavFromPcm16(const std::vector<std::int16_t>& samples, int sampleRate) {
    // Little-endian by construction, byte by byte, so the file is the same on
    // every machine whatever its own byte order.
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * 2);
    std::string w;
    w.reserve(44 + dataBytes);
    w += "RIFF";
    put32(w, 36 + dataBytes);
    w += "WAVE";
    w += "fmt ";
    put32(w, 16);                                         // PCM header size
    put16(w, 1);                                          // PCM
    put16(w, 1);                                          // mono
    put32(w, static_cast<std::uint32_t>(sampleRate));
    put32(w, static_cast<std::uint32_t>(sampleRate) * 2); // bytes per second
    put16(w, 2);                                          // bytes per frame
    put16(w, 16);                                         // bits per sample
    w += "data";
    put32(w, dataBytes);
    for (const std::int16_t v : samples) put16(w, static_cast<std::uint16_t>(v));
    return w;
}

TranscriptResult transcribe(const std::string& wav, const LlmConfig& config, ITransport& transport) {
    TranscriptResult out;
    std::vector<std::string> headers;
    if (!config.apiKey.empty()) headers.push_back("Authorization: Bearer " + config.apiKey);
    std::vector<std::pair<std::string, std::string>> fields = {
        {"model", config.sttModel},
        {"response_format", "json"},
        {"temperature", "0"},
    };
    if (!config.sttLanguage.empty()) fields.emplace_back("language", config.sttLanguage);
    // Whisper takes a short prompt as spelling guidance. The words of this
    // domain are what it would otherwise get wrong.
    fields.emplace_back("prompt", "Queueing simulation: arrivals per hour, minutes, servers, "
                                  "tellers, nurses, triage, discharge, X-ray, 30%, utilisation.");

    const HttpResponse r = transport.postForm(config.baseUrl + "/audio/transcriptions", headers,
                                              fields, "file", "speech.wav", wav);
    if (!r.ok) { out.error = r.error.empty() ? "the request did not complete" : r.error; return out; }
    const JsonParse p = parseJson(r.body);
    if (r.status == 401 || r.status == 403) {
        out.error = "the provider refused the API key (HTTP " + std::to_string(r.status) + ")";
        return out;
    }
    if (r.status == 404) {
        out.error = "this server has no speech-to-text at " + config.host() +
                    " (HTTP 404) -- check stt_model in " + llmConfigPath() +
                    ", or speak with Win+H / Dictation instead";
        return out;
    }
    if (r.status != 200) {
        const std::string why = p.ok ? p.value["error"]["message"].asString() : std::string();
        out.error = "speech-to-text answered HTTP " + std::to_string(r.status) +
                    (why.empty() ? std::string() : ": " + why);
        return out;
    }
    if (!p.ok || !p.value["text"].isString()) { out.error = "the transcript was not readable"; return out; }
    out.text = trim(p.value["text"].asString());
    out.ok = true;
    return out;
}

}  // namespace des
