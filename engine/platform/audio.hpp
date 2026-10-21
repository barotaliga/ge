// THE AUDIO DEVICE: one output stream to the default playback device, pulled from a callback on
// the platform's audio thread. The engine renders at one fixed rate and format (interleaved
// stereo float, `AudioDesc::rate`) and the platform converts to whatever the hardware runs at
// (a Bluetooth headset's 16 kHz, a desktop's 44.1 or 48 kHz), so nothing above this ever sees
// the hardware's format, and a sound's bits never depend on it.
//
// THE PULL is called on the device's thread with a frame count; it fills that many interleaved
// stereo frames (the synth's `render`). It must not block, lock or allocate: everything it needs
// arrives through the synth's ring. Losing focus does not stop the stream; the game silences its
// buses (and `pause_audio` holds the device where the platform wants the session given back).
//
// HEADLESS: SDL's `dummy` driver (SDL_AUDIO_DRIVER=dummy) consumes the stream at the device's
// rate with no hardware, so a test runs the real device path in a container.
// `audio_available()` says whether any output can be opened.
//
// ge:summary The audio device: one stereo float output stream pulled on the device thread
// ge:keywords platform audio device output stream playback callback pull sdl dummy-driver
// ge:keywords audio-thread open_audio pause_audio resume_audio audio_available frames-pulled
// ge:skill platform audio
// ge:test tests/platform_audio_test.cpp
#pragma once

#include <cstdint>
#include <memory>

namespace ge::platform {

// Fill `frames` interleaved stereo frames into `out`. Called on the device's thread.
using AudioPull = void (*)(float* out, int frames, void* user);

struct AudioDesc {
  int rate = 48000;  // what the pull renders at; the platform converts
  AudioPull pull = nullptr;
  void* user = nullptr;
};

class AudioOut {
 public:
  ~AudioOut();
  AudioOut(const AudioOut&) = delete;
  AudioOut& operator=(const AudioOut&) = delete;

  // Frames the pull has been asked for since the device opened (any thread).
  uint64_t frames_pulled() const;
  // The driver in use ("pipewire", "pulseaudio", "alsa", "coreaudio", "wasapi", "dummy", ...).
  const char* driver() const;

  struct Impl;

 private:
  explicit AudioOut(std::unique_ptr<Impl> impl);
  friend std::unique_ptr<AudioOut> open_audio(const AudioDesc& desc);
  friend void pause_audio(AudioOut& out);
  friend void resume_audio(AudioOut& out);
  std::unique_ptr<Impl> impl_;
};

// Whether an output device can be opened in this process.
bool audio_available();

// Open the default output and start pulling. Returns nullptr after an agent-actionable error
// line when it cannot (no device, no driver).
std::unique_ptr<AudioOut> open_audio(const AudioDesc& desc);

// Hold the device (it stops pulling) and let it go again.
void pause_audio(AudioOut& out);
void resume_audio(AudioOut& out);

}  // namespace ge::platform
