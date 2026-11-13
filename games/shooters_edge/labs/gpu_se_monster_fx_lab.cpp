// LAB `se-monster-fx`: THE MONSTERS' ATTACKS, CLASSIC BESIDE THE MODERN LOOK. A campaign
// on the listen server (client/listen.hpp) with the packs' own single-player program, on a map the
// kind lives on. For each kind in `--kinds` its first monster with a clear way to it is found, and
// the player stood in front of it, unhurt (god mode) but seen, so it wakes and attacks. The run is
// played once to find the moment its first shot leaves (a monster's missile appears, or a beam or
// a grenade), then again from the start, filmed from beside the line between them from a little
// before the shot to `--seconds` after it, every look side by side in one picture:
//
// - CLASSIC: the classic look (the classic specks of client/world_fx.hpp and app/monster_fx.hpp's
//   `classic_tracer`).
// - MODERN: the modern look with ce_monsters on (app/monster_fx.hpp; chosen from two variants
//   filmed this way side by side, the other dropped).
//
// Each kind gives a GIF of the two side by side, a sheet of moments, and the specks and lights
// each look threw. With `--game=<base>/<folder>` (a game folder of packs) it is the player's own
// maps, for PRIVATE pictures only. Without it the lab throws each kind's trail, glow and splash
// straight off app/monster_fx.hpp in each look and counts them: deterministic numbers that the
// smoke run checks everywhere.
//
// ge:summary Lab: each monster's attack filmed classic beside the modern look
// ge:keywords lab se-monster-fx monster-fx monsters attacks missile acid flame void lava laser
// ge:keywords flesh classic-vs-enhanced gif side-view ce_monsters
// ge:skill shooter labs render
// ge:test tests/shooters_edge_monster_fx_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "camera/pose.hpp"
#include "camera/to_view.hpp"
#include "console/console.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "progs/file.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/app/monster_fx.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/classic_game.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace fx = shooters_edge::effects;
namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace es = ge::lab::explosion_scene;

constexpr float kFlMonster = 32, kFlGod = 64;  // the program's `flags` bits
constexpr int kLooks = 2;
const char* const kLookNames[kLooks] = {"classic", "modern"};

// A kind: its model's name (progs/<model>.mdl), the maps it is looked for on by default (the first
// that has one in the open at the game's skill), and the shot it throws as the lab names it.
struct Kind {
  const char* name;
  const char* model;
  const char* map;
  const char* shot;
  const char* shot_model;  // its shot's model ("": a beam or its gun's flash)
  float range = 0;         // units the player stands off (0: --range)
};
constexpr Kind kKinds[] = {
    {"wizard", "wizard", "e1m3,e1m4,e1m5,e2m4,e3m5", "acid spit", "progs/w_spike.mdl"},
    {"hknight", "hknight", "e2m3,e2m5,e3m3,e3m6", "fire bolts", "progs/k_spike.mdl"},
    {"shalrath", "shalrath", "e2m6,e3m6,e4m6,e4m7", "void ball", "progs/v_spike.mdl"},
    {"enforcer", "enforcer", "e2m1,e3m1,e4m1", "laser", "progs/laser.mdl"},
    {"soldier", "soldier", "e1m1,e1m2", "shotgun", ""},
    {"zombie", "zombie", "e1m3,e2m6,e4m3", "flesh", "progs/zom_gib.mdl", 160},
    {"ogre", "ogre", "e1m2,e1m3,e1m4", "grenade", "progs/grenade.mdl"},
    {"shambler", "shambler", "e1m3,e1m5,e2m6,e1m8", "lightning", ""},
    {"boss", "boss", "e1m7", "lava ball", "progs/lavaball.mdl"},
};

std::string model_of(const cl::Listen& l, int e) {
  const sh::Edict& ed = l.server->sv.edicts[size_t(e)];
  if (ed.free || ed.modelindex <= 0 || ed.modelindex >= int(l.server->models.size())) return "";
  return l.server->models[size_t(ed.modelindex)];
}

std::vector<int> monsters_of(const cl::Listen& l, const std::string& model) {
  std::vector<int> out;
  for (int e = sh::kMaxClients + 1; e < l.server->sv.num_edicts; e++) {
    const sh::Edict& ed = l.server->sv.edicts[size_t(e)];
    if (ed.free) continue;
    const int flags = static_cast<int>(cl::field_named(l, e, "flags"));
    if ((flags & static_cast<int>(kFlMonster)) == 0) continue;
    if (cl::field_named(l, e, "health") <= 0) continue;
    if (model_of(l, e) == "progs/" + model + ".mdl") out.push_back(e);
  }
  return out;
}

bool open_at(const cl::Listen& l, float x, float y, float z) {
  const float q[3] = {x, y, z};
  return br::hull_contents(l.map->bsp.map, q, 0, 0) != -2;  // not solid
}

// Where the player stands: `range` units off the monster along the first way (from the one it
// faces, turning out) with nothing solid on the line; his yaw toward it.
bool stand_off(const cl::Listen& l, sh::Vec3f at, float facing, float range, sh::Vec3f& out,
               float& yaw) {
  for (const int turn : {0, 1, -1, 2, -2, 3, -3, 4}) {
    const double a = (facing + turn * 30.0) * 3.14159265358979 / 180;
    const float dx = static_cast<float>(ge::core::libm::cos(a)),
                dy = static_cast<float>(ge::core::libm::sin(a));
    bool clear = true;
    for (float s = 24; s <= range + 24 && clear; s += 8)
      for (const float z : {0.f, 24.f, -16.f})
        clear = clear && open_at(l, at.x + dx * s, at.y + dy * s, at.z + z);
    if (!clear) continue;
    out = {at.x + dx * range, at.y + dy * range, at.z};
    yaw = static_cast<float>(facing + turn * 30.0 + 180);
    return true;
  }
  return false;
}

struct Setup {
  const cl::SearchPath* files = nullptr;
  const ge::progs::Progs* progs = nullptr;
  float range = 256;
  double before = 0.4, seconds = 2.2, wait = 8;
  ge::gpu::Device* device = nullptr;
  std::vector<uint8_t> palette;
  shooters_edge::look::Picture looks[kLooks];
  uint32_t w = 400, h = 250;
  std::string map;  // --map, over each kind's own
};

struct Staged {
  cl::Listen l;
  int monster = -1;
  sh::Vec3f stand, at;
  float yaw = 0, pitch = 0;
};

// The map started, the monster found and the player stood before it; false (noted) when none is.
bool stage(gl::Lab& lab, const Setup& s, const Kind& k, const std::string& map_name, Staged& g,
           bool quiet) {
  std::string error;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(*s.files, map_name, error);
  cl::ListenOptions lo = shooters_edge::net::listen_options();
  lo.console.emplace_back("deathmatch 0");
  if (!map || !cl::listen_start(g.l, *s.progs, std::move(map), lo, error)) {
    lab.note(std::string("se-monster-fx: ") + k.name + ": " + error);
    return false;
  }
  for (int i = 0; i < 40; i++) cl::listen_frame(g.l, {});
  for (const int e : monsters_of(g.l, k.model)) {
    const sh::Edict& ed = g.l.server->sv.edicts[size_t(e)];
    for (const float share : {1.f, 0.75f, 0.5f, 0.35f})
      if (stand_off(g.l, ed.origin, ed.angles.y, (k.range > 0 ? k.range : s.range) * share, g.stand,
                    g.yaw)) {
        g.monster = e;
        g.at = ed.origin;
        break;
      }
    if (g.monster >= 0) break;
  }
  if (g.monster < 0) {
    if (quiet) return false;
    lab.note(std::string("se-monster-fx: ") + map_name + " has no " + k.name + " in the open (" +
             std::to_string(monsters_of(g.l, k.model).size()) + " there)");
    return false;
  }
  const float range = static_cast<float>(ge::core::hypot(g.stand.x - g.at.x, g.stand.y - g.at.y));
  g.pitch = -static_cast<float>(ge::core::libm::atan2(g.at.z - (g.stand.z + 22), range) * 180.0 /
                                3.14159265358979);
  g.l.view = {g.pitch, g.yaw, 0};
  return true;
}

void hold(Staged& g) {
  sh::Edict& me = g.l.server->sv.edicts[1];
  me.flags |= static_cast<int>(kFlGod);
  me.origin = g.stand;
  me.velocity = {};
  cl::set_field_named(g.l, 1, "health", 100);
}

// The first frame (after the staging) a shot of the monster's is out, or -1 within `wait` s.
int first_shot(const Setup& s, const Kind& k, Staged& g) {
  const double tic = cl::listen_tic(g.l);
  sh::MoveCmd cmd;
  cmd.msec = static_cast<uint8_t>(std::lround(tic * 1000));
  cmd.angles = {g.pitch, g.yaw, 0};
  const size_t effects0 = g.l.effects.size();
  for (int i = 0; i < int(s.wait / tic); i++) {
    hold(g);
    cl::listen_frame(g.l, cmd);
    // Its own shot: one that appears near it (another monster's does not count).
    const sh::Vec3f me = g.l.server->sv.edicts[size_t(g.monster)].origin;
    for (int e = sh::kMaxClients + 1; e < g.l.server->sv.num_edicts; e++) {
      if (model_of(g.l, e) != k.shot_model) continue;
      const sh::Vec3f p = g.l.server->sv.edicts[size_t(e)].origin;
      if (ge::core::hypot3(p.x - me.x, p.y - me.y, p.z - me.z) < 96) return i;
    }
    if (k.shot_model[0] == 0 && (int(cl::field_named(g.l, g.monster, "effects")) & 2) != 0)
      return i;  // a gun's flash
    for (size_t j = effects0; j < g.l.effects.size(); j++)
      if (g.l.effects[j].kind == cl::EffectKind::beam && g.l.effects[j].entity == g.monster)
        return i;
  }
  return -1;
}

// Nothing solid between two points (map units).
bool clear_line(const cl::Listen& l, sh::Vec3f a, sh::Vec3f b) {
  const float len =
      std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z));
  const int n = std::max(2, int(len / 4));  // a step of 4 units: no wall is thinner
  for (int i = 0; i <= n; i++) {
    const float u = float(i) / float(n);
    if (!open_at(l, a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u))
      return false;
  }
  return true;
}

// The camera: off to one side of the line between the two (as far as the room lets), a little
// back from the player and up, seeing both, looking at a point a third of the way from him to the
// monster, so the shot crosses the picture and lands short of the camera.
ge::camera::Pose side_camera(const Staged& g) {
  const sh::Vec3f eye = {g.stand.x, g.stand.y, g.stand.z + 22};
  const sh::Vec3f aim = {g.at.x, g.at.y, g.at.z + 8};
  const float dx = aim.x - eye.x, dy = aim.y - eye.y;
  const float len = std::max(1.f, std::sqrt(dx * dx + dy * dy));
  const float fx_ = dx / len, fy = dy / len, sx = -fy, sy = fx_;
  // None clear: over his shoulder, at his eye.
  sh::Vec3f cam = {eye.x - fx_ * 8, eye.y - fy * 8, eye.z + 4};
  if (!clear_line(g.l, cam, aim)) cam = eye;
  bool found = false;
  for (const float side : {160.f, 128.f, 96.f, 72.f, 48.f, 24.f})
    for (const float back : {48.f, 16.f, 96.f, -32.f})
      for (const float sign : {1.f, -1.f}) {
        if (found) break;
        const sh::Vec3f c = {eye.x - fx_ * back + sx * side * sign,
                             eye.y - fy * back + sy * side * sign, eye.z + 24};
        if (clear_line(g.l, c, eye) && clear_line(g.l, c, aim)) {
          cam = c;
          found = true;
        }
      }
  const sh::Vec3f look = {eye.x + (aim.x - eye.x) * 0.4f, eye.y + (aim.y - eye.y) * 0.4f,
                          eye.z + (aim.z - eye.z) * 0.4f};
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
  // The first of its maps where one stands in the open and attacks.
  std::string map_name;
  int shot_at = -1;
  const std::string maps = s.map.empty() ? k.map : s.map;
  for (size_t a = 0; a < maps.size() && shot_at < 0;) {
    const size_t b = std::min(maps.find(',', a), maps.size());
    map_name = maps.substr(a, b - a);
    a = b + 1;
    const bool last = a >= maps.size();
    Staged probe;
    if (!stage(lab, s, k, map_name, probe, !last)) continue;
    shot_at = first_shot(s, k, probe);
  }
  if (shot_at < 0) {
    lab.note(std::string("se-monster-fx: no ") + k.name + " attacked within " +
             ge::core::num::to_fixed(s.wait, 0) + " s on " + maps);
    return;
  }
  Staged g;
  if (!stage(lab, s, k, map_name, g, false)) return;
  const double tic = cl::listen_tic(g.l);
  std::string error;
  cl::WorldDrawPtr drawn[kLooks];
  ge::gpu::Target out;
  for (int v = 0; v < kLooks && s.device != nullptr; ++v) {
    drawn[v] = cl::create_world_draw(*s.device, *s.files, *g.l.map, s.w, s.h,
                                     wgpu::TextureFormat::RGBA8Unorm, s.looks[v], {}, error,
                                     {g.l.server->models, {}});
    if (!drawn[v]) {
      lab.note("se-monster-fx: no pictures: " + error);
      return;
    }
  }
  if (s.device != nullptr) out = ge::gpu::create_target(*s.device, "se-monster-fx.final", s.w, s.h);
  cl::ViewState shown;
  cl::FxState state[kLooks];
  cl::FxFrame frame[kLooks];
  for (int v = 0; v < kLooks; ++v) state[v].palette = s.palette;
  sh::MoveCmd cmd;
  cmd.msec = static_cast<uint8_t>(std::lround(tic * 1000));
  cmd.angles = {g.pitch, g.yaw, 0};
  const ge::camera::Pose cam = side_camera(g);
  const int start = std::max(0, shot_at - int(s.before / tic));
  const int end = shot_at + int(s.seconds / tic);
  double t = 0, next_draw = -1;
  double births[kLooks] = {0, 0}, lights[kLooks] = {0, 0};
  std::vector<gl::Picture> gif;
  std::vector<std::vector<gl::Picture>> moments;
  for (int i = 0; i <= end; i++) {
    hold(g);
    cl::listen_frame(g.l, cmd);
    t += tic;
    cl::frame_view(g.l, shown, 1, t, tic);
    for (int v = 0; v < kLooks; ++v) {
      state[v].on = cl::fx_switches(s.looks[v]);
      state[v].models = drawn[v] ? &cl::world_models(*drawn[v]) : nullptr;
      cl::step_fx(g.l, shown, state[v], t, tic, frame[v]);
      if (i >= start) {
        births[v] += double(frame[v].births.size());
        lights[v] += double(frame[v].lights.size());
      }
    }
    if (i < start || s.device == nullptr) continue;
    if (next_draw < 0) next_draw = t;
    // Every look drawn each frame so its particles fly; kept at 25 frames a second.
    std::vector<gl::Picture> row;
    ge::render::FrameView view = cl::frame_view(g.l, shown, 0, t, 0);
    ge::camera::to_frame_view(cam, view);
    view.time_s = t;
    for (int v = 0; v < kLooks; ++v) {
      // Seen from beside him, not through his eyes: no hurt's flash on the picture.
      for (double& c : frame[v].blend) c = 0;
      for (double& c : frame[v].blend_dry) c = 0;
      cl::draw_world(*s.device, *drawn[v], view, frame[v], nullptr, tic, {}, out.view);
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
  const std::string id = k.name;
  for (int v = 0; v < kLooks; ++v) {
    std::string look = id;
    look += v == 0 ? ".classic" : ".modern";
    lab.metric(look + ".specks", births[v], "count", gl::Det::measured);
    lab.metric(look + ".lights", lights[v], "count", gl::Det::measured);
  }
  if (gif.empty()) return;
  // The sheet: four moments, each look a column.
  std::vector<gl::Picture> sheet;
  std::vector<std::string> names;
  const size_t n = moments.size();
  for (size_t m = 0; m < 4; ++m) {
    const size_t i =
        std::min(n - 1, size_t(s.before * 25) + m * (n - 1 - size_t(s.before * 25)) / 4);
    for (int v = 0; v < kLooks; ++v) {
      sheet.push_back(moments[i][size_t(v)]);
      names.push_back(std::string(kLookNames[v]) + "  " +
                      ge::core::num::to_fixed(double(i) / 25 - s.before, 2) + " s");
    }
  }
  lab.shot({id + ".sheet",
            std::string(k.name) + ": " + k.shot,
            "The " + std::string(k.name) + "'s " + k.shot + " on " + map_name +
                ", from beside the line of fire: the classic look beside the modern "
                "look, at moments after the shot leaves.",
            "the shot's trail, its light on the walls and the splash where it lands",
            {{"map", map_name}}},
           es::sheet_of(sheet, names, kLooks), gl::Det::measured);
  lab.attach(id + ".gif", gl::encode_gif_frames(gif, 4), gl::Det::measured);
}

// Without a game folder: each kind's trail, glow and splash thrown straight off monster_fx in
// every look, counted (deterministic).
void numbers(gl::Lab& lab) {
  gl::LabTable table{"Each kind's specks over a 2 m flight and its splash, by look",
                     {"kind", "classic", "modern", "glow (modern)"},
                     {}};
  for (const fx::MonsterShot k :
       {fx::MonsterShot::acid, fx::MonsterShot::flame, fx::MonsterShot::void_ball,
        fx::MonsterShot::lava, fx::MonsterShot::laser, fx::MonsterShot::flesh}) {
    const std::string name = fx::monster_shot_name(k);
    int counts[kLooks] = {0, 0};
    for (int v = 0; v < kLooks; ++v) {
      fx::Particles out;
      for (int step = 0; step < 20; ++step) {
        const double from[3] = {0.1 * step, 1, 0}, to[3] = {0.1 * (step + 1), 1, 0};
        if (v == 0)
          fx::classic_tracer(k, from, to, step, uint32_t(step + 1), {}, &out);
        else
          fx::monster_trail(k, from, to, uint32_t(step + 1), &out);
      }
      if (v > 0) {
        const double p[3] = {2, 1, 0}, n[3] = {-1, 0, 0}, d[3] = {1, 0, 0};
        std::vector<ge::render::Blast> blasts;
        fx::monster_splash(k, p, n, d, 0, 7, 0, &blasts, &out);
      }
      counts[v] = int(out.size());
      lab.metric(name + "." + (v == 0 ? "classic" : "modern") + ".specks", counts[v], "count");
    }
    fx::Light light;
    const double p[3] = {0, 1, 0}, d[3] = {1, 0, 0};
    const bool glows = fx::monster_glow(k, p, d, 1, 0, &light);
    table.rows.push_back({name, std::to_string(counts[0]), std::to_string(counts[1]),
                          glows ? ge::core::num::to_fixed(light.radius, 1) + " m" : "none"});
  }
  lab.table(table);
}

void run(gl::Lab& lab) {
  const std::string folder = lab.arg("game");
  if (folder.empty()) {
    numbers(lab);
    return;
  }
  std::string error;
  cl::SearchPath files;
  const size_t cut = folder.find_last_of('/');
  const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
  const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
  if (!cl::open_game_dir(base, game, files, error)) {
    lab.fail("se-monster-fx: " + error, "Pass --game=<base>/<game folder>. Skill: shooter.");
    return;
  }
  ge::progs::Progs progs;
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes = cl::find_file(files, "progs.dat", store);
    if (!ge::progs::load_progs(bytes.data(), bytes.size(), progs, error, ge::progs::every_form())) {
      lab.fail("se-monster-fx: the pack's progs.dat: " + error,
               "Pass a game folder whose packs hold a progs.dat. Skill: progs.");
      return;
    }
  }
  Setup s;
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = cl::find_file(files, "gfx/palette.lmp", store);
    s.palette.assign(pal.begin(), pal.end());
  }
  s.files = &files;
  s.progs = &progs;
  s.map = lab.arg("map");
  s.range = static_cast<float>(lab.arg_num("range"));
  s.seconds = lab.arg_num("seconds");
  s.wait = lab.arg_num("wait");
  if (const auto size = lab.arg_nums("size"); size.size() == 2 && size[0] >= 64 && size[1] >= 64) {
    s.w = static_cast<uint32_t>(size[0]);
    s.h = static_cast<uint32_t>(size[1]);
  }
  std::unique_ptr<ge::gpu::Device> device =
      ge::gpu::create_device({.label = "se-monster-fx", .validation = 1});
  if (!device) lab.note("no GPU device: no pictures (install Mesa's Vulkan drivers; skill: gpu)");
  s.device = device.get();
  for (int v = 0; v < kLooks; ++v) {
    ge::console::Console con;
    shooters_edge::look::add_cvars(con, s.looks[v]);
    if (v == 0) con.execute("r_look classic");
    con.execute(lab.arg("cvars"));
    con.execute(lab.cvars());
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
      lab.note("se-monster-fx: no kind " + name +
               " (wizard, hknight, shalrath, enforcer, soldier, "
               "zombie, ogre, shambler, boss)");
      continue;
    }
    film(lab, s, *k);
  }
}

const gl::LabSpec kSpec{
    "se-monster-fx",
    gl::LabKind::picture,
    "game:shooters_edge",
    "the monsters' missiles, spit and splashes in the classic and the modern look "
    "(app/monster_fx.hpp, client/world_fx.hpp's monster_missiles, ce_monsters)",
    "shooter",
    "Do the monsters' attacks read as modern effects? Each kind's first shot filmed from the side "
    "on a real map, the classic look beside the modern look.",
    {
        {"game", "", "a game folder (<base>/<folder>); empty: each kind's numbers alone"},
        {"map", "", "with --game: the map for every kind; empty: each kind's own"},
        {"kinds", "wizard,hknight,shalrath,enforcer,soldier,zombie,ogre,shambler",
         "with --game: the monsters by name"},
        {"range", "256", "units from the monster the player stands"},
        {"seconds", "2.2", "seconds filmed after the shot"},
        {"wait", "8", "seconds the monster is given to attack"},
        {"size", "400,250", "with --game: each look's width and height in pixels"},
        {"cvars", "", "with --game: picture cvars for every look (r_look ...)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
