// LAB `se-sequel-monster-fx`: THE SEQUEL'S MONSTERS' OWN ATTACKS, CLASSIC BESIDE MODERN. What only
// the sequel's monsters do (their shots draw as the player's weapons', keyed by the shot's kind):
// a parasite's draining tongue, a medic's cable on a corpse it raises, and the flash of a monster's
// gun as its shot leaves (app/monster_fx.hpp's cables and flashes, sent by the server as named line
// effects). On the window's listen server, for each kind in `--kinds` its first monster with a
// clear way to it is found on one of its maps, and the player stood before it, kept alive; a
// medic is given a corpse (the nearest other monster, killed). The run is played once to find the
// frame its attack is first sent, then again from the start, filmed from beside the line between
// them from a little before to `--seconds` after, the classic look beside the modern one: a GIF
// and a sheet of moments.
//
// With `--game=<folder>` (the folder holding the sequel's base folder) it is the player's own
// maps, for PRIVATE pictures only. Without it each cable and flash is thrown straight off
// app/monster_fx.hpp and counted: deterministic numbers the smoke run checks everywhere.
//
// ge:summary Lab: the sequel's monsters' drain, cable and gun flashes filmed, classic beside modern
// ge:keywords lab se-sequel-monster-fx sequel monsters parasite drain medic cable monster-flash
// ge:keywords classic-vs-enhanced gif side-view ce_monsters
// ge:skill shooter labs render
// ge:test tests/shooters_edge_monster_fx_test.cpp
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "brush/trace.hpp"
#include "camera/pose.hpp"
#include "camera/to_view.hpp"
#include "classicenhance/profile.hpp"
#include "classicenhance/switches.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/app/monster_fx.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace fx = shooters_edge::effects;
namespace gl = ge::lab;
namespace pf = shooters_edge::profiles;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace es = ge::lab::explosion_scene;

constexpr int kLooks = 2;
const char* const kLookNames[kLooks] = {"classic", "modern"};
constexpr double kTic = 0.013;

// A kind: its classname, the maps it is looked for on (the first with one in the open), what the
// lab calls its attack and the named effect that marks it ("te_monster_flash": any gun's).
struct Kind {
  const char* name;
  const char* classname;
  const char* maps;
  const char* attack;
  const char* effect;
  bool corpse = false;  // a medic: given a corpse to raise
};
constexpr Kind kKinds[] = {
    {"parasite", "monster_parasite", "base3,train,jail2,ware2,fact2", "draining tongue",
     "te_parasite_attack"},
    {"medic", "monster_medic", "jail2,jail3,hangar1,city1,lab", "cable on a corpse",
     "te_medic_cable_attack", true},
    {"soldier", "monster_soldier", "base1,base3,train,bunk1", "shotgun flash", "te_monster_flash"},
    {"infantry", "monster_infantry", "base1,base2,base3,train", "machinegun flash",
     "te_monster_flash"},
    {"tank", "monster_tank", "jail2,jail3,jail4,security", "rockets' flash", "te_monster_flash"},
    {"gladiator", "monster_gladiator", "jail1,jail2,mine1,mine2", "rail's flash",
     "te_monster_flash"},
};

void pack(int16_t v[3], sh::Vec3f p) {
  const float c[3] = {p.x, p.y, p.z};
  for (int k = 0; k < 3; k++) v[k] = static_cast<int16_t>(std::lround(c[k] * 8));
}

struct Setup {
  const cl::SearchPath* files = nullptr;
  const cl::GameAdapter* adapter = nullptr;
  ge::gpu::Device* device = nullptr;
  shooters_edge::look::Picture looks[kLooks];
  float range = 320;
  double before = 0.4, seconds = 2.2, wait = 10;
  uint32_t w = 400, h = 250;
  std::string map;  // --map, over each kind's own
};

struct Staged {
  cl::Listen l;
  int monster = -1;  // its entity in the server's list
  sh::Vec3f stand, at;
  sh::Vec3f shot_from, shot_to;  // the first attack's line, where the camera frames it
  float yaw = 0, pitch = 0;
};

const ge::progs::Progs& progs() {
  static const ge::progs::Progs p = [] {
    ge::progs::Progs q;
    std::string why;
    cl::baseline_progs(q, why);
    return q;
  }();
  return p;
}

// Nothing solid between two points (map units), neither of them inside a wall.
bool clear_line(const cl::Listen& l, sh::Vec3f a, sh::Vec3f b) {
  ge::brush::BrushTracer tracer(l.map->bsp.map);
  const float from[3] = {a.x, a.y, a.z}, to[3] = {b.x, b.y, b.z}, zero[3] = {0, 0, 0};
  const ge::brush::Trace t = tracer.trace(from, to, zero, zero, sh::kSqSolid);
  return t.fraction >= 1 && !t.start_solid && !t.all_solid;  // nor from inside a wall
}

// Where the player stands: `range` units off the monster along the first clear way (from the one
// it faces, turning out); his yaw toward it.
bool stand_off(const cl::Listen& l, sh::Vec3f at, float facing, float range, sh::Vec3f& out,
               float& yaw) {
  for (const int turn : {0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6}) {
    const double a = (facing + turn * 30.0) * 3.14159265358979 / 180;
    const float dx = static_cast<float>(ge::core::libm::cos(a)),
                dy = static_cast<float>(ge::core::libm::sin(a));
    const sh::Vec3f p = {at.x + dx * range, at.y + dy * range, at.z};
    bool clear = true;
    for (const float z : {0.f, 24.f, -16.f})
      clear = clear && clear_line(l, {at.x, at.y, at.z + z}, {p.x, p.y, p.z + z});
    if (!clear) continue;
    out = p;
    yaw = static_cast<float>(facing + turn * 30.0 + 180);
    return true;
  }
  return false;
}

// The player kept where he was stood, alive.
void hold(Staged& g) {
  sv::SequelServer* s = sv::sequel_of(*g.l.rules);
  sv::SequelPlayer& p = s->players[0];
  pack(p.move.origin, g.stand);
  for (int16_t& v : p.move.velocity) v = 0;
  p.body.health = 100;
}

bool stage(gl::Lab& lab, const Setup& s, const Kind& k, const std::string& map_name, Staged& g,
           bool quiet) {
  std::string error;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(*s.files, map_name, error);
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.console = {"deathmatch 0"};
  if (!map || !cl::listen_start(g.l, progs(), std::move(map), opt, error)) {
    lab.note(std::string("se-sequel-monster-fx: ") + k.name + ": " + error);
    return false;
  }
  for (int i = 0; i < 20; i++) cl::listen_frame(g.l, {});
  sv::SequelServer* sq = sv::sequel_of(*g.l.rules);
  if (sq == nullptr) return false;
  int found = 0;
  for (const sv::SequelMonster& m : sq->monsters) {
    const sv::SequelEntity& e = sq->entities[size_t(m.entity)];
    if (m.dead || m.waiting || e.classname != k.classname) continue;
    found++;
    for (const float share : {1.f, 0.75f, 0.5f})
      if (stand_off(g.l, e.origin, e.angles.y, s.range * share, g.stand, g.yaw)) {
        g.monster = m.entity;
        g.at = e.origin;
        break;
      }
    if (g.monster >= 0) break;
  }
  if (g.monster < 0) {
    if (!quiet)
      lab.note(std::string("se-sequel-monster-fx: ") + map_name + " has no " + k.name +
               " in the open (" + std::to_string(found) + " there)");
    return false;
  }
  if (k.corpse) {  // the nearest other monster, killed where it stands, for the medic to raise
    int best = -1;
    float best_d = 1e9f;
    for (const sv::SequelMonster& m : sq->monsters) {
      if (m.entity == g.monster || m.dead) continue;
      const sh::Vec3f o = sq->entities[size_t(m.entity)].origin;
      const float d = sh::length(o - g.at);
      if (d < best_d && clear_line(g.l, g.at, o)) {
        best_d = d;
        best = m.entity;
      }
    }
    if (best < 0 || best_d > 900) {
      if (!quiet)
        lab.note(std::string("se-sequel-monster-fx: no corpse near the medic on ") + map_name);
      g.monster = -1;
      return false;
    }
    const int health = sq->entities[size_t(best)].health;
    sv::map_damage(*sq, best, 0, health + 10);  // dead, not thrown apart
  }
  const float range = static_cast<float>(ge::core::hypot(g.stand.x - g.at.x, g.stand.y - g.at.y));
  g.pitch = -static_cast<float>(ge::core::libm::atan2(g.at.z - (g.stand.z + 22), range) * 180.0 /
                                3.14159265358979);
  g.l.view = {g.pitch, g.yaw, 0};
  return true;
}

// The frame (after the staging) its attack is first sent near it, or -1 within `wait` s.
int first_attack(const Setup& s, const Kind& k, Staged& g) {
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.angles = {g.pitch, g.yaw, 0};
  sv::SequelServer* sq = sv::sequel_of(*g.l.rules);
  for (int i = 0; i < int(s.wait / kTic); i++) {
    const size_t first = g.l.effects.size();
    hold(g);
    cl::listen_frame(g.l, cmd);
    const sh::Vec3f me = sq->entities[size_t(g.monster)].origin;
    for (size_t j = std::min(first, g.l.effects.size()); j < g.l.effects.size(); j++) {
      const cl::Effect& e = g.l.effects[j];
      if (e.kind != cl::EffectKind::named_trail || !e.name.starts_with(k.effect)) continue;
      if (sh::length(e.at - me) >= 96) continue;
      g.shot_from = e.at;
      g.shot_to = e.end;
      return i;
    }
  }
  return -1;
}

// Room round a point: nothing solid within `r` along each axis, so the camera is not in a wall.
bool roomy(const cl::Listen& l, sh::Vec3f c, float r) {
  for (const sh::Vec3f d : {sh::Vec3f{r, 0, 0}, sh::Vec3f{-r, 0, 0}, sh::Vec3f{0, r, 0},
                            sh::Vec3f{0, -r, 0}, sh::Vec3f{0, 0, r}, sh::Vec3f{0, 0, -r}})
    if (!clear_line(l, c, c + d)) return false;
  return true;
}

// The camera: off to one side of the attack's line (from the monster to where it ends), a little
// back and up, with room round it, looking at the line's middle; failing that, over the player's
// shoulder.
ge::camera::Pose side_camera(const Staged& g) {
  const sh::Vec3f eye = {g.stand.x, g.stand.y, g.stand.z + 22};
  const sh::Vec3f a = g.shot_from, b = g.shot_to;
  const sh::Vec3f mid = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f};
  float dx = a.x - b.x, dy = a.y - b.y;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1) {
    dx = a.x - eye.x;
    dy = a.y - eye.y;
    len = std::max(1.f, std::sqrt(dx * dx + dy * dy));
  }
  const float fx_ = dx / len, fy = dy / len, sx = -fy, sy = fx_;
  sh::Vec3f cam = {eye.x - fx_ * 24, eye.y - fy * 24, eye.z + 8};
  if (!clear_line(g.l, cam, mid)) cam = eye;
  bool found = false;
  const float reach = std::max(len, 64.f);
  for (const float side : {reach, reach * 0.6f, 192.f, 128.f, 96.f, 64.f})
    for (const float back : {0.3f, 0.0f, 0.6f})
      for (const float up : {32.f, 64.f, 8.f})
        for (const float sign : {1.f, -1.f}) {
          if (found) break;
          const sh::Vec3f c = {mid.x - fx_ * back * side + sx * side * sign,
                               mid.y - fy * back * side + sy * side * sign, mid.z + up};
          if (roomy(g.l, c, 16) && clear_line(g.l, c, a) && clear_line(g.l, c, b) &&
              clear_line(g.l, c, mid)) {
            cam = c;
            found = true;
          }
        }
  const sh::Vec3f look = found ? mid : a;
  const double ex = look.x - cam.x, ey = look.y - cam.y, ez = look.z - cam.z;
  ge::camera::Pose pose;
  pose.eye = cl::engine_point(cam);
  pose.yaw = cl::engine_heading(
      static_cast<float>(ge::core::libm::atan2(ey, ex) * 180 / 3.14159265358979));
  pose.pitch = cl::engine_pitch(static_cast<float>(
      -ge::core::libm::atan2(ez, std::sqrt(ex * ex + ey * ey)) * 180 / 3.14159265358979));
  pose.fov_y = 1.15;
  return pose;
}

void film(gl::Lab& lab, const Setup& s, const Kind& k) {
  std::string map_name;
  int shot_at = -1;
  sh::Vec3f shot_from{}, shot_to{};
  const std::string maps = s.map.empty() ? k.maps : s.map;
  for (size_t a = 0; a < maps.size() && shot_at < 0;) {
    const size_t b = std::min(maps.find(',', a), maps.size());
    map_name = maps.substr(a, b - a);
    a = b + 1;
    Staged probe;
    if (!stage(lab, s, k, map_name, probe, a < maps.size())) {
      cl::listen_stop(probe.l);
      continue;
    }
    shot_at = first_attack(s, k, probe);
    shot_from = probe.shot_from;
    shot_to = probe.shot_to;
    cl::listen_stop(probe.l);
  }
  if (shot_at < 0) {
    lab.note(std::string("se-sequel-monster-fx: no ") + k.name + " attacked within " +
             ge::core::num::to_fixed(s.wait, 0) + " s on " + maps);
    return;
  }
  Staged g;
  if (!stage(lab, s, k, map_name, g, false)) return;
  g.shot_from = shot_from;  // the run is the same again, so the probe's line is this run's
  g.shot_to = shot_to;
  std::string error;
  cl::WorldDrawPtr drawn[kLooks];
  for (int v = 0; v < kLooks; ++v) {
    drawn[v] = cl::create_world_draw(*s.device, *s.files, *g.l.map, s.w, s.h,
                                     wgpu::TextureFormat::RGBA8Unorm, s.looks[v], {}, error,
                                     {g.l.server->models, {}}, *s.adapter);
    if (!drawn[v]) {
      lab.note("se-sequel-monster-fx: no pictures: " + error);
      cl::listen_stop(g.l);
      return;
    }
  }
  ge::gpu::Target out = ge::gpu::create_target(*s.device, "se-sequel-monster-fx.final", s.w, s.h);
  cl::ViewState shown;
  cl::FxState state[kLooks];
  cl::FxFrame frame[kLooks];
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.angles = {g.pitch, g.yaw, 0};
  const ge::camera::Pose cam = side_camera(g);
  const int start = std::max(0, shot_at - int(s.before / kTic));
  const int end = shot_at + int(s.seconds / kTic);
  double t = 0, next_draw = -1;
  double births[kLooks] = {0, 0}, lights[kLooks] = {0, 0}, fires[kLooks] = {0, 0};
  std::vector<gl::Picture> gif;
  std::vector<std::vector<gl::Picture>> moments;
  for (int i = 0; i <= end; i++) {
    hold(g);
    cl::listen_frame(g.l, cmd);
    t += kTic;
    cl::frame_view(g.l, shown, 1, t, kTic);
    for (int v = 0; v < kLooks; ++v) {
      state[v].on = cl::fx_switches(s.looks[v]);
      state[v].models = &cl::world_models(*drawn[v]);
      cl::step_fx(g.l, shown, state[v], t, kTic, frame[v]);
      if (i >= start) {
        births[v] += double(frame[v].births.size());
        lights[v] += double(frame[v].lights.size());
        fires[v] += double(frame[v].flames.size());
      }
    }
    if (i < start) continue;
    if (next_draw < 0) next_draw = t;
    std::vector<gl::Picture> row;
    ge::render::FrameView view = cl::frame_view(g.l, shown, 0, t, 0);
    ge::camera::to_frame_view(cam, view);
    view.time_s = t;
    for (int v = 0; v < kLooks; ++v) {
      for (double& c : frame[v].blend) c = 0;  // seen from beside him: no hurt's flash
      for (double& c : frame[v].blend_dry) c = 0;
      cl::draw_world(*s.device, *drawn[v], view, frame[v], nullptr, kTic, {}, out.view);
      if (t + 1e-9 < next_draw) continue;
      const ge::gpu::Image img = ge::gpu::read_now(*s.device, out);
      row.push_back(gl::Picture{int(img.width), int(img.height), img.rgba});
    }
    if (row.size() != size_t(kLooks)) continue;
    next_draw += 1.0 / 25;
    std::vector<std::string> names(kLookNames, kLookNames + kLooks);
    gif.push_back(es::sheet_of(row, names, kLooks));
    moments.push_back(row);
  }
  cl::listen_stop(g.l);
  for (int v = 0; v < kLooks; ++v) {
    std::string key = k.name;
    key += v == 0 ? ".classic" : ".modern";
    lab.metric(key + ".specks", births[v], "count", gl::Det::measured);
    lab.metric(key + ".lights", lights[v], "count", gl::Det::measured);
    lab.metric(key + ".fires", fires[v], "count", gl::Det::measured);
  }
  if (gif.empty()) return;
  std::vector<gl::Picture> sheet;
  std::vector<std::string> names;
  const size_t n = moments.size();
  const auto lead = size_t(s.before * 25);
  for (const size_t after :
       {size_t(0), size_t(2), size_t(5), size_t(15)}) {  // frames at 25 a second
    const size_t i = std::min(n - 1, lead + after);
    for (int v = 0; v < kLooks; ++v) {
      sheet.push_back(moments[i][size_t(v)]);
      names.push_back(std::string(kLookNames[v]) + "  " +
                      ge::core::num::to_fixed(double(i) / 25 - s.before, 2) + " s");
    }
  }
  std::string caption = "The ";
  caption += k.name;
  caption += "'s " + std::string(k.attack) + " on " + map_name +
             ", from beside the line between it and the player: the classic look beside the "
             "modern look, at moments after it begins.";
  lab.shot({std::string(k.name) + ".sheet",
            std::string(k.name) + ": " + k.attack,
            caption,
            "the cable or the flash, its light on the walls",
            {{"map", map_name}}},
           es::sheet_of(sheet, names, kLooks), gl::Det::measured);
  lab.attach(std::string(k.name) + ".gif", gl::encode_gif_frames(gif, 4), gl::Det::measured);
}

// Without a game folder: each cable and flash thrown straight off monster_fx, counted.
void numbers(gl::Lab& lab) {
  const double from[3] = {0, 1, 0}, to[3] = {3, 1, 0}, dir[3] = {1, 0, 0};
  for (const fx::Cable c : {fx::Cable::drain, fx::Cable::heal}) {
    const std::string name = c == fx::Cable::drain ? "drain" : "heal";
    fx::Particles classic, modern;
    fx::classic_cable(c, from, to, 1, &classic);
    fx::monster_cable(c, from, to, 1, 0.25, &modern);
    lab.metric(name + ".classic.specks", double(classic.size()), "count");
    lab.metric(name + ".modern.specks", double(modern.size()), "count");
  }
  for (const char* id : {"blaster", "gun", "rocket", "rail", "bfg"}) {
    const fx::Flash f = fx::flash_of(std::string("te_monster_flash.") + id);
    fx::Particles sparks;
    fx::flash_sparks(f, from, dir, 1, &sparks);
    ge::render::MuzzleFire fire;
    std::string key = "flash.";
    key += id;
    lab.metric(key + ".sparks", double(sparks.size()), "count");
    lab.metric(key + ".fire", fx::flash_fire(f, from, dir, 1, 0, &fire) ? 1 : 0, "count");
  }
}

void run(gl::Lab& lab) {
  if (lab.arg("game").empty()) {
    numbers(lab);
    return;
  }
  std::string error;
  const pf::GameProfile& game = *pf::find_game("sequel");
  const ge::classicenhance::Profile& content = pf::content_of(game);
  cl::SearchPath files;
  if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content)) {
    lab.fail(error, "Pass --game=<the folder holding " + std::string(content.base_dir) +
                        "/pak0.pak>. Skill: shooter.");
    return;
  }
  auto device = ge::gpu::create_device({.label = "se-sequel-monster-fx", .validation = 1});
  if (!device) {
    lab.note("No GPU adapter on this machine: no pictures (install Mesa's Vulkan drivers).");
    return;
  }
  Setup s;
  s.files = &files;
  s.adapter = &cl::adapter_of(game);
  s.device = device.get();
  s.map = lab.arg("map");
  s.range = static_cast<float>(lab.arg_num("range"));
  s.seconds = lab.arg_num("seconds");
  s.wait = lab.arg_num("wait");
  if (const auto size = lab.arg_nums("size"); size.size() == 2 && size[0] >= 64 && size[1] >= 64) {
    s.w = static_cast<uint32_t>(size[0]);
    s.h = static_cast<uint32_t>(size[1]);
  }
  for (int v = 0; v < kLooks; ++v) {
    s.looks[v].look = kLookNames[v];
    if (v == 0)
      for (const ge::classicenhance::SwitchSpec& sw : ge::classicenhance::switches())
        s.looks[v].ce.*sw.field = false;
  }
  const std::string list = lab.arg("kinds");
  for (size_t a = 0; a < list.size();) {
    const size_t b = std::min(list.find(',', a), list.size());
    const std::string name = list.substr(a, b - a);
    a = b + 1;
    const Kind* k = nullptr;
    for (const Kind& c : kKinds)
      if (name == c.name) k = &c;
    if (k == nullptr) {
      lab.note("se-sequel-monster-fx: no kind " + name +
               " (parasite, medic, soldier, infantry, tank, gladiator)");
      continue;
    }
    film(lab, s, *k);
  }
}

const gl::LabSpec kSpec{
    "se-sequel-monster-fx",
    gl::LabKind::picture,
    "game:shooters_edge",
    "the sequel's monsters' own attacks in the classic and the modern look (app/monster_fx.hpp's "
    "cables and flashes, client/world_fx.cpp, the server's named line effects)",
    "shooter",
    "Do the sequel's monster-only attacks read as modern effects? A parasite's drain, a medic's "
    "cable and the monsters' gun flashes filmed from the side on real maps, classic beside modern.",
    {
        {"game", "", "the folder holding the sequel's base folder; empty: the numbers alone"},
        {"map", "", "with --game: the map for every kind; empty: each kind's own"},
        {"kinds", "parasite,medic,soldier,infantry,tank,gladiator",
         "with --game: the monsters by name"},
        {"range", "320", "units from the monster the player stands"},
        {"seconds", "2.2", "seconds filmed after the attack begins"},
        {"wait", "10", "seconds the monster is given to attack"},
        {"size", "400,250", "with --game: each look's width and height in pixels"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
