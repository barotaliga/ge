// The server trial, defined: the dedicated, listen and networked runs, the LAN browser, the
// joining players.
//
// ge:summary The server trial's definitions: three runs, the browser, the joins, the fold
// ge:keywords server trial run_server_trial lan-browser joins fold
// ge:skill net
// ge:test tests/server_core_test.cpp
#include "server/trial.hpp"

#include <memory>

#include "core/num.hpp"
#include "net/discovery.hpp"
#include "net/sim_network.hpp"
#include "server/sample.hpp"

namespace ge::server {

namespace {

namespace gn = ge::net;

constexpr uint32_t kServerNode = 1, kBrowserNode = 2, kFirstClient = 10;
constexpr uint16_t kClientPort = 40000;

uint64_t fold(uint64_t h, uint64_t v) {
  for (int i = 0; i < 8; i++) h = (h ^ ((v >> (8 * i)) & 0xffu)) * 1099511628211ull;
  return h;
}

struct Player {
  gn::Transport t;
  gn::Node node;
  gn::SessionClient client;
  int64_t join_us = 0;
  bool joined = false;
  double seated_s = -1;
};

uint64_t run_offline(const ServerGame& game, const ServerConfig& c, int64_t frames,
                     std::string& error) {
  Server s;
  if (!server_open(s, game, c, nullptr, nullptr, error)) return 0;
  for (int64_t n = 0; n < frames; n++) server_frame(s);
  return server_digest(s);
}

// A client loop at `hz`, running the server frames due by each of its frames.
uint64_t run_listen(const ServerGame& game, const ServerConfig& c, int64_t frames, double hz,
                    int& client_frames, std::string& error) {
  Server s;
  if (!server_open(s, game, c, nullptr, nullptr, error)) return 0;
  client_frames = 0;
  while (s.frame < frames) {
    client_frames++;
    const auto now =
        static_cast<int64_t>(core::num::round(static_cast<double>(client_frames) / hz * 1e6));
    while (s.frame < frames && server_next_us(s) <= now) server_frame(s);
  }
  return server_digest(s);
}

}  // namespace

ServerTrial run_server_trial(const TrialConfig& tc) {
  ServerTrial out;
  const ServerGame& game = tc.game ? *tc.game : sample_game();
  ServerConfig c = tc.server;
  if (c.map.empty()) c.map = game.default_map;
  if (c.port == 0) c.port = game.default_port;
  c.offline = false;

  // The frame count, off the game's frame length.
  {
    Server probe;
    if (!server_open(probe, game, c, nullptr, nullptr, out.error)) return out;
    out.frames = static_cast<int64_t>(core::num::round(c.seconds / probe.frame_s));
  }
  out.dedicated = run_offline(game, c, out.frames, out.error);
  out.listen = run_listen(game, c, out.frames, tc.client_hz, out.listen_client_frames, out.error);
  if (!out.error.empty()) return out;

  // NETWORKED: the server, the LAN's browser and the joining players on one simulated network.
  gn::SimNetwork net = gn::make_sim_network(tc.seed, tc.link);
  gn::Transport sock, lan, browser;
  gn::open_sim_transport(sock, net, gn::sim_address(kServerNode, static_cast<uint16_t>(c.port)));
  gn::open_sim_transport(lan, net, gn::sim_address(kServerNode, game.discovery_port));
  gn::open_sim_transport(browser, net, gn::sim_address(kBrowserNode, kClientPort));
  Server s;
  if (!server_open(s, game, c, &sock, &lan, out.error)) return out;
  out.banner = server_banner(s);

  std::vector<std::unique_ptr<Player>> players;
  for (int i = 0; i < tc.clients; i++) {
    auto p = std::make_unique<Player>();
    const auto node = kFirstClient + static_cast<uint32_t>(i);
    gn::open_sim_transport(p->t, net, gn::sim_address(node, kClientPort));
    gn::NodeConfig nc;
    nc.protocol = game.protocol;
    nc.channels.count = 1;
    nc.channels.delivery[0] = gn::Delivery::reliable;
    nc.seed = uint64_t{tc.seed} * 1000 + node;
    gn::node_init(p->node, nc, p->t);
    gn::session_client_init(p->client, p->node, 0);
    p->join_us = 1500000 + int64_t{1000000} * i;
    players.push_back(std::move(p));
  }

  std::vector<gn::Found> found;
  uint64_t nonce = 0;
  int64_t next_probe_us = 1000000;
  for (int64_t n = 0; n < out.frames; n++) {
    const int64_t now = server_next_us(s);
    gn::sim_advance(net, now);
    server_frame(s);
    for (size_t i = 0; i < players.size(); i++) {
      Player& p = *players[i];
      if (!p.joined && now >= p.join_us) {
        const std::string name = "player " + std::to_string(i + 1);
        gn::session_client_join(p.client, sock.local, name.c_str(), 0, now);
        p.joined = true;
      }
      gn::node_update(p.node, now);
      gn::session_client_update(p.client, now);
      gn::node_flush(p.node, now);
      if (p.seated_s < 0 && p.client.state == gn::ClientState::seated)
        p.seated_s = static_cast<double>(now - p.join_us) / 1e6;
    }
    const int seats =
        (game.seated ? game.seated(s.state) : 0) + gn::session_player_count(s.session, false);
    const double t_s = static_cast<double>(now) / 1e6;
    if (out.seats_n.empty() || out.seats_n.back() != seats) {
      if (!out.seats_n.empty()) {
        out.seats_t.push_back(t_s);
        out.seats_n.push_back(out.seats_n.back());
      }
      out.seats_t.push_back(t_s);
      out.seats_n.push_back(seats);
    }
    found.clear();
    if (nonce && gn::lan_collect(browser, game.protocol, nonce, found, now) > 0) {
      for (const gn::Found& f : found) {
        TrialSighting sight;
        sight.at_s = static_cast<double>(now) / 1e6;
        sight.name = f.advert.name;
        sight.seated = f.advert.seated;
        sight.max_players = f.advert.max_players;
        sight.locked = (f.advert.flags & gn::kAdvertLocked) != 0;
        out.sightings.push_back(sight);
      }
    }
    if (now >= next_probe_us) {
      nonce = uint64_t{tc.seed} * 7777 + static_cast<uint64_t>(next_probe_us);
      gn::lan_probe(browser, gn::sim_broadcast(game.discovery_port), game.protocol, nonce, now);
      next_probe_us += 1000000;
    }
  }
  out.seats_t.push_back(static_cast<double>(s.now_us) / 1e6);
  out.seats_n.push_back(out.seats_n.empty() ? 0 : out.seats_n.back());
  out.networked = server_digest(s);
  out.status = server_status(s);
  out.clients_seated = gn::session_player_count(s.session, false);
  out.pings = s.hosting.stats.pings;
  out.lan_answers = s.hosting.stats.lan_answers;
  for (const auto& p : players) out.seated_at_s.push_back(p->seated_s);

  uint64_t h = fold(1469598103934665603ull, out.networked);
  for (const TrialSighting& x : out.sightings)
    h = fold(fold(h, static_cast<uint64_t>(x.at_s * 1e6)), static_cast<uint64_t>(x.seated));
  for (double x : out.seated_at_s) h = fold(h, static_cast<uint64_t>(x * 1e6 + 1e6));
  out.digest = fold(h, static_cast<uint64_t>(out.clients_seated));
  out.ok = out.dedicated == out.listen && out.listen == out.networked;
  if (!out.ok)
    out.error =
        "the runs disagree: dedicated, listen and networked digests differ; next: find "
        "what in the server frame reads more than the game's own state";
  return out;
}

}  // namespace ge::server
