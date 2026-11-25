// LAB `se-q2-save`: QUAKE II'S LEVELS KEPT AND ITS GAMES SAVED (server/q2_save.hpp), on
// the window's listen server as a single-player game (`deathmatch 0`).
//
// - **A level entered again**: the player walks the first map by a simple rule (toward the
//   nearest item still lying, a quarter turn left when a wall stops him), leaves for the second,
//   and comes back. The table gives the first map's items taken, monsters dead and level time as he
//   left it and as he found it again; the pictures show the map from above both times (items still
//   there in green, taken ones as red crosses, monsters alive in orange, dead in dark red).
// - **A save loaded**: the game is saved (`listen_save`) part way through the walk, played on, and
//   the save loaded into a new window (`listen_load_q2`) is played on with the same moves.
//   The metric is the frames whose `q2_digest` matched the game never stopped: all of them.
// - **The autosave** made as the second map was entered, loaded: the player at its start with what
//   he carried; **death**: a button a second after dying asks for the load menu.
//
// Without `--game` the maps are our own: two floored halls of brushes in a pack, with items and a
// monster, so CI needs no game data and the counts are a digest. With `--game=<folder>` (the
// folder holding Quake II's base folder) the maps come from its packs (`--maps=<first>,<second>`,
// the full game's first two by default), for PRIVATE pictures only.
//
// ge:summary Lab: Quake II's levels kept within a unit, a save loaded bit for bit, the autosave
// ge:keywords lab se-q2-save q2 save load autosave revisit kept-level listen_load_q2
// ge:keywords q2_digest menu_loadgame
// ge:skill shooter labs save
// ge:test tests/shooters_edge_q2_save_test.cpp
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classic/q2_move.hpp"
#include "classicenhance/profile.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_save.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;

// A floored hall walled round, `w` wide, a start, items, a monster and an exit to `next`.
bool hall(float w, const char* next, bool monster, std::vector<uint8_t>& out, std::string& error) {
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-w, -512, -16, w, 512, 0);
  box(-w - 16, -512, -16, -w, 512, 256);
  box(w, -512, -16, w + 16, 512, 256);
  box(-w - 16, 512, -16, w + 16, 528, 256);
  box(-w - 16, -528, -16, w + 16, -512, 256);
  const auto put = [&](std::vector<std::pair<std::string, std::string>> pairs) {
    br::Entity e;
    e.pairs = std::move(pairs);
    world.entities.push_back(e);
  };
  put({{"classname", "info_player_start"}, {"origin", "0 -300 24"}, {"angle", "90"}});
  for (int i = 0; i < 6; i++)  // three on the walk's first leg, three off it
    put({{"classname", i % 2 == 0 ? "item_armor_shard" : "item_health_small"},
         {"origin", std::to_string(i < 3 ? 0 : -300 + 60 * i) + " " +
                        std::to_string(i < 3 ? -150 + 150 * i : 200) + " 24"}});
  if (monster)
    put({{"classname", "monster_soldier_light"}, {"origin", "300 300 24"}, {"angle", "270"}});
  put({{"classname", "target_changelevel"}, {"targetname", "exit"}, {"map", next}});
  return br::write_brush_world(world, &out, &error);
}

const ge::progs::Progs& progs() {
  static const ge::progs::Progs p = [] {
    ge::progs::Progs out;
    std::string why;
    cl::baseline_progs(out, why);
    return out;
  }();
  return p;
}

cl::ListenOptions options(const cl::SearchPath& files) {
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.console = {"deathmatch 0", "coop 0"};
  opt.files = &files;
  return opt;
}

// The walk's move at frame `f`: toward the nearest item still lying, a quarter turn left for a
// second when a wall stopped him.
struct Walker {
  float yaw = 0;
  sh::Vec3f last{};
  int turned = 0;  // frames left walking the turned way
  sh::MoveCmd step(cl::Listen& l, int f) {
    const sh::Vec3f me = l.server->sv.edicts[1].origin;
    if (f % 24 == 23) {
      const float dx = me.x - last.x, dy = me.y - last.y;
      if (dx * dx + dy * dy < 24 * 24) {
        yaw += 90;
        turned = 72;
      }
      last = me;
    }
    if (turned > 0) {
      turned--;
    } else if (const sv::Q2Server* s = sv::q2_of(*l.rules)) {
      float best = 1e30f;
      for (const sv::Q2Entity& e : s->entities) {
        if (e.item < 0 || e.gone || e.hidden) continue;
        const float dx = e.origin.x - me.x, dy = e.origin.y - me.y, dz = e.origin.z - me.z;
        const float d = dx * dx + dy * dy;
        if (d < best && dz * dz < 48 * 48) {
          best = d;
          yaw = float(ge::core::libm::atan2(dy, dx) * 180 / 3.14159265358979);
        }
      }
    }
    sh::MoveCmd cmd;
    cmd.forward = 300;
    if (f % 40 < 3) cmd.buttons = sh::kButtonAttack;
    l.view.y = yaw;
    return cmd;
  }
};

// What a level holds as seen from above: its items (taken or not) and monsters (dead or not).
struct Seen {
  std::vector<std::pair<sh::Vec3f, bool>> items, monsters;  // place, gone / dead
  int taken = 0, dead = 0;
  double time = 0;
};

// `start` the items the map left out as it started (its skill's, its mode's): not counted.
Seen seen(const sv::Q2Server& s, const std::vector<bool>& start) {
  Seen out;
  out.time = s.level_time;
  for (size_t i = 0; i < s.entities.size(); i++) {
    const sv::Q2Entity& e = s.entities[i];
    if (e.item < 0 || (i < start.size() && start[i])) continue;
    out.items.emplace_back(e.origin, e.gone || e.hidden);
    out.taken += e.gone || e.hidden ? 1 : 0;
  }
  for (const sv::Q2Monster& m : s.monsters) {
    out.monsters.emplace_back(s.entities[size_t(m.entity)].origin, m.dead);
    out.dead += m.dead ? 1 : 0;
  }
  return out;
}

struct Run {
  bool ok = false;
  Seen left, found;
  std::vector<sh::Vec3f> walk;
  int matched = 0, compared = 0;  // the loaded game's frames matching the one never stopped
  bool autosave_ok = false;
  int carried_health = 0, came_health = 0;
  bool load_menu = false;
};

bool change(cl::Listen& l, const cl::SearchPath& files, const std::string& to, std::string& error) {
  l.rules->console("changelevel " + to);
  for (int i = 0; i < 60 && l.pending_map.empty(); i++) cl::listen_frame(l, {});
  if (l.pending_map != to) {
    error = "the level change to " + to + " was never asked for";
    return false;
  }
  l.pending_map.clear();
  std::unique_ptr<cl::LoadedMap> next = cl::load_named_map(files, to, error);
  return next && cl::listen_change(l, std::move(next), error);
}

Run play(const cl::SearchPath& files, const std::string& first, const std::string& second,
         int frames, std::string& error) {
  Run out;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, first, error);
  if (!map) return out;
  const cl::ListenOptions opt = options(files);
  cl::Listen l;
  if (!cl::listen_start(l, progs(), std::move(map), opt, error)) return out;
  sv::Q2Server* sq = sv::q2_of(*l.rules);
  if (sq == nullptr) {
    error = first + " did not play on Quake II's rules (no brushes?)";
    return out;
  }
  std::vector<bool> left_out;  // the items the map started without
  left_out.reserve(sq->entities.size());
  for (const sv::Q2Entity& e : sq->entities) left_out.push_back(e.item >= 0 && e.gone);
  // The walk, saved half way and played on: the frames' digests and moves kept.
  Walker w;
  w.yaw = l.view.y;
  w.last = l.server->sv.edicts[1].origin;
  std::vector<uint8_t> file;
  std::vector<sh::MoveCmd> after;
  std::vector<uint64_t> digests;
  for (int f = 0; f < frames; f++) {
    const sh::MoveCmd cmd = w.step(l, f);
    if (f >= frames / 2 && !file.empty()) {
      after.push_back(cmd);
      after.back().angles.y = l.view.y;
    }
    cl::listen_frame(l, cmd);
    if (f % 8 == 0) out.walk.push_back(l.server->sv.edicts[1].origin);
    if (!file.empty()) digests.push_back(sv::q2_digest(*sv::q2_of(*l.rules)));

    if (f == frames / 2 && !cl::listen_save(l, file, error)) return out;
  }
  // The save loaded into a new window and played on with the same moves.
  {
    cl::Listen other;
    other.opt = opt;
    if (!cl::listen_load_q2(other, opt, file, "lab", error)) return out;
    for (size_t k = 0; k < after.size(); k++) {
      other.view.y = after[k].angles.y;
      cl::listen_frame(other, after[k]);
      out.compared++;
      if (sv::q2_digest(*sv::q2_of(*other.rules)) == digests[k]) out.matched++;
    }
  }
  out.left = seen(*sv::q2_of(*l.rules), left_out);
  if (!change(l, files, second, error)) return out;
  // The autosave made as the second map was entered: loaded, he comes in with what he carried.
  const std::vector<uint8_t> autosave = cl::listen_take_autosave(l);
  out.carried_health = sv::q2_of(*l.rules)->players[0].body.health;
  if (!autosave.empty()) {
    cl::Listen other;
    other.opt = opt;
    if (cl::listen_load_q2(other, opt, autosave, "save0", error)) {
      const sv::Q2Server& a = *sv::q2_of(*other.rules);
      out.came_health = a.players[0].body.health;
      out.autosave_ok = a.players[0].alive && a.map_name == second;
    }
  }
  for (int i = 0; i < 30; i++) cl::listen_frame(l, {});
  if (!change(l, files, first, error)) return out;
  out.found = seen(*sv::q2_of(*l.rules), left_out);
  // Death alone: a button a second on asks for the load menu.
  sv::fight_hurt(*sv::q2_of(*l.rules), 0, -1, {}, 10000, 0, 0, 0);
  for (int i = 0; i < 160 && !l.load_menu_asked; i++) {
    sh::MoveCmd cmd;
    cmd.buttons = sh::kButtonAttack;
    cl::listen_frame(l, cmd);
  }
  out.load_menu = l.load_menu_asked;
  out.ok = true;
  return out;
}

// The map from above: its floors, the items and monsters as `s` holds them, the walk.
gl::Picture overhead(const br::Map& map, const std::string& title, const Seen& s,
                     const std::vector<sh::Vec3f>* walk) {
  constexpr int kSize = 560, kPad = 24;
  gl::Picture p = gl::create_picture(kSize, kSize + 40, {18, 18, 22});
  if (map.models.empty()) return p;
  float lo[3], hi[3];
  for (int k = 0; k < 3; k++) {
    lo[k] = map.models[0].mins[k];
    hi[k] = map.models[0].maxs[k];
  }
  const float span = std::max({hi[0] - lo[0], hi[1] - lo[1], 1.0f});
  const auto px = [&](float x, float y) {
    return std::pair<double, double>{kPad + (x - lo[0]) / span * (kSize - 2 * kPad),
                                     kPad + (hi[1] - y) / span * (kSize - 2 * kPad)};
  };
  const float zspan = std::max(hi[2] - lo[2], 1.0f);
  for (const br::Surface& f : map.surfaces) {
    if (f.kind == br::SurfaceKind::sky) continue;
    for (uint32_t t = 0; t + 2 < f.count; t += 3) {
      const br::Vertex* v[3];
      for (int k = 0; k < 3; k++) v[k] = &map.vertices[map.indices[f.first + t + uint32_t(k)]];
      if (v[0]->nrm[2] < 0.7f) continue;  // floors only
      const float z = (v[0]->pos[2] - lo[2]) / zspan;
      const auto g = static_cast<uint8_t>(50 + 120 * std::clamp(z, 0.0f, 1.0f));
      for (int k = 0; k < 3; k++) {
        const auto [x0, y0] = px(v[k]->pos[0], v[k]->pos[1]);
        const auto [x1, y1] = px(v[(k + 1) % 3]->pos[0], v[(k + 1) % 3]->pos[1]);
        gl::draw_line(p, x0, y0, x1, y1, {g, g, static_cast<uint8_t>(g + 20)});
      }
    }
  }
  if (walk != nullptr)
    for (size_t i = 1; i < walk->size(); i++) {
      const auto [x0, y0] = px((*walk)[i - 1].x, (*walk)[i - 1].y);
      const auto [x1, y1] = px((*walk)[i].x, (*walk)[i].y);
      gl::draw_line(p, x0, y0, x1, y1, {255, 210, 60}, 1.5);
    }
  for (const auto& [at, gone] : s.items) {
    const auto [x, y] = px(at.x, at.y);
    if (gone) {
      gl::draw_line(p, x - 5, y - 5, x + 5, y + 5, {255, 70, 70}, 2.0);
      gl::draw_line(p, x - 5, y + 5, x + 5, y - 5, {255, 70, 70}, 2.0);
    } else {
      gl::fill_disc(p, x, y, 4, {90, 220, 110});
    }
  }
  for (const auto& [at, dead] : s.monsters) {
    const auto [x, y] = px(at.x, at.y);
    gl::fill_disc(p, x, y, 6, dead ? gl::Rgba{120, 30, 30} : gl::Rgba{255, 150, 40});
  }
  gl::fill_disc(p, 16, kSize + 14, 4, {90, 220, 110});
  gl::draw_text(p, 24, kSize + 10, "ITEM", {220, 220, 220});
  gl::draw_line(p, 74, kSize + 9, 84, kSize + 19, {255, 70, 70}, 2.0);
  gl::draw_text(p, 90, kSize + 10, "TAKEN", {220, 220, 220});
  gl::fill_disc(p, 150, kSize + 14, 5, {255, 150, 40});
  gl::draw_text(p, 160, kSize + 10, "MONSTER", {220, 220, 220});
  gl::fill_disc(p, 232, kSize + 14, 5, {120, 30, 30});
  gl::draw_text(p, 242, kSize + 10, "DEAD", {220, 220, 220});
  gl::draw_text(p, 12, kSize + 26, title, {230, 230, 230});
  return p;
}

void run(gl::Lab& lab) {
  const pf::GameProfile& game = *pf::find_game("q2");
  const ge::classicenhance::Profile& content = pf::content_of(game);
  std::string error;
  cl::SearchPath files;
  const bool own = lab.arg("game").empty();
  std::string first = "hall", second = "next";
  if (own) {
    std::vector<uint8_t> a, b, pak;
    if (!hall(512, "next", true, a, error) || !hall(768, "hall", false, b, error) ||
        !ge::vfs::write_pak({{"maps/hall.bsp", a}, {"maps/next.bsp", b}}, &pak, &error) ||
        !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "The fixture halls should write and mount. Skill: brush.");
      return;
    }
  } else {
    if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content)) {
      lab.fail(error, "Pass --game=<the folder holding " + std::string(content.base_dir) +
                          "/pak0.pak>. Skill: shooter.");
      return;
    }
    first = "base1";
    second = "base2";
  }
  if (const std::string& asked = lab.arg("maps"); !asked.empty()) {
    const size_t comma = asked.find(',');
    if (comma == std::string::npos) {
      lab.fail("--maps needs two maps, a comma between",
               "Pass --maps=<first>,<second>. Skill: labs.");
      return;
    }
    first = asked.substr(0, comma);
    second = asked.substr(comma + 1);
  }
  const gl::Det det = own ? gl::Det::deterministic : gl::Det::measured;
  const Run r = play(files, first, second, static_cast<int>(lab.arg_num("frames")), error);
  if (!r.ok) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  gl::LabTable t;
  t.title = first + " as the player left it and as he found it again";
  t.columns = {"what", "left", "found again"};
  t.rows = {{"items taken", std::to_string(r.left.taken), std::to_string(r.found.taken)},
            {"monsters dead", std::to_string(r.left.dead), std::to_string(r.found.dead)},
            {"level time (s)", ge::core::num::to_fixed(r.left.time, 1),
             ge::core::num::to_fixed(r.found.time, 1)}};
  lab.table(t, det);
  gl::LabTable s;
  s.title = "A save loaded into a new window and the autosave";
  s.columns = {"what", "value"};
  s.rows = {{"frames played on after the load", std::to_string(r.compared)},
            {"of them matching the game never stopped", std::to_string(r.matched)},
            {"the autosave loads at " + second, r.autosave_ok ? "yes" : "no"},
            {"health carried in / after its load",
             std::to_string(r.carried_health) + " / " + std::to_string(r.came_health)},
            {"a death's button asks for the load menu", r.load_menu ? "yes" : "no"}};
  lab.table(s, det);
  lab.metric("revisit.items_taken_kept", r.found.taken == r.left.taken ? 1 : 0, "bool", det);
  lab.metric("revisit.dead_kept", r.found.dead >= r.left.dead ? 1 : 0, "bool", det);
  lab.metric("revisit.time_on", r.found.time - r.left.time, "s", det);
  lab.metric("load.frames_matching", r.matched, "count", det);
  lab.metric("load.frames_compared", r.compared, "count", det);
  lab.metric("autosave.loads", r.autosave_ok ? 1 : 0, "bool");
  lab.metric("death.load_menu", r.load_menu ? 1 : 0, "bool");
  if (r.compared == 0 || r.matched != r.compared)
    lab.fail("the loaded game went its own way: " + std::to_string(r.matched) + " of " +
                 std::to_string(r.compared) + " frames matched",
             "A field the save does not walk: compare server/q2_save.cpp's walks with "
             "Q2Server and Q2Player. Skill: save.");
  if (r.found.taken < r.left.taken || r.found.time < r.left.time + 9.9)
    lab.fail("the level was not kept: taken " + std::to_string(r.left.taken) + " then " +
                 std::to_string(r.found.taken),
             "Check server/q2_save.cpp's q2_leave and q2_enter. Skill: shooter.");
  if (!r.autosave_ok || r.came_health != r.carried_health)
    lab.fail("the autosave did not bring the player in with what he carried",
             "Check q2_load_end's autosave branch. Skill: shooter.");
  if (!r.load_menu)
    lab.fail("a death's button did not ask for the load menu",
             "Check q2_save_frame and the listen's take_events. Skill: shooter.");
  std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(files, first, error);
  br::Map read;
  if (!m || !br::read_bsp(m->file, &read, &error)) {
    lab.fail(first + ": " + error, "Read the reason above. Skill: brush.");
    return;
  }
  const std::string from = own ? "fixture" : "local packs";
  lab.shot({first + "-left",
            first + " as he left it",
            "The floors of " + first + " from above as the player left for " + second +
                ": his walk in yellow, the items still lying in green, those he took as red "
                "crosses, monsters alive in orange and dead in dark red.",
            "red crosses along the walk",
            {{"map", first}, {"game", from}}},
           overhead(read, first + ": left at " + ge::core::num::to_fixed(r.left.time, 1) + " s",
                    r.left, &r.walk),
           det);
  lab.shot({first + "-found",
            first + " found again",
            "The same map when he came back from " + second +
                ": the same items gone, the same monsters dead, its time gone on from where he "
                "left it by the ten seconds it ran alone before he came in.",
            "the same red crosses as the picture before",
            {{"map", first}, {"game", from}}},
           overhead(read, first + ": found at " + ge::core::num::to_fixed(r.found.time, 1) + " s",
                    r.found, nullptr),
           det);
}

const gl::LabSpec kSpec{
    "se-q2-save",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "server/q2_save.hpp: a level of Quake II kept within its unit, a save loaded, the "
    "autosave and death's load menu, on the window's listen server",
    "shooter",
    "Does a level of Quake II's game come back as the player left it, does a save loaded into "
    "a new window play on bit for bit as the game never stopped, does the autosave bring the "
    "player in with what he carried, and does a death's button ask for the load menu?",
    {{"game", "", "the folder holding Quake II's base folder (none: two fixture halls)"},
     {"maps", "", "the two maps, first,second (default hall,next; base1,base2 with --game)"},
     {"frames", "600", "the player's frames walking the first map (saved half way)"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
