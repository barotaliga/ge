// The audio device over SDL3: the audio subsystem's lifetime and the stream's callback, which
// pulls in chunks into a buffer on the stack and hands them to SDL.
//
// ge:summary The audio device implemented on SDL3's device stream and its callback
// ge:keywords sdl sdl3 audio-stream callback device-stream put-audio-stream-data
// ge:skill platform audio
// ge:test tests/platform_audio_test.cpp
#include "platform/audio.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <string>

#include "core/output.hpp"

namespace ge::platform {

namespace out = core::output;

namespace {
// Frames a chunk: the pull renders this many at a time into the stack buffer.
constexpr int kChunk = 512;
}  // namespace

struct AudioOut::Impl {
  SDL_AudioStream* stream = nullptr;
  AudioPull pull = nullptr;
  void* user = nullptr;
  std::atomic<uint64_t> pulled{0};
};

namespace {

void SDLCALL feed(void* userdata, SDL_AudioStream* stream, int additional, int /*total*/) {
  auto* impl = static_cast<AudioOut::Impl*>(userdata);
  float buffer[2 * kChunk];
  int frames = additional / static_cast<int>(2 * sizeof(float));
  while (frames > 0) {
    const int n = frames < kChunk ? frames : kChunk;
    impl->pull(buffer, n, impl->user);
    SDL_PutAudioStreamData(stream, buffer, n * static_cast<int>(2 * sizeof(float)));
    impl->pulled.fetch_add(static_cast<uint64_t>(n), std::memory_order_relaxed);
    frames -= n;
  }
}

}  // namespace

AudioOut::AudioOut(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

AudioOut::~AudioOut() {
  if (impl_->stream) SDL_DestroyAudioStream(impl_->stream);
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

uint64_t AudioOut::frames_pulled() const { return impl_->pulled.load(std::memory_order_relaxed); }

const char* AudioOut::driver() const {
  const char* d = SDL_GetCurrentAudioDriver();
  return d ? d : "";
}

bool audio_available() {
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  return true;
}

std::unique_ptr<AudioOut> open_audio(const AudioDesc& desc) {
  if (!desc.pull) {
    out::error("platform: open_audio needs a pull (AudioDesc::pull); skill: platform.");
    return nullptr;
  }
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    out::error(std::string("platform: no audio driver (") + SDL_GetError() +
               "). In a container run with SDL_AUDIO_DRIVER=dummy; skill: platform.");
    return nullptr;
  }
  auto impl = std::make_unique<AudioOut::Impl>();
  impl->pull = desc.pull;
  impl->user = desc.user;
  const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, desc.rate};
  impl->stream =
      SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, impl.get());
  if (!impl->stream) {
    out::error(std::string("platform: no audio output to open (") + SDL_GetError() +
               "). A headless run uses SDL_AUDIO_DRIVER=dummy; skill: platform.");
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return nullptr;
  }
  SDL_ResumeAudioStreamDevice(impl->stream);
  return std::unique_ptr<AudioOut>(new AudioOut(std::move(impl)));
}

void pause_audio(AudioOut& out) { SDL_PauseAudioStreamDevice(out.impl_->stream); }
void resume_audio(AudioOut& out) { SDL_ResumeAudioStreamDevice(out.impl_->stream); }

}  // namespace ge::platform
