// LAB `tarn`: the free ride's tarn (M24l: mountain/tarn/tarn.hpp, sim/free.hpp's `free_map`) in
// Fall Line's own frame (app/lakes.hpp's `show_lakes`, which `create_frame` calls): the tarn laid
// on a seed's free-ride map as the map's day leaves it, and the same site laid three more ways,
// OPEN (a summer's water over the dug bowl), THIN (3 cm of young black ice over it, which does not
// bear a skier), FROZEN (40 cm, snowed over, filled flat to bear him) and THAWING (6 cm of rotten
// ice late in the spring's melt, standing off the shore over a moat), each from below its lip
// (app/views.hpp's `tarn`) and from high over the slope below it (`tarn-high`).
//
// The rows: the site, the surface, the winter so far and the ice on the map's day, whether it
// bears. Each picture is drawn twice, the lake's water and ice drawn and not, and the share of
// pixels they change is measured: the lab fails when a view shows none of the tarn, on a GPU
// error, or on a pipeline made after the first frame's load (each state's frame loads again).
// Without an adapter it notes that and draws placeholders, so the digest is the same on every row;
// GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the tarn in the game's frame: the map's day, open, thin, frozen, thawing
// ge:keywords lab tarn lake lake-ice thin-ice frozen-lake free-ride show_lakes picture in-game
// ge:skill render water labs
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/sim/free.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/app/ice_look.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/tarn/tarn.hpp"
#include "water/world.hpp"

namespace {

using ge::lab::Det;
namespace mg = ge::mountain::mapgen;
namespace tn = ge::mountain::tarn;

bool env_on(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool on = _dupenv_s(&v, &n, name) == 0 && v && std::strcmp(v, "1") == 0;
  std::free(v);
  return on;
#else
  const char* v = std::getenv(name);
  return v && std::strcmp(v, "1") == 0;
#endif
}

std::string fixed(double v, int places) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.*f", places, v);
  return buf;
}

// The share of a picture's pixels that differ from another's by more than a few levels.
double changed(const ge::lab::Picture& a, const ge::lab::Picture& b) {
  size_t n = 0;
  for (size_t i = 0; i < a.rgba.size(); i += 4) {
    int most = 0;
    for (size_t c = 0; c < 3; ++c)
      most = std::max(most, std::abs(int{a.rgba[i + c]} - int{b.rgba[i + c]}));
    n += most > 6;
  }
  return static_cast<double>(n) * 4 / static_cast<double>(a.rgba.size());
}

// One way the tarn is laid: `ice` m (negative: the map's day's, as the free ride lays it).
struct State {
  std::string id, title, look_for;
  double ice;
  double thaw = 0;  // 0..1 through the spring's melt
};

const std::vector<State> kStates = {
    {"day", "The tarn on the map's day",
     "Laid as the free ride lays it: frozen over and snowed where the winter so far bears a "
     "skier, open water in a summer.",
     -1},
    {"open", "Open water",
     "The summer's tarn: water in the dug bowl, the shore in the snow, the headwall behind.", 0},
    {"thin", "Thin black ice",
     "Three centimetres of young ice: clear and nearly black over the deep water, glossy, no snow "
     "on it. It does not bear a skier.",
     0.03},
    {"frozen", "Frozen to bear a skier",
     "Forty centimetres: snow over it but for the wind's swept patches of grey-blue glass, on "
     "ground filled flat.",
     0.4},
    {"thawing", "Thawing",
     "Late in the spring's melt: six centimetres of rotten ice, grey wet slush on it and dark "
     "glass where it was swept, standing off the shore over a moat of open water. It does not "
     "bear a skier.",
     0.06, 0.85},
};

// The free ride's map for `seed`, its tarn laid with `ice` m (the map's day's when negative),
// `thaw` through the spring's melt.
std::shared_ptr<const mg::Level> laid(double seed, int version, double ice, double thaw = 0) {
  if (ice < 0) return std::make_shared<const mg::Level>(fall_line::sim::free_map(seed, version));
  mg::GenerateOptions opts;
  opts.version = version;
  mg::Level level = mg::generate_level(seed, opts);
  if (std::optional<mg::MapLake> lake = tn::find_tarn(level, fall_line::sim::kTarnRadius)) {
    lake->ice = ice;
    lake->thaw = thaw;
    tn::dig_basin(level, *lake, tn::kTarnDepth, tn::bears(*lake));
    level.lakes.push_back(*lake);
  }
  return std::make_shared<const mg::Level>(std::move(level));
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const auto version = static_cast<int>(lab.arg_num("version"));
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int frames = static_cast<int>(lab.arg_num("frames"));
  if (width < 16 || height < 16 || width > 4096 || height > 4096 || frames < 1 || frames > 120) {
    lab.fail("--width and --height must be 16..4096, --frames 1..120",
             "next: pass --width=640 --height=360 --frames=8.");
    return;
  }
  const std::string seed_s = fixed(seed, 0);
  std::shared_ptr<const mg::Level> day;
  try {
    day = laid(seed, version, -1);
  } catch (const mg::GenerateError& e) {
    lab.fail("seed " + seed_s + " built no map: " + e.what(),
             "next: pass another --seed, or run `make lab LAB=analyze ARGS=--seeds=" + seed_s +
                 " --refusals` (skill: mountain).");
    return;
  }
  if (day->lakes.empty()) {
    lab.fail("seed " + seed_s + "'s free ride laid no tarn",
             "Check mountain/tarn/tarn.cpp's find_tarn (the band, the clearance, the relief) on "
             "this map, or pass another --seed. Skill: mountain.");
    return;
  }
  const mg::MapLake& lake = day->lakes[0];
  lab.metric("tarn.x", lake.x, "m");
  lab.metric("tarn.z", lake.z, "m");
  lab.metric("tarn.level", lake.level, "m");
  lab.metric("tarn.depth", lake.depth, "m");
  lab.metric("winter-days", tn::winter_days(*day), "days");
  lab.metric("ice", lake.ice * 100, "cm");
  lab.metric("bears", tn::bears(lake) ? 1 : 0, "bool");
  lab.metric("thaw", lake.thaw, "share");
  lab.note("Seed " + seed_s + "'s tarn: " + fixed(2 * lake.radius, 0) + " m across at " +
           fixed(lake.level, 0) + " m, day " + fixed(day->sun.day_of_year, 0) + " of the year, " +
           fixed(tn::winter_days(*day), 0) + " days of winter, " + fixed(lake.ice * 100, 1) +
           " cm of ice" + (tn::bears(lake) ? " that bears a skier" : ", which does not bear one") +
           ".");

  const std::vector<std::string> views = {"tarn", "tarn-high"};
  auto shot_of = [&](const State& s, const std::string& view, double ice, double thaw) {
    return ge::lab::Shot{
        s.id + "-" + view,
        s.title + (view == "tarn" ? ", from below its lip" : ", from high over the slope"),
        s.look_for,
        "The tarn reads as a lake laid in the mountain: its shore closed by the snow, the cirque's "
        "headwall behind it, the ice as ice and the water as water.",
        {{"seed", seed_s},
         {"view", view},
         {"ice", fixed(ice * 100, 1) + " cm"},
         {"thaw", fixed(thaw, 2)},
         {"bears", tn::bears(mg::MapLake{.ice = ice, .thaw = thaw}) ? "yes" : "no"}},
    };
  };

  auto device = ge::gpu::create_device({.label = "tarn", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const State& s : kStates) {
      for (const std::string& view : views) {
        ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
        ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
        lab.shot(shot_of(s, view, s.ice < 0 ? lake.ice : s.ice, s.ice < 0 ? lake.thaw : s.thaw),
                 none, Det::measured);
      }
    }
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "tarn.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  // Each state is a frame of its own on the one device: the first frame's load makes every
  // pipeline, so a pipeline made after the load is counted over the first state's shots alone.
  uint32_t late = 0;
  for (const State& s : kStates) {
    const std::shared_ptr<const mg::Level> level =
        s.ice < 0 ? day : laid(seed, version, s.ice, s.thaw);
    const double ice = level->lakes.empty() ? 0 : level->lakes[0].ice;
    const double thaw = level->lakes.empty() ? 0 : level->lakes[0].thaw;
    // The moat keeps the water drawn round the sheet (none round ice on ground filled flat).
    const double moat =
        level->lakes.empty() || level->lakes[0].depth == 0 ? 0 : tn::moat_of(level->lakes[0]);
    // The sheet's share of the lake inside its shore: the moat's water is the rest.
    if (!level->lakes.empty() && ice > 0) {
      const mg::MapLake& l = level->lakes[0];
      const std::vector<ge::water::Point> shore = tn::shore_of(l);
      const ge::render::MeshData sheet =
          ge::mountain::app::ice_mesh(shore, l.level, 0, false, moat);
      double sheet_m2 = 0, lake_m2 = 0;
      for (size_t v = 0; v + 2 < sheet.parts.size(); v += 3) {
        const float* p = &sheet.positions[v * 3];
        sheet_m2 += 0.5 * std::abs((p[3] - p[0]) * (p[8] - p[2]) - (p[5] - p[2]) * (p[6] - p[0]));
      }
      for (size_t i = 0; i < shore.size(); ++i) {
        const ge::water::Point& a = shore[i];
        const ge::water::Point& b = shore[(i + 1) % shore.size()];
        lake_m2 += 0.5 * (a.x * b.z - b.x * a.z);
      }
      lab.metric(s.id + ".sheet-share", sheet_m2 / std::abs(lake_m2), "share");
    }
    auto frame = fall_line::app::create_frame(*device, *level, static_cast<uint32_t>(width),
                                              static_cast<uint32_t>(height),
                                              wgpu::TextureFormat::RGBA8Unorm);
    if (!frame) {
      lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
      return;
    }
    auto draw = [&](const ge::render::FrameView& v) {
      fall_line::app::cut(*frame);
      ge::render::FrameView at = v;
      for (int n = 0; n < frames; ++n) {
        at.frame = static_cast<uint64_t>(n);
        at.time_s = 30 - static_cast<double>(frames - 1 - n) / 60;
        fall_line::app::draw(*device, *frame, at, fall_line::app::FrameScene{}, target.view);
      }
      const ge::gpu::Image img = ge::gpu::read_now(*device, target);
      return ge::lab::Picture{width, height, img.rgba};
    };
    for (const std::string& view : views) {
      ge::render::FrameView v;
      fall_line::app::day_view(*level, -1, v);
      fall_line::app::make_view(*level, view, v);
      const ge::lab::Picture pic = draw(v);
      // Without the lake: its ice taken off (the sheet is made already, so no shore is read),
      // then its water hidden; then both put back.
      const int sheet = frame->ice_body;
      if (sheet >= 0) fall_line::app::freeze_water(*device, *frame, sheet, ge::water::Body{}, 0);
      for (auto& b : frame->water.bodies) b.hidden = true;
      const ge::lab::Picture dry = draw(v);
      for (auto& b : frame->water.bodies) b.hidden = false;
      if (sheet >= 0) {
        fall_line::app::freeze_water(*device, *frame, sheet, ge::water::Body{}, ice, false, moat,
                                     thaw);
      }
      const double share = changed(pic, dry);
      lab.metric(s.id + "-" + view + ".lake-share", share, "share", Det::measured);
      if (share < 0.02) {
        lab.fail("the " + s.id + "-" + view + " view shows little of the tarn (" +
                     fixed(share * 100, 1) + "% of the picture changed by its water and ice)",
                 "Check app/views.cpp's tarn views and app/lakes.cpp's show_lakes. Skill: render.");
      }
      lab.shot(shot_of(s, view, ice, thaw), pic, Det::measured);
    }
    if (&s == &kStates.front()) late = device->pipelines_after_load;
  }
  const uint32_t errors = device->errors.load();
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0) {
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
  if (late != 0) {
    lab.fail("a pipeline was made after the load",
             "Add it to create_frame's manifest. Skill: gpu, § Pipelines.");
  }
}

const ge::lab::LabSpec kSpec{
    "tarn",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "the free ride's tarn (mountain/tarn/tarn.hpp, sim/free.hpp's free_map) drawn by "
    "games/fall_line/app/lakes.cpp's show_lakes in the game's frame",
    "render",
    "Does the free ride's tarn read as a lake laid high in the mountain, on the map's day and "
    "open, under thin black ice, frozen to bear a skier and thawing over a moat?",
    {
        {"seed", "38", "the map's seed"},
        {"version", "10", "the generator's version (the free ride's ski area: 8 or later)"},
        {"frames", "8", "frames drawn, at 60 a second, before each shot (TAA-U settles)"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
    },
    "--frames=2 --width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
