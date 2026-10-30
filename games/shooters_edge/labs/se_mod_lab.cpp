// LAB `se-mod`: does a multiplayer mod run under the mod server as its hooks and builtins say?
// A mod (the assembled test mod, or a progs from `--progs`) plays a match on the test arena with
// bots until its time limit changes the level (games/shooters_edge/server/mod_match.hpp): tables
// of the hooks called and how often, every builtin called by number and how often, and the frags
// and deaths; the arena from above with every player's path and every missile; the frags against
// the clock; a timeline of the hooks; the match's digest, which `make labs-smoke` holds.
//
// ge:summary Lab: a multiplayer mod's match with bots: hooks, builtin counts, frags, paths, digest
// ge:keywords lab se-mod mod-match hooks builtins frags missiles paths timeline progs-path
// ge:skill progs shooter labs
// ge:test tests/shooters_edge_mod_match_test.cpp
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/sim/arena.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
using ge::core::num::to_fixed;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
using Inputs = std::vector<std::pair<std::string, std::string>>;

std::string hex64(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
  return buf;
}

std::string key_of(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    if (c == ' ' || c == '_') c = '-';
  }
  return s;
}

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for, const Inputs& in) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, in}, gl::to_picture(chart.plot));
}

void path(gl::Trace& t, const std::vector<sh::Vec3f>& pts, size_t step = 1) {
  for (size_t i = 0; i < pts.size(); i += step) {
    t.x.push_back(pts[i].x);
    t.y.push_back(pts[i].y);
  }
  if (!pts.empty()) {
    t.x.push_back(pts.back().x);
    t.y.push_back(kNan);
  }
}

void rect(gl::Trace& t, sh::Vec3f lo, sh::Vec3f hi) {
  const double xs[5] = {lo.x, hi.x, hi.x, lo.x, lo.x};
  const double ys[5] = {lo.y, lo.y, hi.y, hi.y, lo.y};
  for (int i = 0; i < 5; i++) {
    t.x.push_back(xs[i]);
    t.y.push_back(ys[i]);
  }
  t.x.push_back(lo.x);
  t.y.push_back(kNan);
}

void draw_paths(gl::Lab& lab, const sv::ModMatchReport& r, const Inputs& in) {
  shooters_edge::sim::Arena arena;
  shooters_edge::sim::build_arena(arena);
  gl::Trace walls{"walls and ledge", {}, {}};
  for (size_t i = 1; i < arena.world_brushes.size(); i++) {
    sh::Vec3f lo, hi;
    for (const sh::Plane& p : arena.world_brushes[i].planes)
      for (int k = 0; k < 3; k++) {
        if (p.normal[k] == 1) hi[k] = p.dist;
        if (p.normal[k] == -1) lo[k] = -p.dist;
      }
    rect(walls, lo, hi);
  }
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"THE MATCH FROM ABOVE",
             std::to_string(r.players.size()) + " bots, " + std::to_string(r.frames) + " frames, " +
                 std::to_string(r.missiles.size()) + " rockets",
             "x (units)", "y (units)"};
  c.traces.push_back(walls);
  for (const sv::PlayerPath& p : r.players) {
    gl::Trace t{p.name, {}, {}};
    path(t, p.points, 4);
    c.traces.push_back(t);
  }
  gl::Trace rockets{"rockets", {}, {}};
  for (const sv::MissilePath& m : r.missiles) path(rockets, m.points);
  c.traces.push_back(rockets);
  shoot(lab, "paths", gl::line_chart(c),
        "From above (x east, y north): the arena's walls and the ledge in the south-east, the "
        "doorway open in the dividing wall at x = 128..144; each bot's path from its spawn point "
        "(the map's four info_player_deathmatch), a point every 4 frames; each rocket from the "
        "muzzle to where it burst.",
        "paths that start on the four spots and jump back to one at each respawn; bots crossing "
        "between the rooms only through the doorway at y = -64..64; rockets as straight dashes "
        "ending on a wall or near a bot, none passing through a wall",
        in);
}

void draw_frags(gl::Lab& lab, const sv::ModMatchReport& r, const Inputs& in) {
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"FRAGS AGAINST THE CLOCK", "each bot's frags field after every server frame",
             "server time (s)", "frags"};
  const double t0 = r.end_time - 0.013 * static_cast<double>(r.frames);
  for (const sv::PlayerPath& p : r.players) {
    gl::Trace t{p.name, {}, {}};
    for (size_t i = 0; i < p.frags_at.size(); i++) {
      t.x.push_back(t0 + 0.013 * static_cast<double>(i + 1));
      t.y.push_back(p.frags_at[i]);
    }
    c.traces.push_back(t);
  }
  if (r.ended) c.marks.push_back({r.end_time, "time limit: changelevel"});
  shoot(lab, "frags", gl::line_chart(c),
        "Each bot's frags as the mod keeps them: up one for a kill with a rocket, down one for "
        "his own (T_Damage in the test mod), until the time limit's changelevel.",
        "steps of exactly one; a down-step only where a bot's own blast killed him; the lines end "
        "at the time-limit mark",
        in);
}

void draw_timeline(gl::Lab& lab, const sv::ModMatchReport& r, const Inputs& in) {
  gl::LineChart c;
  c.points = true;
  c.spikes = false;
  c.frame = {"THE HOOKS AGAINST THE CLOCK",
             "every hook call the server logged, one row a hook (" +
                 std::to_string(r.hook_log.size()) + " calls)",
             "server time (s)", "hook (row)"};
  for (int h = 0; h < static_cast<int>(sv::HookId::count); h++) {
    if (r.hook_counts[h] == 0) continue;
    gl::Trace t{sv::hook_name(static_cast<sv::HookId>(h)), {}, {}};
    int64_t seen = 0;
    for (const sv::HookCall& call : r.hook_log) {
      if (static_cast<int>(call.hook) != h) continue;
      // The per-frame hooks are thinned to one dot in ten.
      if (r.hook_counts[h] > 200 && seen++ % 10 != 0) continue;
      t.x.push_back(call.time);
      t.y.push_back(h);
    }
    c.traces.push_back(t);
  }
  shoot(lab, "timeline", gl::line_chart(c),
        "Each hook the server called, at the server time it was called (StartFrame, the "
        "thinks and the frames' other hooks thinned to one dot in ten): the spawn functions and "
        "timeofday at load, each bot's SetNewParms, ClientConnect and PutClientInServer as it "
        "joins, then the frames.",
        "the spawn functions and timeofday only at the start; four SetNewParms, ClientConnect and "
        "PutClientInServer each, right after the load; StartFrame and the two thinks as unbroken "
        "rows to the end; respawns call PutClientInServer from QC, so no new dots appear there",
        in);
}

void run(gl::Lab& lab) {
  sv::ModMatchOptions opt;
  opt.progs = lab.arg("progs");
  opt.bots = static_cast<int>(lab.arg_num("bots"));
  opt.max_frames = static_cast<int>(lab.arg_num("frames"));
  opt.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  opt.console = {"timelimit " + lab.arg("timelimit")};
  const Inputs in = {{"progs", opt.progs.empty() ? "(the test mod)" : opt.progs},
                     {"bots", lab.arg("bots")},
                     {"timelimit min", lab.arg("timelimit")},
                     {"seed", lab.arg("seed")}};
  const sv::ModMatchReport r = sv::run_mod_match(opt);
  if (!r.ok) {
    lab.fail("se-mod: " + r.error, "next: run tests/shooters_edge_mod_match_test.cpp");
    if (r.players.empty()) return;
  }

  gl::LabTable hooks{"the hooks the server called, and how often", {"hook", "calls"}, {}};
  for (int h = 0; h < static_cast<int>(sv::HookId::count); h++) {
    const char* name = sv::hook_name(static_cast<sv::HookId>(h));
    hooks.rows.push_back({name, std::to_string(r.hook_counts[h])});
    lab.metric("hooks." + key_of(name), static_cast<double>(r.hook_counts[h]), "count");
  }
  lab.table(hooks);

  gl::LabTable builtins{"every builtin the mod called, by number", {"#", "builtin", "calls"}, {}};
  for (const sv::BuiltinCount& b : r.builtins) {
    builtins.rows.push_back({std::to_string(b.number), b.name, std::to_string(b.calls)});
    lab.metric("builtins." + key_of(b.name), static_cast<double>(b.calls), "count");
  }
  lab.table(builtins);

  gl::LabTable frags{"the frags", {"player", "frags", "deaths"}, {}};
  for (const sv::PlayerPath& p : r.players) {
    frags.rows.push_back({p.name, to_fixed(p.frags, 0), std::to_string(p.deaths)});
    lab.metric("frags." + p.name, p.frags, "count");
  }
  lab.table(frags);

  gl::LabTable match{"the match", {"what", "value"}, {}};
  const std::pair<std::string, std::string> rows[] = {
      {"progs", r.source},
      {"frames", std::to_string(r.frames)},
      {"server time at the end s", to_fixed(r.end_time, 3)},
      {"ended by the mod's time limit", r.ended ? "yes" : "no"},
      {"rockets", std::to_string(r.missiles.size())},
      {"frag log lines", std::to_string(r.frag_log.size())},
      {"prints", std::to_string(r.prints.size())},
      {"network events", std::to_string(r.events_total)},
      {"multicast bytes", std::to_string(r.multicast_bytes)},
      {"entities inhibited", std::to_string(r.inhibited)},
      {"map messages", std::to_string(r.map_messages.size())},
      {"QC statements", std::to_string(r.statements)},
      {"faults", std::to_string(r.faults)},
      {"digest at the end", hex64(r.final_digest)}};
  for (const auto& [k, v] : rows) match.rows.push_back({k, v});
  lab.table(match);
  lab.metric("match.frames", r.frames, "count");
  lab.metric("match.rockets", static_cast<double>(r.missiles.size()), "count");
  lab.metric("match.frag-log", static_cast<double>(r.frag_log.size()), "count");
  lab.metric("match.statements", static_cast<double>(r.statements), "count");
  lab.metric("match.faults", static_cast<double>(r.faults), "count");

  if (r.faults > 0)
    lab.fail(
        "se-mod: the mod faulted " + std::to_string(r.faults) + " times; the last: " + r.last_fault,
        "next: read the fault's QC stack and the builtin it names in "
        "games/shooters_edge/server/catalogue.hpp");
  if (opt.progs.empty() && !r.ended)
    lab.fail("se-mod: the test mod's time limit never ended the match",
             "next: raise --frames or read StartFrame in games/shooters_edge/server/test_mod.cpp");
  if (opt.progs.empty() && r.frag_log.empty())
    lab.fail("se-mod: nobody was fragged",
             "next: run tests/shooters_edge_mod_match_test.cpp (rockets_frag_and_the_logs_follow)");

  draw_paths(lab, r, in);
  draw_frags(lab, r, in);
  draw_timeline(lab, r, in);
}

const gl::LabSpec kSpec{
    "se-mod",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/server (the mod server: catalogue, builtins, hooks, entity text) over "
    "plugins/progs and plugins/shooter, driving the assembled test mod or a progs from --progs",
    "progs",
    "Does a multiplayer mod run under the mod server as specified: its entities spawned from the "
    "map's text, its hooks called in order, its builtins doing their work, bots fragging until "
    "its time limit?",
    {
        {"progs", "", "a progs file to run (empty: the built-in test mod)"},
        {"bots", "4", "bots in the match, 1..32"},
        {"timelimit", "0.25", "the match's time limit, minutes (the mod reads the cvar)"},
        {"frames", "4000", "server frames of 13 ms at most"},
        {"seed", "1", "the server's generator; the bots' is seed + 1"},
    },
    "--timelimit=0.1",
    run,
};
GE_LAB(kSpec);

}  // namespace
