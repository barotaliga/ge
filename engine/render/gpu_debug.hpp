// A GAME WINDOW'S GPU TOOLS, WHOLE: `gpustats` (gpu_watch.hpp) and `gpudebug`
// (capture/debug_shot.hpp: the frame without the console, its notes below it, copied to the
// clipboard as one picture for an agent), so a game's part is its wiring:
//
//   ge::render::GpuDebug gpu_debug;                       // beside the console
//   ge::render::add_gpu_debug(console, gpu_debug, device.get(), [&] { return my_notes(); });
//   gpu_debug.shot.build = ge::capture::build_info_line(ge::capture::build_info());
//   gpu_debug.shot.to_clipboard = ge::platform::set_clipboard_png;
//   ge::render::WindowShots shot_copy(gpu_debug.shot);    // in place of the ScreenshotCopy
//   if (!gpu_debug.shot.hiding()) <draw the console>;
//   ge::render::gpu_watch_frame(gpu_debug.watch, recorder.stats, frame_s, console);  // drawn
//   shot_copy.frame(device, surface.frame_target(), shots, console);  // before the present
//
// - THE NOTES: the engine's (the time, the build, the frame's size), then the game's own (`notes`:
//   where the player stands and looks, the map, a line that draws the same picture again: what an
//   agent needs to find the place), then "# gpu": the adapter and gpustats' report over the frames
//   measured before the picture.
// - THE PICTURE: ui/debug_sheet.hpp's frame with the notes in a band below it.
// - `WindowShots`: the screenshot's copy and gpudebug's side by side (render/screenshot_copy.hpp),
//   called once a frame where a window called the screenshot's alone.
//
// Header-only, like screenshot_copy.hpp: the frame core takes no link on the capture library; the
// window that includes it links both (and ge_ui, which it already does).
//
// ge:summary A window's gpustats and gpudebug in one: the notes, the sheet, the copies, the wiring
// ge:keywords gpudebug gpustats debug-shot window-shots add_gpu_debug clipboard agent notes
// ge:skill render agent-interface
// ge:test tests/render_gpu_watch_test.cpp
#pragma once

#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "capture/debug_shot.hpp"
#include "console/console.hpp"
#include "gpu/device.hpp"
#include "render/gpu_watch.hpp"
#include "render/screenshot_copy.hpp"
#include "ui/debug_sheet.hpp"

namespace ge::render {

struct GpuDebug {
  GpuWatch watch;           // gpustats', and gpudebug's measuring
  capture::DebugShot shot;  // gpudebug
};

// `gpustats` and `gpudebug` on `con` over `g` (which must outlive it). `notes` gives the game's own
// lines when the frame is taken (null: none); `device` names the adapter in the notes.
inline void add_gpu_debug(console::Console& con, GpuDebug& g, const gpu::Device* device,
                          std::function<std::vector<std::string>()> notes) {
  add_gpustats(con, g.watch);
  g.shot.on_start = [&g](int frames) { start_gpu_watch(g.watch, frames, true); };
  g.shot.notes = [&g, device, notes = std::move(notes)] {
    std::vector<std::string> out = notes ? notes() : std::vector<std::string>{};
    for (std::string& l : gpu_notes(g.watch, device)) out.push_back(std::move(l));
    return out;
  };
  g.shot.compose = [](int w, int h, std::span<const uint8_t> rgba,
                      std::span<const std::string> lines) {
    ui::Image sheet = ui::debug_sheet(w, h, rgba, lines);
    return capture::SheetPixels{sheet.width, sheet.height, std::move(sheet.rgba)};
  };
  capture::add_gpudebug_command(con, g.shot);
}

// The screenshot's copy and gpudebug's, made after the device (screenshot_copy.hpp).
class WindowShots {
 public:
  explicit WindowShots(capture::DebugShot& debug) : debug_(debug) {}
  template <class Writer>
  void frame(gpu::Device& device, const gpu::Target& shown, Writer& shots,
             console::Console& console) {
    shots_.frame(device, shown, shots, console);
    debug_copy_.frame(device, shown, debug_, console);
  }
  // Whether gpudebug is taking a frame now (the console left out of it).
  bool hiding() const { return debug_.hiding(); }

 private:
  capture::DebugShot& debug_;
  ScreenshotCopy shots_, debug_copy_;
};

}  // namespace ge::render
