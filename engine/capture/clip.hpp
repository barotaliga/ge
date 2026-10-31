// A CLIP: a row of pictures written as a short video file, for a lab or a tool that wants a
// person to WATCH something move (a model's animation, a weapon firing) rather than read a strip.
// The pictures are coded by the export's own codec by its word (capture/video.hpp: "av1" plays in
// every browser in a .webm) and muxed in the container the file's name picks (capture/mux.hpp:
// .webm, .mkv or .mp4). No sound. A build without the encoders (GE_ENCODERS off) refuses with
// the reason, so a lab can note it and keep its GIF.
//
// ge:summary A row of pictures written as a short video (WebM AV1 by default): a lab's animation
// ge:keywords capture clip video webm av1 animation frames write_clip watch movie
// ge:skill replay labs
// ge:test tests/capture_clip_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>

#include "lab/raster.hpp"

namespace ge::capture {

// `frames` (all one size; an odd width or height loses its last column or row) at `fps` written
// to `path`, coded by `codec` (capture/video.hpp's words). False with the reason in `error`.
bool write_clip(const std::string& path, std::span<const lab::Picture> frames, double fps,
                std::string& error, std::string_view codec = "av1");

}  // namespace ge::capture
