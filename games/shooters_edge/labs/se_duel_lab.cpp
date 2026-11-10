// LAB `se-duel`: TWO BOTS, ONE ON ONE, ARMED WITH EVERYTHING: the bench for aim and skill. A duel
// on the baseline mod's deathmatch mode 4, dmm4 (common/dmm4.qc: every spawn 250 health, red
// armour, every weapon `--disallow` leaves, 255 of everything; nothing to gather), by default on
// povdmm4 for three minutes, the way duel servers test a player's aim. Each seed (`--seeds=1-6`) is
// one duel of red's skill against blue's (`--red=expert --blue=average`, traits and features each);
// the table gives each duel's frags, and per side over all of them: frags, damage given a
// minute, accuracy by weapon (the scattergun's pellets, the rocket direct and by blast, the
// arc's beam), reactions and the mouse hand's strokes and lifts. `--save` keeps the first
// duel's recording (se-people reads it; the window plays it).
//
// The map comes from `--basedir`'s game folder; without one, a short duel on the fixture halls,
// which is what `make labs-smoke` runs. Deterministic: the digest holds the counts.
//
// ge:summary Lab: a dmm4 duel of two bots on povdmm4: frags, damage, accuracy by weapon
// ge:keywords lab se-duel duel dmm4 deathmatch-mode-4 povdmm4 aim skill accuracy one-on-one bench
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_bot_team.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
using ge::core::num::to_fixed;

// The seeds of `--seeds`: "1-6" or "1,3,5" (empty: 1).
std::vector<uint32_t> seeds_of(const std::string& text) {
  std::vector<uint32_t> out;
  if (const size_t dash = text.find('-'); dash != std::string::npos) {
    const int a = std::atoi(text.substr(0, dash).c_str()),
              b = std::atoi(text.substr(dash + 1).c_str());
    for (int s = a; s <= b && s > 0; s++) out.push_back(static_cast<uint32_t>(s));
  } else {
    size_t at = 0;
    while (at < text.size()) {
      const size_t end = std::min(text.find(',', at), text.size());
      const int s = std::atoi(text.substr(at, end - at).c_str());
      if (s > 0) out.push_back(static_cast<uint32_t>(s));
      at = end + 1;
    }
  }
  if (out.empty()) out.push_back(1);
  return out;
}

struct Side {
  double frags = 0, deaths = 0, given = 0, react_s = 0;
  int64_t reactions = 0, strokes = 0, lifts = 0;
  std::array<cn::HitCount, cn::kHitWeapons> hits{};
};

void run(gl::Lab& lab) {
  std::string error;
  const std::string basedir = lab.arg("basedir");
  const std::string name = basedir.empty() ? "halls" : lab.arg("map");
  std::vector<uint8_t> bytes;
  if (!basedir.empty()) {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: shooter.");
      return;
    }
    std::vector<uint8_t> store;
    const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
    if (f.empty()) {
      lab.fail(
          "se-duel: no maps/" + name + ".bsp in the game folder",
          "next: put the map in <basedir>/" + lab.arg("game") + "/maps or name another (--map)");
      return;
    }
    bytes.assign(f.begin(), f.end());
  } else if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes,
                                   &error)) {
    lab.fail("se-duel: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
    return;
  }
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-duel: the baseline mod: " + error,
             "next: run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  sh::AimSkill skill[2];
  for (int k = 0; k < 2; k++) {
    const std::string& s = lab.arg(k == 0 ? "red" : "blue");
    if (!cn::aim_skill_named(s, skill[k])) {
      lab.fail("se-duel: --" + std::string(k == 0 ? "red" : "blue") + "=" + s + " is no skill",
               "next: novice, average, expert or perfect");
      return;
    }
  }
  const double minutes = lab.arg_num("minutes");
  const std::vector<uint32_t> seeds = seeds_of(lab.arg("seeds"));
  Side side[2];
  gl::LabTable per{"each duel: the seed, red's and blue's frags",
                   {"seed", "red", "blue", "red frags", "blue frags"},
                   {}};
  for (const uint32_t seed : seeds) {
    cn::TeamMatchOptions o;
    o.map = bytes;
    o.map_name = name;
    if (basedir.empty()) o.entities = cn::kTeamHallsEntities;
    o.progs = &progs;
    o.seed = seed;
    o.deathmatch = static_cast<int>(lab.arg_num("deathmatch"));
    o.settings.push_back("k_disallow_weapons " + lab.arg("disallow"));
    o.timelimit = static_cast<int>(minutes);
    o.frames = static_cast<int>(minutes * 60 * 1000 / shooters_edge::bots::kPilotMsec);
    o.record = seed == seeds.front() && !lab.arg("save").empty();
    for (int k = 0; k < 2; k++) {
      o.teams[k].skills = {skill[k]};
      const std::string& tr = lab.arg(k == 0 ? "red-traits" : "blue-traits");
      if (!tr.empty()) o.teams[k].traits = {tr};
      o.teams[k].features = lab.arg("features");
      const std::string& own = lab.arg(k == 0 ? "features-red" : "features-blue");
      if (!own.empty()) o.teams[k].features += "," + own;
    }
    const cn::TeamMatchReport r = cn::run_team_match(o);
    if (!r.ok) {
      lab.fail("se-duel: " + r.error, "next: run tests/shooters_edge_team_test.cpp");
      return;
    }
    float frags[2] = {};
    for (const cn::TeamPlayer& p : r.players) {
      Side& s = side[p.team];
      frags[p.team] += p.frags;
      s.frags += p.frags;
      s.deaths += p.deaths;
      s.given += p.given;
      s.reactions += p.reactions;
      s.react_s += p.react_s;
      s.strokes += p.strokes;
      s.lifts += p.lifts;
      for (int w = 0; w < cn::kHitWeapons; w++) {
        s.hits[static_cast<size_t>(w)].fired += p.hits[static_cast<size_t>(w)].fired;
        s.hits[static_cast<size_t>(w)].hit += p.hits[static_cast<size_t>(w)].hit;
      }
    }
    per.rows.push_back({std::to_string(seed), sh::aim_skill_name(skill[0]),
                        sh::aim_skill_name(skill[1]), to_fixed(frags[0], 0),
                        to_fixed(frags[1], 0)});
    if (o.record && !gl::write_file(lab.arg("save"), {r.mvd.data(), r.mvd.size()}))
      lab.fail("se-duel: --save " + lab.arg("save") + " was not written",
               "next: pass --save=<a writable path>");
  }
  lab.table(per);
  const double mins = minutes * static_cast<double>(seeds.size());
  gl::LabTable acc{"each side's accuracy by weapon over every duel (hits / fired)",
                   {"weapon", "red", "blue"},
                   {}};
  for (int w = 0; w < cn::kHitWeapons; w++) {
    std::vector<std::string> row{cn::hit_weapon_name(static_cast<cn::HitWeapon>(w))};
    for (int k = 0; k < 2; k++) {
      const cn::HitCount& h = side[k].hits[static_cast<size_t>(w)];
      row.push_back(
          h.fired > 0
              ? to_fixed(100.0 * static_cast<double>(h.hit) / static_cast<double>(h.fired), 1) +
                    " % of " + std::to_string(h.fired)
              : "-");
    }
    acc.rows.push_back(row);
  }
  lab.table(acc);
  for (int k = 0; k < 2; k++) {
    const Side& s = side[k];
    const std::string key = std::string("duel.") + (k == 0 ? "red" : "blue");
    lab.metric(key + ".frags", s.frags, "count");
    lab.metric(key + ".deaths", s.deaths, "count");
    lab.metric(key + ".damage-per-min", mins > 0 ? s.given / mins : 0, "per-min");
    lab.metric(key + ".reaction-ms",
               s.reactions > 0 ? 1000 * s.react_s / static_cast<double>(s.reactions) : 0, "ms");
    lab.metric(key + ".strokes-per-min", mins > 0 ? static_cast<double>(s.strokes) / mins : 0,
               "per-min");
    lab.metric(key + ".lifts-per-min", mins > 0 ? static_cast<double>(s.lifts) / mins : 0,
               "per-min");
    for (int w = 0; w < cn::kHitWeapons; w++) {
      const cn::HitCount& h = s.hits[static_cast<size_t>(w)];
      if (h.fired == 0) continue;
      lab.metric(key + ".accuracy." + cn::hit_weapon_name(static_cast<cn::HitWeapon>(w)),
                 static_cast<double>(h.hit) / static_cast<double>(h.fired), "share");
    }
  }
  lab.metric("duel.frags-per-minute", mins > 0 ? (side[0].frags + side[1].frags) / mins : 0,
             "per-min");
}

const gl::LabSpec kSpec{
    "se-duel",
    gl::LabKind::sim,
    "game:shooters_edge",
    "a deathmatch mode 4 (dmm4) duel of two bots (games/shooters_edge/labs/se_duel_lab.cpp, the "
    "baseline "
    "mod's common/dmm4.qc)",
    "bots",
    "Armed with everything and nothing to gather, how well does one bot aim and fight against "
    "another, by weapon, over seeds?",
    {
        {"basedir", "", "a folder holding the game folder the map is read from"},
        {"game", "id1", "the game folder under --basedir"},
        {"map", "povdmm4", "with --basedir: the map"},
        {"minutes", "3", "each duel's length"},
        {"seeds", "1", "the duels' seeds: 1-6, or 1,3,5"},
        {"red", "expert", "red's skill: novice, average, expert or perfect"},
        {"blue", "expert", "blue's skill"},
        {"red-traits", "", "red's traits over its preset (aim_rl=9/timing=3)"},
        {"blue-traits", "", "blue's traits over its preset"},
        {"features", "", "both bots' features over their presets (shooter/features.hpp)"},
        {"features-red", "", "red's over --features (an A/B in one duel)"},
        {"features-blue", "", "blue's over --features"},
        {"deathmatch", "4", "the mode: 4 armed with everything, 1 the items on the map"},
        {"disallow", "0", "k_disallow_weapons: the weapons a spawn lacks (16: the grenades)"},
        {"save", "", "write the first duel's recording here"},
    },
    "--minutes=0.5",
    run,
};
GE_LAB(kSpec);

}  // namespace
