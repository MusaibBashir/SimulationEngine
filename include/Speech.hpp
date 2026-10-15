// ============================================================================
// Speech.hpp  --  v17: what was said, as text
// ============================================================================
// The microphone records 16-bit mono samples (Recorder.hpp); this turns them
// into a WAV file and asks the speech-to-text endpoint of the same server the
// language model lives on -- Whisper on Groq, with the same key -- what was
// said. The transcript goes into the Ask box to be READ before it is sent:
// speech recognition hears "fifteen" for "fifty", and a model built on a
// misheard rate compiles and is wrong.

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "ModelGenerator.hpp"

namespace des {

// Whisper's own rate. Recording at it, rather than at the device's rate and
// converting later, keeps an uploaded minute near 2 MB.
constexpr int kSpeechSampleRate = 16000;

// A complete RIFF/WAVE file: 16-bit PCM, mono, at `sampleRate`.
std::string wavFromPcm16(const std::vector<std::int16_t>& samples, int sampleRate);

struct TranscriptResult {
    bool        ok{false};
    std::string text;        // trimmed; may be empty when nothing was said
    std::string error;
};

// POST {base_url}/audio/transcriptions with the recording, the stt model and,
// when set, the language. Never throws.
TranscriptResult transcribe(const std::string& wav, const LlmConfig& config, ITransport& transport);

}  // namespace des
