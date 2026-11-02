// The screenshot writer's worker: the queue, the name taken by the second, the PNG packed and
// written under a temporary name renamed over.
//
// ge:summary The screenshot writer's worker thread, its queue and the file's name and write
// ge:keywords screenshot worker queue localtime rename temporary png
// ge:skill replay
// ge:test tests/capture_screenshot_test.cpp
#include "capture/screenshot.hpp"

#include <filesystem>
#include <system_error>
#include <utility>

#include "console/console.hpp"
#include "console/cvar.hpp"
#include "core/num_text.hpp"
#include "lab/png.hpp"

namespace ge::capture {

namespace fs = std::filesystem;

namespace {

std::tm local_now() {
  const std::time_t now = std::time(nullptr);
  std::tm t{};
#if defined(_WIN32)
  localtime_s(&t, &now);
#else
  localtime_r(&now, &t);
#endif
  return t;
}

std::string megabytes(size_t bytes) {
  return core::num::to_fixed(static_cast<double>(bytes) / (1024.0 * 1024.0), 1) + " MB";
}

}  // namespace

std::string screenshot_stem(std::string_view prefix, const std::tm& t) {
  // Each field at least `digits` wide, zero-padded (no format buffer for a compiler to doubt).
  const auto padded = [](int v, size_t digits) {
    std::string d = std::to_string(v);
    return d.size() < digits ? std::string(digits - d.size(), '0') + d : d;
  };
  return std::string(prefix) + "-" + padded(t.tm_year + 1900, 4) + "-" + padded(t.tm_mon + 1, 2) +
         "-" + padded(t.tm_mday, 2) + "-" + padded(t.tm_hour, 2) + padded(t.tm_min, 2) +
         padded(t.tm_sec, 2);
}

void add_screenshot_cvars(console::Cvars& cvars, ScreenshotSettings& s) {
  cvars.bind({.name = "scr_format",
              .fallback = "jpg",
              .flags = console::kArchive,
              .help = "the screenshot's format: png (lossless, largest), jpg, avif (smallest; "
                      "falls back to jpg without the encoders) or heic (the platform's encoder; "
                      "falls back to avif)",
              .kind = console::CvarKind::text},
             &s.format);
  cvars.bind({.name = "scr_quality",
              .fallback = "90",
              .flags = console::kArchive,
              .help = "the screenshot's quality for jpg, avif and heic: 1 smallest .. 100 best",
              .kind = console::CvarKind::integer,
              .min = 1,
              .max = 100},
             &s.quality);
  cvars.bind(
      {.name = "scr_chroma",
       .fallback = "444",
       .flags = console::kArchive,
       .help = "a jpg screenshot's colour detail: 444 full (sharp coloured text), 420 halved "
               "each way (about a quarter smaller); avif and heic are always 420",
       .kind = console::CvarKind::integer,
       .min = 420,
       .max = 444},
      &s.chroma);
}

void add_screenshot_command(console::Console& console, Screenshots& writer,
                            std::string_view where) {
  console.add_command("screenshot",
                      "screenshot: the next frame shown written to " + std::string(where) +
                          " in scr_format (bound to F12)",
                      [&writer](console::Console&, const console::Args&) { writer.request(); });
}

StillOptions still_options(const ScreenshotSettings& s, std::string& note) {
  StillOptions o;
  if (!still_format_of(s.format, o.format)) {
    note = "scr_format `" + s.format + "` is not png, jpg, avif or heic, so it is jpg";
    o.format = StillFormat::kJpg;
  }
  o.quality = s.quality;
  o.subsample = s.chroma != 444;
  return o;
}

Screenshots::Screenshots(std::string dir, std::string prefix, const ScreenshotSettings* settings)
    : dir_(std::move(dir)), prefix_(std::move(prefix)), settings_(settings) {}

Screenshots::~Screenshots() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  wake_.notify_all();
  if (worker_.joinable()) worker_.join();
}

bool Screenshots::due() {
  const bool now = asked_;
  asked_ = false;
  return now;
}

std::string Screenshots::next_path(std::string_view ext) {
  // Two in one second are told apart by a count: the second's name on disk is checked too, so a
  // session started again within the second does not write over the first one's.
  const std::string stem = screenshot_stem(prefix_, local_now());
  if (stem != last_stem_) {
    last_stem_ = stem;
    last_count_ = 0;
  }
  std::error_code ec;
  for (;;) {
    ++last_count_;
    const std::string name = stem +
                             (last_count_ > 1 ? "-" + std::to_string(last_count_) : std::string()) +
                             "." + std::string(ext);
    const fs::path path = fs::path(dir_) / name;
    if (!fs::exists(path, ec)) return path.string();
  }
}

bool Screenshots::write(uint32_t width, uint32_t height, std::span<const uint8_t> rgba) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (static_cast<int>(jobs_.size()) >= kMaxQueued) {
    lines_.push_back("screenshot: " + std::to_string(jobs_.size()) +
                     " still being written, so this one was not taken; next: press the key again "
                     "in a moment");
    return false;
  }
  if (rgba.size() != static_cast<size_t>(width) * height * 4 || width == 0 || height == 0) {
    lines_.push_back("screenshot: the frame came back as " + std::to_string(rgba.size()) +
                     " bytes, not " + std::to_string(width) + "x" + std::to_string(height) +
                     " RGBA; this is a bug in the game's copy of its frame");
    return false;
  }
  std::string note;
  const StillOptions o = still_options(settings_ ? *settings_ : ScreenshotSettings{}, note);
  jobs_.push_back({next_path(still_extension(still_writable(o.format))),
                   width,
                   height,
                   {rgba.begin(), rgba.end()},
                   o,
                   std::move(note)});
  if (!worker_.joinable()) worker_ = std::thread([this] { work(); });
  wake_.notify_one();
  return true;
}

std::vector<std::string> Screenshots::poll() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> out;
  out.swap(lines_);
  return out;
}

void Screenshots::flush() {
  std::unique_lock<std::mutex> lock(mutex_);
  idle_.wait(lock, [this] { return jobs_.empty() && !busy_; });
}

void Screenshots::work() {
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
      if (jobs_.empty()) return;  // stopped with nothing left to write
      job = std::move(jobs_.front());
      jobs_.pop_front();
      busy_ = true;
    }
    std::vector<uint8_t> bytes;
    StillFormat used = job.options.format;
    std::string why;
    encode_still(job.options, static_cast<int>(job.width), static_cast<int>(job.height), job.rgba,
                 bytes, used, why);
    if (!why.empty()) job.note += (job.note.empty() ? "" : "; ") + why;
    fs::path path(job.path);
    std::error_code ec;
    // An encoder that refused at open fell back to another format: the name follows it.
    if (path.extension() != "." + std::string(still_extension(used))) {
      const fs::path stem = fs::path(job.path).replace_extension();
      path = stem.string() + "." + std::string(still_extension(used));
      for (int n = 2; fs::exists(path, ec); ++n)
        path = stem.string() + "-" + std::to_string(n) + "." + std::string(still_extension(used));
      job.path = path.string();
    }
    const fs::path part = fs::path(job.path + ".part");
    fs::create_directories(path.parent_path(), ec);
    bool ok = lab::write_file(part.string(), bytes);
    if (ok) {
      fs::rename(part, path, ec);
      ok = !ec;
      if (!ok) fs::remove(part, ec);
    }
    const std::string quality =
        used == StillFormat::kPng
            ? std::string()
            : " quality " + std::to_string(job.options.quality) +
                  (used == StillFormat::kJpg && !job.options.subsample ? " 4:4:4" : " 4:2:0");
    std::string line =
        ok ? "screenshot: " + job.path + " (" + std::to_string(job.width) + "x" +
                 std::to_string(job.height) + ", " + std::string(still_extension(used)) + quality +
                 ", " + megabytes(bytes.size()) + ")" + (job.note.empty() ? "" : "; " + job.note)
           : "screenshot: could not write " + job.path +
                 "; next: check the folder is writable and the disk has room";
    {
      std::lock_guard<std::mutex> lock(mutex_);
      lines_.push_back(std::move(line));
      busy_ = false;
    }
    idle_.notify_all();
  }
}

}  // namespace ge::capture
