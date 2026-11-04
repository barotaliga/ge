// LAB `se-dedicated`: Shooter's Edge's playable dedicated server (net/classic_game.hpp) on the
// engine's server core, as a classic client sees it. The core's server opens on the simulated
// network under each condition preset; `--clients` classic clients join a second apart, follow
// the signon and play the game's own deathmatch for `--seconds`. It answers whether every client
// gets in and how long each takes, whether the core counts them seated, and whether the run
// replays to the bit (each preset run twice). The seats over time are drawn as a chart.
//
// ge:summary Lab: Shooter's Edge's dedicated server on the core, classic clients joining
// ge:keywords lab se-dedicated dedicated-server classic-game classic-client join signon seats
// ge:keywords preset digest
// ge:skill net labs
// ge:test tests/shooters_edge_classic_game_test.cpp
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "core/num.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_game.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace se = shooters_edge::net;

constexpr uint16_t kPort = 27500, kClientPort = 27001;

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string part =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

std::string hex(uint64_t v) {
  char out[20];
  std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(v));
  return out;
}

struct Run {
  std::string error;
  int64_t frames = 0;
  std::vector<double> connect_s, active_s;  // each client's, from its join (-1: never)
  std::vector<double> seats_t, seats_n;
  int seated = 0;
  std::string status;
  uint64_t digest = 1469598103934665603ull;
};

Run run_once(const gn::LinkConditions& link, uint32_t seed, int clients, double seconds) {
  Run r;
  gn::SimNetwork net = gn::make_sim_network(seed, link);
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, kPort));
  gs::ServerConfig c;
  c.hostname = "Arena Night";
  c.map = "arena";
  c.maxclients = 8;
  c.seed = seed;
  gs::Server s;
  if (!gs::server_open(s, se::classic_game(), c, &port, nullptr, r.error)) return r;
  const auto n = static_cast<size_t>(clients);
  std::vector<std::unique_ptr<gn::Transport>> ct(n);
  std::vector<std::unique_ptr<se::ClassicClientEnd>> ends(n);
  std::vector<bool> started(n, false);
  for (size_t i = 0; i < n; i++) {
    ct[i] = std::make_unique<gn::Transport>();
    gn::open_sim_transport(*ct[i], net, gn::sim_address(static_cast<uint32_t>(i + 1), kClientPort));
    ends[i] = std::make_unique<se::ClassicClientEnd>();
  }
  const auto frames = static_cast<int64_t>(ge::core::num::round(seconds / s.frame_s));
  int last = -1;
  for (int64_t f = 0; f < frames; f++) {
    const int64_t now = s.now_us;
    gn::sim_advance(net, now);
    for (size_t i = 0; i < n; i++) {
      const int64_t join = static_cast<int64_t>(i + 1) * 1000000;
      if (now < join) continue;
      if (!started[i]) {
        started[i] = true;
        se::ClientConfig cc;
        cc.name = "player" + std::to_string(i + 1);
        cc.files = {"skins/base.pcx"};
        cc.seed = static_cast<int>(i) + 1;
        cc.qport = static_cast<uint16_t>(1000 + i);
        se::client_start(*ends[i], *ct[i], gn::sim_address(0, kPort), cc,
                         static_cast<double>(now) / 1e6);
      }
      se::client_tick(*ends[i], now);
    }
    gs::server_frame(s);
    r.digest = (r.digest ^ gs::server_digest(s)) * 1099511628211ull;
    const int seated = s.game->seated(s.state);
    if (seated != last) {
      if (last >= 0) {  // a step, not a ramp
        r.seats_t.push_back(gs::server_time(s));
        r.seats_n.push_back(last);
      }
      r.seats_t.push_back(gs::server_time(s));
      r.seats_n.push_back(seated);
      last = seated;
    }
  }
  r.seats_t.push_back(gs::server_time(s));
  r.seats_n.push_back(last);
  r.frames = frames;
  for (size_t i = 0; i < n; i++) {
    const double join = static_cast<double>(i + 1);
    const se::ClassicClientEnd& e = *ends[i];
    r.connect_s.push_back(e.connected_at < 0 ? -1 : e.connected_at - join);
    r.active_s.push_back(e.active_at < 0 ? -1 : e.active_at - join);
    r.digest = (r.digest ^ static_cast<uint64_t>(e.stage)) * 1099511628211ull;
  }
  r.seated = last;
  r.status = gs::server_status(s);
  return r;
}

std::string ms(double s) {
  return s < 0 ? "never" : std::to_string(static_cast<int>(ge::core::num::round(s * 1000)));
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const int clients = static_cast<int>(lab.arg_num("clients"));
  const double seconds = lab.arg_num("seconds");
  gl::LabTable joins{"each client, from its join: connected and playing (ms)",
                     {"preset", "client", "connected", "playing"},
                     {}};
  gl::LabTable runs{"each preset: the seats at the end, the run twice",
                    {"preset", "frames", "seated", "digest", "again", "agree"},
                    {}};
  gl::LineChart line;
  line.frame = {"PLAYERS ON THE DEDICATED SERVER", "seed " + lab.arg("seed"), "t (s)", "seated"};
  line.spikes = false;
  int disagree = 0, missing = 0;
  std::string status;
  for (const std::string& name : split(lab.arg("presets"))) {
    gn::LinkConditions link;
    if (!gn::conditions_preset(name, link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    const Run a = run_once(link, seed, clients, seconds);
    const Run b = run_once(link, seed, clients, seconds);
    if (!a.error.empty()) {
      lab.fail(name + ": " + a.error, "skill: net.");
      continue;
    }
    for (size_t i = 0; i < a.active_s.size(); i++) {
      joins.rows.push_back(
          {name, "player" + std::to_string(i + 1), ms(a.connect_s[i]), ms(a.active_s[i])});
      missing += a.active_s[i] < 0 ? 1 : 0;
    }
    const bool agree = a.digest == b.digest;
    disagree += agree ? 0 : 1;
    runs.rows.push_back({name, std::to_string(a.frames), std::to_string(a.seated), hex(a.digest),
                         hex(b.digest), agree ? "yes" : "NO"});
    line.traces.push_back({name, a.seats_t, a.seats_n});
    lab.metric(name + ".seated", a.seated, "players");
    if (status.empty()) status = a.status;
  }
  lab.metric("clients_never_playing", missing, "clients");
  lab.metric("disagreeing_presets", disagree, "presets");
  lab.table(std::move(joins));
  lab.table(std::move(runs));
  if (!status.empty()) lab.note("last status: " + status);
  if (line.traces.empty()) return;
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"seats",
            chart.title,
            "Shooter's Edge's dedicated server on the engine's server core, on the simulated "
            "network: a classic client joins every second, follows the signon and plays the "
            "game's own deathmatch. The seats the core counts over time, one line a preset.",
            "a step up a second for every client; the bad network's steps a little later; "
            "none missing.",
            {{"presets", lab.arg("presets")}, {"clients", lab.arg("clients")}}},
           gl::to_picture(chart.plot));
}

const gl::LabSpec kSpec{
    "se-dedicated",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_game (the dedicated server on the engine's core)",
    "net",
    "Does Shooter's Edge's dedicated server on the engine's core let every classic client in to "
    "play, count them seated, and replay to the bit under each network?",
    {
        {"presets", "lan,wifi,bad", "condition presets, comma-separated"},
        {"clients", "4", "classic clients joining, one a second"},
        {"seconds", "8", "server time each run, s"},
        {"seed", "1", "the network's and the game's stream"},
    },
    "--presets=lan,bad --seconds=5 --clients=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
