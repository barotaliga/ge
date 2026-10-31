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
// picture the village changes is measured. The evening's, the night's and the small hours' looks
// are lit by the lamps after dark (the street lamps, the windows' spill, the skier's headlamp;
// `--lights=0` draws them unlit, for a before).
//
// THE COLD'S MARKS (M51f: mountain/air/marks.hpp, village_look.hpp's `village_surfaces`): the
// car park at 08:00 under a clear sky picked for it, the night's frost on the parked cars, and
// a street at 11:00 in a freezing rain (rain to the summit from a warm layer aloft over a day
// at `--freezing` C), its carriageway glazed. Each one's before is the same look with the marks
// wiped off. The lab fails when a clear night leaves less frost by 08:00 than whitens a car or
// any at 13:00, or when the freezing rain leaves the road short of a full glaze (deterministic
// rows: the frost at 02:00, 08:00 and 13:00, the glaze). Without an adapter it notes that and draws
// placeholders, so the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead. Built
// only with GE_GPU.
//
// ge:summary Lab: the free ride's village drawn: its streets, its cars, the bus, the felled trees
// ge:keywords lab village village-look streets vehicles traffic felled picture fall-line
// ge:keywords frost hoar-frost black-ice freezing-rain glaze village-cold
// ge:skill render mountain labs
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
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
#include "mountain/air/marks.hpp"
#include "mountain/app/building_look.hpp"
#include "mountain/app/village_look.hpp"
#include "mountain/mapgen/weather.hpp"
#include "mountain/village/traffic.hpp"
#include "wind/climate.hpp"

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
  double hour = -2;  // the sun's hour for this look; -2: the lab's --hour
  // The cold's marks (M51f): 1 the night's frost at the map's own sky, 2 a freezing rain's glaze
  // (`--freezing`'s sky). Its before is the same look without them.
  int cold = 0;
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
     "the aisles and rows of parked cars, each in its bay, in a village's mix of colours; each "
     "car's shadow on the snow beside it"},
    {"low-sun", "THE CAR PARK IN THE AFTERNOON", "The car park from the same place at 14:00.",
     "every parked car's long shadow across its neighbours and the aisle; the trees' shadows "
     "on the streets",
     14},
    {"street", "A CAR ON ITS LANE", "From 9 m behind and 2.5 m over the first car under way.",
     "a car with its glasshouse, bumpers and four wheels sitting on the street, not floating or "
     "sunk; the street under it; its shadow under it and the skier's beside the street"},
    {"front-row", "THE BASE LODGE", "From 45 m before the lodge and 12 m over it, from the runs.",
     "the lodge's boarded ground floor, its band of glass and terrace under a broad snowed roof; "
     "the rental and the school or the ticket office beside it; each on its plinth, none floating "
     "or sunk into the slope; their shadows on the snow"},
    {"church", "THE CHURCH ACROSS THE MAIN STREET",
     "From 75 m before the church and 14 m over the street.",
     "the nave under its steep snowed roof and the tower with its spire over the village; the "
     "shops beside it with their awnings and display windows; the street before them"},
    {"town", "A HOUSE ON THE BACK STREET", "From 35 m before the first house and 12 m over it.",
     "chalets of stone and timber under deep eaves, flats among them, in a village's mix of "
     "renders; the trees felled round them, the forest behind"},
    {"evening", "THE SQUARE AT HALF PAST SIX", "The square from the same place at 18:30.",
     "the shops' display windows and the homes' windows lit warm against the blue snow; a few "
     "still dark; no window lit by day's light alone",
     18.5},
    {"night", "THE VILLAGE AT TEN", "From 140 m over the village's middle at 22:00.",
     "the homes and the hotels mostly lit, the shops dark, the garage lit for the groomers; the "
     "lit windows reading as points of warm light in the dark",
     22},
    {"late", "THE VILLAGE AT TWO", "From 140 m over the village's middle at 02:00.",
     "the village nearly dark: a home or two, the hotels' night porters, first aid's lamp, the "
     "garage and the pump house working",
     2},
    {"frost-dawn", "THE CAR PARK AT EIGHT, AFTER A CLEAR NIGHT",
     "The car park at 08:00 after a clear, calm night (a clear sky picked for it): the night's "
     "frost on it.",
     "the parked cars white with hoar, their windscreens rimed over; the streets a little "
     "paler; gone again in the 11:00 car park",
     8, 1},
    {"black-ice", "A STREET IN A FREEZING RAIN",
     "Behind the first car under way at 11:00, rain falling from a warm layer aloft onto a road "
     "below 0 C.",
     "the carriageway under the car dark and glossy, mirroring the grey sky: black ice; the "
     "sidewalks beside it pale",
     11, 2},
};

// The village's materials as they are without the cold's marks (a cold look's before).
void wipe_cold(ge::gpu::Device& device, fall_line::app::GameFrame& f) {
  const std::vector<r::Surface> bare = ge::mountain::app::village_surfaces({});
  for (r::InstanceFamily* family : {&f.streets, &f.parked, &f.traffic}) {
    if (family->capacity > 0) r::set_family_surfaces(device, *family, bare);
  }
  f.village_frost = f.village_glaze = -1;
  f.cold_t = f.cold_hour = -1e9;
}

// The first building of `kind` in the village, or null.
const vg::Building* first_of(const fs::Village& vil, vg::BuildingKind kind) {
  for (const vg::Building& b : vil.buildings) {
    if (b.kind == kind) return &b;
  }
  return nullptr;
}

// The camera `back` m behind and `up` m over (x, y, z) along `heading`, looking at it; `where`
// the point looked at.
void look_at(const ge::mountain::mapgen::Level& level, double x, double y, double z, double heading,
             double back, double up, r::FrameView& v, double where[3]) {
  where[0] = x;
  where[1] = y;
  where[2] = z;
  const double ex = x - back * lm::sin(heading), ez = z - back * lm::cos(heading);
  v.eye[0] = ex;
  v.eye[2] = ez;
  v.eye[1] = std::max(y + up, level.ground_at(ex, ez) + 1.5);
  const double tx = x - ex, tz = z - ez, ty = y + 0.8 - v.eye[1];
  v.heading = lm::atan2(tx, tz);
  v.pitch = lm::atan2(ty, ge::core::hypot(tx, tz));
}

// Where each look stands its camera at `t`; false when the village has nothing for it.
bool aim(const fs::GameState& st, const char* id, double t, r::FrameView& v, double where[3]) {
  const fs::Village& vil = *st.village;
  const vg::Streets& s = vil.streets;
  const ge::mountain::mapgen::Level& level = *st.level;
  // Down the valley is z of the plan's sign.
  const double valley = s.valley > 0 ? 0 : 3.14159265358979323846;
  if (std::strcmp(id, "above") == 0 || std::strcmp(id, "night") == 0 ||
      std::strcmp(id, "late") == 0) {
    look_at(level, s.centre_x, level.ground_at(s.centre_x, s.centre_z), s.centre_z, valley, 90, 140,
            v, where);
    return true;
  }
  for (const vg::VillageArea& a : s.areas) {
    const bool square = a.kind == vg::AreaKind::square;
    const bool on_square = std::strcmp(id, "square") == 0 || std::strcmp(id, "evening") == 0;
    const bool on_park = std::strcmp(id, "carpark") == 0 || std::strcmp(id, "low-sun") == 0 ||
                         std::strcmp(id, "frost-dawn") == 0;
    if ((on_square && square) || (on_park && !square)) {
      const double back = square ? 30 : 25, up = square ? 18 : 15;
      look_at(level, a.x, a.y, a.z, a.heading + 3.14159265358979323846, back, up, v, where);
      return true;
    }
  }
  const struct {
    const char* id;
    vg::BuildingKind kind;
    double back, up;
  } fronts[] = {{"front-row", vg::BuildingKind::lodge, 45, 12},
                {"church", vg::BuildingKind::church, 75, 14},
                {"town", vg::BuildingKind::house, 35, 12}};
  for (const auto& f : fronts) {
    if (std::strcmp(id, f.id) != 0) continue;
    const vg::Building* b = first_of(vil, f.kind);
    if (b == nullptr) return false;
    // Looking back at its front from before it.
    look_at(level, b->x, b->y, b->z, b->heading + 3.14159265358979323846, f.back, f.up, v, where);
    return true;
  }
  if ((std::strcmp(id, "street") == 0 || std::strcmp(id, "black-ice") == 0) && vil.traffic) {
    const vg::TrafficPlan& plan = *vil.traffic;
    for (int32_t k = 0; k < static_cast<int32_t>(plan.vehicles.size()); ++k) {
      if (plan.vehicles[static_cast<size_t>(k)].kind == vg::VehicleKind::bike) continue;
      const vg::VehiclePose p = vg::vehicle_at(level, plan, k, t);
      if (!p.shown || p.speed < 2) continue;
      look_at(level, p.x, p.y, p.z, p.heading, 9, 2.5, v, where);
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

  // Not const: the freezing rain's look picks its sky on the map the frame draws.
  const auto level = std::make_shared<ge::mountain::mapgen::Level>(
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
  lab.metric("buildings", static_cast<double>(vil.buildings.size()), "count");
  // The windows lit at ten and at two, the sun far down.
  for (const auto& [name, hour] : {std::pair{"lit-at-22", 22.0}, std::pair{"lit-at-2", 2.0}}) {
    double lit = 0;
    for (const vg::Building& b : vil.buildings) lit += ge::mountain::app::lights_on(b, hour, -0.5);
    lab.metric(name, lit, "buildings");
  }
  lab.metric("felled", vil.felled_count, "trees");
  lab.metric("vehicles", vil.traffic ? static_cast<double>(vil.traffic->vehicles.size()) : 0,
             "count");
  lab.metric("parked", vil.traffic ? static_cast<double>(vil.traffic->parked.size()) : 0, "count");
  lab.metric(
      "street-triangles",
      static_cast<double>(ge::mountain::app::street_mesh(*level, vil.ground).vertex_count()) / 3,
      "count");

  // THE COLD'S MARKS on the village (deterministic rows): the frost on a parked car's roof under
  // a clear sky through the night and the morning, and the glaze a freezing rain lays.
  ge::mountain::mapgen::SkyPick clear, freezing;
  clear.kind = ge::mountain::mapgen::WeatherKind::clear;
  freezing.kind = ge::mountain::mapgen::WeatherKind::snow;
  freezing.snowfall = 0.5;
  freezing.snowline = 2;  // a warm layer aloft: rain to the summit
  freezing.mean_c = lab.arg_num("freezing");
  {
    const double x = 0.5 * (vil.box[0] + vil.box[2]), z = 0.5 * (vil.box[1] + vil.box[3]);
    const ge::wind::Site roof{.y = level->ground_at(x, z), .above = 1.5};
    ge::mountain::mapgen::Level night = *level;
    ge::mountain::mapgen::with_sky(night, clear);
    for (const double hour : {2.0, 8.0, 13.0}) {
      const double mm = ge::mountain::air::frost_on(night, roof, hour);
      lab.metric("frost-at-" + std::to_string(static_cast<int>(hour)), mm, "mm");
    }
    const double dawn = ge::mountain::air::frost_on(night, roof, 8);
    const double noon = ge::mountain::air::frost_on(night, roof, 13);
    if (ge::mountain::air::frost_cover(dawn) < 1 || noon > 0) {
      lab.fail("a clear night left " + std::to_string(dawn) + " mm of frost by 08:00 and " +
                   std::to_string(noon) + " mm at 13:00 (want it whitening the cars, then gone)",
               "Check mountain/air/marks.cpp's frost_on and wind/cold.cpp's rates. Skill: "
               "mountain.");
    }
    ge::mountain::mapgen::Level rain = *level;
    ge::mountain::mapgen::with_sky(rain, freezing);
    const double glaze = ge::mountain::air::glaze_at(rain, roof, at, 11);
    lab.metric("glaze-freezing-rain", glaze, "share");
    if (glaze < 0.99) {
      lab.fail("a freezing rain glazed the road only " + std::to_string(glaze),
               "Check mountain/air/marks.cpp's glaze_at. Skill: mountain.");
    }
  }

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
  // --lights: the lamps after dark (app/lamps.hpp: the street lamps, the windows' spill, the
  // skier's headlamp) on or off, for a before and after.
  fall_line::app::FrameOptions options;
  options.lights = lab.arg_num("lights") != 0;
  options.light_shadows = options.lights;
  options.cold = lab.arg_num("cold") != 0;
  auto frame = fall_line::app::create_frame(*device, *level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, options);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  // The skier stood 5 m to the side of what each look is at, so the figures' atlas (where the
  // traffic casts) has a tile there.
  auto film = [&](r::FrameView v, const double where[3], double hour) {
    ge::skier::SkierState skier = st.racers[0].skier;
    skier.x = where[0] + 5;
    skier.z = where[2];
    skier.y = level->ground_at(skier.x, skier.z);
    fall_line::app::cut(*frame);
    for (int k = 0; k < frames; ++k) {
      const double t = at + (k - frames + 1) / 60.0;
      fall_line::app::FrameScene scene{.skiers = std::span<const ge::skier::SkierState>(&skier, 1),
                                       .dt = k == 0 ? 0.0 : 1.0 / 60,
                                       .settle = k == 0,
                                       .run_time = t,
                                       .hour = hour};
      v.frame = static_cast<uint64_t>(k);
      v.time_s = t;
      fall_line::app::draw(*device, *frame, v, scene, target.view);
    }
    return ge::gpu::read_now(*device, target);
  };
  std::vector<r::FrameView> views;
  std::vector<std::array<double, 3>> wheres;
  std::vector<bool> shown;
  std::vector<double> hours;
  for (const Look& l : kLooks) {
    r::FrameView v;
    hours.push_back(l.hour > -2 ? l.hour : lab.arg_num("hour"));
    fall_line::app::day_view(*level, hours.back(), v);
    std::array<double, 3> where{};
    shown.push_back(aim(st, l.id, at, v, where.data()));
    views.push_back(v);
    wheres.push_back(where);
  }
  std::vector<ge::gpu::Image> before(std::size(kLooks));
  for (size_t i = 0; i < std::size(kLooks); ++i) {
    if (shown[i] && kLooks[i].cold == 0) before[i] = film(views[i], wheres[i].data(), hours[i]);
  }
  fall_line::app::show_village(*device, *frame, st.village);
  for (size_t i = 0; i < std::size(kLooks); ++i) {
    const Look& l = kLooks[i];
    if (!shown[i]) {
      lab.note(std::string(l.id) + ": nothing to show at " + std::to_string(at) + " s");
      blank(l, "NOTHING TO SHOW");
      continue;
    }
    if (l.cold != 0) {
      ge::mountain::mapgen::with_sky(*level, l.cold == 1 ? clear : freezing);
      fall_line::app::day_view(*level, hours[i], views[i]);
      aim(st, l.id, at, views[i], wheres[i].data());
    }
    if (l.cold != 0) {
      // The before: the same look with the cold's marks wiped off.
      frame->options.cold = false;
      wipe_cold(*device, *frame);
      before[i] = film(views[i], wheres[i].data(), hours[i]);
      frame->options.cold = options.cold;
    }
    const ge::gpu::Image after = film(views[i], wheres[i].data(), hours[i]);
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
    b.title += l.cold != 0 ? " (BEFORE: NO FROST OR ICE DRAWN)" : " (BEFORE: NO VILLAGE DRAWN)";
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
    "vehicles, building_look.hpp's buildings",
    "render",
    "Does the free ride's village read as streets on the snow with buildings along them and "
    "traffic on them, and the felled trees gone?",
    {
        {"seed", "38", "the map's seed"},
        {"version", "10", "the generator version (the ski area's)"},
        {"at", "120", "s of the run's clock the traffic is drawn at"},
        {"hour", "11", "the sun's hour (-1: the map's own)"},
        {"frames", "8", "frames drawn into each shot at 60 a second"},
        {"px", "768", "the pictures' width (16:9)"},
        {"lights", "1", "the lamps after dark: the street lamps, the windows' spill, the headlamp"},
        {"cold", "1", "the cold's marks on the village (0: none, for a before)"},
        {"freezing", "-4", "the freezing rain's day: its mean at the mountain's base, C"},
    },
    "--px=256 --frames=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
