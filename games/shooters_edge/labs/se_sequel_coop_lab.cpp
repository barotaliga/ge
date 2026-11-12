// LAB `se-sequel-coop`: THE SEQUEL'S CO-OP GAME CARRIED FROM LEVEL TO LEVEL. The window's listen
// server plays the sequel's maps as a co-op game (`deathmatch 0`, `coop 1`: what NEW GAME's
// COOPERATIVE and `coopgame` start), the player takes a shotgun, shells and armour on the first
// map, the level changes to the second, and the window enters it as it would (`listen_change`). In
// co-op the sequel's server is kept from level to level (net/classic_game.cpp), so the player
// enters the next level with what he left with; the same run in deathmatch makes new rules on
// every map and starts him afresh. The table lists what he holds before and after, both ways; the
// pictures show each map from above (its floors, the single-player starts in green, the co-op
// starts in blue, where the player came in, ringed in red).
//
// Without `--game` the maps are our own: two floored halls of brushes, each with a start and a
// co-op start, in a pack, so CI needs no game data and the counts are a digest. With
// `--game=<folder>` (the folder holding the sequel's base folder) the maps come from its packs
// (`--maps=<first>,<second>`, the full game's first two by default), for PRIVATE pictures only.
//
// ge:summary Lab: the sequel's co-op game across a level change: the body carried, the starts
// ge:keywords lab se-sequel-coop sequel coop co-op level-change carried-body coop-start
// ge:keywords keeps-campaign listen_change coopgame
// ge:skill shooter labs
// ge:test tests/shooters_edge_listen_test.cpp
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "progs/vm.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;

// A floored hall walled round, `w` wide, a start and a co-op start on its floor.
bool hall(float w, std::vector<uint8_t>& out, std::string& error) {
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
  const auto put = [&](const char* kind, const char* origin) {
    br::Entity e;
    e.pairs = {{"classname", kind}, {"origin", origin}, {"angle", "90"}};
    world.entities.push_back(e);
  };
  put("info_player_start", "0 0 24");
  put("info_player_coop", "96 0 24");
  put("info_player_coop", "-96 0 24");
  return br::write_brush_world(world, &out, &error);
}

bool pack(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files,
          std::vector<uint8_t>& out, std::string& error) {
  return ge::vfs::write_pak(files, &out, &error);
}

struct Played {
  bool ok = false;
  bool kept = false;  // the same rules played the second map
  sv::SqBody before, after;
  sh::Vec3f came_in{};  // where the player stood on the second map
};

// The body a player takes the shotgun, two boxes of shells and the jacket armour into.
void gear_up(sv::SqBody& b) {
  for (const char* item : {"weapon_shotgun", "ammo_shells", "ammo_shells", "item_armor_jacket"})
    sv::sq_pick_up(sv::sq_item(item), b, false, 1, 0);
}

Played play(const cl::SearchPath& files, const std::string& first, const std::string& second,
            bool coop, std::string& error) {
  Played out;
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs p;
    std::string why;
    cl::baseline_progs(p, why);
    return p;
  }();
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, first, error);
  if (!map) return out;
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.console = coop ? std::vector<std::string>{"deathmatch 0", "coop 1"}
                     : std::vector<std::string>{"deathmatch 1"};
  cl::Listen l;
  if (!cl::listen_start(l, progs, std::move(map), opt, error)) return out;
  for (int i = 0; i < 30; i++) cl::listen_frame(l, {});
  sv::SequelServer* sq = sv::sequel_of(*l.rules);
  if (sq == nullptr) {
    error = first + " did not play on the sequel's rules (no brushes?)";
    return out;
  }
  gear_up(sq->players[0].body);
  out.before = sq->players[0].body;
  l.rules->console("changelevel " + second);
  for (int i = 0; i < 60 && l.pending_map.empty(); i++) cl::listen_frame(l, {});
  if (l.pending_map != second) {
    error = "the level change to " + second + " was never asked for";
    return out;
  }
  std::unique_ptr<cl::LoadedMap> next = cl::load_named_map(files, second, error);
  if (!next || !cl::listen_change(l, std::move(next), error)) return out;
  for (int i = 0; i < 30; i++) cl::listen_frame(l, {});
  const sv::SequelServer* now = sv::sequel_of(*l.rules);
  if (now == nullptr) {
    error = second + " did not play on the sequel's rules";
    return out;
  }
  out.kept = now == sq;
  out.after = now->players[0].body;
  out.came_in = l.server->world.entities[1].origin;
  out.ok = true;
  return out;
}

// What a body holds, item by item: the weapons, the ammo, the armour and health.
std::vector<std::pair<std::string, int>> holdings(const sv::SqBody& b) {
  std::vector<std::pair<std::string, int>> out;
  out.emplace_back("health", b.health);
  out.emplace_back("armour", b.armour_count);
  for (int i = 0; i < sv::kSqItemCount; i++)
    if (sv::kSqItems[i].kind == sv::SqKind::weapon)
      out.emplace_back(sv::kSqItems[i].pickup, b.counts[i]);
  constexpr const char* kAmmo[sv::kSqAmmoKinds] = {"",        "shells", "bullets", "grenades",
                                                   "rockets", "cells",  "slugs"};
  for (int a = 1; a < sv::kSqAmmoKinds; a++) out.emplace_back(kAmmo[a], b.ammo[a]);
  return out;
}

// The map from above: its floors (faces turned up) as lines, the starts and where he came in.
gl::Picture overhead(const br::Map& map, const std::string& title, const sh::Vec3f* came_in) {
  constexpr int kSize = 560, kPad = 24;
  gl::Picture p = gl::create_picture(kSize, kSize + 40, {18, 18, 22});
  if (map.models.empty()) return p;
  const float* lo = map.models[0].mins;
  const float* hi = map.models[0].maxs;
  const float span = std::max({hi[0] - lo[0], hi[1] - lo[1], 1.0f});
  const auto px = [&](float x, float y) {
    return std::pair<double, double>{kPad + (x - lo[0]) / span * (kSize - 2 * kPad),
                                     kPad + (hi[1] - y) / span * (kSize - 2 * kPad)};
  };
  const float zspan = std::max(hi[2] - lo[2], 1.0f);
  for (const br::Surface& s : map.surfaces) {
    if (s.kind == br::SurfaceKind::sky) continue;
    for (uint32_t t = 0; t + 2 < s.count; t += 3) {
      const br::Vertex* v[3];
      for (int k = 0; k < 3; k++) v[k] = &map.vertices[map.indices[s.first + t + uint32_t(k)]];
      if (v[0]->nrm[2] < 0.7f) continue;  // floors only
      const float z = (v[0]->pos[2] - lo[2]) / zspan;
      const auto g = static_cast<uint8_t>(60 + 150 * std::clamp(z, 0.0f, 1.0f));
      const gl::Rgba c{g, g, static_cast<uint8_t>(g + 20 > 255 ? 255 : g + 20)};
      for (int k = 0; k < 3; k++) {
        const auto [x0, y0] = px(v[k]->pos[0], v[k]->pos[1]);
        const auto [x1, y1] = px(v[(k + 1) % 3]->pos[0], v[(k + 1) % 3]->pos[1]);
        gl::draw_line(p, x0, y0, x1, y1, c);
      }
    }
  }
  for (const br::Entity& e : map.entities) {
    const std::string kind = e.get("classname");
    const bool start = kind == "info_player_start", coop = kind == "info_player_coop";
    if (!start && !coop) continue;
    float x = 0, y = 0, z = 0;
    std::sscanf(e.get("origin").c_str(), "%f %f %f", &x, &y, &z);
    const auto [cx, cy] = px(x, y);
    gl::fill_disc(p, cx, cy, 5, start ? gl::Rgba{90, 220, 110} : gl::Rgba{90, 160, 255});
  }
  if (came_in != nullptr) {
    const auto [cx, cy] = px(came_in->x, came_in->y);
    gl::draw_ring(p, cx, cy, 9, {255, 80, 80});
    gl::draw_ring(p, cx, cy, 10, {255, 80, 80});
  }
  gl::fill_disc(p, 16, kSize + 14, 4, {90, 220, 110});
  gl::draw_text(p, 24, kSize + 10, "START", {220, 220, 220});
  gl::fill_disc(p, 86, kSize + 14, 4, {90, 160, 255});
  gl::draw_text(p, 94, kSize + 10, "CO-OP START", {220, 220, 220});
  gl::draw_ring(p, 196, kSize + 14, 5, {255, 80, 80});
  gl::draw_text(p, 206, kSize + 10, "CAME IN", {220, 220, 220});
  gl::draw_text(p, 12, kSize + 26, title, {230, 230, 230});
  return p;
}

void run(gl::Lab& lab) {
  const pf::GameProfile& game = *pf::find_game("sequel");
  const ge::classicenhance::Profile& content = pf::content_of(game);
  std::string error;
  cl::SearchPath files;
  const bool own = lab.arg("game").empty();
  std::string first = "hall", second = "next";
  if (own) {
    std::vector<uint8_t> a, b, pak;
    if (!hall(512, a, error) || !hall(768, b, error) ||
        !pack({{"maps/hall.bsp", a}, {"maps/next.bsp", b}}, pak, error) ||
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
  const Played coop = play(files, first, second, true, error);
  if (!coop.ok) {
    lab.fail("co-op: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  const Played dm = play(files, first, second, false, error);
  if (!dm.ok) {
    lab.fail("deathmatch: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  lab.metric("coop.rules_kept", coop.kept ? 1 : 0, "bool");
  lab.metric("deathmatch.rules_kept", dm.kept ? 1 : 0, "bool");
  const auto held_before = holdings(coop.before);
  const auto held_coop = holdings(coop.after);
  const auto held_dm = holdings(dm.after);
  gl::LabTable t;
  t.title = "What the player holds, leaving " + first + " and entering " + second;
  t.columns = {"what", "leaving", "co-op: entering", "deathmatch: entering"};
  int lost = 0;
  for (size_t i = 0; i < held_before.size(); i++) {
    if (held_before[i].second == 0 && held_coop[i].second == 0 && held_dm[i].second == 0) continue;
    t.rows.push_back({held_before[i].first, std::to_string(held_before[i].second),
                      std::to_string(held_coop[i].second), std::to_string(held_dm[i].second)});
    if (held_coop[i].second != held_before[i].second) lost++;
  }
  lab.metric("coop.items_changed", lost, "count");
  lab.table(t);
  if (!coop.kept || lost != 0)
    lab.fail("co-op: the player did not carry his body to " + second,
             "Check net/classic_game.cpp's keeps_campaign and the sequel's campaign_start. "
             "Skill: shooter.");
  if (dm.kept)
    lab.fail("deathmatch kept the rules across the level change",
             "Deathmatch makes new rules on every map; check keeps_campaign. Skill: shooter.");
  for (const auto& [name, came] :
       {std::pair<std::string, const sh::Vec3f*>{first, nullptr},
        std::pair<std::string, const sh::Vec3f*>{second, &coop.came_in}}) {
    std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(files, name, error);
    br::Map read;
    if (!m || !br::read_bsp(m->file, &read, &error)) {
      lab.fail(std::string(name).append(": ").append(error),
               "Read the reason above. Skill: brush.");
      continue;
    }
    lab.shot({name + "-above",
              name + " from above",
              "The floors of " + name +
                  " from above, its single-player starts in green and its "
                  "co-op starts in blue" +
                  (came != nullptr ? "; the red ring is where the player came in, co-op, carrying "
                                     "what he left " +
                                         first + " with."
                                   : "."),
              "starts on the floors; the player at a start of the level",
              {{"map", name}, {"game", own ? "fixture" : "local packs"}}},
             overhead(read, name, came), own ? gl::Det::deterministic : gl::Det::measured);
  }
}

const gl::LabSpec kSpec{
    "se-sequel-coop",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "net/classic_game.cpp's keeps_campaign and the sequel's campaign: a co-op game across a "
    "level change on the window's listen server",
    "shooter",
    "Does a co-op game of the sequel carry the player's weapons, ammo and armour to the next "
    "level, and does deathmatch still start every map afresh?",
    {{"game", "", "the folder holding the sequel's base folder (none: two fixture halls)"},
     {"maps", "", "the two maps, first,second (default hall,next; base1,base2 with --game)"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
