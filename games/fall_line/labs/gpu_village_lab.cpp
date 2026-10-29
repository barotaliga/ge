// LAB `village-look`: the free ride's village (sim/village.hpp) drawn in Fall Line's frame
// (`show_village`: mountain/app/village_look.hpp's streets and vehicles, the felled trees left
// out of the forest). `--seed`'s free ride is drawn at `--at` s of its clock, `--frames` frames
// at 60 a second so the TAA settles, from four places:
//
// - ABOVE: 140 m over the village's middle looking down the valley: the street plan on the snow,
//   the gap the felled trees leave, the cars as dots on it.
// - SQUARE: 30 m back and 18 m over the square, looking along the main street: the carriageway,
//   the sidewalks, the bus at its stop when it is there.
// - CARPARK: 25 m back and 15 m over the car park: its aisles and its rows of parked cars.
// - STREET: 9 m behind and 2.5 m over the first car under way: a car on its lane at street level.
//
// Each is drawn twice from the same moment: the village shown (the shot) and the frame as it was
// before the village was drawn (no streets, no cars, every tree standing), and the share of the
// picture the village changes is measured. Without an adapter it notes that and draws
// placeholders, so the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead. Built
// only with GE_GPU.
//
// ge:summary Lab: the free ride's village drawn: its streets, its cars, the bus, the felled trees
// ge:keywords lab village village-look streets vehicles traffic felled picture fall-line
// ge:skill render mountain labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/village.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/app/village_look.hpp"
#include "mountain/village/traffic.hpp"

namespace {

using ge::lab::Det;
namespace fs = fall_line::sim;
namespace r = ge::render;
namespace vg = ge::mountain::village;
namespace lm = ge::core::libm;

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

struct Look {
  const char* id;
  const char* title;
  const char* caption;
  const char* look_for;
};
constexpr Look kLooks[] = {
    {"above", "THE VILLAGE FROM ABOVE",
     "From 140 m over the village's middle, looking down the valley.",
     "the streets as grey bands joined in a loop, the square and the car park; a clearing where "
     "the trees were felled; cars on the streets and in the bays"},
    {"square", "THE SQUARE AND THE MAIN STREET",
     "From 30 m back and 18 m over the square, looking along the main street.",
     "the carriageway darker than its sidewalks, laid on the snow with no gap or hole; cars on "
     "their lanes; the bus at its stop when the clock has it there"},
    {"carpark", "THE DAY CAR PARK", "From 25 m back and 15 m over the car park.",
     "the aisles and rows of parked cars, each in its bay, in a village's mix of colours"},
    {"street", "A CAR ON ITS LANE", "From 9 m behind and 2.5 m over the first car under way.",
     "a car with its glasshouse, bumpers and four wheels sitting on the street, not floating or "
     "sunk; the street under it"},
};

// The camera `back` m behind and `up` m over (x, y, z) along `heading`, looking at it.
void look_at(const ge::mountain::mapgen::Level& level, double x, double y, double z, double heading,
             double back, double up, r::FrameView& v) {
  const double ex = x - back * lm::sin(heading), ez = z - back * lm::cos(heading);
  v.eye[0] = ex;
  v.eye[2] = ez;
  v.eye[1] = std::max(y + up, level.ground_at(ex, ez) + 1.5);
  const double tx = x - ex, tz = z - ez, ty = y + 0.8 - v.eye[1];
  v.heading = lm::atan2(tx, tz);
  v.pitch = lm::atan2(ty, ge::core::hypot(tx, tz));
}

// Where each look stands its camera at `t`; false when the village has nothing for it.
bool aim(const fs::GameState& st, const char* id, double t, r::FrameView& v) {
  const fs::Village& vil = *st.village;
  const vg::Streets& s = vil.streets;
  const ge::mountain::mapgen::Level& level = *st.level;
  // Down the valley is z of the plan's sign.
  const double valley = s.valley > 0 ? 0 : 3.14159265358979323846;
  if (std::strcmp(id, "above") == 0) {
    look_at(level, s.centre_x, level.ground_at(s.centre_x, s.centre_z), s.centre_z, valley, 90, 140,
            v);
    return true;
  }
  for (const vg::VillageArea& a : s.areas) {
    const bool square = a.kind == vg::AreaKind::square;
    if ((std::strcmp(id, "square") == 0 && square) ||
        (std::strcmp(id, "carpark") == 0 && !square)) {
      const double back = square ? 30 : 25, up = square ? 18 : 15;
      look_at(level, a.x, a.y, a.z, a.heading + 3.14159265358979323846, back, up, v);
      return true;
    }
  }
  if (std::strcmp(id, "street") == 0 && vil.traffic) {
    const vg::TrafficPlan& plan = *vil.traffic;
    for (int32_t k = 0; k < static_cast<int32_t>(plan.vehicles.size()); ++k) {
      if (plan.vehicles[static_cast<size_t>(k)].kind == vg::VehicleKind::bike) continue;
      const vg::VehiclePose p = vg::vehicle_at(level, plan, k, t);
      if (!p.shown || p.speed < 2) continue;
      look_at(level, p.x, p.y, p.z, p.heading, 9, 2.5, v);
      return true;
    }
  }
  return false;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double at = lab.arg_num("at");
  const int frames = std::max(1, static_cast<int>(lab.arg_num("frames")));
  const int px = static_cast<int>(lab.arg_num("px"));
  const int width = px, height = px * 9 / 16;

  const auto level = std::make_shared<const ge::mountain::mapgen::Level>(
      fs::free_map(seed, static_cast<int>(lab.arg_num("version"))));
  fs::GameState st = fs::create_free_ride(level, 0);
  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  auto shot_of = [&](const Look& l) {
    return ge::lab::Shot{l.id,
                         std::string(l.title) + ", SEED " + seed_s,
                         l.caption,
                         l.look_for,
                         {{"seed", seed_s}, {"at", std::to_string(static_cast<int>(at))}}};
  };
  auto blank = [&](const Look& l, const char* text) {
    ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
    ge::lab::draw_text(none, 4, 4, text, {230, 230, 230});
    lab.shot(shot_of(l), none, Det::measured);
    // The before too: a shot's id is in the digest, so the lab takes the same shots with a GPU
    // and without one.
    ge::lab::Shot b = shot_of(l);
    b.id = std::string(l.id) + "-before";
    lab.shot(b, none, Det::measured);
  };
  if (!st.village) {
    lab.note("seed " + seed_s + " lays no village");
    for (const Look& l : kLooks) blank(l, "NO VILLAGE");
    return;
  }
  const fs::Village& vil = *st.village;
  lab.metric("felled", vil.felled_count, "trees");
  lab.metric("vehicles", vil.traffic ? static_cast<double>(vil.traffic->vehicles.size()) : 0,
             "count");
  lab.metric("parked", vil.traffic ? static_cast<double>(vil.traffic->parked.size()) : 0, "count");
  lab.metric(
      "street-triangles",
      static_cast<double>(ge::mountain::app::street_mesh(*level, vil.ground).vertex_count()) / 3,
      "count");

  auto device = ge::gpu::create_device({.label = "village-look", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const Look& l : kLooks) blank(l, "NO GPU ADAPTER");
    lab.metric("gpu-errors", 0, "count");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "village.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  // One frame, as a game has it: every look filmed as it was before the village (the before),
  // then the village shown in it (show_village, after the frame's load, as the app does) and
  // every look filmed again.
  auto frame = fall_line::app::create_frame(*device, *level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, {});
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::skier::SkierState skier = st.racers[0].skier;
  auto film = [&](r::FrameView v) {
    fall_line::app::cut(*frame);
    for (int k = 0; k < frames; ++k) {
      const double t = at + (k - frames + 1) / 60.0;
      fall_line::app::FrameScene scene{std::span<const ge::skier::SkierState>(&skier, 1),
                                       k == 0 ? 0.0 : 1.0 / 60,
                                       k == 0,
                                       {},
                                       {},
                                       nullptr,
                                       t};
      v.frame = static_cast<uint64_t>(k);
      v.time_s = t;
      fall_line::app::draw(*device, *frame, v, scene, target.view);
    }
    return ge::gpu::read_now(*device, target);
  };
  std::vector<r::FrameView> views;
  std::vector<bool> shown;
  for (const Look& l : kLooks) {
    r::FrameView v;
    fall_line::app::day_view(*level, lab.arg_num("hour"), v);
    shown.push_back(aim(st, l.id, at, v));
    views.push_back(v);
  }
  std::vector<ge::gpu::Image> before(std::size(kLooks));
  for (size_t i = 0; i < std::size(kLooks); ++i) {
    if (shown[i]) before[i] = film(views[i]);
  }
  fall_line::app::show_village(*device, *frame, st.village);
  for (size_t i = 0; i < std::size(kLooks); ++i) {
    const Look& l = kLooks[i];
    if (!shown[i]) {
      lab.note(std::string(l.id) + ": nothing to show at " + std::to_string(at) + " s");
      blank(l, "NOTHING TO SHOW");
      continue;
    }
    const ge::gpu::Image after = film(views[i]);
    const std::vector<uint8_t>& was = before[i].rgba;
    size_t covered = 0;
    for (size_t p = 0; p + 3 < after.rgba.size() && p + 3 < was.size(); p += 4) {
      const int d = std::abs(after.rgba[p] - was[p]) + std::abs(after.rgba[p + 1] - was[p + 1]) +
                    std::abs(after.rgba[p + 2] - was[p + 2]);
      covered += d > 24 ? 1 : 0;
    }
    lab.metric(std::string(l.id) + ".covered",
               100.0 * static_cast<double>(covered) / (static_cast<double>(width) * height), "%",
               Det::measured);
    lab.shot(shot_of(l), ge::lab::Picture{width, height, after.rgba}, Det::measured);
    ge::lab::Shot b = shot_of(l);
    b.id = std::string(l.id) + "-before";
    b.title += " (BEFORE: NO VILLAGE DRAWN)";
    lab.shot(b, ge::lab::Picture{width, height, was}, Det::measured);
  }
  lab.metric("gpu-errors", device->errors.load(), "count");
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while drawing the village",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

const ge::lab::LabSpec kSpec{
    "village-look",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "games/fall_line/app/frame.hpp's show_village: mountain/app/village_look.hpp's streets and "
    "vehicles",
    "render",
    "Does the free ride's village read as streets on the snow with traffic on them, and the "
    "felled trees gone?",
    {
        {"seed", "38", "the map's seed"},
        {"version", "10", "the generator version (the ski area's)"},
        {"at", "120", "s of the run's clock the traffic is drawn at"},
        {"hour", "11", "the sun's hour (-1: the map's own)"},
        {"frames", "8", "frames drawn into each shot at 60 a second"},
        {"px", "768", "the pictures' width (16:9)"},
    },
    "--px=256 --frames=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
