// The screenshot writer's worker: the queue, the name taken by the second, the PNG packed and
// written under a temporary name renamed over.
//
// ge:summary The screenshot writer's worker thread, its queue and the file's name and write
// ge:keywords screenshot worker queue localtime rename temporary png
// ge:skill replay
// ge:test tests/capture_screenshot_test.cpp
#include "capture/screenshot.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>

#include "console/console.hpp"
#include "console/cvar.hpp"
#include "core/num_text.hpp"
#include "lab/png.hpp"
#include "lab/raster.hpp"

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
  cvars.bind(
      {.name = "scr_format",
       .fallback = "jpg",
       .flags = console::kArchive,
       .help = "The screenshot's format.",
       .kind = console::CvarKind::text,
       .values =
           "png: lossless, the largest; jpg: smaller, a little lost (the default); avif: the "
           "smallest, jpg without the encoders; heic: the platform's encoder, avif without it"},
      &s.format);
  cvars.bind({.name = "scr_quality",
              .fallback = "90",
              .flags = console::kArchive,
              .help = "The screenshot's quality for jpg, avif and heic.",
              .kind = console::CvarKind::integer,
              .min = 1,
              .max = 100,
              .values = "1..100: 1 the smallest to 100 the best (90 the default)"},
             &s.quality);
  cvars.bind({.name = "scr_chroma",
              .fallback = "444",
              .flags = console::kArchive,
              .help = "A jpg screenshot's colour detail, avif and heic always halving it.",
              .kind = console::CvarKind::integer,
              .min = 420,
              .max = 444,
              .values = "420: halved each way, about a quarter smaller; 444: full, sharp coloured "
                        "text (the default)"},
             &s.chroma);
  cvars.bind({.name = "scr_folder",
              .fallback = "",
              .flags = console::kArchive,
              .help = "The folder screenshots are kept in, beside the game's own.",
              .kind = console::CvarKind::text,
              .values = "\"\": the game's screenshots folder (the default); TEXT: a folder of that "
                        "name in the game's folder"},
             &s.folder);
}

void add_screenshot_command(console::Console& console, Screenshots& writer,
                            std::string_view where) {
  console.add_command("screenshot",
                      "screenshot [name]: the next frame shown written to " + std::string(where) +
                          " in scr_format, named by the time or by the name given (bound to F12)",
                      [&writer](console::Console& c, const console::Args& a) {
                        if (a.size() < 2) {
                          writer.request();
                          return;
                        }
                        std::string why;
                        if (!writer.request(a[1], &why)) c.print(why);
                      });
}

StillOptions still_options(const ScreenshotSettings& s, std::string& note) {
  StillOptions o;
  if (s.format == "tga" || s.format == "pcx") {
    o.format = StillFormat::kPng;  // the lossless formats of old, written as the lossless one now
  } else if (!still_format_of(s.format, o.format)) {
    note = "the screenshot format " + s.format + " isn't png, jpg, avif or heic, so it is jpg";
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

bool Screenshots::request(std::string name, std::string* why) {
  const fs::path p(name);
  bool bad = name.empty() || p.is_absolute() || name[0] == '/' || name[0] == '\\';
  for (const fs::path& part : p) bad = bad || part == "..";
  if (bad) {
    if (why)
      *why = name.empty() ? "A screenshot needs a name."
                          : "A screenshot's name can't leave its folder: " + name + ".";
    return false;
  }
  asked_ = file_asked_ = true;
  asked_name_ = std::move(name);
  return true;
}

std::string Screenshots::folder() const {
  if (settings_ == nullptr || settings_->folder.empty()) return dir_;
  const fs::path f(settings_->folder);
  bool bad = f.is_absolute();
  for (const fs::path& part : f) bad = bad || part == "..";
  if (bad) return dir_;  // a folder outside the game's is not taken
  return (fs::path(dir_).parent_path() / f).string();
}

bool Screenshots::grab(int width, int height, int quality) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (grab_wanted_ || grab_out_ || width <= 0 || height <= 0) return false;
  grab_wanted_ = grab_out_ = true;
  grab_width_ = width;
  grab_height_ = height;
  grab_quality_ = std::clamp(quality, 1, 100);
  asked_ = true;
  return true;
}

bool Screenshots::take_grab(std::vector<uint8_t>& out) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!grab_ready_) return false;
  out = std::move(grabbed_);
  grabbed_.clear();
  grab_ready_ = grab_out_ = false;
  return true;
}

bool Screenshots::due() {
  const bool now = asked_;
  asked_ = false;
  return now;
}

bool Screenshots::taken(const std::string& path) const {
  return std::find(handed_.begin(), handed_.end(), path) != handed_.end();
}

std::string Screenshots::next_path(std::string_view ext, const std::string& named) {
  std::error_code ec;
  if (!named.empty()) {
    const fs::path base = fs::path(folder()) / named;
    fs::path path = base.string() + "." + std::string(ext);
    for (int n = 2; fs::exists(path, ec) || taken(path.string()); ++n)
      path = base.string() + "-" + std::to_string(n) + "." + std::string(ext);
    handed_.push_back(path.string());
    return path.string();
  }
  // Two in one second are told apart by a count: the second's name on disk is checked too, so a
  // session started again within the second does not write over the first one's.
  const std::string stem = screenshot_stem(prefix_, local_now());
  if (stem != last_stem_) {
    last_stem_ = stem;
    last_count_ = 0;
  }
  for (;;) {
    ++last_count_;
    const std::string name = stem +
                             (last_count_ > 1 ? "-" + std::to_string(last_count_) : std::string()) +
                             "." + std::string(ext);
    const fs::path path = fs::path(folder()) / name;
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
  const bool grab = grab_wanted_;
  if (grab) {
    grab_wanted_ = false;
    Job g;
    g.grab_width = grab_width_;
    g.grab_height = grab_height_;
    g.width = width;
    g.height = height;
    g.rgba.assign(rgba.begin(), rgba.end());
    g.options = {StillFormat::kJpg, grab_quality_, false};
    jobs_.push_back(std::move(g));
  }
  if (!grab || file_asked_) {
    std::string note;
    const StillOptions o = still_options(settings_ ? *settings_ : ScreenshotSettings{}, note);
    std::string named;
    named.swap(asked_name_);
    jobs_.push_back({next_path(still_extension(still_writable(o.format)), named),
                     0,
                     0,
                     width,
                     height,
                     {rgba.begin(), rgba.end()},
                     o,
                     std::move(note)});
  }
  file_asked_ = false;
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
    if (job.path.empty()) {  // a grab: scaled, packed, kept for `take_grab`
      lab::Picture full{static_cast<int>(job.width), static_cast<int>(job.height),
                        std::move(job.rgba)};
      const lab::Picture small = lab::resized(full, job.grab_width, job.grab_height);
      encode_still(job.options, small.width, small.height, small.rgba, bytes, used, why);
      std::lock_guard<std::mutex> lock(mutex_);
      grabbed_ = std::move(bytes);
      grab_ready_ = true;
      busy_ = false;
      idle_.notify_all();
      continue;
    }
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
