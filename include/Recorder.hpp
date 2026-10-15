// ============================================================================
// Recorder.hpp  --  v17: the microphone, as an interface
// ============================================================================
// The same split as Terminal.hpp: the UI talks to an IRecorder, the platform
// supplies one. The real one (tui/Microphone.cpp) uses miniaudio -- WASAPI on
// Windows, Core Audio on a Mac, nothing to install -- and the test suite
// supplies a fake, so the whole talk-and-transcribe flow runs without a sound
// card under the sanitisers.
//
// A terminal cannot report a key being RELEASED, so this is press-to-start,
// press-to-stop, not hold-to-talk.

#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace des {

class IRecorder {
public:
    virtual ~IRecorder() = default;
    // Starts capturing mono 16-bit samples at kSpeechSampleRate. False, with
    // the reason, when there is no microphone or it may not be used.
    virtual bool start(std::string& error) = 0;
    // Stops, and hands over everything captured since start().
    virtual std::vector<std::int16_t> stop() = 0;
    virtual double seconds() const = 0;   // captured so far
    virtual float  level() const = 0;     // 0..1, the recent peak, for a meter
};

// The default microphone. Declared here, defined in tui/ -- the only place a
// platform audio API is allowed. Null, with the reason, when there is none.
std::unique_ptr<IRecorder> openMicrophone(std::string& whyNot);

}  // namespace des
