// A RECORDED SOUND: PCM samples at their own rate, as the sampled voices (`Synth::sample`) play
// them, and the WAV reader that makes one. The synth's own voices are numbers; a game that ships
// recordings (a shooter's gunfire, a door, a voice) reads them into `Pcm`s once, at load, and
// keeps them alive for as long as anything may play them.
//
// THE READER takes RIFF WAVE files as games ship them: PCM at 8 bits (unsigned) or 16 bits
// (signed), 32-bit IEEE float, WAVE_FORMAT_EXTENSIBLE wrapping either, any rate, mono or more
// channels (mixed down to mono: a sampled voice is placed by the listener, never by the file).
// THE LOOP comes from the file, in this order:
//
//   `smpl`           the sampler chunk's first loop: its start and its end (inclusive)
//   `cue ` + `ltxt`  the first cue point's sample offset is the loop's start; a labelled-text
//                    entry for that cue (in a `LIST` `adtl`) gives the loop's length in frames
//   `cue ` alone     the loop runs from the cue to the end of the data
//
// A file with none of them does not loop. A loop that falls outside the data is clamped into it,
// and one with no length left is dropped. Every failure says what was wrong and at which byte.
//
// ge:summary A recorded sound (Pcm: mono samples, rate, loop) and the WAV reader with its loop cue
// ge:keywords pcm sample sampled recording wav wave riff read_wav loop cue smpl ltxt adtl
// ge:keywords 8-bit 16-bit float extensible mono downmix rate
// ge:skill audio
// ge:test tests/audio_pcm_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::audio {

struct Pcm {
  std::vector<float> samples;  // mono, -1..1, one a frame
  int rate = 0;                // frames a second, as recorded
  int64_t loop_start = -1;     // the loop's first frame, or -1: plays once
  int64_t loop_end = 0;        // one past the loop's last frame (<= samples.size())

  bool loops() const { return loop_start >= 0; }
  int64_t frames() const { return static_cast<int64_t>(samples.size()); }
  double seconds() const { return rate > 0 ? static_cast<double>(samples.size()) / rate : 0; }
};

// Read a WAV file's bytes into `out`. On failure returns false and sets `error` to a line naming
// `where` (the file, for the reader), what was wrong, at which byte, and what to do next.
bool read_wav(std::span<const uint8_t> bytes, std::string_view where, Pcm* out, std::string* error);

}  // namespace ge::audio
