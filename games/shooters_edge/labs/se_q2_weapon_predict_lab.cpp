// LAB `se-q2-weapon-predict`: HOW SOON DOES THE WINDOW JOINED TO A SERVER OF QUAKE II SHOW ITS OWN
// SHOT? Our Quake II server on engine/net's simulated network, the window's client holding the
// machinegun with 50 bullets and pressing attack for 0.6 s in every 1.2 s. For each one-way
// latency in `--latencies` the session runs twice: weapon prediction off (`cl_nopred_weapon 1`:
// the shot heard, the gun's frame and the ammunition drawn when the server's frame says so) and on
// (net/q2_weapon.hpp: the hand run forward from the newest frame over the moves sent since). Each
// session runs once more as a stock server would, by its press rule (a press fires at once, a
// held trigger steps with the server's frames), the rule the prediction uses on a server that does
// not say it is ours. A table of the press to the first shot heard, to the gun's first fire frame
// drawn and to the bullets counting down, and the shots heard a press against the server's own; a
// chart of one press at the first latency (the trigger, the shots, the bullets on the bar, off and
// on) and one of the press to the first shot heard by latency.
//
// ge:summary Lab: Quake II's joined window's own shots predicted: press to shot heard, off and on
// ge:keywords lab se-q2-weapon-predict weapon-prediction cl_nopred_weapon fire-delay latency q2
// ge:keywords machinegun predicted-flash own_shots
// ge:skill net labs
// ge:test tests/shooters_edge_q2_weapon_test.cpp
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/labs/arena_chart.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace br = ge::brush;
namespace cn = shooters_edge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;
namespace sl = shooters_edge::labs;
namespace dc = shooters_edge::client;
using ge::core::num::to_fixed;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A hall of 2048 units with one start, a machinegun and bullets on it.
std::shared_ptr<const br::Map> hall(std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "weapon_machinegun"}, {"origin", "-400 -400 16"}}));
  w.entities.push_back(entity({{"classname", "ammo_bullets"}, {"origin", "-390 -400 16"}}));
  std::vector<uint8_t> bytes;
  if (!br::write_brush_world(w, &bytes, &error)) return nullptr;
  auto map = std::make_shared<br::Map>();
  if (!br::read_bsp(bytes, map.get(), &error)) return nullptr;
  return map;
}

struct Session {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct;
  cn::Q2NetServer server;
  cn::Remote remote;
  std::vector<std::string> said;
  int64_t now = 0, wall = 0;  // the server's host ticks, the window's clock
};

bool open_session(Session& s, double latency_ms, std::string& error) {
  s.rules = sv::q2_runner(sv::make_q2_server({}));
  s.rules.console("deathmatch 1");
  s.rules.console("sv_fps 10");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(error);
  if (!map.brushes || !s.rules.start(map, error)) return false;
  gn::LinkConditions link;
  gn::conditions_preset("lan", link);
  link.latency_ms = latency_ms;
  link.jitter_ms = 0;
  s.net = gn::make_sim_network(3, link);
  gn::open_sim_transport(s.st, s.net, gn::sim_address(0, cn::kPortQ2));
  if (!cn::q2_net_start(s.server, s.rules, s.st, {}, error)) return false;
  gn::open_sim_transport(s.ct, s.net, gn::sim_address(1, 27901));
  s.remote.q2_game = [] { return true; };
  s.remote.read_file = [](const std::string&, const std::string&) {
    return std::vector<uint8_t>{1};  // it has every file
  };
  cn::ClientConfig wc;
  wc.name = "window";
  cn::remote_start(s.remote, s.ct, gn::sim_address(0, cn::kPortQ2), wc);
  return true;
}

// One window frame: the server's host ticks up to its end, then the window's step.
void window_frame(Session& s, int64_t window_us) {
  s.wall += window_us;
  while (s.now + cn::kTickUs <= s.wall) {
    s.now += cn::kTickUs;
    gn::sim_advance(s.net, s.now);
    cn::q2_net_frame(s.server, s.now);
  }
  cn::remote_step(s.remote, static_cast<double>(window_us) * 1e-6, s.said);
}

// The player's machinegun shots heard this window frame: its flash's sound on him.
int shots_heard(const dc::DemoState& d) {
  int n = 0;
  for (const dc::DemoSound& snd : d.started)
    if (snd.entity == d.player + 1 && snd.name != nullptr &&
        std::string_view(snd.name).starts_with("weapons/machgf"))
      n++;
  return n;
}

struct Run {
  bool ok = false;
  bool predicted = false;  // the prediction ran through the presses
  double round_trip_ms = 0;
  int presses = 0, heard = 0;
  double first_heard_ms = 0, first_frame_ms = 0, first_ammo_ms = 0;  // means over the presses
  std::vector<double> t, button, shots, bullets;                     // the chart's press
};

constexpr double kPressS = 0.6, kCycleS = 1.2;

// `latch`: the prediction run by a stock server's press rule (the server ours either way).
Run run_session(double latency_ms, bool on, bool latch, int fps, int presses) {
  Run out;
  Session s;
  std::string error;
  if (!open_session(s, latency_ms, error)) return out;
  const int64_t window_us = 1000000 / fps;
  for (int i = 0; i < 6 * fps && (!s.remote.q2 || s.remote.q2->stage != cn::ClientStage::active);
       i++)
    window_frame(s, window_us);
  if (!s.remote.q2 || s.remote.q2->stage != cn::ClientStage::active) return out;
  s.remote.hands.impulse = 4;  // the machinegun (`use Machinegun`)
  for (int i = 0; i < 2 * fps; i++) window_frame(s, window_us);  // picked up and raised
  s.remote.weapon.off = !on;
  s.remote.q2_weapon.stock_rule = latch;  // a stock server's press rule on ours
  const dc::DemoState& d = s.remote.demo.state;
  const int frames = static_cast<int>(kCycleS * fps * presses);
  const int cycle = static_cast<int>(kCycleS * fps);
  const int held = static_cast<int>(kPressS * fps);
  int press_at = -1;
  bool heard_this = false, frame_this = false, ammo_this = false;
  int32_t bullets_at_press = 0;
  int predicted = 0;
  for (int i = 0; i < frames; i++) {
    const int in_cycle = i % cycle;
    if (in_cycle == 0) {
      press_at = i;
      heard_this = frame_this = ammo_this = false;
      bullets_at_press = d.stats[dc::kStatAmmo];
      out.presses++;
    }
    s.remote.hands.buttons = in_cycle < held ? sh::kButtonAttack : 0;
    window_frame(s, window_us);
    if (s.remote.q2_weapon.on) predicted++;
    const int heard_now = shots_heard(d);
    out.heard += heard_now;
    const double since_ms = static_cast<double>(i - press_at) * 1000.0 / fps;
    if (heard_now > 0 && !heard_this) {
      heard_this = true;
      out.first_heard_ms += since_ms;
    }
    const int me = d.player;
    const int gun = me >= 0 && static_cast<size_t>(me) < d.players.size()
                        ? d.players[static_cast<size_t>(me)].weapon_frame
                        : 0;
    const sv::SqWeaponRow& mg = sv::kSqWeapons[static_cast<int>(sv::SqWeapon::machinegun)];
    if (!frame_this && gun >= mg.fire_from && gun <= mg.fire_to) {
      frame_this = true;
      out.first_frame_ms += since_ms;
    }
    if (!ammo_this && d.stats[dc::kStatAmmo] < bullets_at_press) {
      ammo_this = true;
      out.first_ammo_ms += since_ms;
    }
    if (i < cycle) {
      out.t.push_back(since_ms);
      out.button.push_back(in_cycle < held ? 1 : 0);
      out.shots.push_back(heard_now);
      out.bullets.push_back(d.stats[dc::kStatAmmo]);
    }
  }
  out.predicted = predicted > frames / 2;
  out.round_trip_ms = s.remote.q2->latency * 1000;
  out.first_heard_ms /= out.presses;
  out.first_frame_ms /= out.presses;
  out.first_ammo_ms /= out.presses;
  out.ok = true;
  return out;
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
        "se-q2-weapon-predict: --fps must be 30 or more, --presses 1 or more and --latencies "
        "name one or more",
        "next: make lab LAB=se-q2-weapon-predict ARGS=--help");
    return;
  }
  gl::LabTable t{
      "the window's own machinegun on our Quake II server, a press of 0.6 s in every "
      "1.2 s, off and on",
      {"one way ms", "round trip ms", "weapon prediction", "press to first shot heard (ms)",
       "press to the fire frame drawn (ms)", "press to the bullets counting down (ms)",
       "shots heard a press"},
      {}};
  gl::Trace by_off{"prediction off", {}, {}}, by_on{"prediction on", {}, {}},
      by_stock{"prediction on, a stock server's press rule", {}, {}};
  bool first = true;
  for (double ms : latencies) {
    Run runs[3];
    const char* names[3] = {"off", "on", "on, stock rule"};
    for (int k = 0; k < 3; k++) {
      runs[k] = run_session(ms, k >= 1, k == 2, fps, presses);
      const Run& r = runs[k];
      if (!r.ok || r.predicted != (k >= 1)) {
        lab.fail(
            "se-q2-weapon-predict: the window never reached play, or the prediction did not "
            "run when on (or ran when off)",
            "next: build/debug/tests/shooters_edge_q2_weapon_test");
        return;
      }
      const std::string key = "ms-" + to_fixed(ms, 0) +
                              (k == 0   ? ".off"
                               : k == 1 ? ".on"
                                        : ".stock");
      const double per_press = static_cast<double>(r.heard) / r.presses;
      t.rows.push_back({to_fixed(ms, 0), to_fixed(r.round_trip_ms, 1), names[k],
                        to_fixed(r.first_heard_ms, 1), to_fixed(r.first_frame_ms, 1),
                        to_fixed(r.first_ammo_ms, 1), to_fixed(per_press, 2)});
      lab.metric(key + ".shot-heard", r.first_heard_ms, "ms");
      lab.metric(key + ".fire-frame", r.first_frame_ms, "ms");
      lab.metric(key + ".ammo-drawn", r.first_ammo_ms, "ms");
      lab.metric(key + ".shots-a-press", per_press, "shots");
    }
    by_off.x.push_back(ms);
    by_off.y.push_back(runs[0].first_heard_ms);
    by_on.x.push_back(ms);
    by_on.y.push_back(runs[1].first_heard_ms);
    by_stock.x.push_back(ms);
    by_stock.y.push_back(runs[2].first_heard_ms);
    for (int k = 1; k < 3; k++) {
      if (runs[k].first_heard_ms >= runs[0].first_heard_ms)
        lab.fail("se-q2-weapon-predict: at " + to_fixed(ms, 0) +
                     " ms each way the predicted shot came no sooner than the server's",
                 "next: build/debug/tests/shooters_edge_q2_weapon_test");
      const double off_n = static_cast<double>(runs[0].heard) / runs[0].presses;
      const double on_n = static_cast<double>(runs[k].heard) / runs[k].presses;
      if (on_n > off_n + 1 || on_n < off_n - 1)
        lab.fail("se-q2-weapon-predict: at " + to_fixed(ms, 0) + " ms each way prediction heard " +
                     to_fixed(on_n, 2) + " shots a press against the server's " +
                     to_fixed(off_n, 2),
                 "next: build/debug/tests/shooters_edge_q2_weapon_test");
    }
    if (!first) continue;
    first = false;
    gl::LineChart c;
    c.spikes = false;
    c.frame = {"ONE PRESS OF THE MACHINEGUN, " + to_fixed(ms, 0) + " MS EACH WAY",
               "the window at " + std::to_string(fps) +
                   " fps: the bullets used since the press as the bar shows them, off and on",
               "since the press (ms)", "bullets used"};
    for (int k = 0; k < 2; k++) {
      gl::Trace tr{k == 1 ? "prediction on" : "prediction off", runs[k].t, {}};
      for (const double n : runs[k].bullets) tr.y.push_back(runs[k].bullets.front() - n);
      c.traces.push_back(tr);
    }
    double most = 1;
    for (const double y : c.traces[0].y) most = y > most ? y : most;
    gl::Trace trigger{"attack held", runs[1].t, {}};
    for (const double b : runs[1].button) trigger.y.push_back(b * (most + 1));
    c.traces.push_back(trigger);
    sl::shoot(lab, "press-bullets", gl::line_chart(c),
              "The bullets used since the press as the window's bar shows them, a sample each "
              "window frame. Off, the count waits a round trip before it climbs; on, it climbs "
              "with the press, a bullet each 0.1 s server frame, and the server's frames agree.",
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
              "trip after the press; on, with the press, then each 0.1 s.",
              "the on points start at 0 ms; the off points a round trip later; the same count", in);
  }
  if (latencies.size() > 1) {
    gl::LineChart l;
    l.spikes = false;
    l.frame = {"PRESS TO THE FIRST SHOT HEARD", "the mean over the presses, by one-way latency",
               "one-way latency (ms)", "press to first shot heard (ms)"};
    l.traces = {by_off, by_on, by_stock};
    sl::shoot(lab, "latency", gl::line_chart(l),
              "The press to the first shot heard by one-way latency. Off, it grows by the round "
              "trip and the wait for the server's next frame; on, it stays within a move or two "
              "of the press whatever the latency.",
              "the off line climbs with latency; both on lines stay flat near 0", in);
  }
  lab.table(t);
}

const gl::LabSpec kSpec{
    "se-q2-weapon-predict",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/q2_weapon: the joined window's own Quake II weapon predicted from the "
    "newest frame's gun",
    "net",
    "How soon does the window joined to a server of Quake II hear and show its own shot, with "
    "weapon prediction off and on?",
    {
        {"fps", "144", "the window's frames a second"},
        {"presses", "4", "presses measured a session"},
        {"latencies", "75,25,50,100,150",
         "one-way latencies, ms, three sessions each (the first charted)"},
    },
    "--presses=2 --latencies=40",
    run,
};
GE_LAB(kSpec);

}  // namespace
