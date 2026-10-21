// The audio device (engine/platform/audio.hpp) on SDL's dummy driver: it opens with no hardware,
// pulls the synth on its own thread at the device's rate, and a call made on this thread is heard
// there (the synth's ring carries it). The dummy driver is what makes the real device path
// testable in a container; a desktop with a sound server runs the same code.
//
// ge:summary Tests for the audio device: opens on the dummy driver and pulls the synth
// ge:keywords test audio device dummy-driver pull frames-pulled synth ring thread
// ge:skill platform audio testing
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#include "audio/synth.hpp"
#include "check.hpp"
#include "platform/audio.hpp"

namespace {

void pull(float* out, int frames, void* user) {
  static_cast<ge::audio::Synth*>(user)->render(out, frames);
}

}  // namespace

TEST(the_device_pulls_the_synth_on_its_own_thread) {
#if defined(_WIN32)
  _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
  setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
  if (!ge::platform::audio_available()) {
    ge::test::skip("no audio driver, not even SDL's dummy");
    return;
  }
  ge::audio::Synth synth;
  auto out = ge::platform::open_audio({.rate = ge::audio::kRate, .pull = pull, .user = &synth});
  CHECK(out != nullptr);
  if (!out) return;
  CHECK(std::string(out->driver()) == "dummy");
  synth.tone({.from_hz = 440, .duration_ms = 50});
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (out->frames_pulled() < ge::audio::kRate / 5 && std::chrono::steady_clock::now() < until) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  CHECK_MSG(out->frames_pulled() >= ge::audio::kRate / 5,
            "the dummy device pulled " + std::to_string(out->frames_pulled()) +
                " frames in 3 s; SDL's audio thread is not running (skill: platform)");
  CHECK(synth.now_s() > 0.1);
  ge::platform::pause_audio(*out);
  ge::platform::resume_audio(*out);
}
