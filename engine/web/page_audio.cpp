// The page's sound, defined (page_audio.hpp). Built only for Emscripten.
//
// ge:summary The page's sound defined: the audio context, the queue of buffers, the pump
// ge:keywords web audio audio-context audio-buffer schedule resume
// ge:skill net audio
#include "web/page_audio.hpp"

#include <emscripten/emscripten.h>

#include <algorithm>
#include <vector>

#include "audio/voice.hpp"

namespace {

// The context at the synth's rate; the page's first pointer press or key resumes it. 1 when made.
// The JS is written without arrow functions or strict equality: the C++ formatter would split them.
EM_JS(int, ge_web_audio_open, (int rate), {
  if (Module.geAudio) return 1;
  try {
    const C = window.AudioContext || window.webkitAudioContext;
    if (!C) return 0;
    const ctx = new C({sampleRate : rate});
    Module.geAudio = {ctx : ctx, next : 0, rate : rate};
    const wake = function() {
      if (ctx.state != "running") ctx.resume();
    };
    document.addEventListener("pointerdown", wake, true);
    document.addEventListener("keydown", wake, true);
    return 1;
  } catch (e) {
    return 0;
  }
});

// Frames queued past what plays now; -1 while the context does not play.
EM_JS(int, ge_web_audio_queued, (), {
  const a = Module.geAudio;
  if (!a || a.ctx.state != "running") return -1;
  const ahead = a.next - a.ctx.currentTime;
  return ahead > 0 ? Math.floor(ahead * a.rate) : 0;
});

// `frames` interleaved stereo frames queued after the last.
EM_JS(void, ge_web_audio_push, (const float* data, int frames), {
  const a = Module.geAudio;
  if (!a || frames <= 0) return;
  const buf = a.ctx.createBuffer(2, frames, a.rate);
  const l = buf.getChannelData(0);
  const r = buf.getChannelData(1);
  const at = data >> 2;
  for (let i = 0; i < frames; i++) {
    l[i] = HEAPF32[at + 2 * i];
    r[i] = HEAPF32[at + 2 * i + 1];
  }
  const src = a.ctx.createBufferSource();
  src.buffer = buf;
  src.connect(a.ctx.destination);
  const t = Math.max(a.next, a.ctx.currentTime + 0.02);
  src.start(t);
  a.next = t + frames / a.rate;
});

}  // namespace

namespace ge::web {

bool audio_open() { return ge_web_audio_open(ge::audio::kRate) != 0; }

bool audio_playing() { return ge_web_audio_queued() >= 0; }

void audio_pump(ge::audio::Synth& synth, double dt, double lead_s) {
  static std::vector<float> buf;
  const int queued = ge_web_audio_queued();
  const int rate = ge::audio::kRate;
  // Never more than a quarter second a paint: a tab come back from the background starts afresh.
  const int most = rate / 4;
  // A slow paint (a weak machine, software graphics) needs more ahead to last until the next.
  const double lead = std::min(std::max(lead_s, dt * 1.5), 0.25);
  int frames = queued < 0 ? static_cast<int>(dt * rate) : static_cast<int>(lead * rate) - queued;
  frames = std::clamp(frames, 0, most);
  if (frames == 0) return;
  buf.resize(static_cast<size_t>(frames) * 2);
  synth.render(buf.data(), frames);
  if (queued >= 0) ge_web_audio_push(buf.data(), frames);
}

}  // namespace ge::web
