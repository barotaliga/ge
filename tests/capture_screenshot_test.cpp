// Screenshots (engine/capture/screenshot.hpp) and the packed colour PNG (lab/png.hpp's
// encode_png_rgb): the name by the local time, a request due once, the picture written off the
// game's thread and, as a PNG, read back to the same pixels (the alpha dropped); the defaults a
// JPEG; the scr_ cvars reaching the writer frame by frame, an unknown format a JPEG said so; two in
// one second told apart, a folder made when missing, a full queue refused and said so, and a
// packed frame far smaller than the stored writer's.
//
// ge:summary Tests: screenshots named, requested once, written off-thread and read back the same
// ge:keywords test screenshot png rgb packed name request due flush queue folder read-back
// ge:keywords scr_format scr_quality scr_chroma settings cvars jpg default
// ge:skill replay testing
// ge:test tests/capture_screenshot_test.cpp
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "capture/screenshot.hpp"
#include "capture/still.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "lab/png.hpp"

namespace fs = std::filesystem;
namespace gc = ge::capture;

namespace {

// A frame-like picture: a sky's gradient over a ground with a pattern, alpha left at 0 (as a
// surface's copy may hand it).
std::vector<uint8_t> frame(uint32_t w, uint32_t h) {
  std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
  for (uint32_t y = 0; y < h; ++y)
    for (uint32_t x = 0; x < w; ++x) {
      uint8_t* p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
      const bool sky = y < h / 2;
      p[0] = static_cast<uint8_t>(sky ? 90 + y / 4 : ((x / 8 + y / 8) % 2 ? 200 : 120));
      p[1] = static_cast<uint8_t>(sky ? 140 + y / 6 : 110 + x % 17);
      p[2] = static_cast<uint8_t>(sky ? 230 : 60 + (x * y) % 23);
      p[3] = 0;
    }
  return rgba;
}

fs::path fresh_dir(const char* name) {
  const fs::path dir = fs::temp_directory_path() / name;
  std::error_code ec;
  fs::remove_all(dir, ec);
  return dir;
}

std::vector<fs::path> pngs(const fs::path& dir, const char* ext = ".png") {
  std::vector<fs::path> out;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(dir, ec))
    if (e.path().extension() == ext) out.push_back(e.path());
  return out;
}

const gc::ScreenshotSettings kPng{"png", 90, 444, "", "", 6};

}  // namespace

TEST(the_name_is_the_prefix_and_the_local_time) {
  std::tm t{};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 10;
  t.tm_mday = 2;
  t.tm_hour = 9;
  t.tm_min = 5;
  t.tm_sec = 7;
  CHECK(gc::screenshot_stem("shooters_edge", t) == "shooters_edge-2026-11-02-090507");
}

TEST(a_request_is_due_once) {
  gc::Screenshots shots(fresh_dir("ge_screenshot_due").string(), "x");
  CHECK(!shots.due());
  shots.request();
  shots.request();  // pressed twice before a frame: one picture
  CHECK(shots.due());
  CHECK(!shots.due());
}

TEST(a_request_is_asked_until_its_frame_is_taken) {
  // A window keeps drawing while one is asked, so `screenshot` then `quit` still writes it.
  gc::Screenshots shots(fresh_dir("ge_screenshot_asked").string(), "x");
  CHECK(!shots.asked());
  shots.request();
  CHECK(shots.asked());
  CHECK(shots.due());
  CHECK(!shots.asked());
}

TEST(a_frame_is_written_and_reads_back_the_same) {
  const fs::path dir = fresh_dir("ge_screenshot_write") / "made" / "screenshots";
  gc::Screenshots shots(dir.string(), "game", &kPng);
  const std::vector<uint8_t> rgba = frame(64, 48);
  CHECK(shots.write(64, 48, rgba));
  shots.flush();
  const std::vector<std::string> lines = shots.poll();
  CHECK(lines.size() == 1);
  CHECK_MSG(lines[0].find("screenshot: ") == 0 && lines[0].find("64x48") != std::string::npos,
            lines[0]);
  const std::vector<fs::path> files = pngs(dir);
  CHECK(files.size() == 1);
  if (files.size() != 1) return;
  CHECK(files[0].filename().string().rfind("game-", 0) == 0);
  const auto back = ge::lab::read_png(files[0].string());
  CHECK(back.has_value());
  if (!back) return;
  CHECK(back->width == 64 && back->height == 48);
  bool same = true;
  for (size_t i = 0; i < rgba.size(); ++i)
    same = same && back->rgba[i] == (i % 4 == 3 ? 255 : rgba[i]);
  CHECK(same);
  CHECK(shots.poll().empty());
}

TEST(the_defaults_write_a_jpeg_and_the_cvars_reach_the_writer) {
  const fs::path dir = fresh_dir("ge_screenshot_cvars");
  ge::console::Console con;
  gc::ScreenshotSettings settings;
  gc::add_screenshot_cvars(con.cvars(), settings);
  gc::Screenshots shots(dir.string(), "game", &settings);
  const std::vector<uint8_t> rgba = frame(64, 48);
  CHECK(shots.write(64, 48, rgba));  // the defaults: jpg at 90, full chroma
  con.execute("scr_format png");
  CHECK(settings.format == "png");
  CHECK(shots.write(64, 48, rgba));
  con.execute("scr_format bmp");  // a word no writer knows: a JPEG, said so
  CHECK(shots.write(64, 48, rgba));
  con.execute("scr_quality 300");  // clamped to 100
  CHECK(settings.quality == 100);
  con.execute("scr_chroma 420");
  CHECK(settings.chroma == 420);
  shots.flush();
  const std::vector<std::string> lines = shots.poll();
  CHECK(lines.size() == 3);
  CHECK(pngs(dir, ".jpg").size() == 2 && pngs(dir, ".png").size() == 1);
  CHECK_MSG(lines.size() == 3 && lines[0].find("jpg quality 90 4:4:4") != std::string::npos,
            lines.empty() ? "" : lines[0]);
  CHECK_MSG(lines.size() == 3 && lines[2].find("format bmp isn't") != std::string::npos,
            lines.size() == 3 ? lines[2] : "");
}

TEST(two_in_one_second_are_told_apart) {
  const fs::path dir = fresh_dir("ge_screenshot_two");
  gc::Screenshots shots(dir.string(), "game", &kPng);
  const std::vector<uint8_t> rgba = frame(8, 8);
  // Three written back to back land in at most two seconds: at least two share a second and
  // still make three files.
  for (int i = 0; i < 3; ++i) CHECK(shots.write(8, 8, rgba));
  shots.flush();
  CHECK(shots.poll().size() == 3);
  CHECK(pngs(dir).size() == 3);
}

TEST(a_full_queue_and_a_wrong_size_are_refused_and_said) {
  const fs::path dir = fresh_dir("ge_screenshot_full");
  gc::Screenshots shots(dir.string(), "game", &kPng);
  const std::vector<uint8_t> rgba = frame(512, 512);
  int taken = 0;
  for (int i = 0; i < 20; ++i) taken += shots.write(512, 512, rgba) ? 1 : 0;
  CHECK(!shots.write(4, 4, rgba));  // the bytes are not 4x4's
  shots.flush();
  const std::vector<std::string> lines = shots.poll();
  CHECK(static_cast<int>(lines.size()) == 21);
  CHECK(static_cast<int>(pngs(dir).size()) == taken);
  int refused = 0;
  for (const std::string& l : lines) refused += l.find("next:") != std::string::npos ? 1 : 0;
  CHECK(refused == 21 - taken);
  CHECK(refused >= 1);  // the bug line at least
}

TEST(a_packed_frame_is_far_smaller_than_a_stored_one) {
  const std::vector<uint8_t> rgba = frame(320, 180);
  const std::vector<uint8_t> packed = ge::lab::encode_png_rgb(320, 180, rgba);
  ge::lab::Picture p;
  p.width = 320;
  p.height = 180;
  p.rgba = rgba;
  const std::vector<uint8_t> stored = ge::lab::encode_png(p);
  CHECK_MSG(packed.size() * 4 < stored.size(),
            std::to_string(packed.size()) + " against " + std::to_string(stored.size()));
}

TEST(a_named_screenshot_keeps_its_name_and_never_overwrites) {
  const fs::path dir = fresh_dir("ge_screenshot_named") / "screenshots";
  gc::ScreenshotSettings set = kPng;
  gc::Screenshots shots(dir.string(), "game", &set);
  std::string why;
  CHECK(!shots.request("../out", &why) && !why.empty());
  CHECK(!shots.request("", &why));
  CHECK(!shots.due());
  const std::vector<uint8_t> rgba = frame(8, 8);
  for (int i = 0; i < 2; i++) {
    CHECK(shots.request("duel/a_vs_b", &why) && shots.due());
    CHECK(shots.write(8, 8, rgba));
  }
  shots.flush();
  CHECK(fs::exists(dir / "duel" / "a_vs_b.png") && fs::exists(dir / "duel" / "a_vs_b-2.png"));
  // Another folder beside it; one leaving the game's folder is passed over.
  set.folder = "shots";
  CHECK(shots.request("x", &why) && shots.due() && shots.write(8, 8, rgba));
  set.folder = "../../escape";
  CHECK(shots.request("y", &why) && shots.due() && shots.write(8, 8, rgba));
  shots.flush();
  CHECK(fs::exists(dir.parent_path() / "shots" / "x.png") && fs::exists(dir / "y.png"));
  // A whole path is the player's own folder, taken as it is.
  const fs::path own = dir.parent_path() / "elsewhere";
  set.folder = own.string();
  CHECK(shots.request("z", &why) && shots.due() && shots.write(8, 8, rgba));
  shots.flush();
  CHECK(fs::exists(own / "z.png"));
  // The old formats' words: tga and pcx are png, jpeg is jpg.
  std::string note;
  set.format = "tga";
  CHECK(gc::still_options(set, note).format == gc::StillFormat::kPng && note.empty());
  set.format = "jpeg";
  CHECK(gc::still_options(set, note).format == gc::StillFormat::kJpg && note.empty());
}

// A template (`scr_template`) names each screenshot by the first free number in its X's; one with
// no X is a plain name; one leaving the folder is passed over for the time's name. Compression 0
// stores a png's rows unpacked.
TEST(a_template_numbers_its_xs_and_compression_zero_stores_the_rows) {
  CHECK(gc::screenshot_template_name("quakeXXX", 7) == "quake007");
  CHECK(gc::screenshot_template_name("aXXb", 42) == "a42b");
  CHECK(gc::screenshot_template_name("quakeXXX", 1000).empty());
  CHECK(gc::screenshot_template_name("shot", 3) == "shot");
  const fs::path dir = fresh_dir("ge_screenshot_template") / "screenshots";
  gc::ScreenshotSettings set = kPng;
  set.name_template = "quakeXXX";
  gc::Screenshots shots(dir.string(), "game", &set);
  const std::vector<uint8_t> rgba = frame(8, 8);
  for (int i = 0; i < 2; i++) {
    shots.request();
    CHECK(shots.due() && shots.write(8, 8, rgba));
  }
  set.name_template = "../out";
  shots.request();
  CHECK(shots.due() && shots.write(8, 8, rgba));
  shots.flush();
  CHECK(fs::exists(dir / "quake000.png") && fs::exists(dir / "quake001.png"));
  CHECK(!fs::exists(dir.parent_path() / "out.png"));
  std::string note;
  set.compression = 0;
  CHECK(gc::still_options(set, note).stored);
  set.compression = 6;
  CHECK(!gc::still_options(set, note).stored);
  gc::StillOptions o;
  o.format = gc::StillFormat::kPng;
  std::vector<uint8_t> packed, stored;
  gc::StillFormat used;
  const std::vector<uint8_t> big = frame(64, 64);
  CHECK(gc::encode_still(o, 64, 64, big, packed, used, note));
  o.stored = true;
  CHECK(gc::encode_still(o, 64, 64, big, stored, used, note));
  CHECK_MSG(stored.size() > packed.size(), std::to_string(stored.size()));
  auto back = gc::decode_still(stored, note);
  CHECK(back && back->width == 64 && back->rgba == big);
}

TEST(a_grab_is_kept_in_memory_scaled_and_packed_as_jpeg_with_no_file) {
  const fs::path dir = fresh_dir("ge_screenshot_grab");
  gc::Screenshots shots(dir.string(), "game", &kPng);
  std::vector<uint8_t> out;
  CHECK(!shots.take_grab(out));  // none asked
  CHECK(shots.grab(32, 24, 70));
  CHECK(!shots.grab(32, 24, 70));  // one at a time
  CHECK(shots.due());
  CHECK(shots.write(64, 48, frame(64, 48)));
  shots.flush();
  CHECK(shots.take_grab(out));
  CHECK(out.size() > 2 && out[0] == 0xff && out[1] == 0xd8);
  std::string error;
  const auto back = gc::decode_still(out, error);
  CHECK_MSG(back.has_value() && back->width == 32 && back->height == 24, error);
  CHECK(!fs::exists(dir) || pngs(dir).empty());  // the player's folder untouched
  CHECK(!shots.take_grab(out));                  // taken once
  // A grab and the key on the same frame: both.
  CHECK(shots.grab(16, 12, 70));
  shots.request();
  CHECK(shots.write(64, 48, frame(64, 48)));
  shots.flush();
  CHECK(shots.take_grab(out) && !out.empty());
  CHECK(pngs(dir).size() == 1);
}

TEST(screenshotjpg_png_and_tga_write_the_next_frame_in_their_format_by_name) {
  // Whatever scr_format says (png here): screenshotjpg writes a jpg, screenshotpng a png,
  // screenshottga a png too (no TGA writer: the old word's picture is a png).
  const fs::path dir = fresh_dir("ge_screenshot_formats") / "screenshots";
  gc::ScreenshotSettings set = kPng;
  gc::Screenshots shots(dir.string(), "game", &set);
  ge::console::Console con(false);
  gc::add_screenshot_format_commands(con, shots);
  const std::vector<uint8_t> rgba = frame(8, 8);
  for (const char* line : {"screenshotjpg a 80", "screenshotpng b 9", "screenshottga c"}) {
    con.execute(line);
    CHECK_MSG(shots.due(), line);
    CHECK(shots.write(8, 8, rgba));
    shots.flush();
  }
  CHECK(fs::exists(dir / "a.jpg") && fs::exists(dir / "b.png") && fs::exists(dir / "c.png"));
}
