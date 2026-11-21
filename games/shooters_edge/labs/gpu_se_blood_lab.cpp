// LAB `se-blood`: THE BLOOD A FIGHT LEAVES, OVER TIME. On a real map (`--game=<install>`, the
// folder holding the game's packs; `--classic=q1|q2`, `--map`) a player stands a step off a wall
// and is hit again and again for `--fight` seconds, a pellet's blood a hit as a match sends it
// (Effect blood, its count of pellets), seen from a camera off to the side of the line of fire;
// the window draws it (client/world_draw.hpp, the modern look, ce_blood on): the spray, the
// spatter on the wall behind him, the drips running down it, the pool gathering on the floor
// under him and mirroring the room while it is wet, and all of it darkening as it dries.
// The blow is `--weapon`'s (pellet, nail or axe), struck from a shooter in front of him: the wounds
// on his skin, entry and (a nail's) exit, soak his clothes and run down them, drops falling from
// them; at `--walk` s he walks off out of the pool, leaving bloody prints a stride apart.
// Pictures at `--moments` (s from the first hit), from the side, from close over the pool and
// from in front of him (his wounds).
// The spot is found by the map's own spawns (`--spot` picks another of them), so no coordinates
// are needed; PRIVATE pictures only (the game's own maps and models).
//
// Without `--game` (the smoke run) it is the CPU half alone: every blood kind of engine/detail's
// decals, its colour, opacity, gloss and spread at ages through its life (deterministic).
//
// ge:summary Lab: a fight's blood on a real map over time: spatter, drips, pools, drying
// ge:keywords lab se-blood blood pool drip dry darken reflection ce_blood ce_blood_time
// ge:keywords ce_blood_reflect blood-pool blood-drip time-lapse wound exit-wound boot-print
// ge:keywords blood-on-body by-weapon
// ge:skill render shooter labs
// ge:test tests/detail_decals_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/survey.hpp"
#include "camera/pose.hpp"
#include "camera/to_view.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "core/prng.hpp"
#include "detail/decals.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gr = ge::render;
namespace gd = ge::detail;
namespace pf = shooters_edge::profiles;
namespace num = ge::core::num;

constexpr double kDt = 1.0 / 30;    // the frames stepped and drawn
constexpr double kHitEvery = 0.45;  // s between the hits of the fight
constexpr double kFlyS = 0.5;       // the frames drawn before a picture, so the spray flies
constexpr double kDegToRad = 3.14159265358979 / 180;
constexpr double kWalkS = 3.5;  // the walk off out of the pool, s
constexpr double kWalkU = 56;   // its pace, units a second

std::string fixed(double v, int places) { return num::to_fixed(v, places); }

bool solid(const br::Map& m, float x, float y, float z) {
  const float q[3] = {x, y, z};
  return br::cell_at(m, q) == br::Cell::solid;
}

// Whether the segment from `a` to `b` (units) passes no solid.
bool clear(const br::Map& m, const float a[3], const float b[3]) {
  const float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
  const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
  const int steps = std::max(1, static_cast<int>(len / 4));
  for (int i = 0; i <= steps; i++) {
    const float f = static_cast<float>(i) / static_cast<float>(steps);
    if (solid(m, a[0] + dx * f, a[1] + dy * f, a[2] + dz * f)) return false;
  }
  return true;
}

// The floor under `p` (units): its height, false when none is within 128 units.
bool floor_at(const br::Map& m, float x, float y, float z, float* out) {
  if (solid(m, x, y, z)) return false;
  for (float d = 0; d < 128; d += 1)
    if (solid(m, x, y, z - d - 1)) {
      *out = z - d;
      return true;
    }
  return false;
}

// THE SPOT: a player standing a step off a flat wall on a flat floor, and two eyes on him.
struct Eye {
  float at[3] = {0, 0, 0};
  float yaw = 0, pitch = 0;  // degrees, the map's: pitch positive down
};
struct Spot {
  float body[3] = {0, 0, 0};  // the player's origin (24 units over the floor)
  float wall_yaw = 0;         // the way from him to the wall, degrees
  Eye side, close, front;     // front: on the body, from where his shooter stands
};

Eye eye_on(const float from[3], const float target[3]) {
  Eye e;
  for (int k = 0; k < 3; k++) e.at[k] = from[k];
  const float dx = target[0] - from[0], dy = target[1] - from[1], dz = target[2] - from[2];
  e.yaw = static_cast<float>(ge::core::libm::atan2(dy, dx) / kDegToRad);
  e.pitch =
      static_cast<float>(ge::core::libm::atan2(-dz, std::sqrt(dx * dx + dy * dy)) / kDegToRad);
  return e;
}

// From each spawn of the map in turn, the ways round it: a wall 96 to 360 units off, flat across a
// body's width and height; the player 40 units off it on a floor as flat as the spawn's; an eye
// 130 units back off the line at 28 degrees and 56 units over the floor that sees him and the
// wall, and one 70 units off over the floor beside him. `pick` skips that many good spots.
bool find_spot(const br::Map& m, int pick, Spot& out) {
  for (const br::Entity& ent : m.entities) {
    const std::string& cls = ent.get("classname");
    if (cls != "info_player_deathmatch" && cls != "info_player_start" && cls != "info_player_coop")
      continue;
    float o[3];
    if (std::sscanf(ent.get("origin").c_str(), "%f %f %f", &o[0], &o[1], &o[2]) != 3) continue;
    float fz;
    if (!floor_at(m, o[0], o[1], o[2], &fz)) continue;
    for (int a = 0; a < 360; a += 15) {
      const float c = static_cast<float>(ge::core::libm::cos(a * kDegToRad)),
                  s = static_cast<float>(ge::core::libm::sin(a * kDegToRad));
      // The wall at the body's middle, and at its sides, knees and head.
      const auto wall_at = [&](float side, float up) {
        const float bx = o[0] - s * side, by = o[1] + c * side, bz = fz + up;
        for (float d = 0; d < 400; d += 2)
          if (solid(m, bx + c * d, by + s * d, bz)) return d;
        return 1e9f;
      };
      const float d = wall_at(0, 24);
      if (d < 96 || d > 360) continue;
      bool flat = true;
      for (const float side : {-24.f, 24.f})
        for (const float up : {8.f, 48.f}) flat = flat && std::fabs(wall_at(side, up) - d) < 4;
      if (!flat) continue;
      const float bx = o[0] + c * (d - 40), by = o[1] + s * (d - 40);
      float bz;
      if (!floor_at(m, bx, by, fz + 24, &bz) || std::fabs(bz - fz) > 2) continue;
      bool level = true;
      for (const float dx : {-24.f, 24.f})
        for (const float dy : {-24.f, 24.f}) {
          float z;
          level = level && floor_at(m, bx + dx, by + dy, fz + 24, &z) && std::fabs(z - fz) <= 2;
        }
      if (!level) continue;
      const float chest[3] = {bx, by, fz + 32};
      // The side eye: back along the line, turned off it.
      const float ra = static_cast<float>((a + 180 + 28) * kDegToRad);
      const float side_at[3] = {bx + 130 * static_cast<float>(ge::core::libm::cos(ra)),
                                by + 130 * static_cast<float>(ge::core::libm::sin(ra)), fz + 56};
      const float wall_spot[3] = {bx + c * 30, by + s * 30, fz + 20};
      if (solid(m, side_at[0], side_at[1], side_at[2]) || !clear(m, side_at, chest) ||
          !clear(m, side_at, wall_spot))
        continue;
      // The close eye: beside him either side (or back from him), over the floor, looking down
      // at his feet: the first in the clear.
      const float feet[3] = {bx + c * 8, by + s * 8, fz + 2};
      float close_at[3];
      bool close_ok = false;
      for (const int turn : {90, -90, 150, -150, 180})
        for (float dist = 60; dist <= 120 && !close_ok; dist += 20)
          for (const float up : {34.f, 46.f}) {
            if (close_ok) break;
            const float ca = static_cast<float>((a + turn) * kDegToRad);
            close_at[0] = bx + dist * static_cast<float>(ge::core::libm::cos(ca));
            close_at[1] = by + dist * static_cast<float>(ge::core::libm::sin(ca));
            close_at[2] = fz + up;
            close_ok = !solid(m, close_at[0], close_at[1], close_at[2]) && clear(m, close_at, feet);
          }
      if (!close_ok) continue;
      if (pick-- > 0) continue;
      out.body[0] = bx, out.body[1] = by, out.body[2] = fz + 24;
      out.wall_yaw = static_cast<float>(a);
      const float look_at[3] = {bx + c * 14, by + s * 14, fz + 18};
      out.side = eye_on(side_at, look_at);
      out.close = eye_on(close_at, feet);
      // The front eye: off his chest the way he faces, a little to the side.
      const float fa = static_cast<float>((a + 180 + 28 + 20) * kDegToRad);
      float front_at[3] = {bx + 60 * static_cast<float>(ge::core::libm::cos(fa)),
                           by + 60 * static_cast<float>(ge::core::libm::sin(fa)), fz + 40};
      const float mid[3] = {bx, by, fz + 30};
      if (solid(m, front_at[0], front_at[1], front_at[2]) || !clear(m, front_at, mid))
        for (int k = 0; k < 3; k++) front_at[k] = side_at[k];
      out.front = eye_on(front_at, mid);
      return true;
    }
  }
  return false;
}

// THE CPU HALF: every blood kind's look through its life.
void cpu_half(gl::Lab& lab) {
  gl::LabTable t{"the blood's look by its age (engine/detail's look_at)",
                 {"kind", "age (s)", "r", "g", "b", "opacity", "gloss"},
                 {}};
  for (int k = 0; k < gd::kDecalKinds; k++) {
    const auto kind = static_cast<gd::DecalKind>(k);
    const gd::DecalKindSpec& spec = gd::decal_spec(kind);
    if (spec.name.find("blood") == std::string_view::npos) continue;
    for (const double age : {0.0, 2.0, 10.0, 30.0, 60.0, 90.0, 110.0}) {
      const gd::DecalLook l = gd::look_at(kind, age);
      t.rows.push_back({std::string(spec.name), fixed(age, 0), fixed(l.r, 3), fixed(l.g, 3),
                        fixed(l.b, 3), fixed(l.opacity, 2), fixed(l.gloss, 2)});
      lab.metric(std::string(spec.name) + ".r@" + fixed(age, 0), l.r, "linear");
    }
  }
  lab.table(t);
}

void run(gl::Lab& lab) {
  if (lab.arg("game").empty()) {
    cpu_half(lab);
    return;
  }
  std::string error;
  const pf::GameProfile* game = pf::find_game(lab.arg("classic"));
  if (game == nullptr) {
    lab.fail("se-blood: no classic game " + lab.arg("classic"), "Pass --classic=q1 or q2.");
    return;
  }
  const cl::GameAdapter& adapter = cl::adapter_of(*game);
  const ge::classicenhance::Profile& content = pf::content_of(*game);
  cl::SearchPath files;
  if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content)) {
    lab.fail("se-blood: " + error, "Pass --game=<the folder holding " +
                                       std::string(content.base_dir) + ">. Skill: shooter.");
    return;
  }
  std::string map_name = lab.arg("map");
  if (map_name.empty()) map_name = lab.arg("classic") == "q2" ? "q2dm1" : "dm3";
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map) {
    lab.fail("se-blood: " + error, "Pass --map=<a map of the game's packs>. Skill: shooter.");
    return;
  }
  Spot spot;
  if (!find_spot(map->bsp.map, static_cast<int>(lab.arg_num("spot")), spot)) {
    lab.fail("se-blood: " + map_name + " has no spawn a step from a flat wall",
             "Pass another --map or a lower --spot. Skill: shooter.");
    return;
  }
  lab.note("the body at " + fixed(spot.body[0], 0) + "," + fixed(spot.body[1], 0) + "," +
           fixed(spot.body[2], 0) + ", the wall at yaw " + fixed(spot.wall_yaw, 0));
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-blood: the baseline mod: " + error, "Read the reason above. Skill: progs.");
    return;
  }
  cl::ListenOptions lo = shooters_edge::net::listen_options();
  lo.console = {"deathmatch 1"};
  cl::Listen l;
  if (!cl::listen_start(l, progs, std::move(map), lo, error)) {
    lab.fail("se-blood: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  for (int i = 0; i < 20; i++) cl::listen_frame(l, {});
  auto device = ge::gpu::create_device({.label = "se-blood", .validation = 1});
  if (!device) {
    lab.note("no GPU device: no pictures (install Mesa's Vulkan drivers; skill: gpu)");
    return;
  }
  const uint32_t w = static_cast<uint32_t>(lab.arg_num("width")),
                 h = static_cast<uint32_t>(lab.arg_num("height"));
  shooters_edge::look::Picture look;
  ge::console::Console con;
  shooters_edge::look::add_cvars(con, look);
  con.execute(lab.cvars());
  cl::WorldDrawPtr world =
      cl::create_world_draw(*device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm, look, {},
                            error, {l.server->models, {}, {}}, adapter);
  if (!world) {
    lab.fail("se-blood: " + error, "Read the gpu: lines above. Skill: render.");
    return;
  }
  ge::gpu::Target out = ge::gpu::create_target(*device, "se-blood.final", w, h);
  cl::ViewState shown;
  cl::FxScene scene;
  cl::fx_scene(l, shown, scene);
  scene.own = 0;  // no player of the eye's: each hit's blow from the eye
  scene.bodies.clear();
  scene.models.clear();
  cl::Placed body;
  body.entity = 2;
  body.modelindex = l.server->sv.edicts[1].modelindex;
  body.frame = 12;  // standing
  body.origin = {spot.body[0], spot.body[1], spot.body[2]};
  body.angles = {0, spot.wall_yaw + 180 + 28, 0};  // facing the side eye's way
  body.top = 4;
  body.bottom = 12;
  if (body.modelindex > 0 && lab.arg("body") != "0") scene.models.push_back(body);
  const double yaw = (spot.wall_yaw + 180 + 28) * kDegToRad;
  const float face[2] = {static_cast<float>(ge::core::libm::cos(yaw)),
                         static_cast<float>(ge::core::libm::sin(yaw))};
  const std::string weapon = lab.arg("weapon");
  scene.staged_by = weapon == "nail"  ? shooters_edge::effects::BloodBy::nail
                    : weapon == "axe" ? shooters_edge::effects::BloodBy::blade
                                      : shooters_edge::effects::BloodBy::pellet;
  // The shooter: in front of him (an axe's at arm's length), at his eyes' height.
  const float reach = weapon == "axe" ? 40.0f : 300.0f;
  scene.staged_from = {spot.body[0] + face[0] * reach, spot.body[1] + face[1] * reach,
                       spot.body[2] + 22};
  // The walk off: away from the wall, turned off the side eye's line.
  const double walk_s = lab.arg_num("walk");
  // The first way off on a level floor the whole walk long.
  double walk_yaw = spot.wall_yaw + 180 - 60;
  for (const double turn : {-60.0, 60.0, -100.0, 100.0, 0.0}) {
    const double a = (spot.wall_yaw + 180 + turn) * kDegToRad;
    const float c = static_cast<float>(ge::core::libm::cos(a)),
                sn = static_cast<float>(ge::core::libm::sin(a));
    bool level = true;
    for (float d = 0; d <= kWalkU * kWalkS + 32 && level; d += 8) {
      float z;
      level =
          floor_at(l.map->bsp.map, spot.body[0] + c * d, spot.body[1] + sn * d, spot.body[2], &z) &&
          std::fabs(z - (spot.body[2] - 24)) <= 2;
    }
    if (!level) continue;
    walk_yaw = spot.wall_yaw + 180 + turn;
    break;
  }
  const double wa = walk_yaw * kDegToRad;
  const float walk[2] = {static_cast<float>(ge::core::libm::cos(wa)),
                         static_cast<float>(ge::core::libm::sin(wa))};
  // Once he has walked off, the close eye looks down his prints from over where he stood.
  const float half = static_cast<float>(kWalkU * kWalkS * 0.5);
  const float tread_mid[3] = {spot.body[0] + walk[0] * half, spot.body[1] + walk[1] * half,
                              spot.body[2] - 22};
  const float tread_from[3] = {spot.body[0] - walk[0] * 16, spot.body[1] - walk[1] * 16,
                               spot.body[2] + 60};
  const Eye tread_eye = eye_on(tread_from, tread_mid);
  const bool q2 = lab.arg("classic") == "q2";
  std::vector<cl::Effect> effects;
  cl::FxState fx_state;
  cl::FxFrame fx;
  const auto pose_of = [](const Eye& e) {
    ge::camera::Pose p;
    p.eye = cl::engine_point({e.at[0], e.at[1], e.at[2]});
    p.yaw = cl::engine_heading(e.yaw);
    p.pitch = cl::engine_pitch(e.pitch);
    p.fov_y = 1.2;
    return p;
  };
  scene.pose = pose_of(spot.side);
  const double fight_s = lab.arg_num("fight");
  const std::vector<double> moments = lab.arg_nums("moments");
  ge::core::Rng rng = ge::core::make_rng(38);
  double t = 0, next_hit = 0;
  const auto draw = [&](const Eye& e) {
    gr::FrameView view = cl::frame_view(l, shown, 0, t, 0);
    ge::camera::to_frame_view(pose_of(e), view);
    view.time_s = t;
    cl::draw_world(*device, *world, view, fx, nullptr, kDt, {}, out.view);
  };
  const auto grab = [&]() {
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
  };
  gl::LabTable marks{"the marks on the walls and the floor at each moment",
                     {"t (s)", "kind", "live", "largest (m)"},
                     {}};
  const std::vector<std::pair<std::string, std::string>> inputs = {
      {"map", map_name},
      {"game", lab.arg("classic")},
      {"fight", fixed(fight_s, 0) + " s"},
      {"weapon", weapon}};
  size_t next = 0;
  while (next < moments.size()) {
    t += kDt;
    // The fight: a hit every kHitEvery s, on the chest or the belly, one to three pellets.
    if (t <= fight_s && t >= next_hit) {
      cl::Effect e;
      e.kind = cl::EffectKind::blood;
      e.code = 12;
      e.count = weapon == "pellet" ? ge::core::int_in(rng, 1, 3) : 1;
      e.at = {spot.body[0] + static_cast<float>(ge::core::range(rng, -6, 6)),
              spot.body[1] + static_cast<float>(ge::core::range(rng, -6, 6)),
              spot.body[2] + static_cast<float>(ge::core::range(rng, -12, 14))};
      effects.push_back(e);
      next_hit += kHitEvery;
    }
    // Walking off out of the pool, his run's frames playing.
    if (walk_s > 0 && t >= walk_s && t < walk_s + kWalkS && !scene.models.empty()) {
      cl::Placed& b = scene.models[0];
      b.origin.x += walk[0] * static_cast<float>(kWalkU * kDt);
      b.origin.y += walk[1] * static_cast<float>(kWalkU * kDt);
      b.angles.y = static_cast<float>(walk_yaw);
      b.frame = (q2 ? 40 : 6) + static_cast<int>((t - walk_s) * 10) % 6;
    } else if (walk_s > 0 && t >= walk_s + kWalkS && !scene.models.empty()) {
      scene.models[0].frame = 12;
    }
    scene.effects = effects;
    scene.pose = pose_of(spot.side);
    fx_state.on = cl::fx_switches(look);
    fx_state.on.lerp = {};  // a staged scene: no server's states to draw between
    fx_state.models = &cl::world_models(*world);
    cl::step_fx(scene, fx_state, t, kDt, fx);
    const double at = moments[next];
    if (t + kFlyS >= at) draw(spot.side);  // the spray in flight up to the picture
    if (t + 1e-9 < at) continue;
    const std::string when = fixed(at, 1) + " s";
    const bool fighting = at <= fight_s;
    for (int cam = 0; cam < 3; cam++) {
      Eye e = cam == 0 ? spot.side : cam == 1 ? spot.close : spot.front;
      if (cam == 1 && walk_s > 0 && at >= walk_s + kWalkS) e = tread_eye;
      if (cam == 2 && !scene.models.empty()) {  // on him wherever he walked
        e.at[0] += scene.models[0].origin.x - spot.body[0];
        e.at[1] += scene.models[0].origin.y - spot.body[1];
      }
      draw(e);  // twice: the second frame mirrors the first (the reflections' last frame)
      draw(e);
      static const char* const kNames[] = {"side-", "pool-", "body-"};
      static const char* const kTitles[] = {"THE FIGHT'S BLOOD AT ", "THE POOL AT ",
                                            "HIS WOUNDS AT "};
      lab.shot({std::string(kNames[cam]) + std::to_string(next), kTitles[cam] + when,
                (fighting ? "A player hit again and again on "
                          : "A player hit for " + fixed(fight_s, 0) + " s on ") +
                    map_name + ", " + when + " after the first hit" +
                    (fighting ? ", " : ", the fight over, ") +
                    (cam == 0   ? "from beside the line of fire."
                     : cam == 1 ? "from close over the floor."
                                : "from in front of him."),
                cam == 0   ? "spatter on the wall behind him, drips running down it, a pool "
                             "gathering under him; darker as it dries"
                : cam == 1 ? "the pool's edge and its gloss mirroring the room while wet, "
                             "matte and near black once dried; his prints walking off"
                           : "each wound's soaked patch, the blood run down his clothes from it, "
                             "darker as it dries",
                inputs},
               grab(), gl::Det::measured);
    }
    for (int k = 0; k < gd::kDecalKinds; k++) {
      const gd::DecalKindSpec& spec = gd::decal_spec(static_cast<gd::DecalKind>(k));
      if (spec.name.find("blood") == std::string_view::npos) continue;
      int live = 0;
      double largest = 0;
      for (const gd::Decal& d : fx_state.marks.decals) {
        if (!d.live || static_cast<int>(d.kind) != k) continue;
        if (gd::look_of(d, t).opacity <= 0) continue;
        live++;
        largest = std::max(largest, std::max(d.size_m, d.length_m));
      }
      marks.rows.push_back(
          {fixed(at, 1), std::string(spec.name), std::to_string(live), fixed(largest, 2)});
    }
    next++;
  }
  lab.table(marks, gl::Det::measured);
}

const gl::LabSpec kSpec{
    "se-blood",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "the blood a fight leaves on a real map (client/world_fx_blood.cpp, engine/detail's decals, "
    "render/marks.hpp) over time, drawn by client/world_draw.hpp",
    "render",
    "Does a fight's blood spatter the wall, run down it, gather into a pool that mirrors the "
    "room while wet, and darken as it dries?",
    {
        {"game", "", "the folder holding the game's packs (id1, baseq2); empty: the CPU half"},
        {"classic", "q1", "with --game: the game (q1 or q2)"},
        {"map", "", "with --game: the map (empty: dm3, or q2dm1)"},
        {"spot", "0", "with --game: skip this many spots found by the spawns"},
        {"body", "1", "with --game: 0 draws no player where the blood comes from"},
        {"fight", "15", "with --game: seconds the player is hit"},
        {"moments", "0.4,4,10,16,22,45,100", "with --game: the pictures, s from the first hit"},
        {"weapon", "pellet", "with --game: the blow (pellet, nail: through him, axe)"},
        {"walk", "17", "with --game: s at which he walks off out of the pool (0: he stays)"},
        {"width", "960", "with --game: the frame's width, px"},
        {"height", "540", "with --game: the frame's height, px"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
