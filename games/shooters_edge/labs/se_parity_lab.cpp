// LAB `se-parity`: does a player move on our server as on an existing one? One scripted player
// joins an existing server of the classic protocol (`--server`, built beside the checkout, never
// in the tree) and then our own dedicated server (`--ours`, `sesv`), both running the same mod on
// the same map, and plays the same commands on each: a second standing still, then a run down the
// parity hall (games/shooters_edge/content/maps/parity.plan: one start, stairs, a ledge, a ramp,
// a pillar, the walls), jumps, strafes, a turn back, a stop and a run into the wall. Each frame's
// own-player record (origin, velocity, on the ground) is kept by the command it answers (the
// move the frame acknowledged), so the two runs line up command by command whatever the network
// did. The lab tables and draws both paths, from above and from the side, and the gap between
// them at every command against the tolerance.
//
// The players' moves on both servers are a function of the commands alone, so wall time and the
// order packets met the server's frames do not move the result; what can is a server setting the
// two read differently (the jump's rules, friction at an edge, the step up), which is what the lab
// is for. Without `--server` (the smoke run) it reports "not run" with the commands a session
// runs, so the lab stays deterministic in CI.
//
// ge:summary Lab: one scripted player on an existing server and on ours, the paths compared
// ge:keywords lab se-parity parity existing-server movement player-move tolerance parity-hall
// ge:keywords live-server not-run scripted-commands
// ge:skill shooter labs
// ge:test tests/shooters_edge_classic_server_test.cpp
#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/address.hpp"
#include "net/transport.hpp"
#include "shooter/pmove.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/net/classic_client.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace sh = ge::shooter;
namespace sn = shooters_edge::net;
using ge::core::num::to_fixed;

constexpr int kIdle = 77;    // commands standing still first (a second): the player settles
constexpr int kSteps = 700;  // the scripted commands after the idle, 13 ms each

// The script: what the player does at command `k` (from 0, after the idle).
sh::MoveCmd scripted(int k) {
  sh::MoveCmd m;
  m.msec = 13;
  auto jump_every = [&](int period) {
    if (k % period < 3) m.buttons |= sh::kButtonJump;
  };
  if (k < 0) return m;  // the idle
  if (k < 300) {        // the run down the hall: the stairs, the ledge, the ramp
    m.forward = 400;
  } else if (k < 430) {  // jumps and strafes, the view swinging, toward the pillar
    m.forward = 400;
    m.side = static_cast<int16_t>((k / 40) % 2 == 0 ? 350 : -350);
    m.angles.y = static_cast<float>((k / 40) % 2 == 0 ? 15 : -15);
    jump_every(40);
  } else if (k < 540) {  // turned back, jumping
    m.angles.y = 180;
    m.forward = 400;
    jump_every(30);
  } else if (k < 610) {  // nothing pressed: friction stops the player
    m.angles.y = 180;
  } else {  // into the north wall and along it
    m.angles.y = 90;
    m.forward = 400;
    m.side = 200;
  }
  return m;
}

struct Sample {
  sh::Vec3f origin, velocity;
  bool onground = false;
};

struct Track {
  std::string server;
  std::string error;  // why it did not play ("" when it did)
  std::string level;
  std::map<int, Sample> by_step;  // the record each scripted command was answered with
  int frames = 0;
};

Track play(const gn::Address& to, const std::string& text, const std::string& basedir,
           double most_seconds) {
  Track out;
  out.server = text;
  gn::Transport t;
  std::string error;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), &error)) {
    out.error = "the UDP socket did not open: " + error;
    return out;
  }
  sn::ClassicClientEnd c;
  sn::ClientConfig cfg;
  cfg.name = "parity";
  cfg.topcolor = 4;
  cfg.bottomcolor = 4;
  auto files = std::make_shared<shooters_edge::client::SearchPath>();
  auto mounted = std::make_shared<std::string>();
  if (!basedir.empty())
    // NOLINTNEXTLINE(bugprone-exception-escape): what the callback throws reaches its caller
    cfg.read_file = [basedir, files, mounted](const std::string& gamedir, const std::string& path) {
      std::vector<uint8_t> store;
      if (*mounted != gamedir) {
        std::string why;
        *files = {};
        if (!shooters_edge::client::open_game_dir(basedir, gamedir, *files, why)) return store;
        *mounted = gamedir;
      }
      const std::span<const uint8_t> got = shooters_edge::client::find_file(*files, path, store);
      return std::vector<uint8_t>(got.begin(), got.end());
    };
  int moves = 0;
  std::map<uint32_t, int> step_of;  // a packet's sequence to the scripted command it carried
  cfg.brain = [&c, &moves, &step_of](int) {
    const int step = moves++ - kIdle;
    step_of[c.ch.outgoing_sequence] = step;
    return scripted(step);
  };
  sn::client_start(c, t, to, cfg, 0);
  const auto start = std::chrono::steady_clock::now();
  uint32_t last_ack = 0;
  int64_t now_us = 0;
  while (now_us < static_cast<int64_t>(most_seconds * 1e6)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(13));
    now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                 std::chrono::steady_clock::now() - start)
                 .count();
    sn::client_tick(c, now_us);
    if (c.stage == sn::ClientStage::dropped) {
      out.error = "dropped: " + (c.refused.empty() ? c.error : c.refused);
      break;
    }
    if (c.stage != sn::ClientStage::active || c.playernum < 0 || c.playernum >= 32) continue;
    const uint32_t ack = c.ch.incoming_acknowledged;
    if (ack == last_ack) continue;
    last_ack = ack;
    out.frames++;
    const auto at = step_of.find(ack);
    if (at == step_of.end() || at->second < 0) continue;
    const sn::PlayerState& p = c.players[c.playernum];
    out.by_step[at->second] = {p.origin, p.velocity, p.onground};
    if (at->second >= kSteps - 1) break;
  }
  out.level = c.level;
  if (out.error.empty() && out.by_step.empty())
    out.error = "never played a scripted command (stage " +
                std::string(sn::client_stage_name(c.stage)) + ")";
  return out;
}

float dist(const sh::Vec3f& a, const sh::Vec3f& b) {
  const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void not_run(gl::Lab& lab) {
  gl::LabTable steps{"what the lab runs (not run: no --server given)", {"step", "command"}, {}};
  steps.rows.push_back({"1. the parity hall built",
                        "secli map build games/shooters_edge/content/maps/parity.plan "
                        "--textures=<the game folder>; put parity.bsp in its maps"});
  steps.rows.push_back({"2. an existing server beside the checkout",
                        "<server> -port 27500 +map parity (with the mod the match uses)"});
  steps.rows.push_back({"3. our server on the same mod and map",
                        "sesv --root=<dir> --mod=<mod> --map=parity "
                        "--port=27501 --realtime --for=60"});
  steps.rows.push_back({"4. the same commands on both, compared",
                        "make lab LAB=se-parity ARGS=\"--server=127.0.0.1:27500 "
                        "--ours=127.0.0.1:27501 --basedir=<dir>\""});
  lab.table(steps);
  lab.metric("parity.run", 0, "bool");
  lab.note("se-parity: not run. Give --server=<host:port> of an existing server and --ours.");
}

bool address(gl::Lab& lab, const std::string& text, gn::Address& to, std::string& shown) {
  shown = text.find(':') == std::string::npos ? text + ":27500" : text;
  if (gn::parse_address(shown, to)) return true;
  lab.fail("se-parity: '" + text + "' is not host:port", "next: --server=127.0.0.1:27500");
  return false;
}

void shoot(gl::Lab& lab, const std::string& id, const gl::LineChart& spec,
           const std::string& caption, const std::string& look_for,
           const std::vector<std::pair<std::string, std::string>>& in) {
  const gl::Chart chart = gl::line_chart(spec);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, in}, gl::to_picture(chart.plot), gl::Det::measured);
}

void run(gl::Lab& lab) {
  if (lab.arg("server").empty()) {
    not_run(lab);
    return;
  }
  gn::Address existing_at, ours_at;
  std::string existing_text, ours_text;
  if (!address(lab, lab.arg("server"), existing_at, existing_text)) return;
  if (lab.arg("ours").empty()) {
    lab.fail("se-parity: no --ours given beside --server",
             "next: start sesv on the same mod and map and pass --ours=127.0.0.1:<its port>");
    return;
  }
  if (!address(lab, lab.arg("ours"), ours_at, ours_text)) return;
  const std::string basedir = lab.arg("basedir");
  const double seconds = lab.arg_num("seconds");
  const float pos_tol = static_cast<float>(lab.arg_num("tolerance"));
  const float vel_tol = static_cast<float>(lab.arg_num("velocity-tolerance"));

  const Track a = play(existing_at, existing_text, basedir, seconds);
  const Track b = play(ours_at, ours_text, basedir, seconds);
  const std::vector<std::pair<std::string, std::string>> in = {
      {"server", existing_text}, {"ours", ours_text}, {"map", a.level}};

  gl::LabTable runs{
      "the two runs", {"server", "level", "frames", "commands answered", "error"}, {}};
  for (const Track* t : {&a, &b})
    runs.rows.push_back({t->server, t->level, std::to_string(t->frames),
                         std::to_string(t->by_step.size()), t->error.empty() ? "-" : t->error});
  lab.table(runs, gl::Det::measured);
  lab.metric("parity.run", 1, "bool");
  if (!a.error.empty() || !b.error.empty()) {
    lab.fail("se-parity: a run did not play: " + (a.error.empty() ? b.error : a.error),
             "next: check both servers run the parity hall and the mod (the commands in the "
             "smoke run's table)");
    return;
  }

  // Command by command, where both answered it.
  int compared = 0, outside = 0, first_off = -1, ground_differs = 0;
  float worst_pos = 0, worst_vel = 0;
  int worst_at = -1;
  gl::Trace gap{"origin gap (units)", {}, {}}, vgap{"velocity gap (units/s, / 10)", {}, {}};
  gl::LabTable lines{"every 25th command and every one outside the tolerance",
                     {"command", "existing origin", "ours origin", "gap", "velocity gap",
                      "on ground (existing / ours)"},
                     {}};
  auto v3 = [](const sh::Vec3f& v) {
    return to_fixed(v.x, 1) + " " + to_fixed(v.y, 1) + " " + to_fixed(v.z, 1);
  };
  for (const auto& [k, sa] : a.by_step) {
    const auto it = b.by_step.find(k);
    if (it == b.by_step.end()) continue;
    const Sample& sb = it->second;
    compared++;
    const float dp = dist(sa.origin, sb.origin), dv = dist(sa.velocity, sb.velocity);
    gap.x.push_back(k);
    gap.y.push_back(dp);
    vgap.x.push_back(k);
    vgap.y.push_back(dv / 10);
    const bool off = dp > pos_tol || dv > vel_tol;
    if (sa.onground != sb.onground) ground_differs++;
    if (off) {
      outside++;
      if (first_off < 0) first_off = k;
    }
    if (dp > worst_pos) {
      worst_pos = dp;
      worst_at = k;
    }
    if (dv > worst_vel) worst_vel = dv;
    if (k % 25 == 0 || (off && outside <= 40))
      lines.rows.push_back(
          {std::to_string(k), v3(sa.origin), v3(sb.origin), to_fixed(dp, 2), to_fixed(dv, 1),
           std::string(sa.onground ? "yes" : "no") + " / " + (sb.onground ? "yes" : "no")});
  }
  lab.table(lines, gl::Det::measured);
  lab.metric("parity.compared", compared, "commands", gl::Det::measured);
  lab.metric("parity.outside", outside, "commands", gl::Det::measured);
  lab.metric("parity.first_off", first_off, "command", gl::Det::measured);
  lab.metric("parity.worst_origin_gap", worst_pos, "units", gl::Det::measured);
  lab.metric("parity.worst_origin_gap_at", worst_at, "command", gl::Det::measured);
  lab.metric("parity.worst_velocity_gap", worst_vel, "units/s", gl::Det::measured);
  lab.metric("parity.ground_differs", ground_differs, "commands", gl::Det::measured);

  auto path = [](const Track& t, const std::string& label, bool side) {
    gl::Trace tr{label, {}, {}};
    for (const auto& [k, s] : t.by_step) {
      tr.x.push_back(s.origin.x);
      tr.y.push_back(side ? s.origin.z : s.origin.y);
    }
    return tr;
  };
  gl::LineChart above;
  above.spikes = false;
  above.frame = {"THE PATHS FROM ABOVE", "the same commands on " + a.level, "x (units)",
                 "y (units)"};
  above.traces = {path(a, "existing server", false), path(b, "our server", false)};
  shoot(lab, "above", above,
        "The player's origin at every scripted command, from above, on the existing server and "
        "on ours: the run down the hall, the strafing jumps, the turn back and the run along the "
        "north wall.",
        "one line over the other the whole way; a line that parts names the command where the "
        "moves differ (the gap chart has its number)",
        in);
  gl::LineChart side;
  side.spikes = false;
  side.frame = {"THE PATHS FROM THE SIDE", "height against x", "x (units)", "z (units)"};
  side.traces = {path(a, "existing server", true), path(b, "our server", true)};
  shoot(lab, "side", side,
        "The player's height against x on both servers: the stairs, the ledge, the ramp down and "
        "each jump's arc.",
        "the same steps up the stairs and the same arcs; a jump that goes higher on one server is "
        "the jump's rules read differently",
        in);
  gl::LineChart gaps;
  gaps.spikes = false;
  gaps.frame = {
      "THE GAP AT EVERY COMMAND",
      "tolerance " + to_fixed(pos_tol, 2) + " units and " + to_fixed(vel_tol, 1) + " units/s",
      "scripted command (13 ms each)", "gap"};
  gaps.traces = {gap, vgap};
  gaps.rules = {pos_tol, vel_tol / 10};
  shoot(lab, "gap", gaps,
        "How far apart the two servers put the player after each scripted command: the origin's "
        "distance and the velocity's (a tenth, to share the axis), with the tolerance as dashed "
        "lines.",
        "both lines under their dashes all the way; the first command over it is where to look",
        in);
  if (outside > 0)
    lab.note("se-parity: " + std::to_string(outside) + " of " + std::to_string(compared) +
             " commands outside the tolerance, the first at command " + std::to_string(first_off) +
             "; the worst origin gap " + to_fixed(worst_pos, 2) + " units at command " +
             std::to_string(worst_at));
}

const gl::LabSpec kSpec{
    "se-parity",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/net and plugins/shooter's player move on our dedicated server, against "
    "an existing server of the classic protocol",
    "shooter",
    "Does one player, playing the same commands, move on our server as on an existing one? "
    "(--server and --ours; without them the lab reports not run.)",
    {{"server", "", "an existing classic server, as host:port (empty: not run)"},
     {"ours", "", "our server (sesv) on the same mod and map, as host:port"},
     {"basedir", "", "the game's base folder (its id1 and mod folders): the map's checksum"},
     {"seconds", "40", "wall seconds a run may take at most"},
     {"tolerance", "1", "the origin gap allowed, units (records are sent to 1/8 unit)"},
     {"velocity-tolerance", "4", "the velocity gap allowed, units/s (sent as whole units)"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
