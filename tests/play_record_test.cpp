// A run recorded (engine/play/record.hpp) over a toy game: the frame clock at a speed, the clock
// text, the encoder's command line for each format, a bad format or word refused, the raster's
// resize, a recording written as PNG frames with its contact sheet, `play record`'s answer, and
// an MP4 through ffmpeg when the machine has it.
//
// ge:summary Tests: the frame clock, ffmpeg's command line, the resize, play record and its sheet
// ge:keywords test record video frames frame_steps ffmpeg contact-sheet resized play_main
// ge:skill agent-interface testing
#include <array>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/num_text.hpp"
#include "lab/raster.hpp"
#include "play/main.hpp"
#include "play/record.hpp"
#include "play/session.hpp"

namespace gp = ge::play;
namespace fs = std::filesystem;

namespace {

// A body on a line, pushed until it is over at x >= 10; its one view a 61 x 21 strip (odd sides,
// so the frames are grown to even ones).
class Toy final : public gp::Run {
 public:
  void step(std::span<const double> in) override {
    v_ += in[0] * 0.5;
    x_ += v_ / 60;
    hash_ = (hash_ ^ static_cast<uint64_t>(x_ * 1e6)) * 0x100000001b3ull;
  }
  bool over() const override { return x_ >= 10; }
  uint64_t digest() const override { return hash_; }
  std::string observe() override { return "{}"; }
  std::string state() const override { return "{\"x\":" + ge::core::num::to_string(x_) + "}"; }
  std::string report() const override { return "{}"; }
  bool pilot(std::span<double> out) override {
    out[0] = 1;
    return true;
  }
  bool shoot(std::string_view view, ge::lab::Picture& out, std::string& caption,
             std::string& error) override {
    if (view != "line") {
      error = "no such view";
      return false;
    }
    out = ge::lab::create_picture(61, 21, {0, 0, 0});
    ge::lab::fill_rect(out, x_ * 6, 0, 2, 21, {255, 255, 255});
    caption = "the body at x " + ge::core::num::to_fixed(x_, 2);
    return true;
  }

 private:
  double x_ = 0, v_ = 0;
  uint64_t hash_ = 0xcbf29ce484222325ull;
};

std::unique_ptr<gp::Run> open_toy(const gp::Setup&, std::string&) {
  return std::make_unique<Toy>();
}

constexpr std::array<gp::Axis, 1> kAxes{{{"push", gp::AxisKind::lever, "accelerate"}}};
constexpr std::array<gp::View, 1> kViews{{{"line", "the line from above"}}};
const gp::GameSpec kToy{"toy", "a body on a line", 60, 30, kAxes, {}, open_toy, kViews};
GE_GAME(kToy);

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

fs::path scratch(const char* name) {
  const fs::path p = fs::temp_directory_path() / "ge_play_record_test" / name;
  std::error_code ec;
  fs::remove_all(p, ec);
  fs::create_directories(p, ec);
  return p;
}

gp::InputLog pilot_log() {
  gp::Setup s;
  std::string error;
  gp::make_setup(kToy, 0, {}, s, error);
  auto p = gp::pilot_run(kToy, s, error);
  return p ? std::move(p->log) : gp::InputLog{};
}

}  // namespace

TEST(the_frame_clock_steps_by_speed_times_rate_over_fps) {
  CHECK(gp::frame_steps(120, 0, 12, 30, 1) == std::vector<int64_t>({0, 4, 8, 12}));
  CHECK(gp::frame_steps(120, 10, 14, 30, 0.5) == std::vector<int64_t>({10, 12, 14}));
  CHECK(gp::frame_steps(120, 0, 40, 30, 4).size() == 3);  // 0, 16, 32
  CHECK(gp::frame_steps(120, 0, 10, 0, 1).empty());
  CHECK(gp::frame_steps(120, 5, 4, 30, 1).empty());
  CHECK(gp::clock_text(7380, 120) == "1:01.50");
  CHECK(gp::clock_text(0, 60) == "0:00.00");
}

TEST(each_format_has_its_encoder_command) {
  gp::Encoding mp4;
  const std::string a = gp::ffmpeg_command(mp4, 640, 360, 30, "out.mp4", "e.log");
  CHECK(has(a, "-f rawvideo -pix_fmt rgba -s 640x360 -r 30 -i -"));
  CHECK(has(a, "-c:v libx264 -preset medium -pix_fmt yuv420p -crf 23 -movflags +faststart"));
  CHECK(has(a, "\"out.mp4\" 2>> \"e.log\""));
  gp::Encoding webm{"webm", "", -1, "4M", "", ""};
  const std::string b = gp::ffmpeg_command(webm, 64, 64, 24, "o.webm", "l");
  CHECK(has(b, "-c:v libvpx-vp9") && has(b, "-b:v 4M") && !has(b, "-crf"));
  gp::Encoding gif{"gif", "", -1, "", "", ""};
  CHECK(has(gp::ffmpeg_command(gif, 64, 64, 10, "o.gif", "l"), "palettegen"));
  CHECK(gp::extension_of(gif) == "gif");
  CHECK(gp::encoding_problem(mp4).empty());
  CHECK(has(gp::encoding_problem({"mov", "", -1, "", "", ""}), "the formats are"));
  CHECK(has(gp::encoding_problem({"mp4", "x;rm", -1, "", "", ""}), "not a plain word"));
  CHECK(has(gp::encoding_problem({"mp4", "", 70, "", "", ""}), "over 63"));
  // The engine's own MP4 holds AV1 and Opus only.
  const gp::Encoding own_mp4{"mkv", "av1", -1, "", "", "mp4"};
  CHECK(gp::encoding_problem(own_mp4).empty() && gp::extension_of(own_mp4) == "mp4");
  CHECK(has(gp::encoding_problem({"mkv", "", -1, "", "", "mp4"}), "--codec=av1"));
  CHECK(has(gp::encoding_problem({"mkv", "av1", -1, "", "", "avi"}), "no container"));
}

TEST(a_picture_resizes_by_the_mean_of_its_pixels) {
  ge::lab::Picture p = ge::lab::create_picture(4, 2, {0, 0, 0});
  ge::lab::fill_rect(p, 0, 0, 1, 2, {200, 100, 50});
  const ge::lab::Picture half = ge::lab::resized(p, 2, 1);
  CHECK(half.width == 2 && half.height == 1);
  CHECK(half.rgba[0] == 100 && half.rgba[1] == 50 && half.rgba[2] == 25 && half.rgba[4] == 0);
  const ge::lab::Picture grown = ge::lab::resized(p, 8, 4);
  constexpr size_t kRow2Col1 = 68, kRow0Col2 = 8;  // ((2 * 8) + 1) * 4 and 2 * 4
  CHECK(grown.rgba[0] == 200 && grown.rgba[kRow2Col1] == 200 && grown.rgba[kRow0Col2] == 0);
}

TEST(a_run_records_as_frames_and_a_contact_sheet) {
  const gp::InputLog log = pilot_log();
  const fs::path dir = scratch("frames");
  gp::RecordOptions o;
  o.fps = 10;
  o.encoding.format = "png";
  o.video = (dir / "frames").string();
  o.sheet = (dir / "sheet.png").string();
  o.thumbs = 4;
  o.columns = 2;
  std::string error;
  const auto rec = gp::record(kToy, log, o, error);
  CHECK(rec.has_value());
  if (!rec) return;
  CHECK(rec->view == "line" && rec->width == 62 && rec->height == 22);
  CHECK(rec->from == 0 && rec->to <= log.steps() && rec->frames == rec->to / 6 + 1);
  CHECK(rec->encoder.empty());
  const auto at_to = gp::shot_at(kToy, log, rec->to, "", error);  // the run at `to`
  CHECK(at_to && rec->digest == at_to->digest);
  CHECK(fs::exists(dir / "frames" / "frame_000000.png"));
  CHECK(fs::exists(fs::path(o.sheet)));

  o.view = "side";
  CHECK(!gp::record(kToy, log, o, error) && has(error, "the views are line"));
  o.view = "";
  o.gpu = true;  // this executable registers no renderer
  CHECK(!gp::record(kToy, log, o, error) && has(error, "has no GPU renderer"));
  o.gpu = false;
  o.view = "";
  o.from = log.steps() + 1;
  o.to = log.steps() + 5;
  CHECK(!gp::record(kToy, log, o, error) && has(error, "no frame in steps"));
}

TEST(play_record_answers_with_its_files) {
  const fs::path dir = scratch("cli");
  const std::string out_flag = "--out=" + dir.string();
  const char* argv[] = {"play",       "record",  "--fps=5",       "--format=png",
                        "--thumbs=3", "--to=1s", out_flag.c_str()};
  std::istringstream in;
  std::ostringstream out;
  CHECK(gp::play_main(7, const_cast<char**>(argv), in, out) == 0);
  CHECK(has(out.str(), "\"frames_dir\":") && has(out.str(), "\"fps\":5"));
  CHECK(fs::exists(dir / "toy-1-line-sheet.png"));

  const char* bad[] = {"play", "record", "--format=avi"};
  std::ostringstream answer;
  CHECK(gp::play_main(3, const_cast<char**>(bad), in, answer) == 2);
  CHECK(has(answer.str(), "the formats are"));
}

TEST(an_mp4_is_encoded_when_ffmpeg_is_installed) {
#ifdef _WIN32
  const bool ffmpeg = std::system("ffmpeg -version > NUL 2>&1") == 0;
#else
  const bool ffmpeg = std::system("ffmpeg -version > /dev/null 2>&1") == 0;
#endif
  if (!ffmpeg) return;  // the encoder is an external tool; the frames above need none
  const fs::path dir = scratch("mp4");
  gp::RecordOptions o;
  o.fps = 10;
  o.video = (dir / "toy.mp4").string();
  o.log = (dir / "encode.log").string();
  std::string error;
  const auto rec = gp::record(kToy, pilot_log(), o, error);
  CHECK(rec.has_value() && has(rec->encoder, "libx264"));
  CHECK(fs::exists(dir / "toy.mp4") && fs::file_size(dir / "toy.mp4") > 0);
}
