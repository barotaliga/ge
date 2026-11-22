// A PICTURE FOR AN AGENT (`gpudebug`): the frame as the player sees it, without the console, with
// everything an agent asked to look at it would want written below it, copied to the clipboard as
// one picture (and its words as the same copy's text). Nothing is saved: it is pasted.
//
// - THE COMMAND (`add_gpudebug_command`): `gpudebug [frames]` hides the console (`hiding`, which
//   the game reads where it draws it), lets `frames` frames pass (30 by default) while the game
//   measures them (`on_start`: its GPU watch), then takes the next frame shown.
// - THE TAKING is the screenshot's (render/screenshot_copy.hpp): this class is a writer as
//   capture::Screenshots is (`due`, `request`, `write`, `poll`), so the frame comes back with no
//   stall a frame or two later, and a second `ScreenshotCopy` drives it.
// - THE NOTES: a heading with the local time, the build (`build`, the game's
//   capture/build_info.hpp line) and the frame's size, then the game's own (`notes`, asked at the
//   moment the frame comes back: where the player stands and looks, the map, the look, the GPU's
//   numbers). A line starting "# " is a heading.
// - THE PICTURE is the game's `compose` (ui/debug_sheet.hpp: the frame with the notes in a band
//   below it), drawn on the game's thread; packed as PNG on a worker of its own; put on the
//   clipboard by `to_clipboard` (platform/devices.hpp's set_clipboard_png) on the game's thread in
//   `poll`, which also says what was copied, or why not, as a line for the console.
//
// ge:summary gpudebug: the frame without the console, its notes below it, copied to the clipboard
// ge:keywords gpudebug debug-shot clipboard screenshot notes agent report paste build-info
// ge:keywords add_gpudebug_command debug_shot hiding compose to_clipboard
// ge:skill replay agent-interface
// ge:test tests/capture_debug_shot_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace ge::console {
class Console;
}  // namespace ge::console

namespace ge::capture {

// A picture as the game's `compose` hands it back: RGBA8, rows top to bottom.
struct SheetPixels {
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;
};

class DebugShot {
 public:
  static constexpr int kDefaultFrames = 30;

  // THE GAME'S PART, set once before the command runs.
  std::string build;                                // the build's line (build_info_line)
  std::function<void(int frames)> on_start;         // measuring the next `frames` frames begins
  std::function<std::vector<std::string>()> notes;  // its lines, when the frame is taken
  std::function<SheetPixels(int width, int height, std::span<const uint8_t> rgba,
                            std::span<const std::string> lines)>
      compose;  // the frame and the lines as one picture
  std::function<bool(std::span<const uint8_t> png, const std::string& text, std::string* why)>
      to_clipboard;

  DebugShot() = default;
  DebugShot(const DebugShot&) = delete;
  DebugShot& operator=(const DebugShot&) = delete;
  ~DebugShot();

  // `gpudebug [frames]`: the console hidden, `frames` frames measured, then the next one taken.
  // False, with the reason in `why`, while the last one is still being made.
  bool start(int frames, std::string* why);
  // Whether the console is left out of the frames drawn now (from `start` until the frame is
  // taken).
  bool hiding() const { return hiding_; }

  // The screenshot writer's half (render/screenshot_copy.hpp's ScreenshotCopy).
  bool due();
  void request() { asked_ = true; }  // the copy could not be made this frame: the next one
  bool write(uint32_t width, uint32_t height, std::span<const uint8_t> rgba);
  std::vector<std::string> poll();
  // Block until the picture in the worker is packed (a test; the end of a session).
  void flush();

  // The text copied beside the picture: the lines, one a line ("# " headings kept).
  static std::string text_of(std::span<const std::string> lines);

 private:
  bool hiding_ = false;
  bool asked_ = false;
  int left_ = 0;       // frames still to pass before the one taken
  bool busy_ = false;  // a picture in the worker or waiting for the clipboard

  std::mutex mutex_;
  std::thread worker_;
  bool packed_ = false;  // the worker is done (under mutex_)
  std::vector<uint8_t> png_;
  std::string text_, note_;
  int width_ = 0, height_ = 0;
};

// The console's `gpudebug` over `shot` (which must outlive the console).
void add_gpudebug_command(console::Console& console, DebugShot& shot);

}  // namespace ge::capture
