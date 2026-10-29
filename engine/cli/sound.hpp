// `ge sound`: a game's sounds taken in from someone else's free packs without a person listening
// (skill: free-assets). Three commands:
//
//   ge sound match <reference> <candidate>...  rank candidate files (or every sound under a
//                                directory) by how close each comes to the reference in length,
//                                in energy over seven frequency bands and in its loudness envelope
//                                over ten slices; --pool keeps only the paths a regex matches
//   ge sound import <source> <name>  the source decoded to mono at --rate, trimmed of its silence
//                                at both ends, cut to --max seconds, faded in and out (a longer
//                                fade on a --loop), brought to --lufs integrated loudness under a
//                                -1.5 dBFS peak, and written as 16-bit WAV at <to>/<name>.wav
//   ge sound split <source> <name>   one recording of many sounds cut at its silences, each piece
//                                imported as import does to <to>/<name>_01.wav, _02 ...
//
// THE DECODER is ffmpeg on the PATH (any format it reads: ogg, flac, mp3, wav), run once a file
// to raw 32-bit float samples; everything after that (the trim, the loudness, the features, the
// WAV) is this file's own arithmetic, so the tests hold it without ffmpeg. THE LOUDNESS is
// ITU-R BS.1770's integrated loudness: the K-weighting's two biquads designed for the rate,
// 400 ms blocks every 100 ms, the absolute gate at -70 LUFS and the relative one 10 LU under the
// mean; a sound shorter than one block is measured whole.
//
// ge:summary ge sound match, import and split: free sounds ranked against a reference and taken in
// ge:keywords cli ge sound sound-match sound-import sound-split ffmpeg decode loudness lufs
// ge:keywords bs1770 k-weighting trim silence fade features bands envelope match_score free-assets
// ge:skill agent-interface audio
// ge:test tests/cli_sound_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "cli/commands.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

Outcome run_sound(const Args& a);

// Exposed for the tests.

// The ffmpeg line that decodes `in` to mono raw float samples at `rate` in `out`.
std::string ffmpeg_decode_line(const fs::path& in, const fs::path& out, int rate);

// [begin, end) of the samples louder than `threshold_db` (dBFS); {0, 0} when none is.
struct SampleSpan {
  size_t begin = 0, end = 0;
};
SampleSpan loud_span(std::span<const float> x, double threshold_db);

// The sounds in one recording: each run louder than `threshold_db`, runs closer than `gap_s`
// joined, a piece shorter than `min_s` dropped.
std::vector<SampleSpan> sound_pieces(std::span<const float> x, int rate, double threshold_db,
                                     double gap_s, double min_s);

// BS.1770 integrated loudness of mono samples, in LUFS (-inf for silence: -200).
double loudness_lufs(std::span<const float> x, int rate);
// The largest |sample| in dBFS (-200 for silence).
double peak_dbfs(std::span<const float> x);

struct Shape {
  double max_s = 6;           // the longest the result may be
  bool loop = false;          // a loop: a 50 ms fade out instead of 20 ms
  double lufs = -16;          // the integrated loudness wanted
  double peak_db = -1.5;      // the ceiling the gain never takes a sample over
  double threshold_db = -50;  // quieter than this at either end is silence, trimmed
};
struct Shaped {
  std::vector<float> samples;
  double seconds = 0, lufs = 0, peak_db = 0;
  bool peak_limited = false;  // the gain stopped at the ceiling short of the loudness wanted
};
Shaped shape_sound(std::span<const float> x, int rate, const Shape& s);

// What a match compares: the loud span's length, the band energies (log10, the loudest band 0)
// over 0, 150, 400, 800, 1600, 3200, 6400 and 11025 Hz of its first two seconds, and the RMS of
// ten equal slices (the loudest 1).
struct SoundFeatures {
  double seconds = 0;
  std::array<double, 7> bands{};
  std::array<double, 10> envelope{};
};
SoundFeatures sound_features(std::span<const float> x, int rate);
// Lower is closer: |ln of the length ratio| + 0.8 x the mean band gap + 2 x the mean envelope gap.
double match_score(const SoundFeatures& reference, const SoundFeatures& candidate);

}  // namespace ge::cli
