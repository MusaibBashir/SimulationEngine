// ============================================================================
// tui/Microphone.cpp  --  v17: the default microphone, through miniaudio
// ============================================================================
// miniaudio is one public-domain header (third_party/miniaudio) that speaks
// WASAPI on Windows and Core Audio on a Mac, so recording needs nothing
// installed -- the promise every package this project ships has kept so far.
// Only capture is compiled in; the decoders, the engine and playback are not.
//
// Platform code, like Terminal_*.cpp: nothing here can be tested without a
// sound card, which is why the UI sees only IRecorder.

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#if defined(__APPLE__)
  // Link Core Audio rather than load it at run time: a package that loads
  // frameworks by name is the kind macOS's checks distrust.
  #define MA_NO_RUNTIME_LINKING
#endif
#include "miniaudio.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include "Recorder.hpp"
#include "Speech.hpp"

namespace des {

namespace {

class MiniaudioRecorder : public IRecorder {
public:
    ~MiniaudioRecorder() override {
        if (m_started) ma_device_uninit(&m_device);
    }

    bool start(std::string& error) override {
        ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
        cfg.capture.format   = ma_format_s16;
        cfg.capture.channels = 1;
        cfg.sampleRate       = kSpeechSampleRate;   // miniaudio converts from the device's own
        cfg.dataCallback     = &MiniaudioRecorder::onData;
        cfg.pUserData        = this;
        const ma_result init = ma_device_init(nullptr, &cfg, &m_device);
        if (init != MA_SUCCESS) {
            error = std::string("could not open the microphone (") + ma_result_description(init) +
                    "). " + permissionHint();
            return false;
        }
        m_started = true;
        const ma_result go = ma_device_start(&m_device);
        if (go != MA_SUCCESS) {
            error = std::string("the microphone would not start (") + ma_result_description(go) +
                    "). " + permissionHint();
            return false;
        }
        return true;
    }

    std::vector<std::int16_t> stop() override {
        if (m_started) {
            ma_device_stop(&m_device);
            ma_device_uninit(&m_device);
            m_started = false;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::move(m_samples);
    }

    double seconds() const override {
        return static_cast<double>(m_count.load()) / kSpeechSampleRate;
    }

    float level() const override { return m_level.load(); }

private:
    ma_device m_device{};
    bool      m_started{false};
    std::mutex                m_mutex;
    std::vector<std::int16_t> m_samples;
    std::atomic<std::size_t>  m_count{0};
    std::atomic<float>        m_level{0.0f};

    static std::string permissionHint() {
#if defined(_WIN32)
        return "Check Settings > Privacy & security > Microphone, and that desktop apps may use it.";
#elif defined(__APPLE__)
        return "Check System Settings > Privacy & Security > Microphone, and allow Terminal.";
#else
        return "Check that a microphone is connected and not in use.";
#endif
    }

    // On miniaudio's own thread. Copies and measures, nothing more.
    static void onData(ma_device* device, void* /*output*/, const void* input, ma_uint32 frames) {
        auto* self = static_cast<MiniaudioRecorder*>(device->pUserData);
        if (self == nullptr || input == nullptr) return;
        const auto* in = static_cast<const std::int16_t*>(input);
        int peak = 0;
        for (ma_uint32 i = 0; i < frames; ++i) peak = std::max(peak, std::abs(static_cast<int>(in[i])));
        {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_samples.insert(self->m_samples.end(), in, in + frames);
        }
        self->m_count.fetch_add(frames);
        // The meter falls back gently rather than flickering to zero between words.
        const float now = static_cast<float>(peak) / 32768.0f;
        self->m_level.store(std::max(now, self->m_level.load() * 0.85f));
    }
};

}  // namespace

std::unique_ptr<IRecorder> openMicrophone(std::string& whyNot) {
    whyNot.clear();
    return std::unique_ptr<IRecorder>(new MiniaudioRecorder());
}

}  // namespace des
