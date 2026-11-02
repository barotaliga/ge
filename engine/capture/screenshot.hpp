// SCREENSHOTS (docs/replays.md § Screenshots): the frame the player asks for kept as a picture
// file in the game's own folder, with no frame of the game waiting for it.
//
// - THE KEY: a game binds a key (F12 in both games) to its `screenshot` command, which calls
//   `request`. Once a frame, after the frame is drawn and before it is presented, the game asks
//   `due`; when it says yes the game copies the frame back with no stall (gpu/readback_ring.hpp,
//   `Surface::frame_target`) and hands the pixels to `write` when they arrive a frame or two later.
// - THE WRITE: `write` copies the pixels and the player's settings and returns at once. A worker
//   thread of its own packs them in the format the settings name (capture/still.hpp: png, jpg,
//   avif or heic, falling back when this build or machine cannot make it) and writes the file
//   under a temporary name renamed over, so a half-written screenshot is never left. The folder
//   is made when it is missing. With `kMaxQueued` pictures already waiting a new one is refused
//   and said so, never queued without bound.
// - THE SETTINGS are the player's cvars (`add_screenshot_cvars`, archived): `scr_format` png, jpg
//   (the default: opens everywhere, a tenth of a PNG), avif or heic; `scr_quality` 1..100 (90)
//   for the lossy ones; `scr_chroma` 444 (the default: a JPEG's colour at full size, so a HUD's
//   coloured letters stay sharp) or 420 (halved each way, about a quarter smaller).
// - THE NAME: `<prefix>-YYYY-MM-DD-HHMMSS.<ext>` in local time (`screenshot_stem`), then `-2`, `-3`
//   ... when that second already has one, so the folder lists in the order they were taken and no
//   screenshot is ever written over another.
// - THE REPORT: each file written, or why it was not, comes back from `poll` on the game's thread
//   as a line for its console.
//
// ge:summary Screenshots: the key's request, the settings, the frame packed and written off-thread
// ge:keywords screenshot f12 print-screen png still picture frame worker no-stall request due
// ge:keywords write poll flush screenshot_stem screenshots-folder scr_format scr_quality scr_chroma
// ge:keywords add_screenshot_cvars still_options jpeg-quality
// ge:skill replay
// ge:test tests/capture_screenshot_test.cpp
#pragma once

#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <deque>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "capture/still.hpp"

namespace ge::console {
class Cvars;
}

namespace ge::capture {

struct ScreenshotSettings {
  std::string format = "jpg";  // scr_format: png, jpg, avif, heic
  int32_t quality = 90;        // scr_quality: 1..100
  int32_t chroma = 444;        // scr_chroma: 444 full, 420 halved (JPEG)
};

// scr_format, scr_quality and scr_chroma bound to `s`, archived (the player's to keep).
void add_screenshot_cvars(console::Cvars& cvars, ScreenshotSettings& s);
// The settings as the writer's options; an unknown format is JPEG, said in `note`.
StillOptions still_options(const ScreenshotSettings& s, std::string& note);

// `<prefix>-YYYY-MM-DD-HHMMSS` for the local time `t`.
std::string screenshot_stem(std::string_view prefix, const std::tm& t);

class Screenshots {
 public:
  static constexpr int kMaxQueued = 4;  // pictures waiting for the worker before one is refused

  // Screenshots written to `dir` (made when the first is written) as `<prefix>-<time>.<ext>`, in
  // the format `settings` names when each is handed in (the defaults when null; it must outlive
  // the writer).
  Screenshots(std::string dir, std::string prefix, const ScreenshotSettings* settings = nullptr);
  Screenshots(const Screenshots&) = delete;
  Screenshots& operator=(const Screenshots&) = delete;
  ~Screenshots();  // the pictures still waiting are written first

  // The key was pressed: the next frame drawn is taken.
  void request() { asked_ = true; }
  // Whether this frame is the one to copy back; true once a request, then false until the next.
  bool due();
  // The frame's pixels (RGBA8, rows top to bottom, no padding) handed to the worker, named by the
  // local time now. False, with the reason in the next `poll`, when too many are waiting.
  bool write(uint32_t width, uint32_t height, std::span<const uint8_t> rgba);
  // The lines the worker has to report since the last call (a file written, or why not).
  std::vector<std::string> poll();
  // Block until every picture handed in is written (the end of a session, a test).
  void flush();

  const std::string& dir() const { return dir_; }

 private:
  struct Job {
    std::string path;
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgba;
    StillOptions options;
    std::string note;  // why the format is not the one asked for
  };
  void work();
  std::string next_path(std::string_view ext);

  std::string dir_;
  std::string prefix_;
  const ScreenshotSettings* settings_ = nullptr;
  bool asked_ = false;
  std::string last_stem_;  // the second the last name was taken in, and its count
  int last_count_ = 0;

  std::mutex mutex_;
  std::condition_variable wake_, idle_;
  std::deque<Job> jobs_;
  std::vector<std::string> lines_;
  bool busy_ = false;
  bool stop_ = false;
  std::thread worker_;  // started by the first write
};

}  // namespace ge::capture
