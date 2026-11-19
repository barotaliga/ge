// LAB `mod`: does a C++ mod change Shooter's Edge's match the way it says, and only that way, and
// does the match with it replay to the bit? The mod package (the sample `rocket-bounce` the tree
// builds, or `--mod=<package>`) is loaded through engine/mod's loader, its verdict a table; then
// the scripted match (games/shooters_edge/sim/match.hpp) is played twice with the mod and once
// without. Tables: the loader's verdict, the two matches side by side (rockets, their speed,
// grenade bounces, explosions, hurts, the mod's calls and refusals, the final hashes), and the
// mod's log. Pictures: the arena from above and from the side with both matches' missiles, a
// rocket's distance from where it was fired against the clock, and the grenade's height against
// the clock. ON THE SERVER: the same package stacked on a server's rules runner (the bytecode test
// program on its arena), its first rocket's flight and the runner's digests beside the plain
// runner's. ON QUAKE II'S RULES: the sample `vampire` stacked on Quake II's runner in its
// scripted duel, the shooter's health against the clock beside the plain duel's. The lab fails when
// the mod is refused, when the two modded runs part on any frame (or on the server, at the end), or
// when the mod changed nothing. IN THE STORE FORM: where the package lists a wasm32 module
// (cmake/Mods.cmake's WASM), loaded as a store build loads it, the same match is played with it on
// the engine's interpreter; its frames' hashes beside the native build's, both rockets' flights on
// one chart, and what each match cost on this machine (measured: shown, not hashed, so a build
// without the toolchain keeps the same digest). The lab fails when the module's match parts from
// the native one on any frame.
//
// ge:summary Lab: a C++ mod's match beside the plain one: the loader, missiles, bounces, replay
// ge:keywords lab mod mods cpp-mod rocket-bounce loader match compare replay missiles grenade
// ge:keywords bounce digest mod-log rules-runner on-the-server
// ge:skill mod shooter labs
// ge:test tests/shooters_edge_mod_test.cpp tests/mod_loader_test.cpp
// ge:test tests/shooters_edge_rules_mod_test.cpp
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "mod/loader.hpp"
#include "prof/zones.hpp"
#include "shooters_edge/labs/arena_chart.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/q2_mod_duel.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "shooters_edge/sim/match.hpp"
#include "shooters_edge/sim/mod_api.hpp"

namespace {

namespace gl = ge::lab;
namespace gm = ge::mod;
namespace sh = ge::shooter;
namespace se = shooters_edge::sim;
namespace sv = shooters_edge::server;
using ge::core::num::to_fixed;

using shooters_edge::labs::brush_bounds;
using shooters_edge::labs::path;
using shooters_edge::labs::rect;
using shooters_edge::labs::shoot;

using Inputs = std::vector<std::pair<std::string, std::string>>;

std::string hex64(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
  return buf;
}

template <class T>
double num(T v) {
  return static_cast<double>(v);
}

int count(const se::MatchReport& r, se::EventKind k) {
  int n = 0;
  for (const se::GameEvent& e : r.events) n += e.kind == k ? 1 : 0;
  return n;
}

// A rocket's speed over its first two recorded frames, units/s (0: no rocket).
double rocket_speed(const se::MatchReport& r) {
  for (const se::MissileTrack& m : r.missiles)
    if (m.rocket && m.points.size() >= 3)
      return sh::length(m.points[2] - m.points[1]) / (se::kServerFrameMs * 0.001);
  return 0;
}

// The match's longest-lived rocket, or its first grenade.
const se::MissileTrack* track(const se::MatchReport& r, bool rocket) {
  const se::MissileTrack* best = nullptr;
  for (const se::MissileTrack& m : r.missiles)
    if (m.rocket == rocket && !m.points.empty()) {
      if (!rocket) return &m;
      if (!best || m.points.size() > best->points.size()) best = &m;
    }
  return best;
}

// The arena's walls and both matches' missiles in two axes (0 x, 1 y, 2 z).
void draw_arena(gl::Lab& lab, const se::Arena& arena, const se::MatchReport& plain,
                const se::MatchReport& modded, int a, int b, const Inputs& in) {
  const bool above = b == 1;
  gl::Trace walls{"walls", {}, {}};
  for (size_t i = above ? 1 : 0; i < arena.world_brushes.size(); i++) {
    sh::Vec3f lo, hi;
    brush_bounds(arena.world_brushes[i], lo, hi);
    const bool ledge = i == arena.world_brushes.size() - 1;
    if (!above && !ledge && (lo.y > 0 || hi.y < 0)) continue;
    rect(walls, lo, hi, a, b);
  }
  gl::Trace pr{"rockets, plain", {}, {}}, pg{"grenade, plain", {}, {}};
  gl::Trace mr{"rockets, modded", {}, {}}, mg{"grenade, modded", {}, {}};
  for (const se::MissileTrack& m : plain.missiles) path(m.rocket ? pr : pg, m.points, a, b);
  for (const se::MissileTrack& m : modded.missiles) path(m.rocket ? mr : mg, m.points, a, b);
  gl::LineChart c;
  c.spikes = false;
  const char* axis[3] = {"x (units)", "y (units)", "z (units)"};
  c.frame = {above ? "BOTH MATCHES' MISSILES FROM ABOVE" : "BOTH MATCHES' MISSILES FROM THE SIDE",
             std::to_string(plain.missiles.size()) + " missiles plain, " +
                 std::to_string(modded.missiles.size()) + " with the mod",
             axis[a], axis[b]};
  c.traces = {walls, pr, pg, mr, mg};
  shoot(lab, above ? "missiles-top" : "missiles-side", gl::line_chart(c),
        std::string(above ? "From above (x east, y north)" : "From the south (x east, z up)") +
            ": the arena's walls and every missile of the match without the mod and of the same "
            "match with it, a server frame a point. The players' inputs are the same script; "
            "only the mod differs.",
        "the modded lines are drawn over the plain ones, so a plain colour shows only where "
        "the two matches part: the rockets fly the same lines (only faster: see the rocket "
        "chart), and the plain grenade's line goes on past the point where the modded one "
        "exploded at its third bounce",
        in);
}

// The longest-lived rocket's distance from its first point against the clock, both matches.
void draw_rocket(gl::Lab& lab, const se::MatchReport& plain, const se::MatchReport& modded,
                 const Inputs& in) {
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"THE LONGEST ROCKET FLIGHT", "its distance from where it was first seen",
             "time since fired (s)", "distance (units)"};
  const std::pair<const se::MatchReport*, const char*> runs[] = {{&plain, "plain"},
                                                                 {&modded, "modded"}};
  for (const auto& [r, name] : runs) {
    gl::Trace t{name, {}, {}};
    if (const se::MissileTrack* m = track(*r, true))
      for (size_t i = 0; i < m->points.size(); i++) {
        t.x.push_back(static_cast<double>(i) * se::kServerFrameMs * 0.001);
        t.y.push_back(sh::length(m->points[i] - m->points[0]));
      }
    c.traces.push_back(t);
  }
  shoot(lab, "rocket", gl::line_chart(c),
        "How far the match's longest-flying rocket (player 2's, across the east room) has flown, "
        "a server frame at a time, without the mod and with it. The mod scales a new rocket's "
        "velocity by 1.5 in the player's post-think, "
        "the frame it was fired.",
        "two straight lines from 0, the modded one 1.5 times as steep; each ends where its rocket "
        "met a wall, the modded one sooner",
        in);
}

// The first grenade's height against the clock, both matches.
void draw_grenade(gl::Lab& lab, const se::MatchReport& plain, const se::MatchReport& modded,
                  const Inputs& in) {
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"THE GRENADE", "its height against the clock since it was thrown, both matches",
             "time since thrown (s)", "z (units)"};
  const std::pair<const se::MatchReport*, const char*> runs[] = {{&plain, "plain"},
                                                                 {&modded, "modded"}};
  for (const auto& [r, name] : runs) {
    gl::Trace t{name, {}, {}};
    if (const se::MissileTrack* m = track(*r, false))
      for (size_t i = 0; i < m->points.size(); i++) {
        t.x.push_back(static_cast<double>(i) * se::kServerFrameMs * 0.001);
        t.y.push_back(m->points[i].z);
      }
    c.traces.push_back(t);
  }
  shoot(lab, "grenade", gl::line_chart(c),
        "The first grenade's height, a server frame at a time. The mod takes over the grenade's "
        "touch: it counts the bounces off anything but the thrower, runs the game's own touch "
        "after its count, and on the third sets the fuse to now.",
        "the two lines lie on each other up to the third bounce; there the modded one ends (it "
        "exploded), the plain one bounces on until its fuse",
        in);
}

// ON THE SERVER (docs/mods.md § On the server): the same package stacked on a server's rules
// runner, the bytecode test program on its arena, a player holding attack. The first rocket's
// distance against the clock, with and without the package, and the runner's digests.
struct ServerRun {
  std::vector<double> t, d;  // the first rocket's flight
  uint64_t hash = 0;         // rules_hash after the last frame
  std::string error;
  std::vector<std::string> console;  // the server console's lines from the mod
};

ServerRun server_run(const ge_mod_v1* table, int frames) {
  ServerRun run;
  ge::progs::Progs progs;
  if (!sv::build_test_mod(progs, run.error)) return run;
  sv::QcServerOptions o;
  o.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> made = sv::make_qc_server(progs, o, run.error);
  if (!made) return run;
  const sv::RulesRunner r = sv::qc_runner(std::move(made));
  if (table != nullptr) {
    r->mods.tables.push_back(table);
    r->mods.names.push_back(table->name);
  }
  r.console("deathmatch 1");
  sv::MapSpec map;
  map.models = sv::arena_models();
  map.entities = sv::kArenaEntities;
  if (!r.start(map, run.error)) return run;
  r.connect(0, "gunner", false, true);
  int rocket = 0;
  sh::Vec3f from;
  for (int f = 0; f < frames; f++) {
    sh::Packet p;
    p.client = 0;
    p.sequence = static_cast<uint32_t>(f + 1);
    for (sh::MoveCmd& c : p.cmds) {
      c.msec = 13;
      c.buttons = 1;
    }
    r.frame(&p, 1);
    if (rocket == 0)
      for (int e = 1; e < r->sv.num_edicts && rocket == 0; e++) {
        const sh::Edict& ed = r->sv.edicts[static_cast<size_t>(e)];
        if (!ed.free && ed.movetype == sh::kMovetypeFlyMissile) {
          rocket = e;
          from = ed.origin;
        }
      }
    if (rocket > 0) {
      const sh::Edict& ed = r->sv.edicts[static_cast<size_t>(rocket)];
      if (ed.free || ed.movetype != sh::kMovetypeFlyMissile) {
        rocket = -1;  // it met a wall
      } else {
        run.t.push_back(static_cast<double>(run.t.size()) * 0.013);
        run.d.push_back(sh::length(ed.origin - from));
      }
    }
  }
  run.hash = sv::rules_hash(*r);
  for (const std::string& line : r->console)
    if (line.rfind("mod ", 0) == 0) run.console.push_back(line);
  return run;
}

void on_the_server(gl::Lab& lab, const ge_mod_v1* table, const Inputs& in) {
  const int frames = 400;
  const ServerRun plain = server_run(nullptr, frames);
  const ServerRun modded = server_run(table, frames);
  const ServerRun again = server_run(table, frames);
  for (const ServerRun* r : {&plain, &modded})
    if (!r->error.empty()) {
      lab.fail("mod: the server's runner did not start: " + r->error,
               "next: make test T=shooters_edge_rules_mod");
      return;
    }
  auto speed = [](const ServerRun& r) { return r.d.size() >= 3 ? (r.d[2] - r.d[1]) / 0.013 : 0.0; };
  gl::LabTable t{"on the server: the package stacked on a rules runner (the bytecode test program)",
                 {"", "plain", "with the package"},
                 {}};
  t.rows.push_back(
      {"first rocket's speed (u/s)", to_fixed(speed(plain), 1), to_fixed(speed(modded), 1)});
  t.rows.push_back(
      {"frames it flew", std::to_string(plain.d.size()), std::to_string(modded.d.size())});
  t.rows.push_back({"digest after " + std::to_string(frames) + " frames", hex64(plain.hash),
                    hex64(modded.hash)});
  t.rows.push_back({"a second modded run", "", modded.hash == again.hash ? "the same" : "PARTED"});
  t.rows.push_back({"the mod's console lines", std::to_string(plain.console.size()),
                    std::to_string(modded.console.size())});
  lab.table(t);
  lab.metric("server.plain.rocket-speed", speed(plain), "u/s");
  lab.metric("server.modded.rocket-speed", speed(modded), "u/s");
  if (modded.hash != again.hash)
    lab.fail("mod: the modded server did not replay: two runs end on different digests",
             "next: make test T=shooters_edge_rules_mod");
  if (speed(modded) <= speed(plain))
    lab.fail("mod: the package stacked on the server's runner did not speed its rockets",
             "next: make test T=shooters_edge_rules_mod");

  gl::LineChart c;
  c.spikes = false;
  c.frame = {"ON THE SERVER: THE FIRST ROCKET", "its distance from where it was first seen",
             "time since fired (s)", "distance (units)"};
  c.traces.push_back({"plain runner", plain.t, plain.d});
  c.traces.push_back({"runner + package", modded.t, modded.d});
  shoot(lab, "server-rocket", gl::line_chart(c),
        "The same package on the real server's rules runner (the bytecode test program on its "
        "arena, one player holding attack), stacked by the runner's start as a server does from "
        "sv_mod: the first rocket's distance a server frame at a time, without and with it.",
        "two straight lines from 0, the one with the package 1.5 times as steep and ending sooner "
        "at the wall",
        in);
}

// ON QUAKE II'S RULES (q2_mods.hpp): the sample `vampire`, which reads and changes the
// players alone, stacked on Quake II's runner in its scripted duel (q2_mod_duel.hpp), beside
// the duel without it: the shooter's health against the clock, the digests, a second modded run.
void on_the_q2(gl::Lab& lab, const Inputs& in) {
  gm::LoadedMod vampire;
  const gm::LoadResult load =
      gm::load_mod(std::string(GE_MOD_DIR) + "/vampire", {SE_MOD_GAME, SE_MOD_API}, vampire);
  if (!load.ok()) {
    lab.fail("mod: the sample vampire did not load: " +
                 (load.errors.empty() ? std::string("no reason given") : load.errors.front()),
             "next: make build (it builds games/shooters_edge/mods/vampire)");
    return;
  }
  const int frames = 600;
  const sv::Q2Duel plain = sv::run_q2_duel(nullptr, frames);
  const sv::Q2Duel modded = sv::run_q2_duel(vampire.table, frames);
  const sv::Q2Duel again = sv::run_q2_duel(vampire.table, frames);
  for (const sv::Q2Duel* d : {&plain, &modded})
    if (!d->started) {
      lab.fail("mod: Quake II's runner did not start: " + d->error,
               "next: make test T=shooters_edge_rules_mod");
      return;
    }
  const auto most = [](const sv::Q2Duel& d) {
    float m = 0;
    for (float h : d.health) m = std::max(m, h);
    return m;
  };
  gl::LabTable t{"on Quake II's rules: vampire stacked on Quake II's runner (its duel)",
                 {"", "plain", "with vampire"},
                 {}};
  t.rows.push_back(
      {"the shooter's most health", to_fixed(most(plain), 0), to_fixed(most(modded), 0)});
  t.rows.push_back({"deaths", std::to_string(plain.obituaries.size()),
                    std::to_string(modded.obituaries.size())});
  t.rows.push_back({"the mod's hooks run", "0", std::to_string(modded.hook_calls)});
  t.rows.push_back({"the mod's refusals", "0", std::to_string(modded.refusals)});
  t.rows.push_back({"the mod's console lines", std::to_string(plain.mod_lines.size()),
                    std::to_string(modded.mod_lines.size())});
  t.rows.push_back({"digest after " + std::to_string(frames) + " frames",
                    hex64(plain.hashes.back()), hex64(modded.hashes.back())});
  t.rows.push_back(
      {"a second modded run", "", modded.hashes == again.hashes ? "the same" : "PARTED"});
  lab.table(t);
  lab.metric("q2.plain.most-health", most(plain), "health");
  lab.metric("q2.modded.most-health", most(modded), "health");
  if (modded.hashes != again.hashes)
    lab.fail("mod: the modded Quake II duel did not replay: two runs part",
             "next: make test T=shooters_edge_rules_mod");
  if (most(modded) <= most(plain))
    lab.fail("mod: vampire on Quake II's rules did not feed the shooter",
             "next: make test T=shooters_edge_rules_mod");

  gl::LineChart c;
  c.spikes = false;
  c.frame = {"ON QUAKE II'S RULES: THE SHOOTER'S HEALTH", "Ana holds attack at Ben from 1.3 s",
             "time (s)", "health"};
  std::vector<double> ts, hp, hm;
  for (size_t i = 0; i < plain.health.size(); i++) {
    ts.push_back(static_cast<double>(i) * 0.013);
    hp.push_back(plain.health[i]);
    hm.push_back(i < modded.health.size() ? modded.health[i] : 0.0);
  }
  c.traces.push_back({"Quake II rules", ts, hp});
  c.traces.push_back({"Quake II rules + vampire", ts, hm});
  shoot(lab, "q2-health", gl::line_chart(c),
        "The sample vampire on Quake II's rules, which keep their own players: the shooter's "
        "health a host frame at a time in the scripted duel, without and with it. Each of her "
        "blows lands as the rules made it and gives her half its points.",
        "the plain line flat at 100; the vampire's line climbing in steps from 1.3 s as her bolts "
        "land, to at most 200",
        in);
}

// Both builds' rockets on one chart: measured, and shot (its id is in the digest) even where the
// build made no module, so every machine's digest is the same.
void draw_store(gl::Lab& lab, const se::MatchReport& native, const se::MatchReport* store,
                const Inputs& in) {
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"THE ROCKET, NATIVE AND WASM32",
             "the longest rocket flight with each build of the mod", "time since fired (s)",
             "distance (units)"};
  const std::pair<const se::MatchReport*, const char*> runs[] = {{&native, "native library"},
                                                                 {store, "wasm32 module"}};
  for (const auto& [r, name] : runs) {
    if (!r) continue;
    gl::Trace tr{name, {}, {}};
    if (const se::MissileTrack* m = track(*r, true))
      for (size_t i = 0; i < m->points.size(); i++) {
        tr.x.push_back(static_cast<double>(i) * se::kServerFrameMs * 0.001);
        tr.y.push_back(sh::length(m->points[i] - m->points[0]));
      }
    c.traces.push_back(tr);
  }
  const gl::Chart chart = gl::line_chart(c);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach("store-rocket.svg", std::vector<uint8_t>(svg.begin(), svg.end()), gl::Det::measured);
  lab.shot({"store-rocket", chart.title,
            "The same rocket's flight with the mod built as a native library and as a wasm32 "
            "module run on the engine's interpreter, both given the same match.",
            "one line: the wasm32 trace lies exactly on the native one", in},
           gl::to_picture(chart.plot), gl::Det::measured);
}

// The same package's wasm32 module played on the interpreter against the native build's match.
void in_the_store_form(gl::Lab& lab, const std::string& package, se::MatchOptions opt,
                       const se::MatchReport& native, const Inputs& in) {
  // The package's wasm32 module, through the loader as a store build loads it.
  gm::LoadedMod module;
  gm::GameOffer offer{SE_MOD_GAME, SE_MOD_API};
  offer.form = gm::Form::wasm_only;
  (void)gm::load_mod(package, offer, module);
  const std::vector<uint8_t>& bytes = module.wasm;
  gl::LabTable t{"in the store form: the wasm32 module beside the native library",
                 {"measure", "native", "wasm32"},
                 {}};
  if (bytes.empty()) {
    t.rows.push_back(
        {"wasm32 module", "-", "none in the package (built only where clang has wasm32)"});
    lab.table(t, gl::Det::measured);
    draw_store(lab, native, nullptr, in);
    return;
  }
  // Each match timed over a few runs, the fastest kept.
  const auto best_ms = [](const se::MatchOptions& o, se::MatchReport& out) {
    int64_t best = INT64_MAX;
    for (int i = 0; i < 3; i++) {
      const int64_t t0 = ge::prof::now_ns();
      out = se::run_match(o);
      best = std::min(best, ge::prof::now_ns() - t0);
    }
    return static_cast<double>(best) * 1e-6;
  };
  se::MatchReport plain, again, store;
  se::MatchOptions plain_opt = opt;
  plain_opt.mod = nullptr;
  const double plain_ms = best_ms(plain_opt, plain);
  const double native_ms = best_ms(opt, again);
  se::MatchOptions store_opt = opt;
  store_opt.mod = nullptr;
  store_opt.wasm_mod = &bytes;
  const double store_ms = best_ms(store_opt, store);
  for (const std::string& e : store.mod_errors)
    lab.fail("mod: the game refused the wasm32 module: " + e,
             "next: rebuild it with cmake/Mods.cmake's WASM (docs/mods.md § The store form)");
  int parted = -1;
  const size_t n = std::min(native.frames.size(), store.frames.size());
  for (size_t i = 0; i < n && parted < 0; i++)
    if (native.frames[i].hash != store.frames[i].hash) parted = static_cast<int>(i);
  if (parted < 0 && native.frames.size() != store.frames.size()) parted = static_cast<int>(n);
  if (parted >= 0)
    lab.fail("mod: the wasm32 module's match parts from the native one at frame " +
                 std::to_string(parted),
             "next: run tests/shooters_edge_mod_test.cpp; the two builds must lay the state "
             "block out alike (no pointers or longs in it)");
  t.rows.push_back({"module bytes", "-", std::to_string(bytes.size())});
  t.rows.push_back({"frames that agree", std::to_string(native.frames.size()),
                    std::to_string(parted < 0 ? n : static_cast<size_t>(parted))});
  t.rows.push_back({"final hash", hex64(native.final_hash), hex64(store.final_hash)});
  t.rows.push_back({"hooks and functions run", std::to_string(again.mod_run.hook_calls),
                    std::to_string(store.mod_run.hook_calls)});
  t.rows.push_back({"match ms (plain " + to_fixed(plain_ms, 1) + ")", to_fixed(native_ms, 1),
                    to_fixed(store_ms, 1)});
  // The hooks are a small part of a match: a share under 2% is the clock's noise.
  t.rows.push_back({"the mod's share of the match",
                    to_fixed(100.0 * (native_ms - plain_ms) / plain_ms, 1) + "%",
                    to_fixed(100.0 * (store_ms - plain_ms) / plain_ms, 1) + "%"});
  lab.table(t, gl::Det::measured);
  lab.metric("store.parted-at", parted, "frame", gl::Det::measured);
  lab.metric("store.match-ms", store_ms, "ms", gl::Det::measured);
  lab.metric("store.native-match-ms", native_ms, "ms", gl::Det::measured);

  draw_store(lab, native, &store, in);
}

void run(gl::Lab& lab) {
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const int seed = static_cast<int>(lab.arg_num("seed"));
  if (frames < 1 || seed < 0) {
    lab.fail("mod: --frames must be at least 1 and --seed not negative",
             "next: make lab LAB=mod ARGS=--help");
    return;
  }
  std::string package = lab.arg("mod");
  if (package.empty()) package = std::string(GE_MOD_DIR) + "/rocket-bounce";
  const Inputs in = {{"frames", lab.arg("frames")},
                     {"seed", lab.arg("seed")},
                     {"mod", lab.arg("mod").empty() ? "rocket-bounce (the sample)" : package}};

  gm::LoadedMod mod;
  const gm::LoadResult load = gm::load_mod(package, {SE_MOD_GAME, SE_MOD_API}, mod);
  gl::LabTable verdict{"the loader's verdict", {"check", "value"}, {}};
  verdict.rows.push_back({"package", lab.arg("mod").empty() ? "rocket-bounce" : package});
  verdict.rows.push_back({"accepted", load.ok() ? "yes" : "no"});
  for (const std::string& e : load.errors) verdict.rows.push_back({"error", e});
  for (const std::string& w : load.warnings) verdict.rows.push_back({"warning", w});
  if (load.ok()) {
    verdict.rows.push_back({"name", mod.manifest.name});
    verdict.rows.push_back({"version", mod.manifest.version});
    verdict.rows.push_back({"game", mod.manifest.game});
    verdict.rows.push_back({"platform", mod.platform});
    verdict.rows.push_back({"state bytes", std::to_string(mod.fields.state_bytes)});
  }
  // Measured: the platform's name and a package's path differ machine to machine.
  lab.table(verdict, gl::Det::measured);
  lab.metric("loader.accepted", load.ok() ? 1 : 0, "count");
  if (!load.ok()) {
    lab.fail("mod: the loader refused " + package + ": " + load.errors.front(),
             "next: ge mod check " + package);
    return;
  }

  se::MatchOptions opt;
  opt.frames = frames;
  const se::MatchReport plain = se::run_match(opt);
  opt.mod = mod.table;
  opt.mod_seed = static_cast<uint32_t>(seed);
  const se::MatchReport modded = se::run_match(opt);
  const se::MatchReport again = se::run_match(opt);
  for (const std::string& e : modded.mod_errors)
    lab.fail("mod: the game refused the mod's table: " + e,
             "next: build the mod against games/shooters_edge/sim/mod_api.hpp");

  // The replay: the second modded run against the first, frame by frame.
  int parted = -1;
  const size_t n = std::min(modded.frames.size(), again.frames.size());
  for (size_t i = 0; i < n && parted < 0; i++)
    if (modded.frames[i].hash != again.frames[i].hash) parted = static_cast<int>(i);
  if (parted < 0 && modded.frames.size() != again.frames.size()) parted = static_cast<int>(n);
  if (parted >= 0)
    lab.fail("mod: the modded match did not replay: the two runs part at frame " +
                 std::to_string(parted),
             "next: run tests/shooters_edge_mod_test.cpp; a mod keeps all its state in its block "
             "and draws only from host->random (docs/mods.md § Determinism)");

  struct Row {
    const char* name;
    const char* key;
    double plain, modded;
    int digits;
  };
  const Row rows[] = {
      {"rockets fired", "rockets", 1.0 * count(plain, se::EventKind::rocket_fired),
       1.0 * count(modded, se::EventKind::rocket_fired), 0},
      {"first rocket speed u/s", "rocket-speed", rocket_speed(plain), rocket_speed(modded), 1},
      {"grenades thrown", "grenades", 1.0 * count(plain, se::EventKind::grenade_thrown),
       1.0 * count(modded, se::EventKind::grenade_thrown), 0},
      {"grenade bounces", "bounces", 1.0 * count(plain, se::EventKind::grenade_bounce),
       1.0 * count(modded, se::EventKind::grenade_bounce), 0},
      {"explosions", "explosions", 1.0 * count(plain, se::EventKind::explosion),
       1.0 * count(modded, se::EventKind::explosion), 0},
      {"hurts", "hurts", 1.0 * count(plain, se::EventKind::hurt),
       1.0 * count(modded, se::EventKind::hurt), 0},
      {"missiles tracked", "missiles", num(plain.missiles.size()), num(modded.missiles.size()), 0},
      {"mod hooks and functions run", "hook-calls", 0, num(modded.mod_run.hook_calls), 0},
      {"service calls refused", "refusals", 0, num(modded.mod_run.refusals), 0},
      {"mod log lines", "log-lines", 0, num(modded.mod_log.size()), 0},
  };
  gl::LabTable compare{
      "the match without the mod and with it", {"measure", "plain", "with the mod", "change"}, {}};
  for (const Row& r : rows) {
    compare.rows.push_back({r.name, to_fixed(r.plain, r.digits), to_fixed(r.modded, r.digits),
                            to_fixed(r.modded - r.plain, r.digits)});
    const char* unit = r.digits > 0 ? "u/s" : "count";
    lab.metric(std::string("plain.") + r.key, r.plain, unit);
    lab.metric(std::string("modded.") + r.key, r.modded, unit);
  }
  compare.rows.push_back({"final hash", hex64(plain.final_hash), hex64(modded.final_hash),
                          plain.final_hash == modded.final_hash ? "same" : "moved"});
  compare.rows.push_back({"replayed to the bit", "-", parted < 0 ? "yes" : "no", "-"});
  lab.table(compare);
  lab.metric("modded.replay-parted-at", parted, "frame");

  gl::LabTable log{"the mod's log, in order", {"#", "line"}, {}};
  for (size_t i = 0; i < modded.mod_log.size(); i++)
    log.rows.push_back({std::to_string(i + 1), modded.mod_log[i]});
  if (modded.mod_log.empty()) log.rows.push_back({"-", "(the mod logged nothing)"});
  lab.table(log);

  if (plain.final_hash == modded.final_hash)
    lab.fail("mod: the match with the mod ended on the plain match's hash",
             "next: read the mod's log in the table above; a hook that does not fire is a table "
             "whose `size` or `api` the game refused");

  se::Arena arena;
  se::build_arena(arena);
  draw_arena(lab, arena, plain, modded, 0, 1, in);
  draw_arena(lab, arena, plain, modded, 0, 2, in);
  draw_rocket(lab, plain, modded, in);
  draw_grenade(lab, plain, modded, in);
  on_the_server(lab, mod.table, in);
  on_the_q2(lab, in);
  in_the_store_form(lab, package, opt, modded, in);
}

const gl::LabSpec kSpec{
    "mod",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "engine/mod's loader and games/shooters_edge/sim/mod_host.hpp's seam, with the sample mod "
    "games/shooters_edge/mods/rocket_bounce on the scripted match and stacked on the server's "
    "rules runner (rules_runner.hpp), and the sample vampire on Quake II's rules "
    "(q2_mods.hpp)",
    "mod",
    "Does a C++ mod load, change Shooter's Edge's match the way it says and only that way, and "
    "does the match with it replay to the bit?",
    {
        {"frames", "1100", "server frames of 13 ms to run each match for"},
        {"mod", "", "a mod package directory (default: the sample rocket-bounce this tree builds)"},
        {"seed", "1", "the seed of the mod's random stream"},
    },
    "--frames=700",
    run,
};
GE_LAB(kSpec);

}  // namespace
