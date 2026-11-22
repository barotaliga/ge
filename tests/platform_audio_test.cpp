// The audio device (engine/platform/audio.hpp) on SDL's dummy driver: it opens with no hardware,
// pulls the synth on its own thread at the device's rate, and a call made on this thread is heard
// there (the synth's ring carries it). The dummy driver is what makes the real device path
// testable in a container; a desktop with a sound server runs the same code.
//
// ge:summary Tests for the audio device and the microphone on the dummy driver
// ge:keywords test audio device dummy-driver pull frames-pulled synth ring thread
// ge:skill platform audio testing
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "audio/synth.hpp"
#include "check.hpp"
#include "platform/audio.hpp"
#include "platform/window_cvars.hpp"

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

TEST(the_microphone_opens_and_is_read_on_the_game_thread) {
#if defined(_WIN32)
  _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
  setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
  if (!ge::platform::audio_available()) {
    ge::test::skip("no audio driver, not even SDL's dummy");
    return;
  }
  auto in = ge::platform::open_audio_in(48000);
  if (!in) {
    ge::test::skip("this SDL's dummy driver records nothing");
    return;
  }
  float buffer[960];
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (in->frames_read() < 4800 && std::chrono::steady_clock::now() < until) {
    ge::platform::read_audio_in(*in, buffer, 960);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  CHECK_MSG(in->frames_read() >= 4800, "the dummy microphone gave " +
                                           std::to_string(in->frames_read()) +
                                           " frames in 3 s (skill: platform)");
  CHECK(ge::platform::read_audio_in(*in, buffer, 0) == 0);
}

TEST(the_sound_moves_to_the_device_the_player_names) {
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
  const std::vector<std::string> devices = ge::platform::audio_devices();
  CHECK(!ge::platform::move_audio(*out, "no such device"));  // left where it was
  CHECK(ge::platform::move_audio(*out, ""));                 // the system's default again
  if (!devices.empty()) CHECK(ge::platform::move_audio(*out, devices.front()));
  const uint64_t before = out->frames_pulled();
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (out->frames_pulled() < before + ge::audio::kRate / 10 &&
         std::chrono::steady_clock::now() < until)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  CHECK_MSG(out->frames_pulled() >= before + ge::audio::kRate / 10,
            "the moved stream stopped pulling (skill: platform)");
  // s_device: ? lists and is put back; a name not there is said, the sound left as it was.
  ge::console::Console con;
  ge::platform::WindowCvars w;
  ge::platform::add_window_cvars(con, w);
  con.execute("s_device ?");
  ge::platform::follow_sound_device(out.get(), w, con);
  CHECK(con.cvars().find("s_device")->value.empty() && w.sound_device.empty());
  CHECK(!con.output().empty());
  bool listed = false;
  for (const std::string& line : con.output())
    listed = listed || line == "Available devices:" || line == "No sound devices.";
  CHECK(listed);
  con.execute("s_device \"Speakers 9\"");
  ge::platform::follow_sound_device(out.get(), w, con);
  CHECK(con.output().back() ==
        "There is no sound device called Speakers 9; the sound stays on the default device.");
  const size_t said = con.output().size();
  ge::platform::follow_sound_device(out.get(), w, con);  // asked once
  CHECK(con.output().size() == said);
}
