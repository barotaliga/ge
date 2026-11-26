// LAB `se-q1-campaign`: can a bot clear Quake's whole single-player campaign, every map at 100%?
// The leader alone in single player on the pack's own program (coop_episode.hpp's party of one),
// the 100% run on (way_through.hpp `complete`: every place, button, secret and monster, then the
// exit) and `god` on, so a map's clear is about the way through it and not the fight. Each episode
// is played from its first map along its normal exits to its end (an exit to `start` or `end`), and
// each secret level on its own from its start; then `end`. A map not left within `--map-seconds`
// is a failure.
//
// With `--game` (the player's own release zip, pack or folder) the maps are the pack's; without it,
// the coop hall (a button door before the exit) once: the smoke run.
//
// A MISSION PACK (`--mod=rogue`, `--mod=hipnotic`): the mod folder's packs beside the game's
// `id1` are laid over its packs, the mod's program and maps winning, as the game mounts them, and
// `--episodes` names the pack's own episodes (Scourge of Armagon's three and `hipend`,
// Dissolution of Eternity's two: `campaign_of`) or its maps (`r1m1,r1m2`). THE CENSUS
// (`--census=1`, on with `--mod`): a table of every class name the maps placed or the run made, how
// many stood, were made, moved, went and had their model taken (an item picked up)
// (coop_episode.hpp `census`), so a pack's monsters, items, movers and traps are seen at work;
// `--classes` keeps the rows of those names alone.
//
// THE PARTY (`--party=1,2,3`): each map played again by a party of that many bots in co-op
// (coop_episode.hpp's partners, plugins/shooterbots/party.hpp: each stop to the nearest bot, the
// exit once nothing is left for any), so the table says how much sooner two or three clear it; a
// party's picture draws each bot's track in its own colour. With `--beside=1` the leader plays as a
// person would and the others are helpers beside him (a hosted game's `addbot helper`): a second
// table says how near they kept to him and how much of the map they took off his hands.
//
// The table, a map a row: got out or not, the time to the exit (the intermission's start), the
// level's own count of monsters killed and secrets found (the program's globals), the deaths, the
// things given up. Pictures, a map each: its floor from above, his track, the exits and where
// monsters fell.
//
// ge:summary Lab: Quake's campaign played at 100% by a bot: kills, secrets and time to the exit
// ge:keywords lab se-q1-campaign campaign single-player 100-percent complete kills secrets
// ge:keywords exit-time episodes secret-level god play-through party co-op partners
// ge:keywords mission-pack mod rogue hipnotic census class-names
// ge:skill shooter bots labs
// ge:test tests/shooters_edge_companion_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "classicenhance/source.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "progs/file.hpp"
#include "shooters_edge/bots/coop_episode.hpp"
#include "shooters_edge/bots/coop_run.hpp"
#include "shooters_edge/livemap/route.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sb = ge::shooterbots;
namespace fb = shooters_edge::bots;
namespace sv = shooters_edge::server;
namespace ce = ge::classicenhance;
namespace pg = ge::progs;
namespace lv = shooters_edge::livemap;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

const gl::Rgba kInk{30, 30, 30, 255};
const gl::Rgba kPaper{246, 246, 240, 255};
const gl::Rgba kTrack{210, 60, 40, 255};
const gl::Rgba kExit{240, 170, 20, 255};
const gl::Rgba kFell{40, 40, 40, 255};

struct PackMaps {
  std::vector<uint8_t> content;
  ge::vfs::Pak pak;
  std::unique_ptr<sv::BspMap> bsp;
  pg::Progs progs;
};

bool pack_map(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  auto& m = *static_cast<PackMaps*>(ctx);
  const ge::vfs::PakEntry* e = ge::vfs::pak_find(m.pak, ("maps/" + name + ".bsp").c_str());
  if (e == nullptr) {
    error = "no maps/" + name + ".bsp in the packs; next: pass --episodes with ones they hold";
    return false;
  }
  m.bsp = std::make_unique<sv::BspMap>();
  return sv::read_bsp_map(ge::vfs::pak_file(m.content, *e), name, m.bsp.get(), &out, error);
}

// A run: the map it starts on, the exits it leaves alone (the secret level's), at most `maps`.
struct Leg {
  std::string first;
  std::vector<std::string> avoid;
  int maps = 8;
};

// A campaign's episodes: each one's maps in their order, its secret level among them (reached by
// a secret exit and left back into the episode; none for an episode without one), and the end.
struct Episode {
  std::vector<std::string> maps;
  std::string secret;
};
struct Campaign {
  std::vector<Episode> episodes;
  std::string end;           // the last level on its own (empty: none)
  std::string episodes_arg;  // what `--episodes` plays when left at its default
};

std::vector<std::string> numbered(const std::string& stem, int n) {
  std::vector<std::string> out;
  for (int m = 1; m <= n; m++) out.push_back(stem + std::to_string(m));
  return out;
}

// The campaign a mod folder plays: Quake's four episodes, or a mission pack's own (their maps and
// secret levels read off each map's changelevel triggers): Scourge of Armagon's three, each with a
// secret level, and `hipend`; Dissolution of Eternity's two, with none and no end map apart.
const Campaign& campaign_of(const std::string& mod) {
  static const Campaign quake = {{{numbered("e1m", 8), "e1m8"},
                                  {numbered("e2m", 7), "e2m7"},
                                  {numbered("e3m", 7), "e3m7"},
                                  {numbered("e4m", 8), "e4m8"}},
                                 "end",
                                 "1,2,3,4,end"};
  static const Campaign scourge = {{{numbered("hip1m", 5), "hip1m5"},
                                    {numbered("hip2m", 6), "hip2m6"},
                                    {{"hip3m1", "hip3m2", "hip3m3", "hip3m4", "hipdm1"}, "hipdm1"}},
                                   "hipend",
                                   "1,2,3,end"};
  static const Campaign dissolution = {
      {{numbered("r1m", 7), ""}, {numbered("r2m", 8), ""}}, "", "1,2"};
  if (mod == "hipnotic") return scourge;
  if (mod == "rogue") return dissolution;
  return quake;
}

// The legs of `--episodes` (a comma list of the campaign's episode numbers and `end`): an
// episode's normal way, then its secret level on its own; with `each`, every map of the episode
// on its own (one map that fails no longer ends the rest of its episode's run).
bool legs_of(const Campaign& c, const std::string& list, bool each, std::vector<Leg>& out,
             std::string& bad) {
  std::stringstream in(list);
  std::string w;
  while (std::getline(in, w, ',')) {
    const int k = w.size() == 1 && w[0] >= '1' && w[0] <= '9' ? w[0] - '1' : -1;
    if (w == "end" && !c.end.empty()) {
      out.push_back({c.end, {}, 1});
    } else if (k >= 0 && k < static_cast<int>(c.episodes.size())) {
      const Episode& e = c.episodes[static_cast<size_t>(k)];
      std::vector<std::string> avoid;
      if (!e.secret.empty()) avoid.push_back(e.secret);
      if (each) {
        for (const std::string& name : e.maps)
          out.push_back({name, name == e.secret ? std::vector<std::string>{} : avoid, 1});
        continue;
      }
      out.push_back({e.maps.front(), avoid, static_cast<int>(e.maps.size() - avoid.size())});
      if (!e.secret.empty()) out.push_back({e.secret, {}, 1});
    } else if (!w.empty() && k < 0 && w != "end") {
      out.push_back({w, {}, 1});  // one map by its name
    } else {
      bad = w;
      return false;
    }
  }
  return true;
}

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
  for (size_t i = 0; i + 1 < m.leader.size(); i++) {
    const sh::Vec3f a = m.leader[i], b = m.leader[i + 1];
    const float dx = b.x - a.x, dy = b.y - a.y;
    if (dx * dx + dy * dy > 400 * 400) continue;  // a teleporter
    gl::draw_line(p, px(a.x), py(a.y), px(b.x), py(b.y), kTrack, 2);
  }
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
    gl::fill_rect(p, px(s.x) - 5, py(s.y) - 5, 10, 10, kTrack);
  }
  int x = 8;
  const auto key = [&](const std::string& name, gl::Rgba c) {
    gl::fill_rect(p, x, 9, 14, 6, c);
    gl::draw_text(p, x + 18, 8, name, kInk);
    x += 18 + gl::text_width(name) + 14;
  };
  key("TRACK (SQUARE: START)", kTrack);
  key("EXIT", kExit);
  key("FELL", kFell);
  return p;
}

// The route on the live map's plate of the map in the packs (`ok` false: none to draw it on).
gl::Picture route_on_plate(const PackMaps& packs, const fb::EpisodeMap& m, bool& ok) {
  ok = false;
  gl::Picture out;
  const ge::vfs::PakEntry* pal = ge::vfs::pak_find(packs.pak, "gfx/palette.lmp");
  const ge::vfs::PakEntry* bsp = ge::vfs::pak_find(packs.pak, ("maps/" + m.name + ".bsp").c_str());
  if (pal == nullptr || bsp == nullptr) return out;
  lv::Plate plate;
  std::string error;
  if (!lv::build_plate(ge::vfs::pak_file(packs.content, *bsp), m.name,
                       ge::vfs::pak_file(packs.content, *pal), 1400, &plate, &error))
    return out;
  lv::RouteMarks marks;
  const auto put = [](std::vector<std::array<float, 3>>& to, const std::vector<sh::Vec3f>& from) {
    for (const sh::Vec3f& v : from) to.push_back({v.x, v.y, v.z});
  };
  put(marks.track, m.leader);
  for (size_t k = 0; k < m.mates.size(); k++) {
    marks.party.emplace_back();
    put(marks.party.back(), m.mates[k]);
  }
  put(marks.kills, m.kills);
  put(marks.secrets, m.secrets);
  put(marks.exits, m.exits);
  for (const std::vector<sh::Vec3f>& b : m.blasts) {
    marks.blasts.emplace_back();
    put(marks.blasts.back(), b);
  }
  const ge::ui::Image img = lv::draw_route(plate, marks);
  out.width = img.width;
  out.height = img.height;
  out.rgba = img.rgba;
  ok = true;
  return out;
}

// The mod folder's packs (`<game>/<mod>/pak0.pak`, `pak1.pak` ... in order) laid over `base`:
// every file of every pack once, the latest copy of a path winning. False with the reason.
bool lay_mod(const std::string& game, const std::string& mod, std::vector<uint8_t>& base,
             std::string& error) {
  namespace fs = std::filesystem;
  fs::path dir = fs::path(game) / mod;
  if (!fs::is_directory(dir)) dir = fs::path(game).parent_path() / mod;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  std::map<std::string, size_t> at;
  const auto take = [&](std::span<const uint8_t> bytes) {
    ge::vfs::Pak pak;
    std::string why;
    if (!ge::vfs::read_pak(bytes, &pak, &why)) return false;
    for (const ge::vfs::PakEntry& e : pak.entries) {
      const std::span<const uint8_t> b = ge::vfs::pak_file(bytes, e);
      const auto [it, fresh] = at.emplace(e.path, files.size());
      if (fresh)
        files.emplace_back(e.path, std::vector<uint8_t>(b.begin(), b.end()));
      else
        files[it->second].second.assign(b.begin(), b.end());
    }
    return true;
  };
  take(base);
  int read = 0;
  for (int n = 0;; n++) {
    const fs::path p = dir / ("pak" + std::to_string(n) + ".pak");
    std::ifstream in(p, std::ios::binary);
    if (!in) break;
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
    if (!take(bytes)) {
      error = p.string() + " does not read as a pack";
      return false;
    }
    read++;
  }
  if (read == 0) {
    error = "no pak0.pak in " + dir.string();
    return false;
  }
  return ge::vfs::write_pak(files, &base, &error);
}

std::string of(int a, int b) {
  return a < 0 || b < 0 ? "-" : std::to_string(a) + "/" + std::to_string(b);
}

void run(gl::Lab& lab) {
  fb::EpisodeOptions o;
  o.root = lab.arg("root");
  o.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  o.skill = static_cast<int>(lab.arg_num("skill"));
  o.map_seconds = lab.arg_num("map-seconds");
  o.plan = lab.arg("plan") == "1";
  o.skills.clear();
  o.single = true;
  o.complete = lab.arg("complete") != "0";
  o.tour = lab.arg("tour") != "0";
  o.god = lab.arg("god") != "0";
  o.stop = {"start", "end"};
  o.census = lab.arg("census") == "1" || (lab.arg("census") != "0" && !lab.arg("mod").empty());
  PackMaps packs;
  const std::string& game = lab.arg("game");
  std::vector<Leg> legs;
  std::string episodes = lab.arg("episodes");  // a mission pack's own by default (below)
  if (game.empty()) {
    o.load = &fb::coop_hall_exit_map;
    legs.push_back({"hall", {}, 1});
  } else {
    ce::Source src;
    std::string error;
    if (!ce::open_source(game, &src, &error)) {
      lab.fail("se-q1-campaign: " + error, "next: pass --game=<a release zip, a pack or a folder>");
      return;
    }
    packs.content = src.content();
    const std::string& mod = lab.arg("mod");
    if (!mod.empty()) {
      std::string why;
      if (!lay_mod(game, mod, packs.content, why)) {
        lab.fail("se-q1-campaign: the mod " + mod + ": " + why,
                 "next: pass --game=<the game's folder> holding id1 and " + mod);
        return;
      }
    }
    if (!ge::vfs::read_pak(packs.content, &packs.pak, &error)) {
      lab.fail("se-q1-campaign: the pack does not read: " + error, "next: check the --game path");
      return;
    }
    o.load = &pack_map;
    o.load_ctx = &packs;
    o.wide_models = true;
    o.registered = ge::vfs::pak_find(packs.pak, "gfx/pop.lmp") != nullptr;
    const ge::vfs::PakEntry* e = ge::vfs::pak_find(packs.pak, "progs.dat");
    const std::span<const uint8_t> bytes =
        e != nullptr ? ge::vfs::pak_file(packs.content, *e) : std::span<const uint8_t>{};
    if (e == nullptr ||
        !pg::load_progs(bytes.data(), bytes.size(), packs.progs, error, pg::every_form())) {
      lab.fail("se-q1-campaign: the pack's program does not load: " + error,
               "next: pass the game's own folder (its id1)");
      return;
    }
    o.progs = &packs.progs;
    std::string bad;
    // A mission pack's own episodes unless others are asked for: its maps, not Quake's.
    const Campaign& campaign = campaign_of(mod);
    if (lab.arg("episodes") == campaign_of("").episodes_arg) episodes = campaign.episodes_arg;
    if (!legs_of(campaign, episodes, lab.arg("each") == "1", legs, bad)) {
      lab.fail("se-q1-campaign: no episode " + bad + " in this campaign",
               "next: --episodes=" + campaign.episodes_arg + " or map names");
      return;
    }
  }
  const In in = {{"game", game.empty() ? "(the coop hall)" : "the player's own"},
                 {"episodes", episodes},
                 {"skill", lab.arg("skill")},
                 {"seed", lab.arg("seed")},
                 {"god", o.god ? "on" : "off"},
                 {"complete", o.complete ? "on" : "off"}};
  std::vector<int> sizes;
  {
    std::stringstream ps(lab.arg("party"));
    std::string w;
    while (std::getline(ps, w, ','))
      if (!w.empty()) sizes.push_back(std::clamp(std::atoi(w.c_str()), 1, 4));
    if (sizes.empty()) sizes.push_back(1);
  }
  const bool party_runs = sizes.size() > 1 || sizes[0] > 1;
  const bool beside = lab.arg("beside") == "1";
  gl::LabTable t{"each map at 100%",
                 {"map", "bots", "out", "exit s", "kills", "secrets", "100%", "ended", "deaths",
                  "given up", "plans: exit/button/key/trigger/explore/monster/secret/arm", "nodes",
                  "tricks: rocket/grenade/combo", "blast jumps", "faults"},
                 {}};
  std::vector<fb::EpisodeMap> maps;
  std::vector<int> sizes_of;  // each map's party size
  uint64_t digest = 1469598103934665603ull;
  int64_t faults = 0;
  for (const Leg& leg : legs)
    for (const int size : sizes) {
      o.first = leg.first;
      o.avoid = leg.avoid;
      o.maps = leg.maps;
      // A party plays co-op (a party of one too, so the sizes compare on the same rules).
      o.single = !party_runs;
      o.partners.assign(static_cast<size_t>(size) - 1, sb::AimSkill::expert);
      o.beside = beside;
      // Each leg and each map's result said as it happens (stderr, flushed): a run that dies
      // part way still names the map it died on and what the maps before it scored.
      std::fprintf(stderr, "se-q1-campaign: playing from %s (party of %d)\n", leg.first.c_str(),
                   size);
      std::fflush(stderr);
      fb::EpisodeReport r = fb::run_coop_episode(o);
      for (const fb::EpisodeMap& m : r.maps)
        std::fprintf(
            stderr, "se-q1-campaign: %s done: out %s, kills %d/%d, secrets %d/%d, %.0f s, %s\n",
            m.name.c_str(), m.finished ? "yes" : "no", m.killed_monsters, m.total_monsters,
            m.found_secrets, m.total_secrets, m.exit_s >= 0 ? m.exit_s : m.seconds, m.end.c_str());
      std::fflush(stderr);
      if (!r.ok) {
        lab.fail("se-q1-campaign: " + leg.first + ": " + r.error,
                 "next: read the error; `labs se-q1-campaign` alone runs the hall");
        return;
      }
      digest = (digest ^ r.digest) * 1099511628211ull;
      faults += r.faults;
      for (fb::EpisodeMap& m : r.maps) {
        maps.push_back(std::move(m));
        sizes_of.push_back(size);
      }
    }
  int finished = 0, cleared = 0, kills = 0, monsters = 0, found = 0, secrets = 0;
  double exit_sum = 0;
  for (const fb::EpisodeMap& m : maps) {
    const bool all_k = m.total_monsters < 0 || m.killed_monsters >= m.total_monsters;
    const bool all_s = m.total_secrets < 0 || m.found_secrets >= m.total_secrets;
    const bool full = m.finished && all_k && all_s;
    finished += m.finished ? 1 : 0;
    cleared += full ? 1 : 0;
    kills += std::max(m.killed_monsters, 0);
    monsters += std::max(m.total_monsters, 0);
    found += std::max(m.found_secrets, 0);
    secrets += std::max(m.total_secrets, 0);
    exit_sum += m.exit_s >= 0 ? m.exit_s : m.seconds;
    std::string plans;
    for (int k = 0; k < fb::kWayKinds; k++)
      plans += (k ? "/" : "") + std::to_string(m.way.by_kind[k]);
    const size_t mi = static_cast<size_t>(&m - maps.data());
    t.rows.push_back({m.name, std::to_string(sizes_of[mi]), m.finished ? "yes" : "no",
                      m.exit_s >= 0 ? to_fixed(m.exit_s, 1) : "-",
                      of(m.killed_monsters, m.total_monsters), of(m.found_secrets, m.total_secrets),
                      full ? "yes" : "no", m.end, std::to_string(m.leader_deaths),
                      std::to_string(m.way.given_up), plans, std::to_string(m.nav_nodes),
                      std::to_string(m.nav_rockets) + "/" + std::to_string(m.nav_grenades) + "/" +
                          std::to_string(m.nav_combos),
                      std::to_string(m.blasts.size()), std::to_string(m.faults)});
  }
  t.rows.push_back({"all", "", std::to_string(finished) + "/" + std::to_string(maps.size()),
                    to_fixed(exit_sum, 0), of(kills, monsters), of(found, secrets),
                    std::to_string(cleared) + "/" + std::to_string(maps.size()), "", "", "", "", "",
                    "", "", std::to_string(faults)});
  // The bots column only when a party played, so a lone bot's table is as it was.
  if (!party_runs) {
    t.columns.erase(t.columns.begin() + 1);
    for (auto& row : t.rows) row.erase(row.begin() + 1);
  }
  lab.table(t);
  if (o.census) {
    // Every class name across the maps, in the order first seen; `--classes` keeps those alone.
    std::vector<std::string> keep;
    {
      std::stringstream cs(lab.arg("classes"));
      std::string w;
      while (std::getline(cs, w, ','))
        if (!w.empty()) keep.push_back(w);
    }
    struct Row {
      fb::ClassCensus c;
      int maps = 0;
    };
    std::vector<Row> rows;
    for (const fb::EpisodeMap& m : maps)
      for (const fb::ClassCensus& c : m.census) {
        if (!keep.empty() && std::find(keep.begin(), keep.end(), c.name) == keep.end()) continue;
        auto it = std::find_if(rows.begin(), rows.end(),
                               [&](const Row& r) { return r.c.name == c.name; });
        if (it == rows.end()) {
          rows.push_back({{c.name}, 0});
          it = rows.end() - 1;
        }
        it->maps++;
        it->c.spawned += c.spawned;
        it->c.made += c.made;
        it->c.moved += c.moved;
        it->c.gone += c.gone;
        it->c.taken += c.taken;
        it->c.most = std::max(it->c.most, c.most);
      }
    gl::LabTable ct{"the classes at work",
                    {"class", "maps", "stood", "made", "moved", "went", "taken", "most"},
                    {}};
    for (const Row& r : rows)
      ct.rows.push_back({r.c.name, std::to_string(r.maps), std::to_string(r.c.spawned),
                         std::to_string(r.c.made), std::to_string(r.c.moved),
                         std::to_string(r.c.gone), std::to_string(r.c.taken),
                         std::to_string(r.c.most)});
    for (const std::string& k : keep)
      if (std::none_of(rows.begin(), rows.end(), [&](const Row& r) { return r.c.name == k; }))
        ct.rows.push_back({k, "0", "0", "0", "0", "0", "0", "0"});
    lab.table(ct);
  }
  if (beside) {
    gl::LabTable h{"helpers beside the leader (a person's stand-in)",
                   {"map", "bots", "exit s", "kills: his/helpers'", "near share", "farthest",
                    "back to him", "gave way", "exits turned"},
                   {}};
    for (size_t i = 0; i < maps.size(); i++) {
      const fb::EpisodeMap& m = maps[i];
      h.rows.push_back(
          {m.name, std::to_string(sizes_of[i]), m.exit_s >= 0 ? to_fixed(m.exit_s, 1) : "-",
           std::to_string(m.killed_by_leader) + "/" + std::to_string(m.killed_by_partners),
           to_fixed(m.helper_near_share, 3), to_fixed(m.helper_farthest, 0),
           std::to_string(m.helper_stays), std::to_string(m.helper_gave_way),
           std::to_string(m.helper_exits_turned)});
    }
    lab.table(h);
  }
  lab.metric("maps.played", static_cast<double>(maps.size()), "count");
  lab.metric("maps.finished", finished, "count");
  lab.metric("maps.cleared", cleared, "count");
  lab.metric("kills", kills, "count");
  lab.metric("monsters", monsters, "count");
  lab.metric("secrets.found", found, "count");
  lab.metric("secrets", secrets, "count");
  lab.metric("exit_s.total", exit_sum, "s");
  lab.metric("faults", static_cast<double>(faults), "count");
  for (const fb::EpisodeMap& m : maps)
    if (!m.last_fault.empty()) lab.note(m.name + ": the program's last fault: " + m.last_fault);
  for (const fb::EpisodeMap& m : maps) {
    if (m.plan.empty()) continue;
    gl::LabTable pt{"the plan on " + m.name, {"change"}, {}};
    for (const std::string& line : m.plan) pt.rows.push_back({line});
    lab.table(pt);
  }
  char hex[24];
  std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(digest));
  lab.note(std::string("the run's digest (every frame's player folded): ") + hex);
  for (size_t i = 0; i < maps.size(); i++) {
    const fb::EpisodeMap& m = maps[i];
    lab.shot(
        {"map-" + std::to_string(i + 1) + "-" + m.name +
             (sizes_of[i] > 1 ? "-party" + std::to_string(sizes_of[i]) : std::string()),
         "MAP " + m.name + (sizes_of[i] > 1 ? ", " + std::to_string(sizes_of[i]) + " BOTS" : "") +
             (m.finished ? ": OUT" : ": NOT OUT") + ", KILLS " +
             of(m.killed_monsters, m.total_monsters) + ", SECRETS " +
             of(m.found_secrets, m.total_secrets),
         "The map from above as the live map draws it (or, with no pack, the walkable floor "
         "darker higher), " +
             std::string(sizes_of[i] > 1
                             ? "each bot's track in its own colour (red the leader, then blue, "
                               "green, violet)"
                             : "his track blue at the start to red at the end") +
             ", the exits (gold "
             "rings), where monsters fell (crosses), where secrets counted (gold diamonds) and his "
             "rocket and grenade jumps (orange, filled take-off to white landing); "
             "out at " +
             (m.exit_s >= 0 ? to_fixed(m.exit_s, 0) + " s." : std::string("no time: ") + m.end),
         "the track reaches every part of the floor and ends at an exit", in},
        [&] {
          bool ok = false;
          gl::Picture p = game.empty() ? gl::Picture{} : route_on_plate(packs, m, ok);
          return ok ? p : map_picture(m, 900);
        }());
  }
}

const gl::LabSpec kSpec{
    "se-q1-campaign",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/bots/coop_episode.hpp and way_through.hpp (`complete`): Quake's campaign "
    "at 100% by a bot alone",
    "bots",
    "Can a bot clear every map of Quake's campaign, every monster killed and every secret found, "
    "and how long does each take?",
    {
        {"game", "", "the player's own release zip, pack or folder (none: the coop hall once)"},
        {"episodes", "1,2,3,4,end",
         "with --game: episodes (1..4, each with its secret level), "
         "`end`, or single maps by name; with --mod, left as it is, the pack's own (1,2,3,end "
         "for hipnotic, 1,2 for rogue)"},
        {"map-seconds", "900",
         "a map's time limit (its last quarter for the way out alone): one not "
         "left by then is a failure, s"},
        {"each", "0", "1: every map of the episodes on its own, from a fresh start"},
        {"skill", "1", "the campaign's skill (0..3)"},
        {"seed", "1", "the campaign's and the bot's seed"},
        {"god", "1", "0: the bot can die"},
        {"complete", "1", "0: the way through alone, the exit as soon as it is open"},
        {"tour", "1", "0: the nearest stop each time rather than the tour's order (tour.hpp)"},
        {"root", ".", "the repository's root"},
        {"mod", "", "a mission pack's folder beside id1 (`rogue`, `hipnotic`), laid over the game"},
        {"census", "", "1: the classes at work (on with --mod; 0: off)"},
        {"classes", "", "the census's class names to show, a comma list (none: every one)"},
        {"plan", "0", "1: each map's changes of plan as a table"},
        {"party", "1",
         "the party sizes each map is played by, a comma list (1..4: `1,2,3`); more than one bot, "
         "or more than one size, plays co-op"},
        {"beside", "0",
         "1: the leader plays as a person would and the rest of the party are helpers beside him "
         "(a hosted game's `addbot helper`: leashed to him, the exit his), a table of how they "
         "kept to him"},
    },
    "--map-seconds=60",
    run,
};
GE_LAB(kSpec);

}  // namespace
