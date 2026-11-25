// gpudebug: the frame without the console, its notes below it, to the clipboard (debug_shot.hpp).
//
// ge:summary gpudebug's command, the frame's notes and picture, the PNG packed off-thread
// ge:keywords gpudebug debug-shot clipboard notes add_gpudebug_command
// ge:skill replay agent-interface
// ge:test tests/capture_debug_shot_test.cpp
#include "capture/debug_shot.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <utility>

#include "capture/still.hpp"
#include "console/console.hpp"

namespace ge::capture {

DebugShot::~DebugShot() { flush(); }

void DebugShot::flush() {
  if (worker_.joinable()) worker_.join();
}

bool DebugShot::start(int frames, std::string* why) {
  if (busy_ || hiding_) {
    if (why != nullptr) *why = "The last picture is still being made.";
    return false;
  }
  frames = std::clamp(frames, 1, 6000);
  hiding_ = true;
  asked_ = false;
  left_ = frames;
  if (on_start) on_start(frames);
  return true;
}

bool DebugShot::due() {
  if (asked_) {
    asked_ = false;
    return true;
  }
  if (left_ <= 0) return false;
  --left_;
  return left_ == 0;
}

std::string DebugShot::text_of(std::span<const std::string> lines) {
  std::string out;
  for (const std::string& l : lines) out += l + "\n";
  return out;
}

bool DebugShot::write(uint32_t width, uint32_t height, std::span<const uint8_t> rgba) {
  if (!hiding_ || busy_) return false;  // a frame nobody asked for
  hiding_ = false;
  busy_ = true;
  // The notes: when, which build, the frame; then the game's own.
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char when[32];
  std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &local);
  std::vector<std::string> lines = {
      "# gpudebug " + std::string(when),
      "build " + (build.empty() ? std::string("unknown") : build),
      "frame " + std::to_string(width) + "x" + std::to_string(height) + " (the console left out)"};
  if (notes)
    for (std::string& l : notes()) lines.push_back(std::move(l));
  // The picture on this thread (its font is the frame's); packed on the worker.
  SheetPixels sheet;
  if (compose) {
    sheet = compose(static_cast<int>(width), static_cast<int>(height), rgba, lines);
  } else {
    sheet = {static_cast<int>(width), static_cast<int>(height), {rgba.begin(), rgba.end()}};
  }
  flush();
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    packed_ = false;
    text_ = text_of(lines);
    width_ = sheet.width;
    height_ = sheet.height;
  }
  worker_ = std::thread([this, sheet = std::move(sheet)] {
    std::vector<uint8_t> png;
    StillFormat used = StillFormat::kPng;
    std::string note;
    StillOptions o;
    o.format = StillFormat::kPng;
    encode_still(o, sheet.width, sheet.height, sheet.rgba, png, used, note);
    const std::lock_guard<std::mutex> lock(mutex_);
    png_ = std::move(png);
    note_ = std::move(note);
    packed_ = true;
  });
  return true;
}

std::vector<std::string> DebugShot::poll() {
  std::vector<std::string> out;
  if (!busy_) return out;
  std::vector<uint8_t> png;
  std::string text;
  int w = 0, h = 0;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!packed_) return out;
    packed_ = false;
    png = std::move(png_);
    text = std::move(text_);
    w = width_, h = height_;
  }
  flush();
  busy_ = false;
  const std::string size = std::to_string(w) + "x" + std::to_string(h);
  if (png.empty()) {
    out.push_back("gpudebug: the picture could not be made.");
    return out;
  }
  std::string why;
  if (!to_clipboard) {
    out.push_back("gpudebug: this game has no clipboard to copy the picture to.");
  } else if (!to_clipboard(png, text, &why)) {
    out.push_back("gpudebug: " + why + ".");
  } else {
    out.push_back("gpudebug: the picture (" + size +
                  ") and its notes are on the clipboard; paste them to an agent.");
  }
  return out;
}

void add_gpudebug_command(console::Console& console, DebugShot& shot) {
  console.add_command(
      "gpudebug",
      "gpudebug [frames]: the next frame without the console, with where you stand, the build and "
      "what the GPU spent over the frames before it (30 by default), copied to the clipboard as "
      "one picture for an agent",
      [&shot](console::Console& c, const console::Args& a) {
        const int frames = a.size() > 1 ? std::atoi(a[1].c_str()) : DebugShot::kDefaultFrames;
        std::string why;
        if (!shot.start(frames, &why)) {
          c.print("gpudebug: " + why);
          return;
        }
        c.print("gpudebug: measuring " + std::to_string(std::clamp(frames, 1, 6000)) +
                " frames, then copying the picture...");
      });
}

}  // namespace ge::capture
