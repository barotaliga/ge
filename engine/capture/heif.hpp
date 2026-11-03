// THE HEIF STILL (docs/replays.md § Screenshots): one coded picture in the ISO image file format,
// as AVIF (an AV1 keyframe) or HEIC (an HEVC one), the forms a phone, a browser and an operating
// system's viewer open. The engine's encoders code the picture (capture/video.hpp); this file only
// boxes it.
//
// THE FILE: `ftyp` (avif or heic, with mif1 and miaf), then `meta` (the `pict` handler, the
// primary item 1, its location, its type av01 or hvc1, and its properties: the codec's
// configuration (`av1C` or `hvcC`, the bytes the encoder's codec private already is), the size
// `ispe`, three 8-bit channels `pixi`, and the colour `colr`: BT.709 primaries and matrix, the
// sRGB transfer a frame is drawn in, limited range as capture/yuv.hpp converts), then `mdat` with
// the picture's bytes. The reader takes the same form back, for the tests and the labs.
//
// ge:summary The HEIF still: one AV1 or HEVC picture boxed as an AVIF or HEIC file, and read back
// ge:keywords heif avif heic still image ftyp meta iloc iinf infe ipco ipma ispe pixi colr nclx
// ge:keywords av1c hvcc mdat write_heif read_heif screenshot
// ge:skill replay
// ge:test tests/capture_heif_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::capture {

enum class HeifKind : uint8_t { kAvif, kHeic };

struct HeifStill {
  HeifKind kind = HeifKind::kAvif;
  int width = 0, height = 0;
  std::vector<uint8_t> config;  // the av1C or hvcC record's bytes (the encoder's codec private)
  std::vector<uint8_t> data;    // the coded picture: AV1 OBUs, or HEVC NAL units led by lengths
};

std::vector<uint8_t> write_heif(const HeifStill& still);

// The still a file holds (its primary item); false, with the reason in `error`, for a file that
// is not one picture of AV1 or HEVC in this form.
bool read_heif(std::span<const uint8_t> bytes, HeifStill& out, std::string& error);

}  // namespace ge::capture
