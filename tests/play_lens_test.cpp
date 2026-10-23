// What draws a view (engine/play/session.hpp's Lens) over a toy game with a toy GPU renderer: the
// CPU's views without --gpu, the renderer's views and modes with it (the first of each by
// default, a wrong name answered with the list), the first picture a cut and the rest not, the
// renderer opened once for a whole recording, and `play shot --gpu` / `play record --gpu` naming
// their files by the view and the mode.
//
// ge:summary Tests: the lens, a GPU renderer's views and modes, cuts, shot and record with --gpu
// ge:keywords test lens open_lens shoot_lens renderer renderer-spec gpu mode cut record shot
// ge:skill agent-interface testing
#include <array>
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

class Toy final : public gp::Run {
 public:
  void step(std::span<const double> in) override { x_ += in[0] / 60; }
  bool over() const override { return x_ >= 2; }
  uint64_t digest() const override { return static_cast<uint64_t>(x_ * 1e6); }
  std::string observe() override { return "{}"; }
  std::string state() const override { return "{\"x\":" + ge::core::num::to_string(x_) + "}"; }
  std::string report() const override { return "{}"; }
  bool pilot(std::span<double> out) override {
    out[0] = 1;
    return true;
  }
  bool shoot(std::string_view, ge::lab::Picture& out, std::string& caption, std::string&) override {
    out = ge::lab::create_picture(8, 8, {0, 0, 0});
    caption = "cpu";
    return true;
  }
  double x() const { return x_; }

 private:
  double x_ = 0;
};

std::unique_ptr<gp::Run> open_toy(const gp::Setup&, std::string&) {
  return std::make_unique<Toy>();
}

constexpr std::array<gp::Axis, 1> kAxes{{{"push", gp::AxisKind::lever, "move"}}};
constexpr std::array<gp::View, 1> kViews{{{"line", "the line from above"}}};
const gp::GameSpec kToy{"toy", "a body on a line", 60, 30, kAxes, {}, open_toy, kViews};
GE_GAME(kToy);

// What the toy renderer was asked, over every renderer it opened.
int g_opened = 0;
std::vector<bool> g_cuts;

// Paints the picture by its mode: red lit, green depth; the body's place in the caption.
class ToyRenderer final : public gp::Renderer {
 public:
  bool draw(const gp::Run& run, const gp::RenderShot& shot, ge::lab::Picture& out,
            std::string& caption, std::string& error) override {
    const auto* toy = dynamic_cast<const Toy*>(&run);
    if (!toy) {
      error = "not a toy run";
      return false;
    }
    g_cuts.push_back(shot.cut);
    const ge::lab::Rgba c =
        shot.mode == "lit" ? ge::lab::Rgba{255, 0, 0} : ge::lab::Rgba{0, 255, 0};
    out = ge::lab::create_picture(shot.width, shot.height, c);
    caption = std::string(shot.view) + " " + std::string(shot.mode) + " at x " +
              ge::core::num::to_fixed(toy->x(), 2);
    return true;
  }
};

std::unique_ptr<gp::Renderer> open_toy_renderer(std::string&) {
  ++g_opened;
  return std::make_unique<ToyRenderer>();
}

constexpr std::array<gp::View, 2> kGpuViews{{{"chase", "behind"}, {"top", "from above"}}};
constexpr std::array<gp::View, 2> kModes{{{"lit", "lit"}, {"depth", "the depth"}}};
const gp::RendererSpec kToyRenderer{kGpuViews, kModes, 32, 18, open_toy_renderer};
GE_RENDERER(kToyRenderer);

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

gp::InputLog pilot_log() {
  gp::Setup s;
  std::string error;
  gp::make_setup(kToy, 0, {}, s, error);
  auto p = gp::pilot_run(kToy, s, error);
  return p ? std::move(p->log) : gp::InputLog{};
}

fs::path scratch(const char* name) {
  const fs::path p = fs::temp_directory_path() / "ge_play_lens_test" / name;
  std::error_code ec;
  fs::remove_all(p, ec);
  fs::create_directories(p, ec);
  return p;
}

}  // namespace

TEST(a_lens_picks_the_cpu_view_or_the_renderers_view_and_mode) {
  std::string error;
  gp::Lens cpu;
  CHECK(gp::open_lens(kToy, {}, cpu, error) && !cpu.renderer && cpu.name() == "line");
  gp::LensAsk ask;
  ask.gpu = true;
  gp::Lens gpu;
  CHECK(gp::open_lens(kToy, ask, gpu, error) && gpu.renderer != nullptr);
  CHECK(gpu.name() == "chase/lit" && gpu.width == 32 && gpu.height == 18);
  ask.view = "top";
  ask.mode = "depth";
  ask.width = 40;
  ask.height = 20;
  CHECK(gp::open_lens(kToy, ask, gpu, error) && gpu.name() == "top/depth" && gpu.width == 40);
  ask.mode = "wire";
  CHECK(!gp::open_lens(kToy, ask, gpu, error) &&
        has(error, "no mode `wire`; the modes are lit, depth"));
  ask.mode = "";
  ask.view = "line";
  CHECK(!gp::open_lens(kToy, ask, gpu, error) && has(error, "the views are chase, top"));
}

TEST(the_first_picture_is_a_cut_and_the_rest_are_not) {
  gp::LensAsk ask;
  ask.gpu = true;
  gp::Lens lens;
  std::string error;
  CHECK(gp::open_lens(kToy, ask, lens, error));
  Toy run;
  g_cuts.clear();
  ge::lab::Picture p;
  std::string caption;
  for (int i = 0; i < 3; ++i) CHECK(gp::shoot_lens(lens, run, p, caption, error));
  CHECK(g_cuts == std::vector<bool>({true, false, false}));
  CHECK(p.width == 32 && p.height == 18 && p.rgba[0] == 255 && caption == "chase lit at x 0.00");
}

TEST(a_gpu_recording_opens_the_renderer_once_and_draws_every_frame) {
  const gp::InputLog log = pilot_log();
  const fs::path dir = scratch("record");
  gp::RecordOptions o;
  o.gpu = true;
  o.mode = "depth";
  o.fps = 10;
  o.encoding.format = "png";
  o.video = (dir / "frames").string();
  o.sheet = (dir / "sheet.png").string();
  o.thumbs = 2;
  const int opened = g_opened;
  g_cuts.clear();
  std::string error;
  const auto rec = gp::record(kToy, log, o, error);
  CHECK(rec.has_value());
  if (!rec) return;
  CHECK(g_opened == opened + 1);
  CHECK(rec->view == "chase/depth" && rec->width == 32 && rec->height == 18);
  CHECK(static_cast<int64_t>(g_cuts.size()) == rec->frames && g_cuts[0] && !g_cuts[1]);
  CHECK(fs::exists(dir / "frames" / "frame_000000.png") && fs::exists(dir / "sheet.png"));
}

TEST(play_shot_and_record_take_gpu_and_mode) {
  const fs::path dir = scratch("cli");
  const std::string out_flag = "--out=" + dir.string();
  std::istringstream in;
  const char* shot[] = {"play", "shot", "--gpu", "--mode=depth", "--size=20x10", out_flag.c_str()};
  std::ostringstream a;
  CHECK(gp::play_main(6, const_cast<char**>(shot), in, a) == 0);
  CHECK(has(a.str(), "\"view\":\"chase/depth\"") && has(a.str(), "\"gpu\":true"));
  CHECK(has(a.str(), "toy-1-chase-depth-"));

  const char* record[] = {"play",    "record",       "--gpu",   "--view=top",
                          "--fps=5", "--format=png", "--to=1s", out_flag.c_str()};
  std::ostringstream b;
  CHECK(gp::play_main(8, const_cast<char**>(record), in, b) == 0);
  CHECK(has(b.str(), "\"view\":\"top/lit\"") && fs::exists(dir / "toy-1-top-lit-sheet.png"));

  const char* describe[] = {"play", "describe"};
  std::ostringstream c;
  CHECK(gp::play_main(2, const_cast<char**>(describe), in, c) == 0);
  CHECK(has(c.str(), "\"gpu\":{\"views\":[{\"name\":\"chase\"") && has(c.str(), "\"modes\":["));

  const char* bad[] = {"play", "shot", "--gpu", "--mode=wire"};
  std::ostringstream d;
  CHECK(gp::play_main(4, const_cast<char**>(bad), in, d) == 1);
  CHECK(has(d.str(), "the modes are lit, depth"));
}
