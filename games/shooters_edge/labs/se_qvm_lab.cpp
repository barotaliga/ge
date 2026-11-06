// LAB `se-qvm`: does a mod shipped as a compiled module run a match on the module server? The
// module (the hand-assembled test mod on the test arena, or a mod folder's `qwprogs.qvm` from
// `--root` and `--mod` on one of its maps) starts the map, takes `--players` clients and runs
// `--seconds` of frames: tables of the exports called and how often, every system call by name
// and how often, the entities by class, the console's lines and the faults; the entities from
// above at the end; the live entities and the system calls a frame against the clock; the run's
// digest, which `make labs-smoke` holds for the test mod.
//
// ge:summary Lab: a compiled-module mod's match: exports, system calls, entities, console, faults
// ge:keywords lab se-qvm qvm compiled-module exports system-calls entities-by-class faults
// ge:keywords mod-folder qwprogs-qvm module-server
// ge:skill progs shooter labs
// ge:test tests/shooters_edge_qvm_server_test.cpp
#include <algorithm>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "progs/layout.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qvm_server.hpp"
#include "shooters_edge/server/qvm_test_mod.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace pg = ge::progs;
namespace sv = shooters_edge::server;
namespace sim = shooters_edge::sim;
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
using Inputs = std::vector<std::pair<std::string, std::string>>;

const char* const kExportNames[sim::kQvmExports] = {
    "init",         "load entities",  "shutdown",   "connect", "put in server", "userinfo",
    "disconnect",   "client command", "prethink",   "(9)",     "postthink",     "start frame",
    "change parms", "new parms",      "console",    "touch",   "think",         "blocked",
    "say",          "paused tic",     "clear edict"};

std::string hex64(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
  return buf;
}

std::string key_of(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    if (c == ' ' || c == '_' || c == '(' || c == ')') c = '-';
  }
  return s;
}

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for, const Inputs& in) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, in}, gl::to_picture(chart.plot));
}

bool read_game_file(void* ctx, const std::string& name, std::vector<uint8_t>& bytes) {
  const auto& files = *static_cast<const cl::SearchPath*>(ctx);
  std::vector<uint8_t> store;
  const std::span<const uint8_t> found = cl::find_file(files, name, store);
  if (found.empty()) return false;
  bytes.assign(found.begin(), found.end());
  return true;
}

// The class of a live entity: its classname, "player" for a client slot, "(none)" when unset.
std::string class_of(const sv::QvmServer& s, int e) {
  std::string c(sim::entity_text(s.q, e, pg::kFClassname));
  if (c.empty()) c = e >= 1 && e <= 32 ? "player" : "(none)";
  return c;
}

struct Run {
  std::unique_ptr<sv::QvmServer> s;
  cl::SearchPath files;
  std::unique_ptr<cl::LoadedMap> map;
  std::string source;
};

bool start(gl::Lab& lab, Run& r, std::string& error) {
  const std::string root = lab.arg("root"), mod = lab.arg("mod"), map = lab.arg("map");
  sv::QvmServerOptions so;
  so.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  so.loop.max_edicts = 1024;
  if (root.empty()) {
    r.source = "the test mod";
    r.s = sv::make_qvm_server(sv::qvm_test_mod(), so, error);
    if (!r.s) return false;
    sv::MapSpec spec;
    spec.name = "arena";
    spec.models = sv::arena_models();
    spec.entities = sv::kBaselineArenaEntities;
    return sv::qvm_load_map(*r.s, spec, error);
  }
  if (!cl::open_game_dir(root, mod, r.files, error)) return false;
  std::vector<uint8_t> module;
  if (!read_game_file(&r.files, "qwprogs.qvm", module)) {
    error = "the mod folder " + mod + " under " + root + " holds no qwprogs.qvm";
    return false;
  }
  r.source = mod + "/qwprogs.qvm (" + std::to_string(module.size()) + " bytes)";
  r.map = cl::load_named_map(r.files, map, error);
  if (!r.map) return false;
  so.read_file = &read_game_file;
  so.files_ctx = &r.files;
  so.list_dirs = {r.files.folder};
  so.write_dir = "";  // the lab writes nothing into the mod's folder
  r.s = sv::make_qvm_server(std::move(module), so, error);
  if (!r.s) return false;
  sv::qvm_console(*r.s, "deathmatch 1");
  sv::qvm_console(*r.s, "serverinfo *gamedir \"" + mod + "\"");
  return sv::qvm_load_map(*r.s, r.map->spec, error);
}

void run(gl::Lab& lab) {
  const int players = std::clamp(static_cast<int>(lab.arg_num("players")), 0, 16);
  const double seconds = lab.arg_num("seconds");
  const Inputs in = {{"root", lab.arg("root").empty() ? "(none: the test mod)" : "a mod folder"},
                     {"mod", lab.arg("mod")},
                     {"map", lab.arg("map")},
                     {"players", lab.arg("players")},
                     {"seconds", lab.arg("seconds")}};
  Run r;
  std::string error;
  if (!start(lab, r, error)) {
    lab.fail("se-qvm: the module did not start: " + error,
             "next: run tests/shooters_edge_qvm_server_test.cpp; for a mod folder check --root "
             "and --mod name a folder holding qwprogs.qvm and maps/" +
                 lab.arg("map") + ".bsp");
    return;
  }
  sv::QvmServer& s = *r.s;
  const int64_t faults_at_load = s.q.faults;
  const size_t console_at_load = s.console.size();

  for (int p = 0; p < players; p++)
    sv::qvm_connect(s, p, "player" + std::to_string(p + 1), false, false);
  gl::Trace alive{"live entities", {}, {}};
  gl::Trace calls{"system calls a frame", {}, {}};
  int64_t frames = 0, last_total = 0;
  for (int64_t c : s.syscall_counts) last_total += c;
  const double t0 = s.sv.time;
  while (s.sv.time - t0 < seconds && frames < 100000) {
    sv::qvm_frame(s, nullptr, 0);
    frames++;
    int live = 0;
    for (int e = 0; e < s.sv.num_edicts; e++)
      live += s.sv.edicts[static_cast<size_t>(e)].free ? 0 : 1;
    int64_t total = 0;
    for (int64_t c : s.syscall_counts) total += c;
    alive.x.push_back(s.sv.time);
    alive.y.push_back(live);
    calls.x.push_back(s.sv.time);
    calls.y.push_back(static_cast<double>(total - last_total));
    last_total = total;
  }

  gl::LabTable exports{
      "the exports the server called, and how often", {"#", "export", "calls"}, {}};
  for (int x = 0; x < sim::kQvmExports; x++) {
    if (s.q.export_counts[x] == 0) continue;
    exports.rows.push_back(
        {std::to_string(x), kExportNames[x], std::to_string(s.q.export_counts[x])});
    lab.metric("exports." + key_of(kExportNames[x]), static_cast<double>(s.q.export_counts[x]),
               "count");
  }
  lab.table(exports);

  gl::LabTable syscalls{"every system call the module made, by number", {"#", "call", "calls"}, {}};
  for (int n = 0; n < sv::kQvmSyscalls; n++) {
    if (s.syscall_counts[n] == 0) continue;
    const std::string name = sv::qvm_syscall_name(n);
    syscalls.rows.push_back({std::to_string(n), name.empty() ? "(unknown)" : name,
                             std::to_string(s.syscall_counts[n])});
    lab.metric("syscalls." + key_of(name.empty() ? std::to_string(n) : name),
               static_cast<double>(s.syscall_counts[n]), "count");
  }
  lab.table(syscalls);

  std::map<std::string, int> classes;
  for (int e = 1; e < s.sv.num_edicts; e++)
    if (!s.sv.edicts[static_cast<size_t>(e)].free) classes[class_of(s, e)]++;
  gl::LabTable ents{"the live entities at the end, by class", {"class", "count"}, {}};
  for (const auto& [c, n] : classes) ents.rows.push_back({c, std::to_string(n)});
  lab.table(ents);

  gl::LabTable console{"the console's lines (the first 40)", {"#", "line"}, {}};
  for (size_t i = 0; i < s.console.size() && i < 40; i++) {
    std::string line = s.console[i];
    for (char& ch : line) ch = static_cast<char>(static_cast<unsigned char>(ch) & 0x7f);
    while (!line.empty() && line.back() == '\n') line.pop_back();
    console.rows.push_back({std::to_string(i + 1), line});
  }
  lab.table(console);

  gl::LabTable clients{"the clients at the end", {"slot", "name", "x", "y", "z", "health"}, {}};
  for (int p = 0; p < players; p++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(p + 1)];
    clients.rows.push_back({std::to_string(p), s.world.clients[static_cast<size_t>(p)].name,
                            to_fixed(d.origin.x, 1), to_fixed(d.origin.y, 1),
                            to_fixed(d.origin.z, 1),
                            to_fixed(sim::field_f(s.q, p + 1, pg::kFHealth), 0)});
  }
  lab.table(clients);

  int prints = 0;
  for (const sv::NetEvent& e : s.out.events) prints += e.kind == sv::EventKind::print ? 1 : 0;
  const uint64_t digest = sh::server_hash(s.sv);
  gl::LabTable run_t{"the run", {"what", "value"}, {}};
  const std::pair<std::string, std::string> rows[] = {
      {"module", r.source},
      {"interface", std::to_string(s.q.data.api)},
      {"entity record bytes", std::to_string(s.q.data.entity_size)},
      {"entity limit", std::to_string(s.q.data.max_entities)},
      {"string fields by reference", s.q.references ? "yes" : "no"},
      {"map", s.world.map_name},
      {"frames", std::to_string(frames)},
      {"server time at the end s", to_fixed(s.sv.time, 3)},
      {"entity slots used", std::to_string(s.sv.num_edicts)},
      {"models precached", std::to_string(s.models.size() - 1)},
      {"sounds precached", std::to_string(s.sounds.size() - 1)},
      {"console lines at load", std::to_string(console_at_load)},
      {"console lines", std::to_string(s.console.size())},
      {"prints to clients", std::to_string(prints)},
      {"faults at load", std::to_string(faults_at_load)},
      {"faults", std::to_string(s.q.faults)},
      {"last fault", s.q.last_fault.empty() ? "(none)" : s.q.last_fault},
      {"digest at the end", hex64(digest)}};
  for (const auto& [k, v] : rows) run_t.rows.push_back({k, v});
  lab.table(run_t);
  lab.metric("run.frames", static_cast<double>(frames), "count");
  lab.metric("run.entities", static_cast<double>(s.sv.num_edicts), "count");
  lab.metric("run.models", static_cast<double>(s.models.size() - 1), "count");
  lab.metric("run.faults", static_cast<double>(s.q.faults), "count");

  if (s.q.faults > 0)
    lab.fail("se-qvm: the module faulted " + std::to_string(s.q.faults) +
                 " times; the last: " + s.q.last_fault,
             "next: read the system call it names in games/shooters_edge/server/qvm_calls.cpp");
  if (players > 0 && !s.world.clients[0].active)
    lab.fail("se-qvm: the first client is not in the game after connecting",
             "next: read qvm_connect in games/shooters_edge/server/qvm_server.cpp");

  // The entities from above, a dot each, by class.
  gl::LineChart above;
  above.points = true;
  above.spikes = false;
  above.frame = {"THE ENTITIES FROM ABOVE",
                 r.source + " on " + s.world.map_name + ", " + std::to_string(classes.size()) +
                     " classes after " + to_fixed(seconds, 1) + " s",
                 "x (units)", "y (units)"};
  std::map<std::string, gl::Trace> by_group;
  for (int e = 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
    if (d.free) continue;
    std::string c = class_of(s, e);
    const size_t u = c.find('_');
    const std::string group = u == std::string::npos ? c : c.substr(0, u);
    gl::Trace& t = by_group[group];
    t.label = group;
    t.x.push_back(d.origin.x);
    t.y.push_back(d.origin.y);
  }
  for (auto& [g, t] : by_group) above.traces.push_back(std::move(t));
  shoot(lab, "above", gl::line_chart(above),
        "Every live entity at the end at its origin seen from above (x east, y north), coloured by "
        "the first word of its class (item_*, weapon_*, info_*, trigger_*, ...): what the module "
        "spawned from the map's entity text itself, through its own system calls.",
        "the map's items, spawn points and triggers where the map puts them; players on spawn "
        "points; no pile of entities at the origin (an entity the module never placed)",
        in);

  gl::LineChart clock;
  clock.spikes = false;
  clock.log_y = true;
  clock.frame = {"THE MODULE AGAINST THE CLOCK", "every server frame (log scale)",
                 "server time (s)", "count"};
  clock.traces.push_back(alive);
  clock.traces.push_back(calls);
  shoot(lab, "clock", gl::line_chart(clock),
        "The live entities after each frame, and how many system calls the module made in it.",
        "a flat count of entities once the map is up (a step where a client joined or an item "
        "respawned); system calls a frame steady, with no run-away climb",
        in);
}

const gl::LabSpec kSpec{
    "se-qvm",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/server (the module server: qvm_server, qvm_calls) and "
    "games/shooters_edge/sim/qvm_program over plugins/progs' compiled-module VM, driving the "
    "hand-assembled test mod or a mod folder's qwprogs.qvm",
    "progs",
    "Does a mod shipped as a compiled module run a match on the module server: its game data "
    "block bound, its entities spawned from the map's text, its exports called in order, its "
    "system calls answered, no faults?",
    {
        {"root", "", "a game root holding the mod folder (empty: the test mod on the test arena)"},
        {"mod", "", "the mod folder under --root holding qwprogs.qvm"},
        {"map", "arena", "the map (maps/<name>.bsp from the mod's search path)"},
        {"players", "2", "clients connected after the load, 0..16"},
        {"seconds", "5", "server seconds to run"},
        {"seed", "1", "the server's seed (init's)"},
    },
    "--seconds=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
