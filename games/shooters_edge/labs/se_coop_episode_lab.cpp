// LAB `se-coop-episode`: can a co-op party play an episode through, start to finish? Our campaign
// in coop on a chain of maps (games/shooters_edge/bots/coop_episode.hpp): a fighter leads, finding
// each map's way out by its own entities (way_through.hpp), and the companions keep beside him.
// With
// `--game` (the player's own release zip, pack or folder) the maps are the pack's, from `--first`
// on; without it, the coop hall with an exit behind its button door, twice (the smoke run).
// The table, a map a row: got out or not and in how long, the monsters and who they fell to, the
// deaths, how near the companions kept, the buttons they pressed, what the leader's plan went for.
// Pictures, a map each: its floor from above (the nav's nodes shaded by height), the leader's track
// and the companions', the exits, where monsters fell.
//
// ge:summary Lab: a co-op episode played through, map by map: got out, kills, nearness, tracks
// ge:keywords lab se-coop-episode coop episode play-through e1m1 leader companions way-through
// ge:keywords exit tracks real-maps
// ge:skill shooter bots labs
// ge:test tests/shooters_edge_companion_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "classicenhance/source.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/bots/coop_episode.hpp"
#include "shooters_edge/bots/coop_run.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace fb = shooters_edge::bots;
namespace sv = shooters_edge::server;
namespace ce = ge::classicenhance;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

const gl::Rgba kInk{30, 30, 30, 255};
const gl::Rgba kPaper{246, 246, 240, 255};
const gl::Rgba kLeader{210, 60, 40, 255};
const gl::Rgba kMates[3] = {{30, 90, 210, 255}, {20, 150, 90, 255}, {150, 70, 190, 255}};
const gl::Rgba kExit{240, 170, 20, 255};
const gl::Rgba kFell{40, 40, 40, 255};

// The packs' maps for the campaign host: a map file read into the spec, its bytes kept until the
// next call.
struct PackMaps {
  std::vector<uint8_t> content;
  ge::vfs::Pak pak;
  std::unique_ptr<sv::BspMap> bsp;
};

bool pack_map(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  auto& m = *static_cast<PackMaps*>(ctx);
  const ge::vfs::PakEntry* e = ge::vfs::pak_find(m.pak, ("maps/" + name + ".bsp").c_str());
  if (e == nullptr) {
    error = "no maps/" + name + ".bsp in the packs; next: pass --first with a map they hold";
    return false;
  }
  m.bsp = std::make_unique<sv::BspMap>();
  return sv::read_bsp_map(ge::vfs::pak_file(m.content, *e), name, m.bsp.get(), &out, error);
}

bool parse_skill(const std::string& w, sh::AimSkill& out) {
  for (const sh::AimSkill k :
       {sh::AimSkill::novice, sh::AimSkill::average, sh::AimSkill::expert, sh::AimSkill::perfect})
    if (w == sh::aim_skill_name(k)) {
      out = k;
      return true;
    }
  return false;
}

// A map from above, scaled to fit `size` pixels on its longer side: the nodes shaded by height,
// the tracks, the exits and where monsters fell.
gl::Picture map_picture(const fb::EpisodeMap& m, int size) {
  const float wx = std::max(m.hi.x - m.lo.x, 64.f), wy = std::max(m.hi.y - m.lo.y, 64.f);
  const float k = static_cast<float>(size) / std::max(wx, wy);
  const int margin = 28;
  const int w = static_cast<int>(wx * k) + 2 * margin, h = static_cast<int>(wy * k) + 2 * margin;
  gl::Picture p = gl::create_picture(w, h, kPaper);
  const auto px = [&](float x) { return margin + (x - m.lo.x) * k; };
  const auto py = [&](float y) { return margin + (m.hi.y - y) * k; };
  const float zspan = std::max(m.hi.z - m.lo.z, 1.f);
  std::vector<sh::Vec3f> nodes = m.nodes;
  std::sort(nodes.begin(), nodes.end(), [](sh::Vec3f a, sh::Vec3f b) { return a.z < b.z; });
  const int dot = std::max(1, static_cast<int>(std::lround(32 * k)));
  for (const sh::Vec3f& n : nodes) {
    const float t = (n.z - m.lo.z) / zspan;
    const auto g = static_cast<uint8_t>(205 - 120 * t);
    gl::fill_rect(p, px(n.x) - dot * 0.5, py(n.y) - dot * 0.5, dot, dot,
                  {g, static_cast<uint8_t>(g + 12 < 255 ? g + 12 : 255), g, 255});
  }
  const auto trail = [&](const std::vector<sh::Vec3f>& path, gl::Rgba c) {
    for (size_t i = 0; i + 1 < path.size(); i++) {
      const sh::Vec3f a = path[i], b = path[i + 1];
      const float dx = b.x - a.x, dy = b.y - a.y;
      if (dx * dx + dy * dy > 400 * 400) continue;  // a respawn or a teleporter
      gl::draw_line(p, px(a.x), py(a.y), px(b.x), py(b.y), c, 2);
    }
  };
  for (size_t j = 0; j < m.mates.size(); j++) trail(m.mates[j], kMates[j % 3]);
  trail(m.leader, kLeader);
  for (const sh::Vec3f& f : m.kills) {
    gl::draw_line(p, px(f.x) - 4, py(f.y) - 4, px(f.x) + 4, py(f.y) + 4, kFell, 2);
    gl::draw_line(p, px(f.x) - 4, py(f.y) + 4, px(f.x) + 4, py(f.y) - 4, kFell, 2);
  }
  for (const sh::Vec3f& e : m.exits) {
    gl::draw_ring(p, px(e.x), py(e.y), 10, kExit);
    gl::draw_ring(p, px(e.x), py(e.y), 11, kExit);
  }
  if (!m.leader.empty()) {
    const sh::Vec3f s = m.leader.front();
    gl::fill_rect(p, px(s.x) - 5, py(s.y) - 5, 10, 10, kLeader);
  }
  int x = 8;
  const auto key = [&](const std::string& name, gl::Rgba c) {
    gl::fill_rect(p, x, 9, 14, 6, c);
    gl::draw_text(p, x + 18, 8, name, kInk);
    x += 18 + gl::text_width(name) + 14;
  };
  key("LEADER (SQUARE: START)", kLeader);
  for (size_t j = 0; j < m.mates.size(); j++) key("MATE" + std::to_string(j + 1), kMates[j % 3]);
  key("EXIT", kExit);
  key("FELL", kFell);
  return p;
}

void run(gl::Lab& lab) {
  fb::EpisodeOptions o;
  o.root = lab.arg("root");
  o.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  o.skill = static_cast<int>(lab.arg_num("skill"));
  o.maps = static_cast<int>(lab.arg_num("maps"));
  o.map_seconds = lab.arg_num("map-seconds");
  o.plan = lab.arg("plan") == "1";
  o.skills.clear();
  {
    std::stringstream in(lab.arg("skills"));
    std::string w;
    while (std::getline(in, w, ',')) {
      sh::AimSkill k;
      if (!parse_skill(w, k)) {
        lab.fail("se-coop-episode: no skill " + w, "next: --skills=novice,average,expert,perfect");
        return;
      }
      o.skills.push_back(k);
    }
  }
  if (!parse_skill(lab.arg("leader"), o.leader)) {
    lab.fail("se-coop-episode: no skill " + lab.arg("leader"), "next: --leader=expert");
    return;
  }
  PackMaps packs;
  const std::string& game = lab.arg("game");
  if (game.empty()) {
    o.load = &fb::coop_hall_exit_map;
    o.first = "hall";
  } else {
    ce::Source src;
    std::string error;
    if (!ce::open_source(game, &src, &error)) {
      lab.fail("se-coop-episode: " + error,
               "next: pass --game=<a release zip, a pack or a folder>");
      return;
    }
    packs.content = src.content();
    if (!ge::vfs::read_pak(packs.content, &packs.pak, &error)) {
      lab.fail("se-coop-episode: the pack does not read: " + error, "next: check the --game path");
      return;
    }
    o.load = &pack_map;
    o.load_ctx = &packs;
    o.first = lab.arg("first");
  }
  const fb::EpisodeReport r = fb::run_coop_episode(o);
  if (!r.ok) {
    lab.fail("se-coop-episode: " + r.error, "next: read the error; `labs se-coop` for the hall");
    return;
  }
  const In in = {{"game", game.empty() ? "(the coop hall)" : "the player's own"},
                 {"first", o.first},
                 {"skills", lab.arg("skills")},
                 {"leader", lab.arg("leader")},
                 {"skill", lab.arg("skill")},
                 {"seed", lab.arg("seed")}};
  gl::LabTable t{"each map of the episode",
                 {"map", "out", "s", "ended", "monsters", "by leader", "by mates", "unseen",
                  "leader died", "mates died", "near share", "farthest", "mate buttons",
                  "plans: exit/button/key/trigger/explore", "steps left", "nodes", "faults"},
                 {}};
  int finished = 0, kills = 0, mate_kills = 0, monsters = 0;
  double near_sum = 0;
  for (const fb::EpisodeMap& m : r.maps) {
    finished += m.finished ? 1 : 0;
    kills += m.killed_by_leader + m.killed_by_mates + m.killed_unseen;
    mate_kills += m.killed_by_mates;
    monsters += m.monsters;
    near_sum += m.near_share;
    std::string plans;
    for (int k = 0; k < fb::kWayKinds; k++)
      plans += (k ? "/" : "") + std::to_string(m.way.by_kind[k]);
    t.rows.push_back({m.name, m.finished ? "yes" : "no", to_fixed(m.seconds, 1), m.end,
                      std::to_string(m.monsters), std::to_string(m.killed_by_leader),
                      std::to_string(m.killed_by_mates), std::to_string(m.killed_unseen),
                      std::to_string(m.leader_deaths), std::to_string(m.mate_deaths),
                      to_fixed(m.near_share, 3), to_fixed(m.farthest, 0),
                      std::to_string(m.mate_buttons), plans, std::to_string(m.way.left),
                      std::to_string(m.nav_nodes), std::to_string(m.faults)});
  }
  lab.table(t);
  const double n = r.maps.empty() ? 1 : static_cast<double>(r.maps.size());
  lab.metric("maps.played", static_cast<double>(r.maps.size()), "count");
  lab.metric("maps.finished", finished, "count");
  lab.metric("monsters", monsters, "count");
  lab.metric("kills", kills, "count");
  lab.metric("kills.by_mates", mate_kills, "count");
  lab.metric("near_share.mean", near_sum / n, "share");
  lab.metric("faults", static_cast<double>(r.faults), "count");
  for (const fb::EpisodeMap& m : r.maps) {
    if (m.plan.empty()) continue;
    gl::LabTable pt{"the leader's plan on " + m.name, {"change"}, {}};
    for (const std::string& line : m.plan) pt.rows.push_back({line});
    lab.table(pt);
  }
  char hex[24];
  std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(r.digest));
  lab.note(std::string("the run's digest (every frame's players folded): ") + hex);
  for (size_t i = 0; i < r.maps.size(); i++) {
    const fb::EpisodeMap& m = r.maps[i];
    lab.shot({"map-" + std::to_string(i + 1) + "-" + m.name,
              "MAP " + m.name + (m.finished ? ": OUT" : ": NOT OUT"),
              "The map from above (its walkable floor, darker higher), the leader's track and each "
              "companion's, the exits and where monsters fell; " +
                  to_fixed(m.seconds, 0) + " s, near share " + to_fixed(m.near_share, 2) + ".",
              "the leader's track ends at an exit; the companions' tracks lie over his", in},
             map_picture(m, 900));
  }
}

const gl::LabSpec kSpec{
    "se-coop-episode",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/bots/coop_episode.hpp, way_through.hpp and companion.hpp: our campaign "
    "in coop over a chain of maps",
    "bots",
    "Does a co-op party get through an episode, map after map, the companions beside the one who "
    "leads it the whole way?",
    {
        {"game", "", "the player's own release zip, pack or folder (none: the coop hall twice)"},
        {"first", "e1m1", "with --game: the map the episode starts on"},
        {"maps", "8", "at most this many maps"},
        {"map-seconds", "600", "a map's time before it is called not got out of, s"},
        {"skills", "expert", "one companion a skill, a comma list: novice, average, expert"},
        {"leader", "expert", "the leader's skill"},
        {"skill", "1", "the campaign's skill (0..3)"},
        {"seed", "1", "the campaign's and the players' seed"},
        {"root", ".", "the repository's root (the campaign's sources are under it)"},
        {"plan", "0", "1: each map's changes of the leader's plan as a table"},
    },
    "--maps=2 --map-seconds=60",
    run,
};
GE_LAB(kSpec);

}  // namespace
