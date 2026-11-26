// LAB `se-q2-coop-sweep`: QUAKE II'S CAMPAIGN IN CO-OP, EVERY MAP. Two players (the window's own
// and a companion seated by `addbot`) play each map of the campaign in co-op on the window's listen
// server, the way NEW GAME's COOPERATIVE starts it: both come in, the companion dies and comes
// back, leaves and joins again, each player takes every key lying on the map, the player walks into
// an exit, and the window enters the level it names (`listen_change`) with both carried. The maps
// are found by their exits, from `--first` on (every `target_changelevel` a trigger fires), so
// the sweep follows the campaign's own graph, units and returns alike, to its end.
//
// The table: a map a row: both in, the death and the return, the leave and the rejoin, the keys
// both took of those lying out, the exit taken and whether both came into the next level alive.
// Pictures: each map from above (its floors, the exits in orange, the keys in yellow, where each
// player came in), drawn by the live map's plate (livemap/live_map.hpp: the map's own textures
// under its light, the liquids over them), the same picture TV and recordings show from above;
// the plate's library builds with the GPU, so this lab does too, though it draws on the CPU.
//
// Without `--game` the maps are our own: two halls of brushes, each with an exit and a key, in a
// pack, so CI needs no game data and the table is a digest. With `--game=<folder>` (the folder
// holding Quake II's base folder) the maps are its packs', for PRIVATE pictures only.
// `--mod=<folder>` (xatrix, rogue) sweeps a mission pack's campaign on the pack's rules, from its
// own first map.
//
// ge:summary Lab: Quake II's campaign in co-op, every map: join, death, rejoin, keys, exit, carry
// ge:keywords lab se-q2-coop-sweep q2 coop co-op playthrough sweep keys exit unit carried
// ge:keywords companion addbot removebot respawn target-changelevel mission-pack xatrix func-button
// ge:skill shooter labs
// ge:test tests/shooters_edge_listen_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/livemap/live_map.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/q2_campaign.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_native_host.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_wire.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace lv = shooters_edge::livemap;

// A floored hall walled round, a start, two co-op starts, a key, and an exit (a trigger in the
// far corner firing a `target_changelevel` to `next`).
bool hall(float w, const std::string& next, std::vector<uint8_t>& out, std::string& error) {
  br::BrushWorld world;
  world.models.resize(2);
  const auto box = [&](int m, float x0, float y0, float z0, float x1, float y1, float z1,
                       uint32_t contents, const char* tex) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[size_t(m)].push_back(br::box_brush(lo, hi, contents, tex));
  };
  box(0, -w, -512, -16, w, 512, 0, sh::kSqSolid, "world/wall");
  box(0, -w - 16, -512, -16, -w, 512, 256, sh::kSqSolid, "world/wall");
  box(0, w, -512, -16, w + 16, 512, 256, sh::kSqSolid, "world/wall");
  box(0, -w - 16, 512, -16, w + 16, 528, 256, sh::kSqSolid, "world/wall");
  box(0, -w - 16, -528, -16, w + 16, -512, 256, sh::kSqSolid, "world/wall");
  box(1, w - 96, 352, 0, w - 16, 496, 96, sh::kSqSolid, "common/trigger");
  const auto put = [&](std::vector<std::pair<std::string, std::string>> pairs) {
    br::Entity e;
    e.pairs = std::move(pairs);
    world.entities.push_back(e);
  };
  put({{"classname", "info_player_start"}, {"origin", "0 0 24"}, {"angle", "90"}});
  put({{"classname", "info_player_coop"}, {"origin", "96 0 24"}, {"angle", "90"}});
  put({{"classname", "info_player_coop"}, {"origin", "-96 0 24"}, {"angle", "90"}});
  put({{"classname", "key_blue_key"}, {"origin", "-256 -256 24"}});
  put({{"classname", "trigger_multiple"}, {"model", "*1"}, {"target", "out"}});
  put({{"classname", "target_changelevel"}, {"targetname", "out"}, {"map", next}});
  return br::write_brush_world(world, &out, &error);
}

// An exit: the trigger's box centre, the level string it names.
struct Exit {
  sh::Vec3f at;
  std::string level;
  float yaw = 0;  // a trigger with an angle is taken only facing that way
  // A button that fires the changelevel (The Reckoning's xswamp ends its unit so): pressed by
  // walking into it from each side in turn, `half` its box's half size.
  bool button = false;
  sh::Vec3f half;
};

struct MapFacts {
  std::vector<Exit> exits;
  std::vector<std::pair<std::string, sh::Vec3f>> keys;  // lying out at the start
};

sh::Vec3f origin_of(const br::Entity& e) {
  sh::Vec3f v;
  std::sscanf(e.get("origin").c_str(), "%f %f %f", &v.x, &v.y, &v.z);
  return v;
}

// The exits and keys a map's entities hold.
MapFacts facts_of(const br::Map& map) {
  MapFacts f;
  std::map<std::string, std::string> levels;  // a changelevel's name to its level string
  for (const br::Entity& e : map.entities)
    if (e.get("classname") == "target_changelevel" && !e.get("targetname").empty())
      levels[e.get("targetname")] = e.get("map");
  for (const br::Entity& e : map.entities) {
    const std::string kind = e.get("classname");
    if (kind.rfind("key_", 0) == 0 && e.get("targetname").empty())
      f.keys.emplace_back(kind, origin_of(e));
    const bool button = kind == "func_button";
    if (kind.rfind("trigger_", 0) != 0 && !button) continue;
    const auto it = levels.find(e.get("target"));
    const std::string model = e.get("model");
    if (it == levels.end() || model.size() < 2 || model[0] != '*') continue;
    const size_t m = size_t(std::atoi(model.c_str() + 1));
    if (m >= map.models.size()) continue;
    const br::Model& b = map.models[m];
    f.exits.push_back({{(b.mins[0] + b.maxs[0]) / 2, (b.mins[1] + b.maxs[1]) / 2, b.mins[2] + 24},
                       it->second,
                       static_cast<float>(std::atof(e.get("angle").c_str())),
                       false,
                       {}});
    if (button) {
      Exit& x = f.exits.back();
      x.button = true;
      x.half = {(b.maxs[0] - b.mins[0]) / 2, (b.maxs[1] - b.mins[1]) / 2, 0};
      x.at.z = b.mins[2] + 8;  // standing at the button's foot, a little in from its bottom
    }
  }
  return f;
}

struct Row {
  std::string map;
  bool both_in = false, died = false, back = false, left = false, rejoined = false;
  int keys = 0, keys_taken = 0;  // lying out; taken by both
  int keys_gone = 0;             // the map's, taken away by the rules at the start
  std::string exit, next;        // the level string taken; the map entered
  bool carried = false;          // both in the next level, alive
  bool ended = false;            // the exit was the campaign's end, its show played
  std::string problem;
  sh::Vec3f came_in[2];
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

sh::Vec3f where(const sv::Q2Server& s, int slot) {
  const auto& o = s.players[size_t(slot)].move.origin;
  return {float(o[0]) * 0.125f, float(o[1]) * 0.125f, float(o[2]) * 0.125f};
}

void put_at(sv::Q2Server& s, int slot, sh::Vec3f at) {
  auto& m = s.players[size_t(slot)].move;
  m.origin[0] = static_cast<int16_t>(at.x * 8);
  m.origin[1] = static_cast<int16_t>(at.y * 8);
  m.origin[2] = static_cast<int16_t>(at.z * 8);
  for (int32_t& v : m.velocity) v = 0;
}

bool alive(const cl::Listen& l, int slot) {
  const sv::Q2Server* s = sv::q2_of(*l.rules);
  return s != nullptr && l.server->clients[size_t(slot)].active && s->players[size_t(slot)].alive;
}

void frames(cl::Listen& l, int n) {
  for (int i = 0; i < n && l.server != nullptr; i++) cl::listen_frame(l, {});
}

// One map played in co-op from a new game: the steps of the row, then out by `exit`.
Row sweep(const cl::SearchPath& files, const std::string& name, const MapFacts& facts,
          const Exit* exit, bool pack, const std::string& enter) {
  Row r;
  r.map = name;
  std::string error;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, name, error);
  if (!map) {
    r.problem = error;
    return r;
  }
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.console = {"deathmatch 0", "coop 1"};
  if (pack) opt.files = &files;  // a mission pack's folder plays its rules (`q2_pack_of_folder`)
  cl::Listen l;
  if (!cl::listen_start(l, progs(), std::move(map), opt, error)) {
    r.problem = error;
    return r;
  }
  frames(l, 30);
  sv::Q2Server* s = sv::q2_of(*l.rules);
  if (s == nullptr) {
    r.problem = "not on Quake II's rules";
    return r;
  }
  if (pack && s->opt.pack == sv::Q2Pack::base) {
    r.problem = "on the base game's rules, not the pack's (the folder " + files.folder + ")";
    return r;
  }
  // The level string the campaign comes in by (`xship$xhangar2`): its spot's name, so the companion
  // takes the co-op spots of that name as it would coming in from there (a map whose only start
  // has no such spots near it would put it on the first player's head).
  if (!enter.empty()) s->spawn_name = sv::level_string(enter).spawn;
  cl::listen_lobby(l, "addbot expert");
  frames(l, 20);
  r.both_in = alive(l, 0) && alive(l, 1);
  // The companion dies and comes back (a button pressed, as a bot does). `kill` is refused in
  // the first 5 s after a spawn, so it is asked again until it takes.
  for (int i = 0; i < 1200 && !r.died; i++) {
    if (i % 72 == 0) l.rules->client_command(1, "kill");
    frames(l, 1);
    r.died = !alive(l, 1);
  }
  for (int i = 0; i < 1200 && !alive(l, 1); i++) frames(l, 1);
  r.back = alive(l, 1);
  // It leaves and joins again.
  cl::listen_lobby(l, "removebot");
  frames(l, 10);
  r.left = !l.server->clients[1].active;
  cl::listen_lobby(l, "addbot expert");
  frames(l, 20);
  r.rejoined = alive(l, 1);
  // Every key lying out, taken by each in turn. A key the rules took away at the start (one that
  // starts inside a solid: xhangar2's red key in its door) is no one's to take, in co-op or alone:
  // counted apart.
  for (const auto& [kind, at] : facts.keys) {
    const int item = sv::sq_item(kind);
    if (item < 0) continue;
    bool lying = false;
    for (const sv::Q2Entity& e : s->entities) {
      const float dx = e.origin.x - at.x, dy = e.origin.y - at.y;
      lying = lying || (!e.gone && e.classname == kind && dx * dx + dy * dy < 64 * 64);
    }
    if (!lying) {
      r.keys_gone++;
      continue;
    }
    r.keys++;
    bool both = true;
    for (int slot = 0; slot < 2; slot++) {
      for (int i = 0; i < 8 && s->players[size_t(slot)].body.counts[item] == 0; i++) {
        put_at(*s, slot, at);
        frames(l, 1);
      }
      both = both && s->players[size_t(slot)].body.counts[item] > 0;
    }
    r.keys_taken += both ? 1 : 0;
  }
  if (exit == nullptr) return r;
  r.exit = exit->level;
  // Into the exit; a new unit's intermission ends on a button after 5 s.
  for (int i = 0; i < 1200 && l.pending_map.empty() && !s->game_over; i++) {
    sh::MoveCmd walk;
    if (s->intermission_at <= 0) {
      if (!alive(l, 0)) break;
      if (exit->button) {
        // A side a second: from 40 units off the box's face, walking into it.
        const int side = (i / 40) % 4;
        const float dx = side == 0 ? 1.f : side == 1 ? -1.f : 0.f;
        const float dy = side == 2 ? 1.f : side == 3 ? -1.f : 0.f;
        if (i % 40 == 0)
          put_at(*s, 0,
                 {exit->at.x + dx * (exit->half.x + 40), exit->at.y + dy * (exit->half.y + 40),
                  exit->at.z + 16});
        l.view.y = side == 0 ? 180.f : side == 1 ? 0.f : side == 2 ? 270.f : 90.f;
        walk.forward = 400;
      } else {
        put_at(*s, 0, exit->at);
        l.view.y = exit->yaw;
      }
    }
    sh::MoveCmd press;
    press.buttons = (i / 8) % 2 == 0 ? sh::kButtonAttack : 0;
    cl::listen_frame(l, s->intermission_at > 0 ? press : walk);
  }
  // An exit to no map is the campaign's end: the game is over, its show plays (The Reckoning's
  // `xout.cin+victory.pcx`).
  if (s->game_over && sv::level_string(exit->level).map.empty()) {
    r.next = "the end (" + s->cinematic + ")";
    r.ended = true;
    return r;
  }
  if (l.pending_map.empty()) {
    r.problem = "the exit to " + exit->level + " was never taken" +
                (alive(l, 0) ? "" : " (the player was dead)");
    return r;
  }
  r.next = l.pending_map;
  std::unique_ptr<cl::LoadedMap> next = cl::load_named_map(files, r.next, error);
  if (!next || !cl::listen_change(l, std::move(next), error)) {
    r.problem = "entering " + r.next + ": " + error;
    return r;
  }
  frames(l, 40);
  const sv::Q2Server* now = sv::q2_of(*l.rules);
  r.carried = now != nullptr && alive(l, 0) && alive(l, 1);
  if (now != nullptr)
    for (int slot = 0; slot < 2; slot++) r.came_in[slot] = where(*now, slot);
  return r;
}

// --- the same over a game library hosted unchanged (`--library`): two players on the runner ---

struct Native {
  sv::RulesRunner r;
  sv::NativeQ2* s = nullptr;
  uint32_t sequence[2] = {};
  sh::MoveCmd cmd[2];
};

sv::native::NativeEntityHead* player(Native& n, int slot) { return n.s->host->entity(slot + 1); }

bool native_alive(Native& n, int slot) {
  const sv::native::NativeEntityHead* e = player(n, slot);
  return n.s->clients[size_t(slot)].active && e->inuse != 0 && e->client != nullptr &&
         n.s->host->with_player(e, [](const auto& ps) { return ps.stats[1]; }) > 0;
}

void native_frames(Native& n, int count) {
  for (int i = 0; i < count; i++) {
    sh::Packet p[2];
    int k = 0;
    for (int slot = 0; slot < 2; slot++) {
      if (!n.s->clients[size_t(slot)].active) continue;
      p[k].client = slot;
      p[k].sequence = ++n.sequence[slot];
      n.cmd[slot].msec = 13;
      for (auto& c : p[k].cmds) c = n.cmd[slot];
      k++;
    }
    n.r.frame(p, k);
  }
}

void native_put(Native& n, int slot, sh::Vec3f at) {
  sv::native::NativeEntityHead* e = player(n, slot);
  e->s.origin[0] = at.x;  // a game's move starts from the entity's own place
  e->s.origin[1] = at.y;
  e->s.origin[2] = at.z;
  n.s->host->with_player(e, [&at](auto& ps) {  // either interface's record
    auto& m = ps.move;
    using Packed = std::remove_reference_t<decltype(m.origin[0])>;
    m.origin[0] = static_cast<Packed>(at.x * 8);
    m.origin[1] = static_cast<Packed>(at.y * 8);
    m.origin[2] = static_cast<Packed>(at.z * 8);
    for (auto& v : m.velocity) v = 0;
  });
}

// How many of the item the game holds for the slot, by its pickup name (the inventory it sends).
int native_holds(Native& n, int slot, const std::string& pickup) {
  int index = -1;
  for (int k = 1; k < 256; k++)
    if (n.s->raw.config[1056 + size_t(k)] == pickup) index = k;
  if (index < 0) return 0;
  n.r.client_command(slot, "inven");
  native_frames(n, 1);
  const auto& inv = n.s->host->inventory[slot];
  const int held = size_t(index) < inv.size() ? inv[size_t(index)] : 0;
  n.r.client_command(slot, "inven");  // shut again
  return held;
}

bool native_start(Native& n, const cl::LoadedMap& map, std::string& error) {
  n.s->map_checksum = sv::q2_map_checksum(map.file);
  if (!n.r.start(map.spec, error)) return false;
  for (int slot = 0; slot < 2; slot++) {
    n.r->clients[size_t(slot)].userinfo = {{"name", slot == 0 ? "player" : "mate"},
                                           {"skin", "male/grunt"}};
    n.r.connect(slot, slot == 0 ? "player" : "mate", false, false, nullptr);
  }
  return true;
}

Row sweep_native(const std::filesystem::path& library, const cl::SearchPath& files,
                 const std::string& name, const std::string& enter, const MapFacts& facts,
                 const Exit* exit) {
  Row r;
  r.map = name;
  std::string error;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, name, error);
  if (!map) {
    r.problem = error;
    return r;
  }
  sv::NativeQ2Options o;
  o.library = library;
  o.max_clients = 2;
  o.settings = {{"deathmatch", "0"}, {"coop", "1"}, {"skill", "1"}};
  std::unique_ptr<sv::NativeQ2> ns = sv::load_native_q2(o, error);
  if (!ns) {
    r.problem = error;
    return r;
  }
  Native n;
  n.s = ns.get();
  n.r = sv::native_q2_runner(std::move(ns));
  if (!enter.empty()) n.s->host->level = enter;  // the spot the campaign comes in at
  if (!native_start(n, *map, error)) {
    r.problem = error;
    return r;
  }
  native_frames(n, 60);
  r.both_in = native_alive(n, 0) && native_alive(n, 1);
  for (int i = 0; i < 1500 && !r.died; i++) {  // `kill` waits 5 s after a spawn
    if (i % 77 == 0) n.r.client_command(1, "kill");
    native_frames(n, 1);
    r.died = !native_alive(n, 1);
  }
  for (int i = 0; i < 1500 && !native_alive(n, 1); i++) {
    n.cmd[1].buttons = (i / 8) % 2 == 0 ? sh::kButtonAttack : 0;
    native_frames(n, 1);
  }
  n.cmd[1].buttons = 0;
  r.back = native_alive(n, 1);
  n.r.drop(1);
  native_frames(n, 10);
  r.left = !n.s->clients[1].active;
  n.r->clients[1].userinfo = {{"name", "mate"}, {"skin", "male/grunt"}};
  n.r.connect(1, "mate", false, false, nullptr);
  native_frames(n, 20);
  r.rejoined = native_alive(n, 1);
  for (const auto& [kind, at] : facts.keys) {
    const int item = sv::sq_item(kind);
    if (item < 0) continue;
    r.keys++;
    const std::string pickup = sv::kSqItems[item].pickup;
    bool both = true;
    for (int slot = 0; slot < 2; slot++) {
      for (int i = 0; i < 8; i++) {
        native_put(n, slot, at);
        native_frames(n, 1);
      }
      both = both && native_holds(n, slot, pickup) > 0;
    }
    r.keys_taken += both ? 1 : 0;
  }
  if (exit == nullptr) return r;
  r.exit = exit->level;
  for (int i = 0; i < 1500 && n.s->pending_map.empty(); i++) {
    if (native_alive(n, 0)) {
      native_put(n, 0, exit->at);
      const int16_t delta = n.s->host->with_player(
          player(n, 0), [](const auto& ps) { return ps.move.delta_angles[1]; });
      n.cmd[0].angles.y = exit->yaw - static_cast<float>(delta) * (360.0f / 65536.0f);
    }
    n.cmd[0].buttons = n.cmd[1].buttons = (i / 8) % 2 == 0 ? sh::kButtonAttack : 0;
    native_frames(n, 1);
  }
  n.cmd[0] = n.cmd[1] = {};
  if (n.s->pending_map.empty()) {
    r.problem = "the exit to " + exit->level + " was never taken" +
                (n.s->map_stopped ? " (" + n.s->stop_reason + ")" : "");
    return r;
  }
  r.next = n.s->pending_map;
  std::unique_ptr<cl::LoadedMap> next = cl::load_named_map(files, r.next, error);
  if (!next) {
    r.problem = "entering " + r.next + ": " + error;
    return r;
  }
  n.r.carry();
  if (!native_start(n, *next, error)) {
    r.problem = "entering " + r.next + ": " + error;
    return r;
  }
  native_frames(n, 40);
  r.carried = native_alive(n, 0) && native_alive(n, 1);
  for (int slot = 0; slot < 2; slot++) {
    r.came_in[slot] = n.s->host->with_player(player(n, slot), [](const auto& ps) {
      const auto& at = ps.move.origin;
      return sh::Vec3f{float(at[0]) * 0.125f, float(at[1]) * 0.125f, float(at[2]) * 0.125f};
    });
  }
  return r;
}

// `plate`'s floors with its liquids laid over them, as a lab picture with room for a title.
gl::Picture plate_picture(const lv::Plate& plate) {
  const ge::ui::Image& im = plate.image;
  gl::Picture p = gl::create_picture(im.width, im.height + 24, {18, 18, 22});
  const ge::ui::Image& wet = plate.liquid.image;
  const bool layer = wet.width == im.width && wet.height == im.height;
  for (size_t i = 0; i < size_t(im.width) * size_t(im.height); i++)
    for (size_t c = 0; c < 3; c++) {
      const double under = im.rgba[i * 4 + c];
      const double a = layer ? wet.rgba[i * 4 + 3] / 255.0 : 0;
      const double v = layer ? under * (1 - a) + wet.rgba[i * 4 + c] * a : under;
      p.rgba[i * 4 + c] = static_cast<uint8_t>(std::clamp(v, 0.0, 255.0));
    }
  return p;
}

// The map from above: floors, exits, keys, and where the players came in. With `plate` (the live
// map's picture of the map) on it; else the up-facing faces' edges, shaded by height.
gl::Picture overhead(const br::Map& map, const MapFacts& f, const std::string& title,
                     const Row* entered, const lv::Plate* plate) {
  if (plate != nullptr && !plate->image.empty()) {
    gl::Picture p = plate_picture(*plate);
    const ge::overview::Frame& fr = plate->frame;
    for (const Exit& e : f.exits) gl::fill_disc(p, fr.px(e.at.x), fr.py(e.at.y), 7, {255, 150, 40});
    for (const auto& [kind, at] : f.keys)
      gl::fill_disc(p, fr.px(at.x), fr.py(at.y), 6, {255, 230, 60});
    if (entered != nullptr)
      for (int slot = 0; slot < 2; slot++) {
        const double x = fr.px(entered->came_in[slot].x), y = fr.py(entered->came_in[slot].y);
        const gl::Rgba ring = slot == 0 ? gl::Rgba{90, 220, 110} : gl::Rgba{230, 90, 230};
        gl::draw_ring(p, x, y, 10, ring);
        gl::draw_ring(p, x, y, 11, ring);
      }
    gl::draw_text(p, 10, plate->image.height + 6, title, {230, 230, 230});
    return p;
  }
  constexpr int kSize = 480, kPad = 20;
  gl::Picture p = gl::create_picture(kSize, kSize + 24, {18, 18, 22});
  if (map.models.empty()) return p;
  const br::Model& w = map.models[0];
  const float span = std::max({w.maxs[0] - w.mins[0], w.maxs[1] - w.mins[1], 1.0f});
  const auto px = [&](float x, float y) {
    return std::pair<double, double>{kPad + (x - w.mins[0]) / span * (kSize - 2 * kPad),
                                     kPad + (w.maxs[1] - y) / span * (kSize - 2 * kPad)};
  };
  const float zspan = std::max(w.maxs[2] - w.mins[2], 1.0f);
  for (const br::Surface& s : map.surfaces) {
    if (s.kind == br::SurfaceKind::sky) continue;
    for (uint32_t t = 0; t + 2 < s.count; t += 3) {
      const br::Vertex* v[3];
      for (int k = 0; k < 3; k++) v[k] = &map.vertices[map.indices[s.first + t + uint32_t(k)]];
      if (v[0]->nrm[2] < 0.7f) continue;
      const float z = (v[0]->pos[2] - w.mins[2]) / zspan;
      const auto g = static_cast<uint8_t>(50 + 140 * std::clamp(z, 0.0f, 1.0f));
      for (int k = 0; k < 3; k++) {
        const auto [x0, y0] = px(v[k]->pos[0], v[k]->pos[1]);
        const auto [x1, y1] = px(v[(k + 1) % 3]->pos[0], v[(k + 1) % 3]->pos[1]);
        gl::draw_line(p, x0, y0, x1, y1, {g, g, static_cast<uint8_t>(std::min(255, g + 20))});
      }
    }
  }
  for (const Exit& e : f.exits) {
    const auto [x, y] = px(e.at.x, e.at.y);
    gl::fill_disc(p, x, y, 6, {255, 150, 40});
  }
  for (const auto& [kind, at] : f.keys) {
    const auto [x, y] = px(at.x, at.y);
    gl::fill_disc(p, x, y, 5, {255, 230, 60});
  }
  if (entered != nullptr)
    for (int slot = 0; slot < 2; slot++) {
      const auto [x, y] = px(entered->came_in[slot].x, entered->came_in[slot].y);
      gl::draw_ring(p, x, y, 8, slot == 0 ? gl::Rgba{90, 220, 110} : gl::Rgba{230, 90, 230});
    }
  gl::draw_text(p, 10, kSize + 6, title, {230, 230, 230});
  return p;
}

std::string yes(bool b) { return b ? "yes" : "NO"; }

void run(gl::Lab& lab) {
  const pf::GameProfile& game = *pf::find_game("q2");
  const ge::classicenhance::Profile& content = pf::content_of(game);
  std::string error;
  cl::SearchPath files;
  const bool own = lab.arg("game").empty();
  const std::string mod = lab.arg("mod");
  std::string first = lab.arg("first");
  if (own) {
    std::vector<uint8_t> a, b, pak;
    if (!hall(512, "next$east", a, error) || !hall(768, "*hall", b, error) ||
        !ge::vfs::write_pak({{"maps/hall.bsp", a}, {"maps/next.bsp", b}}, &pak, &error) ||
        !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "The fixture halls should write and mount. Skill: brush.");
      return;
    }
    if (first.empty()) first = "hall";
  } else {
    if (!cl::open_game_dir(lab.arg("game"), mod.empty() ? std::string(content.base_dir) : mod,
                           files, error, content)) {
      lab.fail(error, "Pass --game=<the folder holding " + std::string(content.base_dir) +
                          "/pak0.pak and the pack's folder>. Skill: shooter.");
      return;
    }
    const sv::Q2Pack pack = sv::q2_pack_of_folder(mod);
    if (first.empty())
      first = pack == sv::Q2Pack::base ? "base1" : std::string(sv::q2_pack_start_map(pack));
  }
  const auto most = static_cast<size_t>(lab.arg_num("maps"));
  // A game library hosted unchanged in place of our rules (never shipped: the player's own).
  const std::filesystem::path library = lab.arg("library");
  // The campaign's graph, breadth first from the first map: each map once, each by its exits.
  std::vector<std::string> order;
  std::map<std::string, MapFacts> facts;
  std::map<std::string, std::vector<uint8_t>> bytes;  // each map's file, for its plate
  std::map<std::string, br::Map> read;
  // The level string each map is first entered by (`base2$base1`): its spot. `--first` may name
  // one (`--first=base2$base1`).
  std::map<std::string, std::string> enters;
  if (const sv::LevelString fl = sv::level_string(first); !fl.map.empty() && fl.map != first) {
    enters[fl.map] = first;
    first = fl.map;
  }
  std::deque<std::string> todo{first};
  while (!todo.empty() && order.size() < most) {
    const std::string name = todo.front();
    todo.pop_front();
    if (facts.count(name) != 0) continue;
    std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(files, name, error);
    br::Map& bsp = read[name];
    if (!m || !br::read_bsp(m->file, &bsp, &error)) {
      lab.fail(std::string(name).append(": ").append(error),
               "Read the reason above. Skill: brush.");
      facts[name] = {};
      continue;
    }
    facts[name] = facts_of(bsp);
    bytes[name] = m->file;
    order.push_back(name);
    for (const Exit& e : facts[name].exits) {
      const std::string next = sv::level_string(e.level).map;
      if (next.empty() || facts.count(next) != 0) continue;
      todo.push_back(next);
      if (enters.count(next) == 0) enters[next] = e.level;
    }
  }
  gl::LabTable t;
  t.title = "Quake II's campaign in co-op, a map a row (from " + first + ")";
  t.columns = {"map",        "both in",        "death, back", "left, rejoined", "keys both took",
               "exit taken", "both carried in"};
  int failed = 0;
  std::vector<Row> rows;
  for (const std::string& name : order) {
    const MapFacts& f = facts[name];
    // The exit to a map not yet entered first, else the first: the sweep walks the campaign on.
    const Exit* exit = f.exits.empty() ? nullptr : &f.exits.front();
    Row r = library.empty() ? sweep(files, name, f, exit, !own && !mod.empty(), enters[name])
                            : sweep_native(library, files, name, enters[name], f, exit);
    const bool ok = r.problem.empty() && r.both_in && r.died && r.back && r.left && r.rejoined &&
                    r.keys_taken == r.keys && (exit == nullptr || r.carried || r.ended);
    failed += ok ? 0 : 1;
    t.rows.push_back({name, yes(r.both_in), yes(r.died && r.back), yes(r.left && r.rejoined),
                      std::to_string(r.keys_taken) + "/" + std::to_string(r.keys) +
                          (r.keys_gone > 0 ? " (" + std::to_string(r.keys_gone) + " gone)" : ""),
                      exit == nullptr ? "(none: the end)" : r.exit + " to " + r.next,
                      exit == nullptr ? "-"
                      : r.ended       ? "(the end)"
                                      : yes(r.carried)});
    if (!r.problem.empty()) t.rows.back().back() += " (" + r.problem + ")";
    lab.note(name + ": " + (ok ? "played through" : "FAILED " + r.problem));
    rows.push_back(std::move(r));
  }
  const gl::Det det = own ? gl::Det::deterministic : gl::Det::measured;
  lab.table(t, det);
  lab.metric("maps", static_cast<double>(order.size()), "count", det);
  lab.metric("maps_failed", failed, "count", det);
  if (failed != 0)
    lab.fail(std::to_string(failed) + " map(s) did not play through in co-op",
             "Read the table's NO cells and the reasons in brackets. Skill: shooter.");
  // The game's palette, which the plate shades a map's own textures by.
  std::vector<uint8_t> palette_store;
  const std::span<const uint8_t> palette = cl::game_palette(files, palette_store, content);
  for (size_t i = 0; i < order.size(); i++) {
    const std::string& name = order[i];
    lv::Plate plate;
    std::string why;
    const bool drawn =
        !palette.empty() && lv::build_plate(bytes[name], name, palette, 720, &plate, &why, &files);
    if (!drawn && !own)
      lab.note(std::string(name).append(": drawn without its plate (").append(why).append(")"));
    // Where the players came in to this map, from the row that entered it.
    const Row* entered = nullptr;
    for (const Row& r : rows)
      if (r.next == name) entered = &r;
    lab.shot({name + "-coop",
              name + " in co-op from above",
              "The floors of " + name + " from above, its exits in orange and its keys in " +
                  "yellow; the rings are where the player (green) and the companion (magenta) " +
                  "came in from the level before.",
              "both rings at the level's starts",
              {{"map", name}, {"game", own ? "fixture" : "local packs"}}},
             overhead(read[name], facts[name], name, entered, drawn ? &plate : nullptr), det);
  }
}

const gl::LabSpec kSpec{
    "se-q2-coop-sweep",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "Quake II's campaign in co-op on the window's listen server: joins, deaths, rejoins, keys, "
    "exits and the level changes",
    "shooter",
    "Does every map of Quake II's campaign play through in two-player co-op: both in, a death and "
    "the return, a leave and a rejoin, every key taken by both, the exit taken and both carried "
    "into the next level?",
    {{"game", "", "the folder holding Quake II's base folder (none: two fixture halls)"},
     {"first", "",
      "the map the sweep starts from (hall; base1 with --game; a pack's first map with --mod)"},
     {"mod", "",
      "a mission pack's folder beside the base one (xatrix, rogue): its maps on its rules"},
     {"maps", "64", "the most maps swept"},
     {"library", "", "a game library to host in place of our rules (none: ours)"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
