// LAB `se-pack-dm`: DOES A GAME'S OWN DEATHMATCH MAP PLAY UNDER ITS OWN PROGRAM? A mission pack's
// deathmatch map (`--map=hipdm1`) on the pack's program, laid over the base game's packs
// (`--game=<id1>`, `--mod=<the pack's folder>`), played by fighter bots (`--bots`) with
// `deathmatch` and `teamplay` as asked, until the program's time or frag limit ends it or
// `--seconds` run out (bots/pack_fight.hpp). Tables: each bot's frags, deaths and items taken;
// every item class taken and how often; the frag log's length. Picture: the map from above (the
// nav's floor shaded by height), every bot's trail in its colour, the items and where each bot
// died.
//
// Without `--game` the smoke run plays the fixture halls under the baseline mod, so it needs no
// game files. The lab fails on a program fault or a match where nobody is fragged.
//
// ge:summary Lab: a pack's deathmatch map on its own program, played by fighters: frags, items
// ge:keywords lab se-pack-dm deathmatch mission-pack hipdm1 ctf1 teamplay pack-program fighters
// ge:keywords items-taken frags faults
// ge:skill bots shooter labs
// ge:test tests/shooters_edge_fight_test.cpp
#include <algorithm>
#include <cmath>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "classicenhance/source.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "pack_mod.hpp"
#include "progs/file.hpp"
#include "shooterbots/knowledge.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/bots/pack_fight.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/qw_team.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sb = ge::shooterbots;
namespace pg = ge::progs;
namespace fb = shooters_edge::bots;
namespace sv = shooters_edge::server;
namespace ce = ge::classicenhance;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

const gl::Rgba kPlayer[4] = {
    {30, 90, 210, 255}, {210, 60, 40, 255}, {40, 150, 70, 255}, {150, 70, 190, 255}};
const gl::Rgba kInk{30, 30, 30, 255};
const gl::Rgba kPaper{246, 246, 240, 255};

struct Maps {
  std::vector<uint8_t> content;  // the packs laid together (none: the fixture halls)
  ge::vfs::Pak pak;
  std::vector<uint8_t> halls;  // the fixture's bytes
  std::unique_ptr<sv::BspMap> bsp;
};

bool load(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  auto& m = *static_cast<Maps*>(ctx);
  m.bsp = std::make_unique<sv::BspMap>();
  if (!m.halls.empty()) {
    if (!sv::read_bsp_map(m.halls, name, m.bsp.get(), &out, error)) return false;
    out.entities = shooters_edge::net::kTeamHallsEntities;
    return true;
  }
  const ge::vfs::PakEntry* e = ge::vfs::pak_find(m.pak, ("maps/" + name + ".bsp").c_str());
  if (e == nullptr) {
    error = "no maps/" + name + ".bsp in the packs; next: name a map they hold (--map)";
    return false;
  }
  return sv::read_bsp_map(ge::vfs::pak_file(m.content, *e), name, m.bsp.get(), &out, error);
}

std::string short_name(std::string name) {
  for (const char* pre : {"weapon_", "item_"})
    if (name.rfind(pre, 0) == 0) name = name.substr(std::string(pre).size());
  return name;
}

void run(gl::Lab& lab) {
  std::string error;
  const std::string& game = lab.arg("game");
  Maps maps;
  pg::Progs progs;
  fb::PackFightOptions o;
  if (game.empty()) {
    if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &maps.halls,
                              &error)) {
      lab.fail("se-pack-dm: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
      return;
    }
    if (!shooters_edge::client::baseline_progs(progs, error)) {
      lab.fail("se-pack-dm: the baseline mod: " + error,
               "next: run tests/shooters_edge_baseline_mod_test.cpp");
      return;
    }
    o.map = "halls";
  } else {
    ce::Source src;
    if (!ce::open_source(game, &src, &error)) {
      lab.fail("se-pack-dm: " + error, "next: pass --game=<a release zip, a pack or a folder>");
      return;
    }
    maps.content = src.content();
    if (const std::string& mod = lab.arg("mod"); !mod.empty()) {
      if (!shooters_edge::labs::lay_mod(game, mod, maps.content, error)) {
        lab.fail("se-pack-dm: " + error, "next: pass --mod=<the pack's folder beside id1>");
        return;
      }
    }
    if (!ge::vfs::read_pak(maps.content, &maps.pak, &error)) {
      lab.fail("se-pack-dm: the packs do not read: " + error, "next: check the --game path");
      return;
    }
    const ge::vfs::PakEntry* e = ge::vfs::pak_find(maps.pak, "progs.dat");
    const std::span<const uint8_t> bytes =
        e != nullptr ? ge::vfs::pak_file(maps.content, *e) : std::span<const uint8_t>{};
    if (e == nullptr ||
        !pg::load_progs(bytes.data(), bytes.size(), progs, error, pg::every_form())) {
      lab.fail("se-pack-dm: the packs' program does not load: " + error,
               "next: pass the game's own folder (its id1) and the pack's (--mod)");
      return;
    }
    o.map = lab.arg("map");
    o.registered = ge::vfs::pak_find(maps.pak, "gfx/pop.lmp") != nullptr;
    o.wide_models = true;
    // The pack's own guns known, or Quake's alone (`--arsenal=base`: the before of a change to
    // them).
    o.arsenal =
        lab.arg("arsenal") == "base" ? &fb::first_arsenal() : &fb::arsenal_for_mod(lab.arg("mod"));
  }
  o.load = &load;
  o.load_ctx = &maps;
  o.progs = &progs;
  const int bots = static_cast<int>(lab.arg_num("bots"));
  static const sb::AimSkill kSkills[4] = {sb::AimSkill::expert, sb::AimSkill::average,
                                          sb::AimSkill::average, sb::AimSkill::novice};
  o.skills.clear();
  for (int k = 0; k < bots; k++) o.skills.push_back(kSkills[k % 4]);
  o.deathmatch = static_cast<int>(lab.arg_num("deathmatch"));
  o.teamplay = static_cast<int>(lab.arg_num("teamplay"));
  o.timelimit = static_cast<int>(lab.arg_num("timelimit"));
  o.fraglimit = static_cast<int>(lab.arg_num("fraglimit"));
  o.frames = static_cast<int>(lab.arg_num("seconds") * 72);
  o.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const fb::PackFightReport pr = fb::run_pack_fight(o);
  const fb::FightReport& r = pr.fight;
  if (!r.ok) {
    lab.fail("se-pack-dm: " + r.error, "next: read the error's next step");
    return;
  }
  const In in = {{"game", game.empty() ? "(the fixture halls, the baseline mod)" : "the player's"},
                 {"mod", lab.arg("mod")},
                 {"map", o.map},
                 {"bots", lab.arg("bots")},
                 {"deathmatch", lab.arg("deathmatch")},
                 {"teamplay", lab.arg("teamplay")},
                 {"seconds", lab.arg("seconds")},
                 {"seed", lab.arg("seed")}};
  const double secs = r.frames / 72.0;

  gl::LabTable t{
      "each bot", {"bot", "aim", "frags", "deaths", "items taken", "shots", "switches"}, {}};
  double frags = 0;
  int took = 0;
  for (const fb::FightPlayer& p : r.players) {
    t.rows.push_back({p.name, sb::aim_skill_name(p.skill), to_fixed(p.frags, 0),
                      std::to_string(p.deaths), std::to_string(p.took),
                      std::to_string(p.stats.shots), std::to_string(p.stats.switches)});
    frags += p.frags;
    took += p.took;
  }
  lab.table(t);
  gl::LabTable items{"the items taken, by class", {"class", "taken"}, {}};
  for (const auto& [name, n] : pr.taken) items.rows.push_back({name, std::to_string(n)});
  lab.table(items);
  // The deaths by the gun the victim held, and how many cost it a frag (its own shot, or the
  // world): a gun the bots handle badly shows as deaths in its own hand.
  {
    const fb::BotArsenal& names = fb::arsenal_for_mod(lab.arg("mod"));
    std::vector<std::pair<int32_t, std::pair<int, int>>> by;
    int own = 0;
    for (const fb::FightDeath& d : r.deaths) {
      auto it =
          std::find_if(by.begin(), by.end(), [&](const auto& e) { return e.first == d.weapon; });
      if (it == by.end()) it = by.insert(by.end(), {d.weapon, {0, 0}});
      it->second.first++;
      it->second.second += d.own ? 1 : 0;
      own += d.own ? 1 : 0;
    }
    gl::LabTable g{"the deaths, by the gun in the victim's hand", {"gun", "deaths", "its own"}, {}};
    for (const auto& [bit, n] : by) {
      const fb::BotWeapon* w = fb::weapon_row(names, bit);
      g.rows.push_back({w != nullptr ? w->name : "bit " + std::to_string(bit),
                        std::to_string(n.first), std::to_string(n.second)});
    }
    lab.table(g);
    lab.metric("dm.own_deaths", own, "count");
    // Where the deaths that cost a frag fell, by 256-unit cell: a trap or a pit the bots walk
    // into shows as one cell holding many.
    std::vector<std::pair<std::pair<int, int>, std::pair<int, sh::Vec3f>>> cells;
    for (const fb::FightDeath& d : r.deaths) {
      if (!d.own) continue;
      const std::pair<int, int> c{static_cast<int>(std::floor(d.at.x / 256)),
                                  static_cast<int>(std::floor(d.at.y / 256))};
      auto it =
          std::find_if(cells.begin(), cells.end(), [&](const auto& e) { return e.first == c; });
      if (it == cells.end()) it = cells.insert(cells.end(), {c, {0, d.at}});
      it->second.first++;
    }
    std::sort(cells.begin(), cells.end(),
              [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
    gl::LabTable w{"the deaths that cost a frag, by place", {"near", "deaths"}, {}};
    for (size_t i = 0; i < cells.size() && i < 6; i++) {
      const sh::Vec3f a = cells[i].second.second;
      w.rows.push_back({to_fixed(a.x, 0) + " " + to_fixed(a.y, 0) + " " + to_fixed(a.z, 0),
                        std::to_string(cells[i].second.first)});
    }
    lab.table(w);
  }
  lab.metric("dm.seconds", secs, "s");
  lab.metric("dm.frags", frags, "count");
  lab.metric("dm.deaths", static_cast<double>(r.deaths.size()), "count");
  lab.metric("dm.items_taken", took, "count");
  lab.metric("dm.item_classes_taken", static_cast<double>(pr.taken.size()), "count");
  lab.metric("dm.frag_log", static_cast<double>(r.frag_log.size()), "count");
  lab.metric("dm.intermission", pr.intermission ? 1 : 0, "bool");
  lab.metric("dm.faults", static_cast<double>(r.faults), "count");
  if (r.faults > 0) {
    lab.fail("se-pack-dm: the program faulted " + std::to_string(r.faults) +
                 " times; the last: " + pr.last_fault,
             "next: read the fault's QC stack and the builtin it names");
  }
  if (r.deaths.empty()) {
    lab.fail("se-pack-dm: nobody died in " + to_fixed(secs, 0) + " s",
             "next: run longer (--seconds) or read the fighters' modes (se-bots)");
  }

  // The map from above.
  const float wx = std::max(r.hi.x - r.lo.x, 64.f), wy = std::max(r.hi.y - r.lo.y, 64.f);
  const float k = 900.f / std::max(wx, wy);
  const int margin = 28;
  gl::Picture p = gl::create_picture(static_cast<int>(wx * k) + 2 * margin,
                                     static_cast<int>(wy * k) + 2 * margin, kPaper);
  const auto px = [&](float x) { return margin + (x - r.lo.x) * k; };
  const auto py = [&](float y) { return margin + (r.hi.y - y) * k; };
  const float zspan = std::max(r.hi.z - r.lo.z, 1.f);
  std::vector<sh::Vec3f> nodes = pr.nodes;
  std::sort(nodes.begin(), nodes.end(), [](sh::Vec3f a, sh::Vec3f b) { return a.z < b.z; });
  const int dot = std::max(1, static_cast<int>(std::lround(32 * k)));
  for (const sh::Vec3f& n : nodes) {
    const auto g = static_cast<uint8_t>(215 - 120 * ((n.z - r.lo.z) / zspan));
    gl::fill_rect(p, px(n.x) - dot * 0.5, py(n.y) - dot * 0.5, dot, dot, {g, g, g, 255});
  }
  for (size_t b = 0; b < r.players.size(); b++) {
    gl::Rgba c = kPlayer[b % 4];
    c.a = 160;
    const std::vector<sh::Vec3f>& path = r.players[b].path;
    for (size_t i = 0; i + 3 < path.size(); i += 3) {
      const sh::Vec3f a = path[i], e = path[i + 3];
      if ((a - e).x * (a - e).x + (a - e).y * (a - e).y > 200 * 200) continue;  // a respawn
      gl::draw_line(p, px(a.x), py(a.y), px(e.x), py(e.y), c, 1);
    }
  }
  for (const fb::FightItem& it : r.items) {
    gl::fill_rect(p, px(it.at.x) - 4, py(it.at.y) - 4, 8, 8, {250, 250, 240, 255});
    gl::draw_ring(p, px(it.at.x), py(it.at.y), 6, kInk);
    gl::draw_text(p, px(it.at.x) + 8, py(it.at.y) - 4, short_name(it.name), kInk);
  }
  for (const fb::FightDeath& d : r.deaths) {
    const gl::Rgba c = kPlayer[static_cast<size_t>(d.victim) % 4];
    gl::draw_line(p, px(d.at.x) - 5, py(d.at.y) - 5, px(d.at.x) + 5, py(d.at.y) + 5, c, 2);
    gl::draw_line(p, px(d.at.x) - 5, py(d.at.y) + 5, px(d.at.x) + 5, py(d.at.y) - 5, c, 2);
    if (d.own) gl::draw_ring(p, px(d.at.x), py(d.at.y), 8, c);  // a frag lost with it
  }
  int x = 8;
  for (size_t b = 0; b < r.players.size(); b++) {
    const std::string label = r.players[b].name + " " + to_fixed(r.players[b].frags, 0);
    gl::fill_rect(p, x, 9, 14, 6, kPlayer[b % 4]);
    gl::draw_text(p, x + 18, 8, label, kInk);
    x += 18 + gl::text_width(label) + 14;
  }
  gl::draw_text(p, x, 8, "X A DEATH", kInk);
  lab.shot({"map", "THE DEATHMATCH FROM ABOVE",
            o.map + " from above for " + to_fixed(secs, 0) +
                " s: the floor the bots can walk, darker higher; each bot's trail in its colour "
                "with its frags in the key; the items as the map places them; a cross where each "
                "bot died (" +
                std::to_string(r.deaths.size()) + " deaths).",
            "trails over most of the floor, deaths spread across the map, every bot scoring", in},
           p);
}

const gl::LabSpec kSpec{
    "se-pack-dm",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/bots/pack_fight.hpp: a game's own deathmatch map on its own program "
    "(a mission pack's), played by fighters",
    "bots",
    "Does a mission pack's deathmatch map play under the pack's own program: do bots frag, die, "
    "take its items, and does the program run without a fault?",
    {
        {"game", "", "the player's own release zip, pack or folder (none: the fixture halls)"},
        {"mod", "", "the mission pack's folder beside the game's (hipnotic, rogue)"},
        {"map", "hipdm1", "with --game: the deathmatch map"},
        {"bots", "4", "fighters, 1..16 (expert, average, average, novice, again)"},
        {"deathmatch", "1", "the deathmatch mode"},
        {"teamplay", "0", "the team mode (bots alternate red and blue)"},
        {"timelimit", "0", "the program's time limit, minutes (0: none)"},
        {"fraglimit", "0", "the program's frag limit (0: none)"},
        {"seconds", "120", "the match's length at most, s"},
        {"seed", "1", "the server's and the fighters' seed"},
        {"arsenal", "pack", "pack: the bots know the pack's own guns; base: Quake's alone"},
    },
    "--seconds=20",
    run,
};
GE_LAB(kSpec);

}  // namespace
