// LAB `se-baseline`: does the baseline mod (games/shooters_edge/qc/, compiled from its sources by
// the QC compiler) play a deathmatch on the test arena the way its sources say? The mod is built
// from `<root>/games/shooters_edge/qc/arena/progs.src` and played by bots on the arena with its
// door and plat (games/shooters_edge/server/baseline_mod.hpp) until its time limit changes the
// level. Tables: the build (functions, statements, globals, crc), the frags and deaths, the
// obituaries by how each death came, the pickups, the missiles by kind, the builtins called.
// Pictures: the arena from above with the items, the door and the plat, every bot's path and every
// missile; the frags against the clock; the door's and the plat's heights against the clock. The
// lab fails when the mod does not compile, faults, spawns an entity it cannot place, or nobody is
// fragged.
//
// ge:summary Lab: the baseline QC mod compiled from source and played by bots: frags, items, movers
// ge:keywords lab se-baseline baseline-mod qc-compiler deathmatch frags obituaries pickups door
// ge:keywords plat missiles paths digest
// ge:skill progs shooter labs
// ge:test tests/shooters_edge_baseline_mod_test.cpp
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "arena_chart.hpp"
#include "core/num_text.hpp"
#include "progs/entity_text.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/sim/arena.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;
using Inputs = std::vector<std::pair<std::string, std::string>>;

constexpr double kFrame = 0.013;  // a server frame, s

// "bot2 was blown apart by bot1\n" -> "blown apart"; "bot1 fragged themselves\n" -> the rest.
std::string obituary_kind(const std::string& line) {
  const size_t was = line.find(" was ");
  const size_t by = line.rfind(" by ");
  if (was != std::string::npos && by != std::string::npos && by > was)
    return line.substr(was + 5, by - was - 5);
  if (line.find(" fragged themselves") != std::string::npos) return "fragged themselves";
  return "";
}

double time_of(const sv::ModMatchReport& r, size_t frame) {
  return r.end_time - kFrame * static_cast<double>(r.frames) +
         kFrame * static_cast<double>(frame + 1);
}

void draw_arena(gl::Lab& lab, const sv::ModMatchReport& r, const Inputs& in) {
  shooters_edge::sim::Arena arena;
  shooters_edge::sim::build_arena(arena);
  gl::Trace walls{"walls and ledge", {}, {}};
  for (size_t i = 1; i < arena.world_brushes.size(); i++) {
    sh::Vec3f lo, hi;
    la::brush_bounds(arena.world_brushes[i], lo, hi);
    la::rect(walls, lo, hi, 0, 1);
  }
  gl::Trace movers{"door and plat", {}, {}};
  for (const sh::Brush* b : {&arena.door_brush, &arena.plat_brush}) {
    sh::Vec3f lo, hi;
    la::brush_bounds(*b, lo, hi);
    la::rect(movers, lo, hi, 0, 1);
  }
  gl::Trace items{"items and weapons", {}, {}};
  std::vector<ge::progs::EntityBlock> blocks;
  std::string error;
  ge::progs::parse_entity_text(sv::kBaselineArenaEntities, blocks, error);
  for (const ge::progs::EntityBlock& b : blocks) {
    const char* cls = ge::progs::block_value(b, "classname");
    const char* org = ge::progs::block_value(b, "origin");
    if (cls == nullptr || org == nullptr) continue;
    const std::string c = cls;
    if (c.rfind("item_", 0) != 0 && c.rfind("weapon_", 0) != 0) continue;
    float x = 0, y = 0, z = 0;
    if (std::sscanf(org, "%f %f %f", &x, &y, &z) != 3) continue;
    la::rect(items, {x - 12, y - 12, 0}, {x + 12, y + 12, 0}, 0, 1);
  }
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"THE BASELINE MATCH FROM ABOVE",
             std::to_string(r.players.size()) + " bots, " + to_fixed(r.end_time, 1) + " s, " +
                 std::to_string(r.missiles.size()) + " missiles",
             "x (units)", "y (units)"};
  c.traces.push_back(walls);
  c.traces.push_back(movers);
  c.traces.push_back(items);
  for (const sv::PlayerPath& p : r.players) {
    gl::Trace t{p.name, {}, {}};
    std::vector<sh::Vec3f> thin;
    for (size_t i = 0; i < p.points.size(); i += 4) thin.push_back(p.points[i]);
    la::path(t, thin, 0, 1);
    c.traces.push_back(t);
  }
  gl::Trace missiles{"missiles", {}, {}};
  for (const sv::MissilePath& m : r.missiles) la::path(missiles, m.points, 0, 1);
  c.traces.push_back(missiles);
  la::shoot(lab, "arena", gl::line_chart(c),
            "From above (x east, y north): the arena's walls and the ledge in the south-east, the "
            "door in the doorway at x = 128..144 and the plat beside the ledge, every item and "
            "weapon of the baseline entity text as a small square; each bot's path, a point every "
            "4 frames, jumping back to a spawn point at each respawn; each missile (rockets, "
            "grenades, spikes) from the muzzle to where it ended.",
            "paths crossing between the rooms only through the doorway; paths that visit the "
            "item squares (pickups); a path that starts on the plat; missiles ending on a wall or "
            "near a bot, none passing through a wall",
            in);
}

void draw_frags(gl::Lab& lab, const sv::ModMatchReport& r, const Inputs& in) {
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"FRAGS AGAINST THE CLOCK", "each bot's frags field after every server frame",
             "server time (s)", "frags"};
  for (const sv::PlayerPath& p : r.players) {
    gl::Trace t{p.name, {}, {}};
    for (size_t i = 0; i < p.frags_at.size(); i++) {
      t.x.push_back(time_of(r, i));
      t.y.push_back(p.frags_at[i]);
    }
    c.traces.push_back(t);
  }
  if (r.ended) c.marks.push_back({r.end_time, "the level changes"});
  la::shoot(lab, "frags", gl::line_chart(c),
            "Each bot's frags as the baseline mod keeps them (combat.qc's score_death): up one for "
            "a kill, down one for a death by his own hand or the world's; flat for the five "
            "seconds the scores stand after the time limit, then the level changes.",
            "steps of exactly one; down-steps only where a bot killed himself; the lines flat "
            "for the last five seconds before the mark",
            in);
}

void draw_movers(gl::Lab& lab, const sv::ModMatchReport& r, const Inputs& in) {
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"THE DOOR AND THE PLAT", "each brush mover's height after every server frame",
             "server time (s)", "origin z (units)"};
  for (const sv::MoverPath& m : r.movers) {
    gl::Trace t{m.classname + " (" + m.model + ")", {}, {}};
    for (size_t i = 0; i < m.z.size(); i++) {
      t.x.push_back(time_of(r, i));
      t.y.push_back(m.z[i]);
    }
    c.traces.push_back(t);
  }
  c.rules = {96, 120};
  la::shoot(lab, "movers", gl::line_chart(c),
            "The door (world.qc's func_door) rises 120 units at 100 a second when a player comes "
            "near, waits 3 s after the last touch and comes down, going back up when it meets a "
            "player; the plat (func_plat) waits at the bottom until a player stands on it, rides "
            "96 units up at 64 a second, waits 3 s and comes down.",
            "ramps of a constant slope between 0 and the dashed tops; the door holding at 120 "
            "while bots pass and turning back up part-way when blocked; the plat up once for the "
            "player who spawned on it, and again whenever a bot steps on",
            in);
}

void run(gl::Lab& lab) {
  ge::progs::Progs progs;
  std::string error;
  if (!sv::compile_baseline_mod(lab.arg("root"), progs, error)) {
    lab.fail("se-baseline: " + error,
             "next: run the lab from the repository's root, or pass --root");
    return;
  }
  int builtins = 0;
  for (const ge::progs::Function& f : progs.functions) builtins += f.first_statement < 0 ? 1 : 0;
  gl::LabTable build{"the build", {"what", "value"}, {}};
  build.rows = {{"source", sv::kBaselineSource},
                {"functions", std::to_string(progs.functions.size() - 1 - builtins)},
                {"builtins declared", std::to_string(builtins)},
                {"statements", std::to_string(progs.statements.size())},
                {"global words", std::to_string(progs.globals.size())},
                {"crc", std::to_string(progs.crc)}};
  lab.table(build);
  lab.metric("build.statements", static_cast<double>(progs.statements.size()), "count");
  lab.metric("build.globals", static_cast<double>(progs.globals.size()), "count");

  sv::ModMatchOptions opt;
  opt.compiled = &progs;
  opt.compiled_name = "the baseline mod";
  opt.entities = sv::kBaselineArenaEntities;
  opt.bots = static_cast<int>(lab.arg_num("bots"));
  opt.max_frames = static_cast<int>(lab.arg_num("frames"));
  opt.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  opt.console = {"deathmatch 1", "timelimit " + lab.arg("timelimit")};
  const Inputs in = {{"bots", lab.arg("bots")},
                     {"timelimit min", lab.arg("timelimit")},
                     {"seed", lab.arg("seed")}};
  const sv::ModMatchReport r = sv::run_mod_match(opt);
  if (!r.ok) {
    lab.fail("se-baseline: " + r.error, "next: run tests/shooters_edge_baseline_mod_test.cpp");
    if (r.players.empty()) return;
  }

  gl::LabTable frags{"the frags", {"player", "frags", "deaths"}, {}};
  for (const sv::PlayerPath& p : r.players) {
    frags.rows.push_back({p.name, to_fixed(p.frags, 0), std::to_string(p.deaths)});
    lab.metric("frags." + p.name, p.frags, "count");
  }
  lab.table(frags);

  std::map<std::string, int> kinds, pickups;
  for (const std::string& line : r.prints) {
    const std::string k = obituary_kind(line);
    if (!k.empty()) kinds[k]++;
    if (line.rfind("You ", 0) == 0) pickups[line.substr(0, line.size() - 1)]++;
  }
  gl::LabTable deaths{"the obituaries, by how each death came", {"how", "deaths"}, {}};
  for (const auto& [k, n] : kinds) deaths.rows.push_back({k, std::to_string(n)});
  lab.table(deaths);
  gl::LabTable picks{"the pickups, by what each player was told", {"message", "times"}, {}};
  int picked = 0;
  for (const auto& [k, n] : pickups) {
    picks.rows.push_back({k, std::to_string(n)});
    picked += n;
  }
  lab.table(picks);

  gl::LabTable builtin_table{"every builtin the mod called", {"#", "builtin", "calls"}, {}};
  for (const sv::BuiltinCount& b : r.builtins)
    builtin_table.rows.push_back({std::to_string(b.number), b.name, std::to_string(b.calls)});
  lab.table(builtin_table);

  gl::LabTable match{"the match", {"what", "value"}, {}};
  match.rows = {{"frames", std::to_string(r.frames)},
                {"server time at the end s", to_fixed(r.end_time, 3)},
                {"the level changed (time limit)", r.ended ? "yes" : "no"},
                {"missiles", std::to_string(r.missiles.size())},
                {"pickups", std::to_string(picked)},
                {"frag log lines", std::to_string(r.frag_log.size())},
                {"entities inhibited", std::to_string(r.inhibited)},
                {"map messages", std::to_string(r.map_messages.size())},
                {"QC statements", std::to_string(r.statements)},
                {"faults", std::to_string(r.faults)}};
  lab.table(match);
  lab.metric("match.frames", r.frames, "count");
  lab.metric("match.missiles", static_cast<double>(r.missiles.size()), "count");
  lab.metric("match.pickups", picked, "count");
  lab.metric("match.frag-log", static_cast<double>(r.frag_log.size()), "count");
  lab.metric("match.statements", static_cast<double>(r.statements), "count");
  lab.metric("match.faults", static_cast<double>(r.faults), "count");

  if (r.faults > 0)
    lab.fail("se-baseline: the mod faulted " + std::to_string(r.faults) +
                 " times; the last: " + r.last_fault,
             "next: read the QC stack in the fault; the line table names the .qc line");
  for (const std::string& m : r.map_messages)
    lab.fail(
        "se-baseline: the map said: " + m,
        "next: declare the field in games/shooters_edge/qc/common/defs.qc, or fix the entity text "
        "in games/shooters_edge/server/baseline_mod.cpp");
  if (r.frag_log.empty())
    lab.fail("se-baseline: nobody was fragged",
             "next: run tests/shooters_edge_baseline_mod_test.cpp");

  draw_arena(lab, r, in);
  draw_frags(lab, r, in);
  draw_movers(lab, r, in);
}

const gl::LabSpec kSpec{
    "se-baseline",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/qc (the baseline deathmatch mod, compiled by plugins/progs' QC compiler) "
    "on games/shooters_edge/server's mod server, played by bots on the arena with its door and "
    "plat",
    "progs",
    "Does the baseline mod, compiled from its sources, play a deathmatch as they say: pickups, "
    "weapons, frags and obituaries, respawns, the door and the plat, the time limit?",
    {
        {"root", ".", "the repository's root (the mod's sources are under it)"},
        {"bots", "2", "bots in the match, 1..32"},
        {"timelimit", "1", "the match's time limit, minutes (the mod reads the cvar)"},
        {"frames", "5100", "server frames of 13 ms at most"},
        {"seed", "1", "the server's generator; the bots' is seed + 1"},
    },
    "--timelimit=0.25 --frames=1600",
    run,
};
GE_LAB(kSpec);

}  // namespace
