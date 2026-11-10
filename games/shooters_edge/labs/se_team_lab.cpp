// LAB `se-team`: does a team game of bots play like a team game? Two teams of four fighters
// (classic_team.hpp: a client each over our server, the team's plan bots/team.hpp) play the
// baseline mod with `teamplay 1` for `--minutes`, recorded as a multiview demo. By default on the
// fixture halls (plugins/brush, a map written in code); `--basedir` with `--map` plays a map of a
// game folder's instead (the classic small team map is `e1m2` of the shareware pack).
//
// Pictured: the map from above (the nav graph's floor shaded by height) with every player's trail
// in his team's colour, the items and the two key areas; each player's role through the match as a
// strip (hold, rush, carry, free, dead); and each team's frags against time. The table gives each
// player's frags, deaths, shots, shots held for a teammate, backpacks, his share of time in each
// role and the weapon he held most; the teams' rows their totals, rushes and quads. The recording
// is attached (`match.mvd`); `--save` writes it to a path as well, packed as it is recorded when
// the path ends in `z` (`.mvdz`: net/classic_record.hpp).
//
// ITEM CONTROL (`team.lying.<item>`): each key item's share of the time it lay on its spot, and of
// that time by what the bots made of it (classic_team.hpp `kLyingWhy`): nobody alive believed it
// there, nobody wanted it, the one who wanted it was fighting, one on his way (far, within 400
// units, standing on it). People take the yellow armour within 4 s of its showing (se-people).
//
// ge:summary Lab: a recorded 4on4 of bots on teamplay: roles, key areas, frags by team, the MVD
// ge:keywords lab se-team team-game 4on4 teamplay roles hold rush carry quad key-area mvd record
// ge:keywords item-control lying untaken armour-control
// ge:skill shooter bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cn = shooters_edge::net;
namespace bt = shooters_edge::bots;
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

const gl::Rgba kTeam[2] = {{200, 50, 40, 255}, {40, 90, 210, 255}};
const gl::Rgba kRole[6] = {{150, 160, 170, 255},  // free
                           {40, 150, 70, 255},    // hold
                           {240, 160, 20, 255},   // rush
                           {150, 60, 200, 255},   // carry
                           {220, 60, 90, 255},    // push
                           {40, 40, 40, 255}};    // dead
const gl::Rgba kInk{30, 30, 30, 255};

// The map scaled to fit `size` pixels across its longer side.
struct View {
  sh::Vec3f lo, hi;
  double scale = 1;
  int margin = 28;
  double px(double x) const { return margin + (x - lo.x) * scale; }
  double py(double y) const { return margin + (hi.y - y) * scale; }
};

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const double minutes = lab.arg_num("minutes");
  const std::string basedir = lab.arg("basedir"), map_name = lab.arg("map");
  if (minutes <= 0) {
    lab.fail("se-team: --minutes must be over 0", "next: make lab LAB=se-team ARGS=--help");
    return;
  }
  std::string error;
  std::vector<uint8_t> bytes;
  std::string name = "halls";
  ge::shooter::Locs locs;  // the place names: --locs's file, else the game's (its adapter's)
  if (!lab.arg("locs").empty()) {
    std::FILE* lf = std::fopen(lab.arg("locs").c_str(), "rb");
    if (!lf) {
      lab.fail("se-team: cannot read " + lab.arg("locs"),
               "next: pass --locs=<a .loc file that exists>, or leave it out to name places off "
               "the items");
      return;
    }
    std::string text;
    char buf[4096];
    size_t got = 0;
    while ((got = std::fread(buf, 1, sizeof buf, lf)) > 0) text.append(buf, got);
    std::fclose(lf);
    if (ge::shooter::parse_locs(text, locs) > 0) locs.from = lab.arg("locs");
  }
  if (!basedir.empty()) {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: shooter.");
      return;
    }
    name = map_name.empty() ? "e1m2" : map_name;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
    if (f.empty()) {
      lab.fail("se-team: no maps/" + name + ".bsp in the game folder",
               "Name a map the game folder holds (--map). Skill: shooter.");
      return;
    }
    bytes.assign(f.begin(), f.end());
    // The map's place names, when the game's files hold them (the first game's locs/<map>.loc).
    const cl::GameAdapter& game = cl::classic_adapter();
    if (locs.points.empty()) game.open_locations(game, files, name, locs);
  } else if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes,
                                   &error)) {
    lab.fail("se-team: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
    return;
  }
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-team: the baseline mod: " + error,
             "next: run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  const In in = {{"map", name}, {"minutes", lab.arg("minutes")}, {"seed", lab.arg("seed")}};
  cn::TeamMatchOptions o;
  o.map = bytes;
  o.map_name = name;
  if (basedir.empty()) o.entities = cn::kTeamHallsEntities;
  o.locs = std::move(locs);
  o.progs = &progs;
  o.seed = seed;
  o.client_fed = lab.arg_num("client-fed") != 0;
  o.latency_ms = lab.arg_num("latency");
  o.antilag = lab.arg_num("antilag") != 0;
  o.lead_lag = lab.arg_num("lead") != 0;
  {
    // A weapon drill: its name in the hit table, the mod's bit for it (classic_hits.hpp's order).
    const std::string drill = lab.arg("drill");
    const char* names[] = {"scattergun", "double", "spiker", "heavy", "grenades", "rockets", "arc"};
    for (int k = 0; k < 7; k++)
      if (drill == names[k]) o.drill = 1 << k;
    if (!drill.empty() && o.drill == 0) {
      lab.fail("se-team: no weapon " + drill + " to drill",
               "next: --drill=scattergun, double, spiker, heavy, grenades, rockets or arc.");
      return;
    }
  }
  // The temperaments, red's then blue's, a letter or a word each (a, b, c), comma separated.
  {
    const std::string list = lab.arg("tempers");
    size_t at = 0, k = 0;
    while (!list.empty() && at <= list.size()) {
      const size_t end = std::min(list.find(',', at), list.size());
      sh::Temper tp;
      if (!sh::temper_of(std::string_view(list).substr(at, end - at), tp)) {
        lab.fail("se-team: --tempers: '" + list.substr(at, end - at) + "' is no temperament",
                 "next: aggressive, balanced or coward (a, b or c), one a player, red's first");
        return;
      }
      cn::TeamSide& side = o.teams[k < o.teams[0].skills.size() ? 0 : 1];
      side.tempers.push_back(tp);
      k++;
      at = end + 1;
    }
  }
  // The traits over each player's preset, red's then blue's, comma separated, each its changes
  // joined by '/' ("aim_rl=9/timing=3"; empty: the preset).
  {
    const std::string& list = lab.arg("traits");
    size_t at = 0, k = 0;
    while (!list.empty() && at <= list.size()) {
      const size_t end = std::min(list.find(',', at), list.size());
      cn::TeamSide& side = o.teams[k < o.teams[0].skills.size() ? 0 : 1];
      side.traits.push_back(list.substr(at, end - at));
      k++;
      at = end + 1;
    }
  }
  // The bots' features (shooter/features.hpp): every player's, then red's and blue's over them.
  for (int k = 0; k < 2; k++) {
    o.teams[k].features = lab.arg("features");
    const std::string& own = lab.arg(k == 0 ? "features-red" : "features-blue");
    if (!own.empty()) o.teams[k].features += "," + own;
  }
  const std::string save = lab.arg("save");
  o.packed = save.ends_with("z");  // a `.mvdz`: recorded packed as it goes
  // THE TRACE (`--trace=<file>`): a TSV row per bot every `--trace-every` commands: where it is,
  // how fast, what it means to do and what its pilot did and why. Read by the bot plan's scripts.
  std::ofstream trace;
  if (!lab.arg("trace").empty()) {
    trace.open(lab.arg("trace"));
    if (!trace) {
      lab.fail("se-team: cannot write --trace=" + lab.arg("trace"),
               "next: pass a path in a folder that exists");
      return;
    }
    trace << "t\tbot\tx\ty\tz\tspeed\thealth\tmode\tgoal\tgoal_node\tunlocking\tpilot\tlink"
             "\tscript_step\tbecause\tyaw\tpitch\tview\n";
    const int every = std::max(1, static_cast<int>(lab.arg_num("trace-every")));
    static struct Tracer {
      std::ofstream* out = nullptr;
      int every = 4;
      std::array<int, 64> count{};
    } tracer;
    tracer = Tracer{&trace, every, {}};
    o.on_command_ctx = &tracer;
    o.on_command = [](void* ctx, int k, const shooters_edge::bots::Fighter& f,
                      const shooters_edge::server::RulesCommon& w, const sh::MoveCmd& cmd) {
      Tracer& tr = *static_cast<Tracer*>(ctx);
      if (k < 0 || k >= 64 || (tr.count[static_cast<size_t>(k)]++ % tr.every) != 0) return;
      std::ofstream& out = *tr.out;
      // The fighter's own entity is its slot's (not the client's player number).
      const auto& e = w.world.entities[static_cast<size_t>(f.slot) + 1];
      const sh::Pilot& p = f.bot.pilot.pilot;
      const char* link = "-";
      if (p.g != nullptr && p.k + 1 < p.route.size()) {
        const sh::NavNode& a = p.g->nodes[static_cast<size_t>(p.route[p.k])];
        for (int32_t q = 0; q < a.links; q++) {
          const sh::NavLink& l =
              p.g->links[static_cast<size_t>(a.first_link) + static_cast<size_t>(q)];
          if (l.to == p.route[p.k + 1]) {
            link = sh::nav_kind_name(l.kind);
            break;
          }
        }
      }
      const char* goal =
          f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size()
              ? shooters_edge::bots::item_call(f.items[static_cast<size_t>(f.goal_item)])
              : "-";
      const float sp = std::sqrt(e.velocity.x * e.velocity.x + e.velocity.y * e.velocity.y);
      out << to_fixed(w.sv.time, 2) << '\t' << k << '\t' << to_fixed(e.origin.x, 0) << '\t'
          << to_fixed(e.origin.y, 0) << '\t' << to_fixed(e.origin.z - 24, 0) << '\t'
          << to_fixed(sp, 0) << '\t' << to_fixed(e.health, 0) << '\t'
          << shooters_edge::bots::fight_mode_name(f.mode) << '\t' << goal << '\t' << f.goal_node
          << '\t' << (f.unlocking ? 1 : 0) << '\t' << sh::pilot_mode_name(p.mode) << '\t' << link
          << '\t' << p.script_step << '\t' << p.because << '\t' << to_fixed(cmd.angles.y, 2) << '\t'
          << to_fixed(cmd.angles.x, 2) << '\t' << f.bot.view_by << '\n';
    };
  }
  o.frames = static_cast<int>(std::lround(minutes * 60 * 1000 / shooters_edge::bots::kPilotMsec));
  const cn::TeamMatchReport r = cn::run_team_match(o);
  if (!r.ok) {
    lab.fail("se-team: " + r.error, "next: run tests/shooters_edge_team_test.cpp");
    return;
  }
  const double secs = r.seconds;

  // The table.
  static const char* kWeapon[9] = {
      "", "blade", "scattergun", "double", "spiker", "heavy spiker", "grenades", "rockets", "arc"};
  gl::LabTable t{"each player over the match",
                 {"player", "team", "aim", "frags", "deaths", "shots", "held for a mate", "packs",
                  "hold", "rush", "carry", "push", "free", "held most"},
                 {}};
  for (const cn::TeamPlayer& p : r.players) {
    int64_t alive = 0;
    for (int k = 0; k < bt::kRoles; k++) alive += p.role_commands[k];
    const auto share = [&](int k) {
      return to_fixed(
          alive > 0 ? static_cast<double>(p.role_commands[k]) / static_cast<double>(alive) : 0, 2);
    };
    int most = 1;
    for (int w = 1; w <= 8; w++)
      if (p.weapon_commands[w] > p.weapon_commands[most]) most = w;
    t.rows.push_back({p.name, o.teams[p.team].name, sh::aim_skill_name(p.skill),
                      to_fixed(p.frags, 0), std::to_string(p.deaths), std::to_string(p.shots),
                      std::to_string(p.held_fire), std::to_string(p.packs), share(1), share(2),
                      share(3), share(4), share(0), kWeapon[most]});
    lab.metric("team." + p.name + ".frags", p.frags, "count");
    lab.metric("team." + p.name + ".deaths", p.deaths, "count");
  }
  lab.table(t);
  // The reactions drawn per sighting (shooter/aim.hpp `draw_reaction`).
  gl::LabTable rt{
      "each player's reactions", {"player", "reaction", "sightings", "mean ms", "pre-aimed"}, {}};
  int64_t sightings = 0;
  double react_s = 0;
  for (const cn::TeamPlayer& p : r.players) {
    const double n = static_cast<double>(p.reactions);
    rt.rows.push_back({p.name, std::to_string(p.traits[sh::Trait::reaction]),
                       std::to_string(p.reactions), to_fixed(n > 0 ? p.react_s / n * 1000 : 0, 0),
                       to_fixed(n > 0 ? static_cast<double>(p.pre_aimed) / n : 0, 2)});
    sightings += p.reactions;
    react_s += p.react_s;
  }
  lab.table(rt);
  // Hit rates by weapon (net/classic_hits.hpp): each player's, and each skill's against the bands
  // players of the three divisions reach.
  {
    gl::LabTable ht{"each player's hit rates", {"player", "aim"}, {}};
    for (int w = 0; w < cn::kHitWeapons; w++)
      ht.columns.push_back(cn::hit_weapon_name(static_cast<cn::HitWeapon>(w)));
    std::array<std::array<cn::HitCount, cn::kHitWeapons>, 3> by_skill{};
    const auto rate = [](const cn::HitCount& c) {
      return c.fired > 0
                 ? to_fixed(100.0 * static_cast<double>(c.hit) / static_cast<double>(c.fired), 0) +
                       "% of " + std::to_string(c.fired)
                 : std::string("-");
    };
    for (const cn::TeamPlayer& p : r.players) {
      std::vector<std::string> row{p.name, sh::aim_skill_name(p.skill)};
      for (int w = 0; w < cn::kHitWeapons; w++) row.push_back(rate(p.hits[static_cast<size_t>(w)]));
      ht.rows.push_back(row);
      const int k = static_cast<int>(p.skill) - 1;  // novice, average, expert
      if (k >= 0 && k < 3)
        for (int w = 0; w < cn::kHitWeapons; w++) {
          by_skill[static_cast<size_t>(k)][static_cast<size_t>(w)].fired +=
              p.hits[static_cast<size_t>(w)].fired;
          by_skill[static_cast<size_t>(k)][static_cast<size_t>(w)].hit +=
              p.hits[static_cast<size_t>(w)].hit;
        }
    }
    lab.table(ht);
    static const char* kSkill[3] = {"novice", "average", "expert"};
    for (int k = 0; k < 3; k++)
      for (int w = 0; w < cn::kHitWeapons; w++) {
        const cn::HitCount& c = by_skill[static_cast<size_t>(k)][static_cast<size_t>(w)];
        if (c.fired == 0) continue;
        std::string key = std::string("team.hits.") + kSkill[k] + "." +
                          cn::hit_weapon_name(static_cast<cn::HitWeapon>(w));
        for (char& ch : key)
          if (ch == ' ') ch = '-';
        lab.metric(key, 100.0 * static_cast<double>(c.hit) / static_cast<double>(c.fired), "%");
      }
  }
  {
    // Where the hitscan shots passed a moving target along his motion, by skill: + led too much.
    static const char* kSkillName[3] = {"novice", "average", "expert"};
    double sum[3] = {}, abs_sum[3] = {};
    int64_t n[3] = {};
    for (const cn::TeamPlayer& p : r.players) {
      const int k = p.skill == sh::AimSkill::novice ? 0 : p.skill == sh::AimSkill::expert ? 2 : 1;
      sum[k] += p.lead_sum;
      abs_sum[k] += p.lead_abs;
      n[k] += p.lead_n;
    }
    for (int k = 0; k < 3; k++) {
      if (n[k] == 0) continue;
      lab.metric(std::string("team.lead-miss.") + kSkillName[k], sum[k] / static_cast<double>(n[k]),
                 "units");
      lab.metric(std::string("team.lead-miss-abs.") + kSkillName[k],
                 abs_sum[k] / static_cast<double>(n[k]), "units");
    }
  }
  {
    // Of the time an enemy is in plain view: seen (in the cone), aimed at, still reacting.
    int64_t v = 0, seen = 0, aim = 0, react = 0;
    for (const cn::TeamPlayer& p : r.players) {
      v += p.in_view;
      seen += p.in_view_seen;
      aim += p.in_view_aiming;
      react += p.in_view_reacting;
    }
    const auto of = [&](int64_t k) {
      return v > 0 ? static_cast<double>(k) / static_cast<double>(v) : 0;
    };
    lab.metric("team.in-view.seen", of(seen), "share");
    lab.metric("team.in-view.aiming", of(aim), "share");
    lab.metric("team.in-view.reacting", of(react), "share");
    int64_t checks = 0, far = 0, off = 0, shots = 0;
    for (const cn::TeamPlayer& p : r.players) {
      checks += p.trigger_checks;
      far += p.trigger_far;
      off += p.trigger_off;
      shots += p.shots;
    }
    const auto per = [&](int64_t k) {
      return checks > 0 ? static_cast<double>(k) / static_cast<double>(checks) : 0;
    };
    lab.metric("team.trigger.fired", per(shots), "share");
    lab.metric("team.trigger.out-of-range", per(far), "share");
    lab.metric("team.trigger.view-off", per(off), "share");
    lab.metric("team.trigger.per-in-view",
               v > 0 ? static_cast<double>(checks) / (4.0 * static_cast<double>(v)) : 0, "share");
  }
  lab.metric("team.sightings", static_cast<double>(sightings), "count");
  lab.metric("team.reaction-mean-ms",
             sightings > 0 ? react_s / static_cast<double>(sightings) * 1000 : 0, "ms");
  gl::LabTable pt{
      "each player's legs",
      {"player", "travelled", "plans", "stuck", "lost", "waits", "barred", "dives", "given up",
       "dive s", "off graph", "walk", "wait", "arrived", "idle", "air", "hop"},
      {}};
  for (const cn::TeamPlayer& p : r.players) {
    int64_t live = 0;
    for (int k = 0; k < sh::kPilotModes; k++) live += p.pilot_modes[k];
    const auto share = [&](int k) {
      return to_fixed(
          live > 0 ? static_cast<double>(p.pilot_modes[k]) / static_cast<double>(live) : 0, 2);
    };
    pt.rows.push_back(
        {p.name, to_fixed(p.travelled, 0), std::to_string(p.plans), std::to_string(p.stuck),
         std::to_string(p.lost), std::to_string(p.waits), std::to_string(p.barred),
         std::to_string(p.dives), std::to_string(p.dives_given_up), to_fixed(p.dive_wet_s, 0),
         std::to_string(p.off_graph), share(1), share(5), share(8), share(0), share(4), share(9)});
  }
  lab.table(pt);
  gl::LabTable tt{
      "each team",
      {"team", "frags", "deaths", "quads taken", "rushes", "shots held", "callouts", "messages"},
      {}};
  for (int k = 0; k < 2; k++) {
    const cn::TeamTotals& tm = r.teams[k];
    tt.rows.push_back({o.teams[k].name, to_fixed(tm.frags, 0), std::to_string(tm.deaths),
                       std::to_string(tm.quad_pickups), std::to_string(tm.rushes),
                       std::to_string(tm.held_shots), std::to_string(tm.callouts),
                       std::to_string(tm.messages)});
    const std::string key = "team." + o.teams[k].name;
    lab.metric(key + ".frags", tm.frags, "count");
    lab.metric(key + ".quads", static_cast<double>(tm.quad_pickups), "count");
    lab.metric(key + ".messages", static_cast<double>(tm.messages), "count");
  }
  lab.table(tt);
  // What each player knew of the map's items, at the start, at the half and at the end.
  gl::LabTable kt{"each player's map: items known (recalled from anywhere)",
                  {"player", "knows the map as", "start", "half", "end", "of", "messages"},
                  {}};
  for (const cn::TeamPlayer& p : r.players) {
    const auto at = [&](size_t k) {
      return p.known.empty() ? 0 : p.known[std::min(k, p.known.size() - 1)];
    };
    kt.rows.push_back({p.name, bt::map_sense_name(bt::map_sense_of(p.skill)), std::to_string(at(0)),
                       std::to_string(at(p.known.size() / 2)), std::to_string(at(p.known.size())),
                       std::to_string(r.item_at.size()), std::to_string(p.said)});
  }
  lab.table(kt);
  gl::LabTable bt_{"the map's buttons: where, how, and what each opens",
                   {"button at (x y z)", "how", "doors", "opens", "used from (x y z)"},
                   {}};
  const auto xyz = [](sh::Vec3f a) {
    return to_fixed(a.x, 0) + " " + to_fixed(a.y, 0) + " " + to_fixed(a.z, 0);
  };
  for (const cn::TeamButton& b : r.buttons) {
    std::string opens;
    for (const std::string& n : b.opens) opens += (opens.empty() ? "" : ", ") + n;
    bt_.rows.push_back({xyz(b.at), b.shoot ? "shoot" : "touch", std::to_string(b.doors.size()),
                        opens.empty() ? "(a way through)" : opens, xyz(b.from)});
  }
  lab.table(bt_);
  lab.metric("team.buttons", static_cast<double>(r.buttons.size()), "count");
  gl::LabTable et{
      "each player's ears and area play",
      {"player", "ears", "sounds", "taken in", "items heard taken", "spawns heard", "spam rockets"},
      {}};
  int64_t heard_taken = 0, spawns = 0, sounds = 0, heard = 0;
  for (const cn::TeamPlayer& p : r.players) {
    et.rows.push_back({p.name, bt::map_sense_name(bt::map_sense_of(p.skill)),
                       std::to_string(p.sounds), std::to_string(p.heard),
                       std::to_string(p.heard_taken), std::to_string(p.heard_spawns),
                       std::to_string(p.spam_shots)});
    heard_taken += p.heard_taken;
    spawns += p.heard_spawns;
    sounds += p.sounds;
    heard += p.heard;
  }
  lab.table(et);
  gl::LabTable tt_{
      "each player's temperament",
      {"player", "temperament", "aim", "frags", "deaths", "outmatched s", "after sounds s"},
      {}};
  for (const cn::TeamPlayer& p : r.players) {
    tt_.rows.push_back({p.name, sh::temper_name(p.temper), sh::aim_skill_name(p.skill),
                        to_fixed(p.frags, 0), std::to_string(p.deaths),
                        to_fixed(static_cast<double>(p.outmatched) * bt::kPilotMsec / 1000, 0),
                        to_fixed(static_cast<double>(p.sound_chases) * bt::kPilotMsec / 1000, 0)});
  }
  lab.table(tt_);
  gl::LabTable traits_table{"each player's traits (1 to 10; shooter/traits.hpp)", {"player"}, {}};
  for (size_t i = 0; i < sh::kTraits; i++)
    traits_table.columns.push_back(sh::trait_name(static_cast<sh::Trait>(i)));
  for (const cn::TeamPlayer& p : r.players) {
    std::vector<std::string> row{p.name};
    for (uint8_t v : p.traits.v) row.push_back(std::to_string(v));
    traits_table.rows.push_back(row);
  }
  lab.table(traits_table);
  // The features, when a player's are not his preset's (shooter/features.hpp).
  gl::LabTable feat_table{
      "each player's bot features (shooter/features.hpp)", {"player", "features"}, {}};
  for (const cn::TeamPlayer& p : r.players)
    if (p.features != sh::features_of(p.skill))
      feat_table.rows.push_back({p.name, sh::features_text(p.features)});
  if (!feat_table.rows.empty()) lab.table(feat_table);
  lab.metric("team.sounds-taken-in", static_cast<double>(heard), "count");
  lab.metric("team.sounds-missed", static_cast<double>(sounds - heard), "count");
  lab.metric("team.items-heard-taken", static_cast<double>(heard_taken), "count");
  lab.metric("team.spawns-heard", static_cast<double>(spawns), "count");
  lab.metric("team.areas-lost-seen",
             static_cast<double>(r.teams[0].lost_seen + r.teams[1].lost_seen), "count");
  lab.metric("team.retakes", static_cast<double>(r.teams[0].retakes + r.teams[1].retakes), "count");
  lab.metric("team.pushes", static_cast<double>(r.teams[0].pushes + r.teams[1].pushes), "count");
  {
    int64_t changes = 0;
    int64_t up = 0, failed = 0, swum = 0;
    for (const cn::TeamPlayer& p : r.players) {
      changes += p.goal_changes;
      up += p.surfaced;
      failed += p.surface_failed;
      swum += p.swim_fights;
    }
    lab.metric("team.surfaced", static_cast<double>(up), "count");
    // Every death's pack known (bots/packs.hpp), by how its place was known; the weapon guessed
    // right by the guesser's skill (novice, average, expert); trips to them and what ended them.
    {
      static constexpr const char* kFrom[5] = {"seen", "cry", "blast", "aim", "lying"};
      int64_t known[5] = {}, guesses[4] = {}, right[4] = {}, guess_rl[4] = {}, truth_rl[4] = {};
      int64_t trips = 0, reached = 0, gone = 0;
      for (const cn::TeamPlayer& p : r.players) {
        for (int k = 0; k < 5; k++) known[k] += p.packs_known[k];
        const auto sk = static_cast<size_t>(p.skill);
        guesses[sk] += p.pack_guesses;
        right[sk] += p.pack_right;
        guess_rl[sk] += p.pack_guess_rl;
        truth_rl[sk] += p.pack_truth_rl;
        trips += p.pack_trips;
        reached += p.pack_reached;
        gone += p.pack_seen_gone;
      }
      for (int k = 0; k < 5; k++)
        lab.metric(std::string("team.packs.known.") + kFrom[k], static_cast<double>(known[k]),
                   "count");
      for (int k = 1; k < 4; k++) {  // by the guesser's skill (none plays `perfect` here)
        const std::string pre =
            std::string("team.packs.guess.") + sh::aim_skill_name(static_cast<sh::AimSkill>(k));
        lab.metric(pre + ".count", static_cast<double>(guesses[k]), "count");
        lab.metric(pre + ".right",
                   guesses[k] ? static_cast<double>(right[k]) / static_cast<double>(guesses[k]) : 0,
                   "share");
        lab.metric(pre + ".rl-guessed", static_cast<double>(guess_rl[k]), "count");
        lab.metric(pre + ".rl-true", static_cast<double>(truth_rl[k]), "count");
      }
      lab.metric("team.packs.trips", static_cast<double>(trips), "count");
      lab.metric("team.packs.reached", static_cast<double>(reached), "count");
      lab.metric("team.packs.seen-gone", static_cast<double>(gone), "count");
    }
    lab.metric("team.swim-fights", static_cast<double>(swum), "count");
    // The share of the time alive waist deep in water (people's recordings: 9 %, se-people `wet`).
    int64_t wet = 0, alive = 0;
    for (const cn::TeamPlayer& p : r.players) {
      wet += p.wet;
      for (int k = 0; k < 4; k++) alive += p.mode_commands[k];
    }
    lab.metric("team.wet", alive > 0 ? static_cast<double>(wet) / static_cast<double>(alive) : 0,
               "share");
    // Shots and damage by the target's state (net/classic_hits.hpp): the pellets' and the rockets'
    // hits over those fired at a target in that state, the damage he took a minute lived in it,
    // and the share of the time lived in it.
    {
      int64_t all = 0;
      for (const int64_t f : r.frames_at) all += f;
      for (int k = 0; k < cn::kTargetStates; k++) {
        const auto i = static_cast<size_t>(k);
        const std::string key = std::string("team.target.") + cn::kTargetStateNames[k];
        const auto rate = [](const cn::HitCount& c) {
          return c.fired > 0 ? static_cast<double>(c.hit) / static_cast<double>(c.fired) : 0;
        };
        // (player-minutes: a frame lasts the match's seconds over its frames)
        const double lived = static_cast<double>(r.frames_at[i]) * r.seconds /
                             std::max<double>(1, static_cast<double>(r.frames)) / 60;
        lab.metric(key + ".pellets", rate(r.pellets_at[i]), "share");
        lab.metric(key + ".rockets", rate(r.rockets_at[i]), "share");
        lab.metric(key + ".rockets-fired", static_cast<double>(r.rockets_at[i].fired), "count");
        lab.metric(key + ".damage-per-min", lived > 0 ? r.given_at[i] / lived : 0, "hp/min");
        lab.metric(key + ".time",
                   all > 0 ? static_cast<double>(r.frames_at[i]) / static_cast<double>(all) : 0,
                   "share");
      }
    }
    // The key items lying untaken (classic_team.hpp), and what the fighters made of it: nobody
    // believed it there, nobody wanted it, the one who wanted it was fighting, one was on his way.
    for (int k = 0; k < cn::kLyingItems; k++) {
      const auto ki = static_cast<size_t>(k);
      if (r.lying_n[ki] == 0) continue;
      const std::string key = std::string("team.lying.") + cn::kLyingItemNames[k];
      lab.metric(key, static_cast<double>(r.lying[ki]) / static_cast<double>(r.lying_n[ki]),
                 "share");
      for (int w = 0; w < cn::kLyingWhy; w++)
        lab.metric(key + "." + cn::kLyingWhyNames[w],
                   r.lying[ki] > 0 ? static_cast<double>(r.lying_why[ki][static_cast<size_t>(w)]) /
                                         static_cast<double>(r.lying[ki])
                                   : 0,
                   "share");
    }
    // The wet time by cause (bots/fighter.hpp kWetWhy), as shares of it: what keeps the bots in
    // the water; and wet aiming, swimming and walking, each's share, speed and time still.
    for (size_t w = 0; w < 12; w++) {
      int64_t why = 0;
      for (const cn::TeamPlayer& p : r.players) why += p.wet_why[w];
      lab.metric(std::string("team.wet-why.") + bt::kWetWhy[w],
                 wet > 0 ? static_cast<double>(why) / static_cast<double>(wet) : 0, "share");
    }
    static const char* kWetHow[3] = {"aiming", "swim", "walk"};
    for (size_t w = 0; w < 3; w++) {
      double sp = 0;
      int64_t n = 0, still = 0;
      for (const cn::TeamPlayer& p : r.players) {
        sp += p.wet_speed[w];
        n += p.wet_n[w];
        still += p.wet_still[w];
      }
      const std::string k = std::string("team.wet-") + kWetHow[w];
      lab.metric(k + ".share", wet > 0 ? static_cast<double>(n) / static_cast<double>(wet) : 0,
                 "share");
      lab.metric(k + ".speed", n > 0 ? sp / static_cast<double>(n) : 0, "units/s");
      lab.metric(k + ".still", n > 0 ? static_cast<double>(still) / static_cast<double>(n) : 0,
                 "share");
    }
    // Pickups by kind a player-minute, set beside people's (a match's statistics count them so).
    static const char* kKinds[12] = {"health", "mega",    "armor1", "armor2", "armor3", "shells",
                                     "nails",  "rockets", "cells",  "weapon", "quad",   "pent"};
    int64_t ways = 0;
    for (const cn::TeamPlayer& p : r.players) ways += p.on_the_way;
    lab.metric("team.on-the-way", static_cast<double>(ways), "count");
    const double player_min = static_cast<double>(r.players.size()) * r.seconds / 60.0;
    for (size_t k = 0; k < 12; k++) {
      int64_t n = 0;
      for (const cn::TeamPlayer& p : r.players) n += p.took[k];
      lab.metric(std::string("team.took-per-min.") + kKinds[k],
                 player_min > 0 ? static_cast<double>(n) / player_min : 0, "per-min");
    }
    lab.metric("team.surface-failed", static_cast<double>(failed), "count");
    lab.metric("team.goal-changes-per-min",
               static_cast<double>(changes) /
                   std::max(1.0, static_cast<double>(r.players.size()) * minutes),
               "per player-min");
  }
  lab.metric("team.push-joins", static_cast<double>(r.teams[0].joins + r.teams[1].joins), "count");
  lab.metric("team.spam-rockets",
             static_cast<double>(r.teams[0].spam_shots + r.teams[1].spam_shots), "count");
  // The team messages: the first lines in the report, all of them as a file.
  {
    std::string all;
    for (const std::string& l : r.chat) all += l + "\n";
    lab.attach("chat.txt", std::vector<uint8_t>(all.begin(), all.end()), gl::Det::deterministic);
    for (size_t k = 0; k < r.chat.size() && k < 24; k++) lab.note(r.chat[k]);
  }
  // The deaths by how (the server's obituaries, the names taken out): what kills the players.
  {
    std::vector<std::string> names;
    names.reserve(r.players.size());
    for (const cn::TeamPlayer& p : r.players) names.push_back(p.name);
    std::map<std::string, int> how;
    std::string all;
    for (std::string l : r.prints) {
      all += l;
      while (!l.empty() && (l.back() == '\n' || l.back() == ' ')) l.pop_back();
      bool death = false;
      for (const std::string& n : names)
        for (size_t at = l.find(n); at != std::string::npos; at = l.find(n, at + 1)) {
          const bool word =
              (at == 0 || l[at - 1] == ' ') &&
              (at + n.size() == l.size() || l[at + n.size()] == ' ' || l[at + n.size()] == '\'');
          if (!word) continue;
          l.replace(at, n.size(), "*");
          death = true;
        }
      if (death && l.size() > 2 && l.find(": ") == std::string::npos) how[l]++;
    }
    lab.attach("prints.txt", std::vector<uint8_t>(all.begin(), all.end()), gl::Det::deterministic);
    std::string dl =
        "t\tplayer\tat\tliquid\tmode\tlink\tat 3 s before\tits mode\tthe way in (2, 1, 0.5 s "
        "before)\n";
    for (const std::string& l : r.deaths) dl += l + "\n";
    lab.attach("deaths.tsv", std::vector<uint8_t>(dl.begin(), dl.end()), gl::Det::deterministic);
    std::vector<std::pair<int, std::string>> by;
    by.reserve(how.size());
    for (const auto& [k, v] : how) by.push_back({-v, k});
    std::sort(by.begin(), by.end());
    gl::LabTable dt{
        "the deaths by how (the server's obituaries, * a player)", {"how", "count"}, {}};
    for (size_t k = 0; k < by.size() && k < 16; k++)
      dt.rows.push_back({by[k].second, std::to_string(-by[k].first)});
    lab.table(dt);
  }
  lab.metric("team.frags-per-minute", (r.teams[0].frags + r.teams[1].frags) / (secs / 60),
             "per-min");
  // The dodge (shooter/dodge.hpp): the share of the fight spent side-stepping, its turns a minute,
  // and every weapon's hits over its shots (what the dodge is for).
  {
    int64_t dodging = 0, turns = 0, fight = 0, fired = 0, hit = 0, watching = 0, alive = 0;
    int64_t reacted = 0, pre = 0, kept = 0, swipes = 0;
    for (const cn::TeamPlayer& p : r.players) {
      watching += p.watching;
      kept += p.target_kept;
      swipes += p.sound_swipes;
      reacted += p.reactions;
      pre += p.pre_aimed;
      for (int k = 0; k < 4; k++) alive += p.mode_commands[k];
      dodging += p.dodging;
      turns += p.dodge_turns;
      fight += p.mode_commands[static_cast<int>(bt::FightMode::fight)];
      for (const cn::HitCount& c : p.hits) {
        fired += c.fired;
        hit += c.hit;
      }
    }
    // The mind's modes: the share of the time alive roaming, fighting, chasing and retreating.
    {
      int64_t by[4] = {};
      for (const cn::TeamPlayer& p : r.players)
        for (int k = 0; k < 4; k++) by[k] += p.mode_commands[k];
      for (int k = 0; k < 4; k++)
        lab.metric(std::string("team.mode.") + bt::fight_mode_name(static_cast<bt::FightMode>(k)),
                   alive > 0 ? static_cast<double>(by[k]) / static_cast<double>(alive) : 0,
                   "share");
    }
    {
      // People's on dm2 (the se-rj lab): an expert rocket jumps 1.9 times in ten minutes.
      int64_t rj = 0, fl = 0;
      for (const cn::TeamPlayer& p : r.players) rj += p.rocket_jumps, fl += p.flips;
      lab.metric("team.rocket-jumps", static_cast<double>(rj), "count");
      lab.metric("team.flips", static_cast<double>(fl), "count");
      int64_t es = 0, eo = 0;
      for (const cn::TeamPlayer& p : r.players) es += p.escapes, eo += p.escapes_out;
      lab.metric("team.escapes", static_cast<double>(es), "count");
      lab.metric("team.escapes-out", static_cast<double>(eo), "count");
      // The gap tricks (shooter/nav_gaps.hpp): jumped out of a hop, landed across, given up.
      // People's on dm2 (the se-rj lab): the YA pit's about 6 a match, the RA pit's some 40.
      int64_t gj = 0, gl = 0, gr = 0;
      for (const cn::TeamPlayer& p : r.players)
        gj += p.gap_jumps, gl += p.gap_landed, gr += p.gap_refused;
      lab.metric("team.gap-jumps", static_cast<double>(gj), "count");
      lab.metric("team.gap-landed", static_cast<double>(gl), "count");
      lab.metric("team.gap-refused", static_cast<double>(gr), "count");
      // The hops (shooter/pilot.hpp): stretches entered, and jumps pressed on their landings.
      int64_t hs = 0, hj = 0;
      for (const cn::TeamPlayer& p : r.players) hs += p.hops, hj += p.hop_jumps;
      lab.metric("team.hops", static_cast<double>(hs), "count");
      lab.metric("team.hop-jumps", static_cast<double>(hj), "count");
    }
    lab.metric("team.dodge-share",
               fight > 0 ? static_cast<double>(dodging) / static_cast<double>(fight) : 0, "share");
    lab.metric("team.dodge-turns-per-min",
               static_cast<double>(turns) / (secs / 60) / static_cast<double>(r.players.size()),
               "per-min");
    double speed = 0;
    for (const cn::TeamPlayer& p : r.players) speed += p.speed_sum;
    // People's 4on4 average on this kind of map: 284 units/s (KTX's match stats).
    lab.metric("team.speed-avg", alive > 0 ? speed / static_cast<double>(alive) : 0, "units/s");
    // Damage given a player-minute: people's 4on4 on e1m2, 398 (KTX's `given`).
    double given = 0;
    for (const cn::TeamPlayer& p : r.players) given += p.given;
    int64_t vn = 0, iv = 0, sn = 0, un = 0;
    for (const cn::TeamPlayer& p : r.players) {
      vn += p.view_n;
      iv += p.in_view;
      sn += p.seen_n;
      un += p.unnoticed;
    }
    const auto of_alive = [&](int64_t k) {
      return vn > 0 ? static_cast<double>(k) / static_cast<double>(vn) : 0;
    };
    int64_t rlp = 0, rls = 0, rll = 0, rockets = 0;
    for (const cn::TeamPlayer& p : r.players) {
      rlp += p.rl_pickups;
      rls += p.rl_samples;
      rll += p.rl_loaded;
      rockets += p.hits[static_cast<size_t>(cn::HitWeapon::rocket_direct)].fired;
    }
    // People's 4on4 on e1m2: about 30 launchers taken in 10 minutes, 3.4 rockets a player-minute.
    lab.metric("team.rl-pickups", static_cast<double>(rlp), "count");
    lab.metric("team.rl-had", of_alive(rls), "share");
    lab.metric("team.rl-loaded", of_alive(rll), "share");
    lab.metric("team.rockets-per-min",
               static_cast<double>(rockets) / (secs / 60) /
                   static_cast<double>(std::max<size_t>(r.players.size(), 1)),
               "per-min");
    lab.metric("team.enemy-in-view", of_alive(iv), "share");
    lab.metric("team.enemy-seen", of_alive(sn), "share");
    lab.metric("team.roam-enemy-in-view", of_alive(un), "share");
    lab.metric("team.damage-per-min",
               given / (secs / 60) / static_cast<double>(std::max<size_t>(r.players.size(), 1)),
               "per-min");
    // Each side's fight apart, when the sides' features differ (an A/B by team): what one side
    // does better than the other in the same matches.
    bool differ = lab.arg("sides") == "1";
    for (const cn::TeamPlayer& p : r.players) differ |= p.features != r.players.front().features;
    for (int side = 0; side < 2 && differ; side++) {
      double dmg = 0, react = 0, s_fired = 0, s_hit = 0;
      int64_t n = 0, reactions = 0, s_pre = 0, view_n = 0, in_view = 0, seen = 0, checks = 0,
              off = 0, shots = 0, deaths = 0, s_wet = 0, s_alive = 0;
      for (const cn::TeamPlayer& p : r.players) {
        if (p.team != side) continue;
        n++;
        dmg += p.given;
        react += p.react_s;
        reactions += p.reactions;
        s_pre += p.pre_aimed;
        view_n += p.view_n;
        in_view += p.in_view;
        seen += p.in_view_seen;
        checks += p.trigger_checks;
        off += p.trigger_off;
        shots += p.shots;
        deaths += p.deaths;
        s_wet += p.wet;
        for (int m = 0; m < 4; m++) s_alive += p.mode_commands[m];
        for (const cn::HitCount& h : p.hits) {
          s_fired += static_cast<double>(h.fired);
          s_hit += static_cast<double>(h.hit);
        }
      }
      const auto ratio = [](double a, double b) { return b > 0 ? a / b : 0; };
      const std::string k = std::string("team.side.") + (side == 0 ? "red." : "blue.");
      lab.metric(k + "deaths", static_cast<double>(deaths), "count");
      lab.metric(k + "wet", ratio(static_cast<double>(s_wet), static_cast<double>(s_alive)),
                 "share");
      lab.metric(k + "damage-per-min", ratio(dmg, secs / 60 * static_cast<double>(n)), "per-min");
      lab.metric(k + "accuracy", 100 * ratio(s_hit, s_fired), "%");
      lab.metric(k + "sightings", static_cast<double>(reactions), "count");
      lab.metric(k + "pre-aimed", ratio(static_cast<double>(s_pre), static_cast<double>(reactions)),
                 "share");
      lab.metric(k + "reaction-mean-ms", 1000 * ratio(react, static_cast<double>(reactions)), "ms");
      lab.metric(k + "enemy-in-view",
                 ratio(static_cast<double>(in_view), static_cast<double>(view_n)), "share");
      lab.metric(k + "in-view.seen", ratio(static_cast<double>(seen), static_cast<double>(in_view)),
                 "share");
      lab.metric(k + "trigger.fired",
                 ratio(static_cast<double>(shots), static_cast<double>(checks)), "share");
      int64_t unseen[4] = {}, unseen_calm = 0, unseen_n = 0;
      for (const cn::TeamPlayer& p : r.players) {
        if (p.team != side) continue;
        for (int b = 0; b < 4; b++) unseen[b] += p.unseen_off[b];
        unseen_calm += p.unseen_calm;
      }
      for (int64_t u : unseen) unseen_n += u;
      const char* kBands[4] = {"under-60", "60-90", "90-135", "behind"};
      for (int b = 0; b < 4; b++)
        lab.metric(k + "unseen." + kBands[b],
                   ratio(static_cast<double>(unseen[b]), static_cast<double>(unseen_n)), "share");
      int64_t mv[6] = {}, mo[6] = {}, mn = 0, moff = 0;
      for (const cn::TeamPlayer& p : r.players) {
        if (p.team != side) continue;
        for (int h = 0; h < 6; h++) {
          mv[h] += p.moving_n[h];
          mo[h] += p.moving_off[h];
        }
      }
      for (int h = 0; h < 6; h++) {
        mn += mv[h];
        moff += mo[h];
      }
      const char* kHolders[6] = {"aim", "look", "calm", "walk", "fly", "other"};
      int64_t cro = 0;
      for (const cn::TeamPlayer& p : r.players)
        if (p.team == side) cro += p.calm_route_off;
      lab.metric(k + "moving.calm.route-off45",
                 ratio(static_cast<double>(cro), static_cast<double>(mv[2])), "share");
      lab.metric(k + "moving.off45", ratio(static_cast<double>(moff), static_cast<double>(mn)),
                 "share");
      for (int h = 0; h < 6; h++) {
        lab.metric(k + "moving." + kHolders[h],
                   ratio(static_cast<double>(mv[h]), static_cast<double>(mn)), "share");
        lab.metric(k + "moving." + kHolders[h] + ".off45",
                   ratio(static_cast<double>(mo[h]), static_cast<double>(mv[h])), "share");
      }
      lab.metric(k + "unseen.calm",
                 ratio(static_cast<double>(unseen_calm), static_cast<double>(unseen_n)), "share");
      lab.metric(k + "trigger.view-off",
                 ratio(static_cast<double>(off), static_cast<double>(checks)), "share");
    }
    // Where the speed goes: each pilot mode's share of the commands alive and its mean speed.
    gl::LabTable modes{
        "the pilot's modes (aiming apart): share of the time alive and mean speed (units/s)",
        {"mode", "share", "speed"},
        {}};
    for (int m = 0; m <= sh::kPilotModes; m++) {
      double sum = 0;
      int64_t n = 0;
      for (const cn::TeamPlayer& p : r.players) {
        sum += p.mode_speed[m];
        n += p.mode_n[m];
      }
      if (n == 0) continue;
      modes.rows.push_back(
          {m == sh::kPilotModes ? "aiming" : sh::pilot_mode_name(static_cast<sh::PilotMode>(m)),
           to_fixed(alive > 0 ? static_cast<double>(n) / static_cast<double>(alive) : 0, 2),
           to_fixed(sum / static_cast<double>(n), 0)});
    }
    lab.table(modes);
    lab.metric("team.watch-share",
               alive > 0 ? static_cast<double>(watching) / static_cast<double>(alive) : 0, "share");
    lab.metric("team.pre-aimed-share",
               reacted > 0 ? static_cast<double>(pre) / static_cast<double>(reacted) : 0, "share");
    lab.metric("team.target-kept", static_cast<double>(kept), "count");
    lab.metric("team.sound-swipes", static_cast<double>(swipes), "count");
    // What the match was fought with: each weapon's share of the commands holding one.
    int64_t held[9] = {}, all = 0;
    for (const cn::TeamPlayer& p : r.players)
      for (int w = 1; w <= 8; w++) {
        held[w] += p.weapon_commands[w];
        all += p.weapon_commands[w];
      }
    static const char* kHeld[9] = {"",      "blade",    "scattergun", "double", "spiker",
                                   "heavy", "grenades", "rockets",    "arc"};
    for (int w = 1; w <= 8; w++)
      lab.metric(std::string("team.held.") + kHeld[w],
                 all > 0 ? static_cast<double>(held[w]) / static_cast<double>(all) : 0, "share");
    lab.metric("team.accuracy",
               fired > 0 ? 100.0 * static_cast<double>(hit) / static_cast<double>(fired) : 0, "%");
  }
  lab.metric("team.faults", static_cast<double>(r.faults), "count");
  lab.metric("team.place-names", static_cast<double>(r.locs), "count");
  lab.note("place names: " + std::to_string(r.locs) + " from " + r.locs_from);
  lab.note("the map's team plan (bots/map_plan.hpp):\n" + r.plan);
  lab.note("the map's goals (bots/goals.hpp):\n" + r.goals);
  lab.metric("team.plan-areas", r.plan_areas, "count");
  lab.metric("team.lines-read", static_cast<double>(r.lines_read), "count");
  lab.metric("team.lines-missed", static_cast<double>(r.lines_missed), "count");
  lab.metric("team.nav-nodes", static_cast<double>(r.nav_nodes), "count");
  lab.metric("team.teleport-links", static_cast<double>(r.teleport_links), "count");
  lab.metric("team.movers", static_cast<double>(r.movers), "count");
  lab.metric("team.closed-links", static_cast<double>(r.closed_links), "count");
  lab.metric("team.swim-wet-cells", static_cast<double>(r.swim_wet), "count");
  lab.metric("team.swim-leading-cells", static_cast<double>(r.swim_leading), "count");
  lab.metric("team.dive-pockets", static_cast<double>(r.dives), "count");
  {
    int64_t begun = 0, given_up = 0;
    double wet_s = 0;
    for (const cn::TeamPlayer& p : r.players) {
      begun += p.dives;
      given_up += p.dives_given_up;
      wet_s += p.dive_wet_s;
    }
    lab.metric("team.dives", static_cast<double>(begun), "count");
    lab.metric("team.dives-given-up", static_cast<double>(given_up), "count");
    lab.metric("team.dive-wet-s", wet_s, "s");
  }
  {
    double lag = 0;
    int64_t n = 0;
    for (const cn::TeamPlayer& p : r.players) {
      lag += p.lag_s;
      n += p.lag_n;
    }
    if (n > 0) lab.metric("team.round-trip-ms", 1000 * lag / static_cast<double>(n), "ms");
  }
  lab.metric("team.nav-build-ms", r.nav_build_ms, "ms", gl::Det::measured);
  lab.metric("team.mvd-bytes", static_cast<double>(r.mvd.size()), "bytes");
  for (int key = 0; key < static_cast<int>(r.item_at.size()); key++) {
    if (key != r.hold_item && key != r.quad_item && lab.arg("items") != "all") continue;
    const sh::Vec3f a = r.item_at[static_cast<size_t>(key)],
                    b = r.item_node_at[static_cast<size_t>(key)];
    float nearest = 1e9f;
    for (const cn::TeamPlayer& pl : r.players)
      for (const sh::Vec3f& q : pl.path) {
        const sh::Vec3f d = q - a;
        nearest = std::min(nearest, std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z));
      }
    lab.note(r.item_name[static_cast<size_t>(key)] + " at " + to_fixed(a.x, 0) + " " +
             to_fixed(a.y, 0) + " " + to_fixed(a.z, 0) + ", its node at " + to_fixed(b.x, 0) + " " +
             to_fixed(b.y, 0) + " " + to_fixed(b.z, 0) + ", the nearest a player came " +
             to_fixed(nearest, 0));
  }
  std::string unrouted;
  for (size_t i = 0; i < r.item_routed.size(); i++)
    if (!r.item_routed[i]) unrouted += (unrouted.empty() ? "" : ", ") + r.item_name[i];
  lab.note("items no route leads to from the map's main part: " +
           (unrouted.empty() ? "none" : unrouted));
  for (const cn::TeamPlayer& pl : r.players) lab.note(pl.name + ": " + pl.last);
  char hex[24];
  std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(r.digest));
  lab.note(std::string("digest ") + hex);

  // The map from above.
  View v{r.lo, r.hi};
  const double span = std::max(r.hi.x - r.lo.x, r.hi.y - r.lo.y);
  v.scale = span > 0 ? 1400.0 / span : 1;
  const int w = static_cast<int>((r.hi.x - r.lo.x) * v.scale) + 2 * v.margin;
  const int h = static_cast<int>((r.hi.y - r.lo.y) * v.scale) + 2 * v.margin;
  gl::Picture p = gl::create_picture(w, h, {236, 236, 230, 255});
  // The floor: every nav node, shaded by height; red where the first spawn cannot reach it.
  {
    const float zspan = std::max(1.f, r.hi.z - r.lo.z);
    for (size_t i = 0; i < r.nav_points.size(); i++) {
      const sh::Vec3f a = r.nav_points[i];
      const auto g = static_cast<uint8_t>(205 - 80 * std::clamp((a.z - r.lo.z) / zspan, 0.f, 1.f));
      const bool reached = i < r.nav_reached.size() && r.nav_reached[i];
      const gl::Rgba c = reached ? gl::Rgba{g, g, static_cast<uint8_t>(g - 10), 255}
                                 : gl::Rgba{240, 170, 160, 255};
      const int sz = std::max(2, static_cast<int>(32 * v.scale) - 1);
      gl::fill_rect(p, static_cast<int>(v.px(a.x - 16)), static_cast<int>(v.py(a.y + 16)), sz, sz,
                    c);
    }
  }
  for (const cn::TeamPlayer& pl : r.players) {
    gl::Rgba c = kTeam[pl.team];
    c.a = 110;
    for (size_t i = 0; i + 1 < pl.path.size(); i++) {
      const sh::Vec3f a = pl.path[i], b = pl.path[i + 1];
      if (pl.roles[i] == 255 || pl.roles[i + 1] == 255) continue;
      const sh::Vec3f d = a - b;
      if (d.x * d.x + d.y * d.y > 160 * 160) continue;  // a respawn or a teleport
      gl::draw_line(p, v.px(a.x), v.py(a.y), v.px(b.x), v.py(b.y), c, 2);
    }
  }
  // The map's places by name (the team's callouts), each at its points' middle.
  for (const sh::Loc& l : r.places) {
    const int lw = gl::text_width(l.name, 2), lx = static_cast<int>(v.px(l.at.x)) - lw / 2,
              ly = static_cast<int>(v.py(l.at.y)) - 30;
    gl::fill_rect(p, lx - 3, ly - 3, lw + 6, 20, gl::Rgba{255, 255, 255, 220});
    gl::draw_text(p, lx, ly, l.name, gl::Rgba{40, 70, 150, 255}, 2);
  }
  for (size_t i = 0; i < r.item_at.size(); i++) {
    const sh::Vec3f a = r.item_at[i];
    const bool key = static_cast<int>(i) == r.hold_item || static_cast<int>(i) == r.quad_item;
    const int s = key ? 7 : 3;
    gl::fill_rect(p, static_cast<int>(v.px(a.x)) - s, static_cast<int>(v.py(a.y)) - s, 2 * s, 2 * s,
                  key ? gl::Rgba{250, 210, 40, 255} : gl::Rgba{250, 250, 240, 255});
    gl::draw_ring(p, v.px(a.x), v.py(a.y), s + 2, kInk);
    std::string n = r.item_name[i];
    for (const char* pre : {"weapon_", "item_"})
      if (n.rfind(pre, 0) == 0) n = n.substr(std::string(pre).size());
    if (key || n.find("armor") != std::string::npos || n.find("launcher") != std::string::npos ||
        n.find("super") != std::string::npos || n == "mega health")
      gl::draw_text(p, static_cast<int>(v.px(a.x)) + s + 4, static_cast<int>(v.py(a.y)) - 4, n,
                    kInk);
  }
  // The buttons: a square (red for a shot, blue for a touch), a line to each door it opens (a
  // ring) and on to every item behind it, and a dot where a player uses it from.
  for (const cn::TeamButton& b : r.buttons) {
    const gl::Rgba c = b.shoot ? gl::Rgba{220, 40, 40, 255} : gl::Rgba{40, 90, 220, 255};
    for (const sh::Vec3f& d : b.doors) {
      gl::draw_line(p, v.px(b.at.x), v.py(b.at.y), v.px(d.x), v.py(d.y), c, 2);
      gl::draw_ring(p, v.px(d.x), v.py(d.y), 9, c);
      for (size_t i = 0; i < r.item_at.size(); i++)
        for (const std::string& n : b.opens)
          if (n == r.item_name[i]) {
            const sh::Vec3f a = r.item_at[i];
            const sh::Vec3f e = a - d;
            if (e.x * e.x + e.y * e.y < 400 * 400)
              gl::draw_line(p, v.px(d.x), v.py(d.y), v.px(a.x), v.py(a.y), c, 1);
          }
    }
    gl::fill_rect(p, static_cast<int>(v.px(b.at.x)) - 6, static_cast<int>(v.py(b.at.y)) - 6, 12, 12,
                  c);
    gl::fill_rect(p, static_cast<int>(v.px(b.from.x)) - 2, static_cast<int>(v.py(b.from.y)) - 2, 4,
                  4, c);
  }
  int x = v.margin;
  for (int k = 0; k < 2; k++) {
    const std::string label = o.teams[k].name + " team";
    gl::fill_rect(p, x, 8, 14, 6, kTeam[k]);
    gl::draw_text(p, x + 18, 7, label, kInk);
    x += 18 + gl::text_width(label) + 16;
  }
  gl::fill_rect(p, x, 6, 10, 10, {250, 210, 40, 255});
  gl::draw_text(p, x + 14, 7, "THE KEY AREAS (THE HELD ARMOUR, THE QUAD)", kInk);
  lab.shot({"map", "THE TEAM GAME FROM ABOVE",
            "From above, " + name + " on the baseline mod with teamplay for " + to_fixed(secs, 0) +
                " s: every player's trail in his team's colour, the items as the fighters know "
                "them, the two key areas in yellow.",
            "both teams' trails thick round the held armour and the quad; the rest of the map "
            "crossed on the way to weapons, not ignored",
            in},
           p);

  // Each player's role through the match.
  const int strip_w = 900, row_h = 20, pad = 120;
  gl::Picture m = gl::create_picture(
      strip_w + pad + 16, row_h * static_cast<int>(r.players.size()) + 52, {246, 246, 242, 255});
  for (size_t k = 0; k < r.players.size(); k++) {
    const std::vector<uint8_t>& rl = r.players[k].roles;
    const int y = 32 + static_cast<int>(k) * row_h;
    gl::fill_rect(m, 6, y + 2, 8, 12, kTeam[r.players[k].team]);
    gl::draw_text(m, 18, y + 5, r.players[k].name + " " + sh::aim_skill_name(r.players[k].skill),
                  kInk);
    for (int px = 0; px < strip_w; px++) {
      const size_t i = rl.size() * static_cast<size_t>(px) / static_cast<size_t>(strip_w);
      if (i >= rl.size()) break;
      gl::fill_rect(m, pad + px, y, 1, row_h - 4,
                    kRole[rl[i] == 255 ? 5 : std::min<int>(rl[i], 4)]);
    }
  }
  int lx = pad;
  static const char* kRoleName[6] = {"FREE", "HOLD", "RUSH", "CARRY THE QUAD", "PUSH", "DEAD"};
  for (int i = 0; i < 6; i++) {
    gl::fill_rect(m, lx, 10, 14, 8, kRole[i]);
    gl::draw_text(m, lx + 18, 9, kRoleName[i], kInk);
    lx += 18 + gl::text_width(kRoleName[i]) + 16;
  }
  lab.shot({"roles", "EACH PLAYER'S ROLE THROUGH THE MATCH",
            "A strip a player, left to right over the " + to_fixed(secs, 0) +
                " s: holding the armour's area, rushing the quad as it comes due, carrying it to "
                "the enemy, gathering what he lacks, or dead.",
            "one holder a team at a time; a rush of two before each quad; the carry after a rush "
            "won; no team without a holder for long",
            in},
           m);

  // The teams' frags through the match.
  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {"TEAM FRAGS THROUGH THE MATCH", name + ", seed " + lab.arg("seed"), "t (s)", "frags"};
  for (int k = 1; k >= 0; k--) {  // blue first: the chart's palette opens blue, then red
    gl::Trace tr{o.teams[k].name, {}, {}};
    for (size_t i = 0; i < r.team_frags[k].size(); i++) {
      tr.x.push_back(static_cast<double>(i * static_cast<size_t>(o.path_every)) *
                     shooters_edge::bots::kPilotMsec * 0.001);
      tr.y.push_back(r.team_frags[k][i]);
    }
    ch.traces.push_back(tr);
  }
  lab.shot(
      {"frags", ch.frame.title, "Each team's frags (kills less suicides) against the match's time.",
       "both lines climbing; steps where a quad was carried", in},
      gl::to_picture(gl::line_chart(ch).plot));

  if (!r.mvd.empty()) {
    lab.attach(o.packed ? "match.mvdz" : "match.mvd", r.mvd, gl::Det::deterministic);
    const std::string& out = save;
    if (!out.empty()) {
      std::ofstream f(out, std::ios::binary);
      f.write(reinterpret_cast<const char*>(r.mvd.data()),
              static_cast<std::streamsize>(r.mvd.size()));
      lab.note(f ? "wrote " + out : "could not write " + out);
    }
  }
}

const gl::LabSpec kSpec{
    "se-team",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/bots/team.hpp (the roles, the key areas, the quad's timer) through "
    "net/classic_team.hpp: fighters on clients of our server, teamplay on the baseline mod",
    "bots",
    "Does a team game of bots play like a team game: does each team hold its key area, take the "
    "quad on its timer, leave items to the teammate who needs them and hold its fire for its "
    "own, and does the recording play back?",
    {
        {"minutes", "10", "the match's length"},
        {"seed", "1", "the mod's and the fighters' seed"},
        {"basedir", "", "a folder holding a game folder: play one of its maps"},
        {"game", ge::classicenhance::kProfiles[0].base_dir.data(),
         "the game folder under --basedir"},
        {"map", "", "with --basedir: the map (e1m2 when empty)"},
        {"save", "", "also write the recording to this path (ending in z, .mvdz: packed)"},
        {"locs", "",
         "the map's place names (a .loc file); else the game folder's locs/<map>.loc, else the "
         "items'"},
        {"tempers", "",
         "each player's temperament, red's then blue's, comma separated: aggressive, balanced or "
         "coward (a, b, c); missing: balanced"},
        {"traits", "",
         "each player's traits over his preset, red's then blue's, comma separated, each "
         "name=value joined by / (aim_rl=9/timing=3); empty: the preset"},
        {"features", "",
         "every bot's features over its skill's preset (shooter/features.hpp): +name, -name, all, "
         "none (plain), comma separated (-swimaim,+shortgoals); empty: the preset"},
        {"features-red", "", "red's features over --features (the A/B by team: one side without)"},
        {"features-blue", "", "blue's features over --features"},
        {"trace", "",
         "a TSV of every bot's state each --trace-every commands (the bot plan's scripts)"},
        {"trace-every", "4", "--trace's row every this many commands of a bot (77 a second)"},
        {"sides", "0",
         "1: each side's fight apart (damage, accuracy, sightings, what it did not see), as when "
         "the sides' features differ"},
        {"items", "key",
         "the items noted with the nearest a player came: key (the held armour, the quad) or all"},
        {"client-fed", "0",
         "1: every fighter reads only what its own client was sent (net/classic_mirror.hpp), as "
         "it must on a server it cannot read"},
        {"latency", "0",
         "ms one way between every client and the server, jitter a tenth of it (50: a round trip "
         "of about 100 ms, a server across an ocean); 0: a LAN"},
        {"lead", "1", "0: the fighters do not lead their shots on the round trip (as on a LAN)"},
        {"antilag", "0",
         "1: the server rewinds hitscan to what the shooter saw, as KTX servers do"},
        {"drill", "",
         "a weapon drill: every player holds only this weapon, its ammunition topped up "
         "(scattergun, double, spiker, heavy, grenades, rockets, arc); empty: a normal match"},
    },
    "--minutes=0.5",
    run,
};
GE_LAB(kSpec);

}  // namespace
