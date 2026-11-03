// A STILL PICTURE IN THE FORMAT ASKED (docs/replays.md § Screenshots): a frame packed as a PNG, a
// JPEG, an AVIF or an HEIC by the engine's own writers and encoders, smallest last:
//
//   png    lossless, every build (lab/png.hpp's encode_png_rgb); the largest
//   jpg    the engine's own baseline JPEG (capture/jpeg.hpp), every build; `quality` 1..100, the
//          chroma at full size or halved (`subsample`); opens everywhere
//   avif   one AV1 keyframe (libaom at its slowest realtime speed) in a HEIF file
//          (capture/heif.hpp), where the engine's encoders are built (GE_ENCODERS); `quality`
//          maps to the encoder's quantizer, (100 - q) * 63 / 100; about half a JPEG's bytes at
//          the same PSNR on a game's frame (the `screenshot` lab)
//   heic   one HEVC picture from the platform's encoder (capture/platform_video.hpp: VideoToolbox
//          today) in a HEIF file, at the platform's constant quality q / 100
//
// AVIF and HEIC carry 4:2:0 at BT.709 limited range (capture/yuv.hpp) and need even sides, so an
// odd last row or column is cut off. A FORMAT THIS BUILD OR MACHINE CANNOT WRITE FALLS BACK, never
// fails: HEIC to AVIF, AVIF to JPEG; `still_writable` says ahead of time (so a screenshot is named
// for what it will be) and `encode_still` falls back too when an encoder refuses at open, saying
// so in its note.
//
// ge:summary A still picture packed as PNG, JPEG, AVIF or HEIC, falling back when one cannot be
// ge:summary made
// ge:keywords still screenshot format png jpg jpeg avif heic heif quality chroma fallback
// ge:keywords encode_still decode_still still_format_of still_writable still_extension
// ge:skill replay
// ge:test tests/capture_still_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "lab/raster.hpp"

namespace ge::capture {

enum class StillFormat : uint8_t { kPng, kJpg, kAvif, kHeic };

// The format a word names: png, jpg (or jpeg), avif, heic; false for any other.
bool still_format_of(std::string_view word, StillFormat& out);
// The file's extension, without the dot: png, jpg, avif, heic.
std::string_view still_extension(StillFormat f);
// What this build and machine will write when asked for `f`: `f` itself when it can, else the
// first of its fallbacks that it can (HEIC to AVIF to JPEG).
StillFormat still_writable(StillFormat f);

struct StillOptions {
  StillFormat format = StillFormat::kJpg;
  int quality = 90;        // 1..100, for the lossy formats
  bool subsample = false;  // JPEG's chroma halved each way (AVIF and HEIC always halve it)
};

// `rgba` (`width` x `height`, rows top to bottom) packed as `o.format`, or as the format it falls
// back to; `used` is the one written, and `note` says why when it is not the one asked for. False
// only when the picture has no pixels.
bool encode_still(const StillOptions& o, int width, int height, std::span<const uint8_t> rgba,
                  std::vector<uint8_t>& out, StillFormat& used, std::string& note);

// The picture a still holds, whatever its format (read by its bytes); for the tests and the labs.
std::optional<lab::Picture> decode_still(std::span<const uint8_t> bytes, std::string& error);

}  // namespace ge::capture
