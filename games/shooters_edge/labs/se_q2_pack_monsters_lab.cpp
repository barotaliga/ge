// LAB `se-q2-pack-monsters`: EVERY MONSTER OF A QUAKE II MISSION PACK'S MAPS, WOKEN AND WATCHED.
// Each of the pack's maps is loaded with the pack's rules (`--mod`, The Reckoning's by default)
// twice over. THE CENSUS: its monsters counted by kind, every one not waiting to be used woken by a
// blow of 1 from the player (standing where the map starts him, his health kept full) and watched
// `--seconds` for whether it moves off its spot. THE DUELS: for each kind on the map, the map
// started again, every other monster killed, the player stood `--range` units off the first of
// the kind in the open and the monster woken: did it hurt him within `--duel` seconds? The tables
// say, map by map and kind by kind, how many there were, how many spawned inside solid, how many
// waited, moved of those woken, the duels that hurt him and the ones that died. A kind that never
// moves, or hurts him in none of its duels, is the sweep's finding: a monster that loads but does
// not play.
//
// It reads the player's own packs (`--game`, the folder holding the base folder and the pack's),
// for PRIVATE numbers only: without `--game` it notes that and stops, so CI never runs it.
//
// ge:summary Lab: a mission pack's monsters on every map of it, woken and watched: moved, attacked
// ge:keywords lab se-q2-pack-monsters q2 mission-pack xatrix reckoning rogue
// ge:keywords ground-zero monsters census sweep start-solid woken moved attacked
// ge:keywords monster-verification
// ge:skill shooter labs
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "brush/trace.hpp"
#include "classicenhance/profile.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_pack.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace gl = ge::lab;
namespace pf = shooters_edge::profiles;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

// The Reckoning's single-player maps (its deathmatch maps hold no monsters).
const char* const kReckoningMaps[] = {"xswamp",   "xsewer1", "xsewer2",  "xcompnd1", "xcompnd2",
                                      "xreactor", "xware",   "xintell",  "industry", "outbase",
                                      "refinery", "w_treat", "badlands", "xhangar1", "xhangar2",
                                      "xship",    "xmoon1",  "xmoon2"};
// Ground Zero's single-player maps (its rdm maps are deathmatch's).
const char* const kGroundZeroMaps[] = {"rmine1",  "rmine2",  "rlava1", "rlava2",   "rware1",
                                       "rware2",  "rbase1",  "rbase2", "rhangar1", "rhangar2",
                                       "rsewer1", "rsewer2", "rammo1", "rammo2",   "rboss"};

constexpr float kMoved = 32;
constexpr double kTic =
    0.013;  // a listen frame's command: 13 ms  // units off its spot that count as moving

struct Count {
  int all = 0, solid = 0, waiting = 0, woken = 0, moved = 0, duels = 0, hurt = 0, died = 0;
  int saw = 0, attacked = 0;  // duels where it took him for its enemy, where it began an attack
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

// Map `name` started on the pack's rules and run 2 s in; null (with a note) when it will not.
sv::Q2Server* start(gl::Lab& lab, const cl::SearchPath& files, const std::string& name,
                    cl::Listen& l, sv::Q2Pack pack) {
  std::string error;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, name, error);
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.console = {"deathmatch 0", "skill 1"};
  opt.files = &files;  // the pack's rules
  if (!map || !cl::listen_start(l, progs(), std::move(map), opt, error)) {
    lab.note("se-q2-pack-monsters: " + name + ": " + error);
    return nullptr;
  }
  for (int i = 0; i < int(2 / kTic); i++) cl::listen_frame(l, {});
  sv::Q2Server* sq = sv::q2_of(*l.rules);
  if (sq == nullptr || sq->opt.pack != pack) {
    lab.note("se-q2-pack-monsters: " + name + " did not start on the pack's rules");
    return nullptr;
  }
  return sq;
}

std::string at(sh::Vec3f p) {
  return std::to_string(std::lround(p.x)) + " " + std::to_string(std::lround(p.y)) + " " +
         std::to_string(std::lround(p.z));
}

// Nothing solid between two points, neither of them inside a wall.
bool clear_line(const cl::Listen& l, sh::Vec3f a, sh::Vec3f b) {
  ge::brush::BrushTracer tracer(l.map->bsp.map);
  const float from[3] = {a.x, a.y, a.z}, to[3] = {b.x, b.y, b.z}, zero[3] = {0, 0, 0};
  const ge::brush::Trace t = tracer.trace(from, to, zero, zero, sh::kSqSolid);
  return t.fraction >= 1 && !t.start_solid && !t.all_solid;
}

// A spot `range` units off `at` with a clear line to it at three heights, the first of twelve
// ways round; false when none is.
bool stand_off(const cl::Listen& l, sh::Vec3f at, float range, sh::Vec3f& out) {
  for (int turn = 0; turn < 12; turn++) {
    const double a = turn * 30.0 * 3.14159265358979 / 180;
    const sh::Vec3f p = {at.x + static_cast<float>(ge::core::libm::cos(a)) * range,
                         at.y + static_cast<float>(ge::core::libm::sin(a)) * range, at.z};
    bool clear = true;
    for (const float z : {0.f, 24.f, -16.f})
      clear = clear && clear_line(l, {at.x, at.y, at.z + z}, {p.x, p.y, p.z + z});
    if (clear) {
      out = p;
      return true;
    }
  }
  return false;
}

sh::MoveCmd tic() {
  sh::MoveCmd cmd;
  cmd.msec = 13;
  return cmd;
}

std::vector<std::string> row_of(const std::string& name, const Count& c) {
  return {name,
          std::to_string(c.all),
          std::to_string(c.solid),
          std::to_string(c.waiting),
          std::to_string(c.moved) + "/" + std::to_string(c.woken),
          std::to_string(c.saw) + "/" + std::to_string(c.duels),
          std::to_string(c.attacked) + "/" + std::to_string(c.duels),
          std::to_string(c.hurt) + "/" + std::to_string(c.duels),
          std::to_string(c.died)};
}

// THE CENSUS of map `name`: each monster counted by kind, every one not waiting woken by a blow
// and watched `seconds` for whether it moves off its spot (`moved`), into `c` and `kinds`.
void census(gl::Lab& lab, const cl::SearchPath& files, sv::Q2Pack pack, const std::string& name,
            double seconds, Count& c, std::map<std::string, Count>& kinds) {
  cl::Listen l;
  sv::Q2Server* sq = start(lab, files, name, l, pack);
  if (sq == nullptr) return;
  const size_t n = sq->monsters.size();
  std::vector<sh::Vec3f> spot(n);
  std::vector<uint8_t> woken(n, 0), moved(n, 0);
  for (size_t k = 0; k < n; k++) {
    const sv::Q2Monster& m = sq->monsters[k];
    spot[k] = sq->entities[size_t(m.entity)].origin;
    woken[k] = !m.waiting && !m.dead ? 1 : 0;
  }
  for (size_t k = 0; k < n; k++)
    if (woken[k] != 0) sv::map_damage(*sq, sq->monsters[k].entity, 0, 1);
  for (int f = 0; f < static_cast<int>(seconds / kTic); f++) {
    cl::listen_frame(l, tic());
    sq = sv::q2_of(*l.rules);
    sq->players[0].body.health = 100;  // kept alive: the sweep is about the monsters
    for (size_t k = 0; k < n && k < sq->monsters.size(); k++)
      if (sh::length(sq->entities[size_t(sq->monsters[k].entity)].origin - spot[k]) > kMoved)
        moved[k] = 1;
  }
  for (size_t k = 0; k < n && k < sq->monsters.size(); k++) {
    const sv::Q2Monster& m = sq->monsters[k];
    for (Count* x : {&c, &kinds[sq->entities[size_t(m.entity)].classname]}) {
      x->all++;
      x->solid += m.start_solid ? 1 : 0;
      x->waiting += woken[k] == 0 ? 1 : 0;
      x->woken += woken[k];
      x->moved += woken[k] != 0 && moved[k] != 0 ? 1 : 0;
      x->died += m.dead ? 1 : 0;
    }
  }
}

// THE DUELS on map `name`: for each kind there, the map started again, every other monster
// killed, the player stood `range` units off the first of the kind that stands in the open, and
// the monster woken: `hurt` once a blow takes his health within `seconds`.
void duels(gl::Lab& lab, const cl::SearchPath& files, sv::Q2Pack pack, const std::string& name,
           double seconds, float range, Count& c, std::map<std::string, Count>& kinds) {
  std::vector<std::string> names;
  {
    cl::Listen l;
    sv::Q2Server* sq = start(lab, files, name, l, pack);
    if (sq == nullptr) return;
    for (const sv::Q2Monster& m : sq->monsters) {
      const std::string& cn = sq->entities[size_t(m.entity)].classname;
      if (!m.waiting && !m.dead && std::find(names.begin(), names.end(), cn) == names.end())
        names.push_back(cn);
    }
  }
  const std::string only = "," + lab.arg("kinds") + ",";
  for (const std::string& kind : names) {
    if (only != ",," && only.find("," + kind + ",") == std::string::npos) continue;
    cl::Listen l;
    sv::Q2Server* sq = start(lab, files, name, l, pack);
    if (sq == nullptr) return;
    int target = -1;
    sh::Vec3f stand;
    for (const sv::Q2Monster& m : sq->monsters) {
      const sv::Q2Entity& e = sq->entities[size_t(m.entity)];
      // One sent to a combat point first runs there and holds it, blind to him on the way: no duel.
      if (target >= 0 || m.waiting || m.dead || e.classname != kind || m.combat >= 0) continue;
      for (const float share : {1.f, 0.6f, 0.35f})
        if (target < 0 && stand_off(l, e.origin, range * share, stand)) target = m.entity;
    }
    if (target < 0) continue;  // none of it in the open here
    for (const sv::Q2Monster& m : sq->monsters)
      if (m.entity != target && !m.dead && !m.waiting)
        sv::map_damage(*sq, m.entity, 0, sq->entities[size_t(m.entity)].health + 10);
    const bool blow = lab.arg("wake") == "blow";
    if (blow) sv::map_damage(*sq, target, 0, 1);
    bool hurt = false, saw = false, attacked = false;
    const sh::Vec3f start_at = sq->entities[size_t(target)].origin;
    double busy_was = 0;
    for (const sv::Q2Monster& m : sq->monsters)
      if (m.entity == target) busy_was = m.busy_until;
    for (int f = 0; f < static_cast<int>(seconds / kTic) && !hurt; f++) {
      if (!blow && f == int(3 / kTic))  // not woken by the sight of him: a blow of 1 does it
        for (const sv::Q2Monster& m : sq->monsters)
          if (m.entity == target && m.enemy < 0) sv::map_damage(*sq, target, 0, 1);
      sv::Q2Player& p = sq->players[0];
      for (int a = 0; a < 3; a++)
        p.move.origin[a] = static_cast<int16_t>(std::lround((&stand.x)[a] * 8));
      for (int32_t& v : p.move.velocity) v = 0;
      p.body.health = 100;
      const int64_t hit_was = p.hit_frame;
      cl::listen_frame(l, tic());
      sq = sv::q2_of(*l.rules);
      // A blow on him moves his status bar's hit frame (his health is put back within the frame).
      hurt = sq->players[0].hit_frame != hit_was;
      for (const sv::Q2Monster& m : sq->monsters) {
        if (m.entity != target) continue;
        saw = saw || m.enemy == 0;
        if (lab.arg("trace") == "1" && f % 20 == 0) {
          const sv::Q2Entity& e = sq->entities[size_t(target)];
          lab.note("trace " + kind + " t=" + ge::core::num::to_fixed(f * kTic, 2) + " at " +
                   at(e.origin) + " enemy " + std::to_string(m.enemy) + " attack " +
                   std::to_string(m.attack) + " busy " +
                   ge::core::num::to_fixed(m.busy_until - sq->level_time, 2) + " finished " +
                   ge::core::num::to_fixed(m.attack_finished - sq->level_time, 2) + " combat " +
                   std::to_string(m.combat) + " hold " + std::to_string(m.hold) + " lost " +
                   std::to_string(m.lost) + " corner " + std::to_string(m.corner) + " health " +
                   std::to_string(e.health));
        }
        attacked = attacked || m.attack != 0 || m.busy_until > busy_was;
      }
    }
    for (Count* x : {&c, &kinds[kind]}) {
      x->duels++;
      x->hurt += hurt ? 1 : 0;
      x->saw += saw ? 1 : 0;
      x->attacked += attacked ? 1 : 0;
    }
    if (!hurt) {
      std::string why = "se-q2-pack-monsters: " + name + ": " + kind + " never hurt him (";
      why += !saw        ? "never took him for its enemy"
             : !attacked ? "saw him, never attacked"
                         : "attacked, never landed";
      why += "; it at " + at(start_at) + ", him at " + at(stand) + ")";
      lab.note(why);
    }
  }
}

void run(gl::Lab& lab) {
  if (lab.arg("game").empty()) {
    lab.note(
        "se-q2-pack-monsters reads the player's own packs: pass --game=<the folder holding "
        "baseq2 and the pack's folder>.");
    return;
  }
  const std::string mod = lab.arg("mod");
  const sv::Q2Pack pack = sv::q2_pack_of_folder(mod);
  if (pack == sv::Q2Pack::base) {
    lab.fail("--mod=" + mod + " is not a pack this lab sweeps",
             "Pass --mod=xatrix (The Reckoning) or --mod=rogue (Ground Zero). Skill: shooter.");
    return;
  }
  const pf::GameProfile& game = *pf::find_game("q2");
  const ge::classicenhance::Profile& content = pf::content_of(game);
  cl::SearchPath files;
  std::string error;
  if (!cl::open_game_dir(lab.arg("game"), mod, files, error, content)) {
    lab.fail(error, "Pass --game=<the folder holding " + std::string(content.base_dir) + " and " +
                        mod + ">. Skill: shooter.");
    return;
  }
  std::vector<std::string> maps;
  if (!lab.arg("maps").empty()) {
    const std::string& list = lab.arg("maps");
    for (size_t a = 0; a < list.size();) {
      const size_t b = std::min(list.find(',', a), list.size());
      if (b > a) maps.push_back(list.substr(a, b - a));
      a = b + 1;
    }
  } else {
    if (pack == sv::Q2Pack::reckoning)
      for (const char* m : kReckoningMaps) maps.emplace_back(m);
    else
      for (const char* m : kGroundZeroMaps) maps.emplace_back(m);
  }
  const double seconds = lab.arg_num("seconds");
  const double duel = lab.arg_num("duel");
  const float range = static_cast<float>(lab.arg_num("range"));
  gl::LabTable by_map;
  by_map.title = "Each map's monsters: moved of those woken in " +
                 ge::core::num::to_fixed(seconds, 0) + " s; kinds that hurt him in a duel of " +
                 ge::core::num::to_fixed(duel, 0) + " s";
  by_map.columns = {"map",     "monsters", "in solid", "waiting", "moved",
                    "saw him", "attacked", "hurt him", "died"};
  std::map<std::string, Count> kinds;
  Count total;
  for (const std::string& name : maps) {
    Count c;
    census(lab, files, pack, name, seconds, c, kinds);
    duels(lab, files, pack, name, duel, range, c, kinds);
    by_map.rows.push_back(row_of(name, c));
    for (int Count::*f : {&Count::all, &Count::solid, &Count::waiting, &Count::woken, &Count::moved,
                          &Count::duels, &Count::saw, &Count::attacked, &Count::hurt, &Count::died})
      total.*f += c.*f;
    lab.metric(name + ".monsters", c.all, "count", gl::Det::measured);
    lab.metric(name + ".moved", c.moved, "count", gl::Det::measured);
    lab.metric(name + ".hurt", c.hurt, "count", gl::Det::measured);
  }
  by_map.rows.push_back(row_of("all", total));
  lab.table(by_map, gl::Det::measured);
  gl::LabTable by_kind;
  by_kind.title = "Each kind over every map: moved of those woken, duels that hurt him";
  by_kind.columns = {"kind",    "monsters", "in solid", "waiting", "moved",
                     "saw him", "attacked", "hurt him", "died"};
  for (const auto& [kind, c] : kinds) by_kind.rows.push_back(row_of(kind, c));
  lab.table(by_kind, gl::Det::measured);
  for (const auto& [kind, c] : kinds) {
    if (c.woken > 0 && c.moved == 0)
      lab.note("se-q2-pack-monsters: " + kind + " never moved on any map (" +
               std::to_string(c.woken) + " woken): look at its think.");
    if (c.duels > 0 && c.hurt == 0)
      lab.note("se-q2-pack-monsters: " + kind + " hurt him in none of its " +
               std::to_string(c.duels) + " duels: look at its attack.");
  }
  lab.metric("all.monsters", total.all, "count", gl::Det::measured);
  lab.metric("all.in_solid", total.solid, "count", gl::Det::measured);
  lab.metric("all.hurt", total.hurt, "count", gl::Det::measured);
}

const gl::LabSpec kSpec{
    "se-q2-pack-monsters",
    gl::LabKind::sim,
    "game:shooters_edge",
    "a Quake II mission pack's monsters on its own maps (server/q2_monsters.cpp and the pack's "
    "monsters beside it)",
    "shooter",
    "Does every monster of the pack play on the pack's own maps? Each one counted by kind, woken "
    "by a blow and watched: did it move, did it attack, did it start inside a wall?",
    {
        {"game", "", "the folder holding Quake II's base folder and the pack's; empty: nothing"},
        {"mod", "xatrix", "the pack's folder (xatrix: The Reckoning; rogue: Ground Zero)"},
        {"maps", "", "the maps, comma-separated (empty: every single-player map of the pack)"},
        {"seconds", "20", "seconds each map's woken monsters are watched"},
        {"wake", "sight",
         "how a duel's monster wakes: sight (his, a blow after 3 s if not) or blow"},
        {"duel", "8", "seconds a kind is given to hurt the player in its duel"},
        {"range", "256", "units off the monster the player stands for a duel"},
        {"trace", "0", "1: a duel's monster's state every 0.26 s, as notes"},
        {"kinds", "", "only these kinds' duels, comma-separated (empty: every kind)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
