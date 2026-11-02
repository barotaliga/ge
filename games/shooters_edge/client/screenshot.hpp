// SHOOTER'S EDGE'S SCREENSHOTS: F12 (a default bind, controls.cpp) runs the console's
// `screenshot`, and the next frame shown (the world, the HUD, a menu or the console when they are
// up; a played demo too) is written as `<game folder>/screenshots/shooters_edge-<time>.<ext>` in
// the format `scr_format`, `scr_quality` and `scr_chroma` set (jpg at 90 unless changed), beside
// the game folder's recordings (`record`), `saves/` and `settings/`. The frame is copied back with
// no stall and packed and written off the game's thread (engine/capture/screenshot.hpp); the
// console names the file once it is written.
//
// ge:summary Shooter's Edge's screenshot command and F12: the shown frame to the game folder
// ge:keywords screenshot f12 screenshots-folder game-folder png jpg avif heic console-command shots
// ge:keywords scr_format scr_quality scr_chroma
// ge:keywords screenshot_dir screenshot_frame
// ge:skill replay shooter
// ge:test tests/capture_screenshot_test.cpp
#pragma once

#include <filesystem>
#include <string>

#include "capture/screenshot.hpp"
#include "console/console.hpp"
#include "gpu/readback_ring.hpp"
#include "gpu/surface.hpp"

namespace shooters_edge::client {

// `<basedir>/<game>/screenshots` (`.` for no basedir), the folder `settings/` is beside.
std::filesystem::path screenshot_dir(const std::string& basedir, const std::string& game);

// The writer into `screenshot_dir` and the console's `screenshot` that asks for the next frame:
// made before the console runs the command line's `+` lines, so `+screenshot` takes the first.
struct Shots {
  Shots(ge::console::Console& console, const std::string& basedir, const std::string& game);
  ge::capture::ScreenshotSettings settings;  // the scr_ cvars
  ge::capture::Screenshots writer;
};

// The frame's copy back: made after the device, so its buffers go before it.
struct ShotCopy {
  ge::gpu::ReadbackRing ring{2};
  ge::gpu::Image picture;  // the copy that came back, its storage reused
};

// After the frame is drawn into the surface's texture and before it is presented: the frame
// copied when one is due, the copies that came back handed to the writer, and its lines printed.
void screenshot_frame(Shots& s, ShotCopy& copy, ge::gpu::Device& device,
                      const ge::gpu::Surface& surface, ge::console::Console& console);

}  // namespace shooters_edge::client
