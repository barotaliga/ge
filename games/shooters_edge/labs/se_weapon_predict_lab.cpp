// LAB `se-weapon-predict`: HOW SOON DOES THE JOINED WINDOW SHOW ITS OWN SHOT? The arena's rules
// in C++ served by the QuakeWorld server on engine/net's simulated network, the window's client
// holding a super nailgun and pressing attack for 0.6 s in every 1.2 s. For each one-way latency
// in `--latencies` the session runs twice: weapon prediction off (`cl_nopred_weapon 1`: the shot
// heard and the ammunition drawn when the server's frame says so) and on (net/qw_weapon.hpp: run
// forward from the server's weapon state over the commands sent since). A table of the press to
// the first shot heard, the shots heard a press against the server's own, and the press to the
// ammunition counting down; a chart of one press at the first latency (the button, the shots
// heard, the nails on the bar, off and on).
//
// ge:summary Lab: the joined window's own shots predicted: press to shot heard, off and on
// ge:keywords lab se-weapon-predict weapon-prediction cl_nopred_weapon fire-delay latency
// ge:keywords predicted-sound super-nailgun own_shots
// ge:skill net labs
// ge:test tests/shooters_edge_qw_weapon_test.cpp
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/labs/arena_chart.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/net/qw_server.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace cn = shooters_edge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;
namespace sl = shooters_edge::labs;
namespace dc = shooters_edge::client;
using ge::core::num::to_fixed;

// One spawn point with a super nailgun and nails on it: the window joins holding it, 200 nails.
const char* const kEntities = R"({
"classname" "worldspawn"
"message" "weapon prediction"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 0 24"
}
{
"classname" "weapon_supernailgun"
"origin" "-416 0 0"
}
{
"classname" "item_spikes"
"origin" "-416 0 0"
"spawnflags" "1"
}
{
"classname" "item_spikes"
"origin" "-400 0 0"
"spawnflags" "1"
}
{
"classname" "item_spikes"
"origin" "-432 0 0"
"spawnflags" "1"
}
)";

struct Session {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct;
  cn::QwServer server;
  cn::Remote remote;
  std::vector<std::string> said;
  std::vector<sh::BrushModel> models;
  int64_t now = 0;
};

// A mission pack's program in the arena: a start for its single player too, nothing to pick up
// (the lab gives every weapon with impulse 9).
const char* const kPackEntities = R"({
"classname" "worldspawn"
"message" "weapon prediction"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 0 24"
}
)";

// `pack`: a mission pack's program (its progs.dat) in place of the baseline mod.
bool open_session(Session& s, double latency_ms, std::string& error,
                  const std::vector<uint8_t>* pack = nullptr) {
  if (pack != nullptr ? !ge::progs::load_progs(pack->data(), pack->size(), s.progs, error,
                                               ge::progs::every_form())
                      : !sv::baseline_mod(s.progs, error))
    return false;
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  s.qc = sv::make_qc_server(s.progs, so, error);
  if (!s.qc) return false;
  sv::MapSpec map;
  map.name = "arena";
  map.models = sv::arena_models();
  map.entities = pack != nullptr ? kPackEntities : kEntities;
  if (!sv::load_map(*s.qc, map, error)) return false;
  gn::LinkConditions link;
  gn::conditions_preset("lan", link);
  link.latency_ms = latency_ms;
  link.jitter_ms = 0;
  s.net = gn::make_sim_network(1, link);
  gn::open_sim_transport(s.st, s.net, gn::sim_address(0, cn::kPortServer));
  gn::open_sim_transport(s.ct, s.net, gn::sim_address(1, cn::kPortClient));
  s.rules = sv::qc_runner_over(*s.qc);
  if (pack != nullptr) {  // single player: impulse 9 gives every weapon
    s.rules.console("deathmatch 0");
    s.rules.console("coop 0");
    sv::write_game_kind(*s.qc);
  }
  if (!cn::qw_start(s.server, s.rules, s.st, cn::ServerConfig{}, error)) return false;
  cn::ClientConfig wc;
  wc.name = "window";
  cn::remote_start(s.remote, s.ct, gn::sim_address(0, cn::kPortServer), wc);
  s.models = sv::arena_models();
  return true;
}

void window_frame(Session& s, int64_t wall_us, int64_t window_us) {
  while (s.now + cn::kTickUs <= wall_us) {
    s.now += cn::kTickUs;
    gn::sim_advance(s.net, s.now);
    cn::qw_frame(s.server, s.now);
  }
  cn::remote_step(s.remote, static_cast<double>(window_us) * 1e-6, s.said);
}

bool nail_sound(const dc::DemoState& d, const dc::DemoSound& snd) {
  if (snd.entity != d.player + 1) return false;
  const std::string_view name = snd.name != nullptr ? std::string_view(snd.name)
                                : snd.sound > 0 && static_cast<size_t>(snd.sound) < d.sounds.size()
                                    ? std::string_view(d.sounds[static_cast<size_t>(snd.sound)])
                                    : std::string_view();
  return name == "weapons/spike2.wav" || name == "weapons/rocket1i.wav";
}

struct Run {
  bool ok = false;
  bool agreed = false;
  double round_trip_ms = 0;  // the client's own measure, smoothed
  int presses = 0, heard = 0;
  uint32_t late = 0;  // the server's copies of the prediction's sounds that matched none
  double first_heard_ms = 0, first_ammo_ms = 0;  // means over the presses
  std::vector<double> t, button, shots, nails;   // the chart's press
};

constexpr double kPressS = 0.6, kCycleS = 1.2;

Run run_session(double latency_ms, bool on, int fps, int presses) {
  Run out;
  Session s;
  std::string error;
  if (!open_session(s, latency_ms, error)) return out;
  const int64_t window_us = 1000000 / fps;
  int64_t wall = 0;
  for (int i = 0; i < 6 * fps && s.remote.end.stage != cn::ClientStage::active; i++)
    window_frame(s, wall += window_us, window_us);
  for (int i = 0; i < fps; i++) window_frame(s, wall += window_us, window_us);  // picked up
  if (s.remote.end.stage != cn::ClientStage::active) return out;
  cn::remote_world(s.remote, &s.models);
  s.remote.weapon.off = !on;
  out.agreed = s.remote.end.ext.has(cn::ExtFamily::mvd, cn::kMvdWeaponPred);
  const dc::DemoState& d = s.remote.demo.state;
  const int frames = static_cast<int>(kCycleS * fps * presses);
  const int cycle = static_cast<int>(kCycleS * fps);
  const int held = static_cast<int>(kPressS * fps);
  int shots_seen = d.own_shots;
  int press_at = -1;
  bool heard_this = false, ammo_this = false;
  int32_t nails_at_press = 0;
  for (int i = 0; i < frames; i++) {
    const int in_cycle = i % cycle;
    if (in_cycle == 0) {
      press_at = i;
      heard_this = ammo_this = false;
      nails_at_press = d.stats[dc::kStatNails];
      out.presses++;
    }
    s.remote.hands.buttons = in_cycle < held ? sh::kButtonAttack : 0;
    window_frame(s, wall += window_us, window_us);
    int heard_now = 0;
    if (on) {
      heard_now = d.own_shots - shots_seen;
      shots_seen = d.own_shots;
    } else {
      for (const dc::DemoSound& snd : d.started)
        if (nail_sound(d, snd)) heard_now++;
    }
    out.heard += heard_now;
    const double since_ms = static_cast<double>(i - press_at) * 1000.0 / fps;
    if (heard_now > 0 && !heard_this) {
      heard_this = true;
      out.first_heard_ms += since_ms;
    }
    if (!ammo_this && d.stats[dc::kStatNails] < nails_at_press) {
      ammo_this = true;
      out.first_ammo_ms += since_ms;
    }
    if (i < cycle) {
      out.t.push_back(since_ms);
      out.button.push_back(in_cycle < held ? 1 : 0);
      out.shots.push_back(heard_now);
      out.nails.push_back(d.stats[dc::kStatNails]);
    }
  }
  out.round_trip_ms = s.remote.end.latency_avg * 1000;
  out.late = s.remote.weapon.filled_late;
  out.first_heard_ms /= out.presses;
  out.first_ammo_ms /= out.presses;
  out.ok = true;
  return out;
}

// THE MISSION PACKS' GUNS: a pack's own program served, every weapon given (impulse 9), one gun
// chosen by its impulses, then pressed. Each press's time to its shot heard (the gun's own sample)
// and to its firing frame drawn, off and on.
struct PackGun {
  const char* pack;    // the folder under --packs
  const char* key;     // the metric's name
  const char* what;    // the table's words
  int impulses[2];     // to choose it (0: none)
  const char* sample;  // its shot's sound ("": none, the frame alone)
  double hold_s, cycle_s;
};
constexpr PackGun kPackGuns[] = {
    {"hipnotic",
     "laser",
     "Scourge of Armagon's laser cannon",
     {225, 0},
     "hipweap/laserg.wav",
     0.55,
     1.2},
    {"hipnotic", "mjolnir", "Scourge of Armagon's Mjolnir", {226, 0}, "", 0.1, 1.2},
    {"hipnotic",
     "prox",
     "Scourge of Armagon's proximity gun",
     {6, 6},
     "hipweap/proxbomb.wav",
     0.1,
     1.2},
    {"rogue",
     "lava",
     "Dissolution of Eternity's lava nailgun",
     {60, 0},
     "weapons/rocket1i.wav",
     0.55,
     1.2},
    {"rogue",
     "multirocket",
     "Dissolution of Eternity's multi rocket launcher",
     {63, 0},
     "weapons/sgun1.wav",
     0.1,
     1.2},
    {"rogue",
     "plasma",
     "Dissolution of Eternity's plasma gun",
     {64, 0},
     "plasma/fire.wav",
     0.1,
     1.5},
};

struct PackRun {
  bool ok = false;
  std::string why;
  int presses = 0, heard_n = 0, drawn_n = 0;
  uint32_t late = 0;                  // the server's copies of predicted sounds that matched none
  double heard_ms = 0, drawn_ms = 0;  // means over the presses that showed it
};

bool heard_sample(const dc::DemoState& d, const char* sample) {
  for (const dc::DemoSound& snd : d.started) {
    if (snd.entity != d.player + 1) continue;
    const std::string_view name =
        snd.name != nullptr ? std::string_view(snd.name)
        : snd.sound > 0 && static_cast<size_t>(snd.sound) < d.sounds.size()
            ? std::string_view(d.sounds[static_cast<size_t>(snd.sound)])
            : std::string_view();
    if (name == sample) return true;
  }
  return false;
}

PackRun run_pack(const std::vector<uint8_t>& program, const PackGun& g, double latency_ms, bool on,
                 int fps, int presses) {
  PackRun out;
  Session s;
  std::string error;
  if (!open_session(s, latency_ms, error, &program)) {
    out.why = error;
    return out;
  }
  const int64_t window_us = 1000000 / fps;
  int64_t wall = 0;
  for (int i = 0; i < 6 * fps && s.remote.end.stage != cn::ClientStage::active; i++)
    window_frame(s, wall += window_us, window_us);
  if (s.remote.end.stage != cn::ClientStage::active) {
    out.why = "the window never reached play";
    return out;
  }
  cn::remote_world(s.remote, &s.models);
  const auto impulse = [&](int i) {
    s.remote.hands.impulse = static_cast<uint8_t>(i);
    for (int k = 0; k < fps; k++) window_frame(s, wall += window_us, window_us);
  };
  impulse(9);
  for (const int i : g.impulses)
    if (i != 0) impulse(i);
  // Aimed straight up: a laser bolt that came back off the arena's walls would end the run.
  s.remote.view.x = -89;
  s.remote.weapon.off = !on;
  const dc::DemoState& d = s.remote.demo.state;
  const int cycle = static_cast<int>(g.cycle_s * fps), held = static_cast<int>(g.hold_s * fps);
  int press_at = 0;
  bool heard = false, drawn = false;
  for (int i = 0; i < cycle * presses; i++) {
    const int in_cycle = i % cycle;
    if (in_cycle == 0) {
      press_at = i;
      heard = drawn = false;
      out.presses++;
    }
    s.remote.hands.buttons = in_cycle < held ? sh::kButtonAttack : 0;
    window_frame(s, wall += window_us, window_us);
    const double since = static_cast<double>(i - press_at) * 1000.0 / fps;
    if (!heard && g.sample[0] != 0 && heard_sample(d, g.sample)) {
      heard = true;
      out.heard_ms += since;
      out.heard_n++;
    }
    const int me = d.player;
    const int frame = me >= 0 && static_cast<size_t>(me) < d.players.size()
                          ? d.players[static_cast<size_t>(me)].weapon_frame
                          : 0;
    if (!drawn && frame != 0) {
      drawn = true;
      out.drawn_ms += since;
      out.drawn_n++;
    }
  }
  out.heard_ms /= out.heard_n > 0 ? out.heard_n : 1;
  out.drawn_ms /= out.drawn_n > 0 ? out.drawn_n : 1;
  out.late = s.remote.weapon.filled_late;
  out.ok = true;
  return out;
}

// The packs' table when --packs names a folder holding hipnotic/pak0.pak and rogue/pak0.pak (the
// player's own; never in the tree, so the smoke run has none).
void run_packs(gl::Lab& lab, const std::string& root, double ms, int fps, int presses) {
  gl::LabTable t{"the mission packs' guns at " + to_fixed(ms, 0) + " ms each way, off and on",
                 {"gun", "weapon prediction", "press to shot heard (ms)", "heard",
                  "press to firing frame drawn (ms)", "drawn", "server copies played late"},
                 {}};
  std::string loaded;
  std::vector<uint8_t> program;
  for (const PackGun& g : kPackGuns) {
    if (loaded != g.pack) {
      loaded = g.pack;
      program.clear();
      const std::filesystem::path path = std::filesystem::path(root) / g.pack / "pak0.pak";
      std::ifstream f(path, std::ios::binary);
      const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                       std::istreambuf_iterator<char>());
      ge::vfs::Pak pak;
      std::string error;
      const ge::vfs::PakEntry* e = nullptr;
      if (!bytes.empty() && ge::vfs::read_pak(bytes, &pak, &error))
        e = ge::vfs::pak_find(pak, "progs.dat");
      if (e == nullptr) {
        lab.fail("se-weapon-predict: no progs.dat in " + path.string(),
                 "next: point --packs at the folder holding hipnotic/ and rogue/, each with its "
                 "pak0.pak");
        return;
      }
      const std::span<const uint8_t> p = ge::vfs::pak_file(bytes, *e);
      program.assign(p.begin(), p.end());
    }
    for (const bool on : {false, true}) {
      const PackRun r = run_pack(program, g, ms, on, fps, presses);
      if (!r.ok || r.drawn_n + r.heard_n == 0) {
        lab.fail(std::string("se-weapon-predict: ") + g.key + (on ? " on" : " off") +
                     ": nothing showed (" + (r.why.empty() ? "no shot heard or drawn" : r.why) +
                     ")",
                 "next: build/debug/tests/classic_weapon_predict_test");
        return;
      }
      const std::string key = std::string("pack.") + g.key + (on ? ".on" : ".off");
      if (r.heard_n > 0) lab.metric(key + ".shot-heard", r.heard_ms, "ms");
      if (r.drawn_n > 0) lab.metric(key + ".frame-drawn", r.drawn_ms, "ms");
      const auto count = [&](int n) { return std::to_string(n) + "/" + std::to_string(r.presses); };
      t.rows.push_back({g.what, on ? "on" : "off", r.heard_n > 0 ? to_fixed(r.heard_ms, 1) : "-",
                        count(r.heard_n), r.drawn_n > 0 ? to_fixed(r.drawn_ms, 1) : "-",
                        count(r.drawn_n), on ? std::to_string(r.late) : "-"});
      if (on) lab.metric(key + ".copies-late", static_cast<double>(r.late), "sounds");
    }
  }
  lab.table(t);
}

void run(gl::Lab& lab) {
  const int fps = static_cast<int>(lab.arg_num("fps"));
  const int presses = static_cast<int>(lab.arg_num("presses"));
  std::vector<double> latencies;
  const std::string list = lab.arg("latencies");
  for (size_t at = 0; at < list.size();) {
    size_t comma = list.find(',', at);
    if (comma == std::string::npos) comma = list.size();
    latencies.push_back(std::stod(list.substr(at, comma - at)));
    at = comma + 1;
  }
  const std::vector<std::pair<std::string, std::string>> in = {{"fps", lab.arg("fps")},
                                                               {"presses", lab.arg("presses")},
                                                               {"latencies", lab.arg("latencies")}};
  if (fps < 30 || presses < 1 || latencies.empty()) {
    lab.fail(
        "se-weapon-predict: --fps must be 30 or more, --presses 1 or more and --latencies "
        "name one or more",
        "next: make lab LAB=se-weapon-predict ARGS=--help");
    return;
  }
  gl::LabTable t{
      "the window's own super nailgun, a press of 0.6 s in every 1.2 s, off and on",
      {"one way ms", "round trip ms", "weapon prediction", "press to first shot heard (ms)",
       "press to the nails counting down (ms)", "shots heard a press"},
      {}};
  bool first = true;
  for (double ms : latencies) {
    Run runs[2];
    for (int k = 0; k < 2; k++) {
      runs[k] = run_session(ms, k == 1, fps, presses);
      const Run& r = runs[k];
      if (!r.ok || !r.agreed) {
        lab.fail(
            "se-weapon-predict: the window never reached play, or weapon prediction was not "
            "agreed",
            "next: build/debug/tests/shooters_edge_qw_weapon_test");
        return;
      }
      const std::string key = "ms-" + to_fixed(ms, 0) + (k == 1 ? ".on" : ".off");
      const double per_press = static_cast<double>(r.heard) / r.presses;
      t.rows.push_back({to_fixed(ms, 0), to_fixed(r.round_trip_ms, 1), k == 1 ? "on" : "off",
                        to_fixed(r.first_heard_ms, 1), to_fixed(r.first_ammo_ms, 1),
                        to_fixed(per_press, 2)});
      lab.metric(key + ".shot-heard", r.first_heard_ms, "ms");
      lab.metric(key + ".ammo-drawn", r.first_ammo_ms, "ms");
      lab.metric(key + ".shots-a-press", per_press, "shots");
      if (k == 1) lab.metric(key + ".copies-late", static_cast<double>(r.late), "sounds");
    }
    // On, the shot is heard within two commands of the press whatever the latency.
    if (runs[1].first_heard_ms >= runs[0].first_heard_ms)
      lab.fail("se-weapon-predict: at " + to_fixed(ms, 0) +
                   " ms each way the predicted shot came no sooner than the server's",
               "next: build/debug/tests/shooters_edge_qw_weapon_test");
    const double off_n = static_cast<double>(runs[0].heard) / runs[0].presses;
    const double on_n = static_cast<double>(runs[1].heard) / runs[1].presses;
    if (on_n > off_n + 1 || on_n < off_n - 1)
      lab.fail("se-weapon-predict: at " + to_fixed(ms, 0) + " ms each way prediction heard " +
                   to_fixed(on_n, 2) + " shots a press against the server's " + to_fixed(off_n, 2),
               "next: build/debug/tests/classic_weapon_predict_test");
    if (!first) continue;
    first = false;
    gl::LineChart c;
    c.spikes = false;
    c.frame = {"ONE PRESS OF THE SUPER NAILGUN, " + to_fixed(ms, 0) + " MS EACH WAY",
               "the window at " + std::to_string(fps) +
                   " fps: the nails used since the press as the bar shows them, off and on",
               "since the press (ms)", "nails used"};
    for (int k = 0; k < 2; k++) {
      gl::Trace tr{k == 1 ? "prediction on" : "prediction off", runs[k].t, {}};
      for (const double n : runs[k].nails) tr.y.push_back(runs[k].nails.front() - n);
      c.traces.push_back(tr);
    }
    double most = 1;
    for (const double y : c.traces[0].y) most = y > most ? y : most;
    gl::Trace held{"attack held", runs[1].t, {}};
    for (const double b : runs[1].button) held.y.push_back(b * (most + 2));
    c.traces.push_back(held);
    sl::shoot(lab, "press-nails", gl::line_chart(c),
              "The nails used since the press as the window's bar shows them, a sample each "
              "window frame. Off, the count waits a round trip before it climbs; on, it climbs "
              "with the press, two a shot about every 0.1 s, and the server's frames agree with "
              "it.",
              "the on line leaves 0 at once; the off line a round trip later; both climb in the "
              "same steps to the same count",
              in);
    gl::LineChart h;
    h.spikes = false;
    h.points = true;
    h.frame = {"SHOTS HEARD THROUGH ONE PRESS", to_fixed(ms, 0) + " ms each way",
               "since the press (ms)", "shots heard that window frame"};
    h.traces.push_back({"prediction off", runs[0].t, runs[0].shots});
    h.traces.push_back({"prediction on", runs[1].t, runs[1].shots});
    sl::shoot(lab, "press-shots", gl::line_chart(h),
              "Each window frame's shots heard through one press. Off, the first comes a round "
              "trip after the press; on, with the press, then every 0.1 s.",
              "the on points start at 0 ms; the off points a round trip later; the same count", in);
  }
  lab.table(t);
  if (!lab.arg("packs").empty()) run_packs(lab, lab.arg("packs"), latencies.front(), fps, presses);
}

const gl::LabSpec kSpec{
    "se-weapon-predict",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/qw_weapon: the joined window's own weapon predicted from the "
    "server's weapon state",
    "net",
    "How soon does the window joined to a server hear and show its own shot, with weapon "
    "prediction off and on?",
    {
        {"fps", "144", "the window's frames a second (the server runs 77)"},
        {"presses", "4", "presses measured a session"},
        {"latencies", "75,25",
         "one-way latencies, ms, a pair of sessions each (the first charted)"},
        {"packs", "",
         "a folder holding the Quake mission packs (hipnotic/pak0.pak, rogue/pak0.pak): their "
         "guns measured too, at the first latency"},
    },
    "--presses=2 --latencies=40",
    run,
};
GE_LAB(kSpec);

}  // namespace
