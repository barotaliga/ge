// LAB `se-sequel-weapon-fx`: THE SEQUEL'S WEAPONS AS SEEN. Every weapon the sequel's player holds
// is given, raised and fired on the window's listen server, and the shot is watched where it flies
// and lands: the blaster's and the hyperblaster's glowing bolts, the rockets' and grenades' smoke
// and blasts, the railgun's line, the BFG's green ball and burst, the bullets' and pellets' sparks.
// Each is drawn twice, in the classic look (every enhancement off) and the modern one, so a picture
// shows what the modern effects add (client/world_fx.cpp keyed by the shot's kind, which a
// monster's shot of the same kind shares).
//
// Without `--game` the weapons are fired in a hall built here and what the effects read is
// counted as deterministic metrics (the shots in flight by kind, the named effects by name), so CI
// needs no game data. With `--game=<folder>` (the folder holding the sequel's base folder) they
// are fired on `--map` and each moment is drawn from a step behind and beside the player, in the
// game's own pictures: PRIVATE pictures only.
//
// ge:summary Lab: the sequel's weapons fired, classic beside modern: bolts, smoke, rail, BFG
// ge:keywords lab se-sequel-weapon-fx sequel weapon-effects blaster hyperblaster railgun bfg
// ge:keywords rocket grenade sparks glow-trail rail-beam modern-look screenshot
// ge:skill render labs
// ge:test tests/shooters_edge_listen_test.cpp
#include <cmath>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "classicenhance/switches.hpp"
#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "shooter/pmove.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace cl = shooters_edge::client;
namespace gl = ge::lab;
namespace ls = ge::lab::liquid_scene;
namespace pf = shooters_edge::profiles;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

constexpr double kTic = 0.013;

struct Shot {
  const char* id;
  const char* look_for;
  int ticks;  // frames from the trigger to the picture (`eye`: at most, the shot's next frame)
  sv::SqWeapon weapon;
  // At the floor a few steps ahead, the trigger pulled once and the picture from the eye: the marks
  // the hits leave (bullet holes, a blast's scorch).
  bool wall = false;
  // Not fired: the weapon in hand at rest, level, from the eye (`--held=1`).
  bool held = false;
  // Fired, and the picture from the eye: the shot's flash, light and kick on the weapon in hand.
  bool eye = false;
};
constexpr Shot kShots[] = {
    {"blaster", "a yellow bolt in flight, its glow on the walls", 14, sv::SqWeapon::blaster},
    {"shotgun", "the pellets' sparks where they hit", 12, sv::SqWeapon::shotgun},
    {"super-shotgun", "a wider spray of sparks", 6, sv::SqWeapon::super_shotgun},
    {"machinegun", "a line of bullet sparks walking up the wall", 30, sv::SqWeapon::machinegun},
    {"chaingun", "a dense spray of sparks", 50, sv::SqWeapon::chaingun},
    {"grenade-launcher", "a grenade arcing with its smoke", 30, sv::SqWeapon::grenade_launcher},
    {"rocket-launcher", "a rocket's smoke and flame trail", 18, sv::SqWeapon::rocket_launcher},
    {"hyperblaster", "a stream of yellow bolts, lit", 24, sv::SqWeapon::hyperblaster},
    {"railgun", "the rail's straight blue line from gun to wall", 4, sv::SqWeapon::railgun},
    {"railgun-fading", "the line circling the beam fading, the walls lit blue along it", 30,
     sv::SqWeapon::railgun},
    {"bfg", "the green ball in flight, the room lit green", 95, sv::SqWeapon::bfg},
    {"shotgun-flash", "the flash and smoke at the muzzle, the gun kicked back, its light", 40,
     sv::SqWeapon::shotgun, false, false, true},
    {"machinegun-flash", "a small bullet's flash at the muzzle as it fires", 40,
     sv::SqWeapon::machinegun, false, false, true},
    {"rocket-launcher-flash", "the rocket's flare at the muzzle, the launcher kicked", 40,
     sv::SqWeapon::rocket_launcher, false, false, true},
    {"railgun-flash", "the rail's blue light on the gun as it fires, the gun kicked", 40,
     sv::SqWeapon::railgun, false, false, true},
    {"super-shotgun-marks", "a bullet hole where each pellet struck, spread as the shot spread",
     200, sv::SqWeapon::super_shotgun, true},
    {"rocket-marks", "the black scorch the rocket's blast left on the floor", 160,
     sv::SqWeapon::rocket_launcher, true},
};
constexpr const char* kLooks[] = {"classic", "modern"};

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A floored hall walled round with the player's start facing down its length (no game data).
std::unique_ptr<cl::LoadedMap> hall(std::string& error) {
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-1024, -256, -16, 1024, 256, 0);
  box(-1040, -256, -16, -1024, 256, 320);
  box(1024, -256, -16, 1040, 256, 320);
  box(-1040, 256, -16, 1040, 272, 320);
  box(-1040, -272, -16, 1040, -256, 320);
  box(-1040, -272, 320, 1040, 272, 336);
  world.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-768 0 24"}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  if (!br::write_brush_world(world, &bytes, &error)) return nullptr;
  return cl::map_from_bytes(bytes, "hall", error);
}

const ge::progs::Progs& progs() {
  static const ge::progs::Progs p = [] {
    ge::progs::Progs q;
    std::string why;
    cl::baseline_progs(q, why);
    return q;
  }();
  return p;
}

// The weapon given with plenty of ammo and chosen, the hand given time to raise it.
bool arm(cl::Listen& l, const Shot& shot) {
  sv::SequelServer* s = sv::sequel_of(*l.rules);
  if (s == nullptr) return false;
  const sv::SqWeaponRow& row = sv::kSqWeapons[static_cast<int>(shot.weapon)];
  sv::SqBody& body = s->players[0].body;
  if (row.item != nullptr) body.counts[sv::sq_item(row.item)] = 1;
  if (row.ammo != sv::SqAmmo::none) body.ammo[static_cast<int>(row.ammo)] = 200;
  if (shot.weapon != sv::SqWeapon::blaster)
    l.rules->client_command(0, std::string("use ") + row.name);
  for (int i = 0; i < 120; i++) cl::listen_frame(l, {});
  return s->players[0].hand.weapon == static_cast<int>(shot.weapon);
}

// What the effects were handed for a shot: the most shots in flight of each kind at once, and the
// named effects by name.
struct Seen {
  int rockets = 0, grenades = 0, bolts = 0, bfgs = 0;
  std::map<std::string, int> named;
};

void count(const cl::Listen& l, std::vector<cl::Missile>& missiles, Seen& seen) {
  cl::missiles_of(l, missiles);
  int r = 0, g = 0, b = 0, f = 0;
  for (const cl::Missile& m : missiles) {
    r += m.rocket && !m.grenade;
    g += m.grenade;
    b += m.bolt;
    f += m.bfg;
  }
  seen.rockets = std::max(seen.rockets, r);
  seen.grenades = std::max(seen.grenades, g);
  seen.bolts = std::max(seen.bolts, b);
  seen.bfgs = std::max(seen.bfgs, f);
}

void report(gl::Lab& lab, const Shot& shot, const cl::Listen& l, const Seen& seen) {
  const std::string key = std::string(shot.id) + ".";
  lab.metric(key + "rockets", seen.rockets, "count");
  lab.metric(key + "grenades", seen.grenades, "count");
  lab.metric(key + "bolts", seen.bolts, "count");
  lab.metric(key + "bfg", seen.bfgs, "count");
  std::map<std::string, int> named;
  for (const cl::Effect& e : l.effects) named[e.name.empty() ? "classic" : e.name]++;
  for (const auto& [name, n] : named) {
    std::string metric = key;
    metric += "effect.";
    metric += name;
    lab.metric(metric, n, "count");
  }
}

// The trigger held for the shot's frames (`each` after every frame).
template <class Each>
void fire(cl::Listen& l, const Shot& shot, Each each) {
  for (int i = 0; i < shot.ticks; i++) {
    sh::MoveCmd cmd;
    cmd.msec = 13;
    cmd.buttons = sh::kButtonAttack;
    cl::listen_frame(l, cmd);
    each(i);
  }
}

// The yaw (degrees, 15 apart) whose level line from the player's eye runs nearest `want` units
// before a wall: far, so a shot is seen flying and landing; a few steps, to see its marks.
float line_of(const cl::Listen& l, float want) {
  ge::brush::BrushTracer tracer(l.map->bsp.map);
  const sh::Vec3f o = l.server->world.entities[1].origin;
  const float eye[3] = {o.x, o.y, o.z + 22}, zero[3] = {0, 0, 0};
  float best = 1e9f, best_yaw = 0;
  for (int k = 0; k < 24; k++) {
    const float a = static_cast<float>(k) * 15.0f;
    const float r = a * 3.14159265f / 180.0f;
    const auto c = static_cast<float>(ge::core::libm::cos(r)),
               sn = static_cast<float>(ge::core::libm::sin(r));
    const float end[3] = {eye[0] + 3000 * c, eye[1] + 3000 * sn, eye[2]};
    const ge::brush::Trace t = tracer.trace(eye, end, zero, zero, sh::kSqSolid);
    const float miss = std::abs(t.fraction * 3000 - want);
    if (miss < best) {
      best = miss;
      best_yaw = a;
    }
  }
  return best_yaw;
}

void counts_only(gl::Lab& lab) {
  for (const Shot& shot : kShots) {
    if (shot.wall) continue;  // pictures only
    std::string error;
    std::unique_ptr<cl::LoadedMap> map = hall(error);
    cl::ListenOptions opt = shooters_edge::net::listen_options();
    opt.console = {"deathmatch 1"};
    cl::Listen l;
    if (!map || !cl::listen_start(l, progs(), std::move(map), opt, error)) {
      lab.fail(error, "The hall should write, read and start. Skill: shooter.");
      return;
    }
    if (!arm(l, shot)) {
      lab.fail(std::string(shot.id) + ": the weapon was not raised",
               "Check sq_choose takes it with its ammo. Skill: shooter.");
      cl::listen_stop(l);
      continue;
    }
    Seen seen;
    std::vector<cl::Missile> missiles;
    fire(l, shot, [&](int) { count(l, missiles, seen); });
    report(lab, shot, l, seen);
    cl::listen_stop(l);
  }
}

void run(gl::Lab& lab) {
  if (lab.arg("game").empty()) {
    counts_only(lab);
    return;
  }
  const auto size = lab.arg_nums("size");
  const int w = size.size() > 0 ? static_cast<int>(size[0]) : 640;
  const int h = size.size() > 1 ? static_cast<int>(size[1]) : 400;
  std::string error;
  const pf::GameProfile& game = *pf::find_game("sequel");
  const cl::GameAdapter& adapter = cl::adapter_of(game);
  const ge::classicenhance::Profile& content = pf::content_of(game);
  cl::SearchPath files;
  if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content)) {
    lab.fail(error, "Pass --game=<the folder holding " + std::string(content.base_dir) +
                        "/pak0.pak>. Skill: shooter.");
    return;
  }
  auto device = ge::gpu::create_device({.label = "se-sequel-weapon-fx", .validation = 1});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  if (!device) {
    lab.note("No GPU adapter on this machine: no pictures.");
    return;
  }
  // The game's own weapons in hand (its view models, at the eye).
  std::vector<ge::viewmodel::HeldModel> weapons;
  const bool at_eye = cl::load_held_set(adapter, files, {}, weapons);
  lab.metric("held.models", static_cast<double>(weapons.size()), "count", gl::Det::measured);
  if (!at_eye)
    lab.fail("the sequel's held weapons did not load",
             "Read the shooters_edge: line above for the model it lacks. Skill: shooter.");
  const std::string map_name = lab.arg("map");
  const auto nums = lab.arg_nums("aim");
  float yaw = nums.size() > 0 ? static_cast<float>(nums[0]) : 0.0f;
  const float pitch = nums.size() > 1 ? static_cast<float>(nums[1]) : 0.0f;
  const std::string only = lab.arg("weapon");
  // `--held=1`: every weapon at rest in hand instead of the shots.
  const bool held_only = lab.arg("held") == "1";
  std::vector<std::string> held_ids;
  std::vector<Shot> shots;
  if (held_only) {
    for (int k = 0; k < sv::kSqWeaponCount; k++)
      held_ids.push_back(std::string("held-") + std::to_string(k + 1));
    for (int k = 0; k < sv::kSqWeaponCount; k++)
      shots.push_back({held_ids[size_t(k)].c_str(),
                       "the weapon at rest in the right hand, in its own skin, at the eye", 30,
                       static_cast<sv::SqWeapon>(k), false, true});
  } else {
    shots.assign(std::begin(kShots), std::end(kShots));
  }
  for (const Shot& shot : shots) {
    if (!only.empty() && only != shot.id) continue;
    for (const char* look_name : kLooks) {
      if (shot.held && std::string_view(look_name) != "modern") continue;
      cl::ListenOptions opt = shooters_edge::net::listen_options();
      opt.console = {"deathmatch 1"};
      cl::Listen l;
      std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
      if (!map || !cl::listen_start(l, progs(), std::move(map), opt, error)) {
        lab.fail(error, "Check --map names a map in the game's packs. Skill: shooter.");
        return;
      }
      // Turn to the aim (none given: the longest clear line from the eye), then arm.
      for (int i = 0; i < 20; i++) cl::listen_frame(l, {});
      if (nums.empty()) yaw = line_of(l, 3000.0f);
      // At the floor a few steps ahead: the marks face the eye.
      const float tilt = shot.wall ? 35.0f : pitch;
      l.view = {tilt, yaw, 0};  // the listen sends its own view with every command
      for (int i = 0; i < 20; i++) cl::listen_frame(l, {});
      if (!arm(l, shot)) {
        lab.fail(std::string(shot.id) + ": the weapon was not raised",
                 "Check sq_choose takes it with its ammo. Skill: shooter.");
        cl::listen_stop(l);
        continue;
      }
      shooters_edge::look::Picture look;
      look.look = look_name;
      if (std::string_view(look_name) == "classic")
        for (const ge::classicenhance::SwitchSpec& sw : ge::classicenhance::switches())
          look.ce.*sw.field = false;
      const cl::ModelLoad load{l.server->models, {}};
      cl::WorldDrawPtr frame = cl::create_world_draw(*device, files, *l.map, uint32_t(w),
                                                     uint32_t(h), wgpu::TextureFormat::RGBA8Unorm,
                                                     look, weapons, error, load, adapter);
      if (!frame) {
        lab.fail(error, "Read the gpu: lines above. Skill: render.");
        return;
      }
      ge::gpu::Target out =
          ge::gpu::create_target(*device, "se-sequel-weapon-fx.final", uint32_t(w), uint32_t(h));
      cl::ViewState shown;
      cl::FxState fx_state;
      fx_state.on = cl::fx_switches(look);
      cl::FxFrame fx;
      double t = 0;
      ge::render::FrameView v = cl::frame_view(l, shown, 1, t, kTic);
      Seen seen;
      std::vector<cl::Missile> missiles;
      cl::HeldState held;
      held.hand.at_eye = at_eye;
      cl::HeldFire held_fire;
      if (std::string why; !cl::load_held_fire(held_fire, &why,
                                               at_eye && adapter.game != nullptr
                                                   ? adapter.game->held_weapons
                                                   : std::span<const std::string_view>{})) {
        lab.fail(why,
                 "Fix the weapon's fire in plugins/classicenhance/weapons.cpp. Skill: render.");
        return;
      }
      shooters_edge::view::HandFrame hand;
      const auto step = [&](int) {
        t += kTic;
        count(l, missiles, seen);
        v = cl::frame_view(l, shown, 1, t, kTic);
        cl::step_fx(l, shown, fx_state, t, kTic, fx);
        hand = cl::step_held(l, shown, held, {}, weapons, t, kTic);
        cl::fire_held(held_fire, hand, fx_state.on.muzzles, t, fx);
        cl::draw_world(*device, *frame, v, fx, &hand, kTic, {}, out.view);
      };
      sh::MoveCmd aim;
      aim.angles = {tilt, yaw, 0};
      aim.msec = 13;
      // The modern look's eye adapts to the scene over its first frames: drawn a while before
      // the trigger, so the picture is not taken through an eye still settling.
      for (int i = 0; i < 90; i++) {
        cl::listen_frame(l, aim);
        step(i);
      }
      seen = {};
      int fired_at = -1;
      for (int i = 0; i < shot.ticks; i++) {
        sh::MoveCmd cmd = aim;
        if (!shot.held && (!shot.wall || i < 4)) cmd.buttons = sh::kButtonAttack;
        cl::listen_frame(l, cmd);
        step(i);
        // From the eye: the picture a frame after the shot leaves the muzzle (the flash lasts a
        // few hundredths of a second; the hand steps once a rules frame).
        if (shot.eye && hand.fired) fired_at = i;
        if (shot.eye && fired_at >= 0 && i >= fired_at + 1) break;
      }
      if (std::string_view(look_name) == "modern" && !shot.wall && !shot.eye)
        report(lab, shot, l, seen);
      // The eye a step ahead of the player and beside him, turned in, so the shot is seen across
      // its path.
      const double fx_ = ge::core::libm::sin(v.heading), fz = ge::core::libm::cos(v.heading);
      if (!shot.wall && !shot.held && !shot.eye) {
        v.eye[0] += fx_ * 0.6 + fz * 0.9;
        v.eye[2] += fz * 0.6 - fx_ * 0.9;
        v.eye[1] += 0.25;
        v.heading -= 0.55;
      }
      cl::FxFrame still = fx;  // the same moment again: nothing born twice, nothing moved
      still.births.clear();
      // The weapon in hand only where the picture is the eye's own.
      cl::draw_world(*device, *frame, v, still,
                     shot.wall || shot.held || shot.eye ? &hand : nullptr, 0, {}, out.view);
      const ge::gpu::Image img = ge::gpu::read_now(*device, out);
      const std::string id = std::string(shot.id) + "-" + look_name;
      lab.shot(
          {id,
           std::string(sv::kSqWeapons[static_cast<int>(shot.weapon)].name) + ", " + look_name,
           shot.held ? std::string("The ") + sv::kSqWeapons[static_cast<int>(shot.weapon)].name +
                           " at rest in the player's hand on " + map_name +
                           ", the game's own view model drawn at the eye."
                     : std::string("The ") + sv::kSqWeapons[static_cast<int>(shot.weapon)].name +
                           " fired on " + map_name + " in the " + look_name +
                           (shot.wall  ? " look at the floor a few steps ahead, seen from the eye "
                            : shot.eye ? " look, seen from the eye "
                                       : " look, seen from a step ahead and beside the player ") +
                           (shot.eye ? std::string("a frame after the shot left the muzzle.")
                                     : std::to_string(shot.ticks) + " frames after the trigger."),
           shot.look_for,
           {{"map", map_name}, {"weapon", shot.id}, {"look", look_name}}},
          gl::Picture{int(img.width), int(img.height), img.rgba}, gl::Det::measured);
      cl::listen_stop(l);
    }
  }
  ls::checked(lab, static_cast<int>(device->errors.load()),
              static_cast<int>(device->pipelines_after_load));
}

const gl::LabSpec kSpec{
    "se-sequel-weapon-fx",
    gl::LabKind::picture,
    "game:shooters_edge",
    "the sequel's weapon effects: client/listen.cpp's shots by kind and named effects, "
    "client/world_fx.cpp and app/weapon_fx.cpp's modern looks",
    "render",
    "Does every weapon the sequel's player fires show its shot, its trail and its hit, and does "
    "the modern look add glows, the rail's beam and the BFG's burst over the classic one?",
    {{"size", "640,400", "the screen's width and height, px"},
     {"game", "", "the folder holding the sequel's base folder (empty: a hall, counts only)"},
     {"held", "0", "1: every weapon at rest in hand from the eye, in place of the shots"},
     {"weapon", "", "only this weapon's pictures, by its id (blaster, railgun, ...; empty: all)"},
     {"map", "q2dm1", "the map to fire on (with --game)"},
     {"aim", "", "the view's yaw and pitch, degrees (empty: the longest clear line, level)"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
