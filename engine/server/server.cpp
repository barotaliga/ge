// The server core, defined: the settings' cvars, opening a server (the game, then the node, the
// session host and the hosting round it), the frame, the status and banner lines.
//
// ge:summary The server core's definitions: cvars, open, the frame, close, status, banner
// ge:keywords server server_open server_frame server_close add_server_cvars status banner
// ge:skill net
// ge:test tests/server_core_test.cpp
#include "server/server.hpp"

#include <cstdio>

#include "core/num.hpp"
#include "net/packet.hpp"

namespace ge::server {

namespace {

namespace cn = ge::console;
namespace gn = ge::net;

// The session's channel: the node's one reliable channel, the session's alone.
gn::NodeConfig node_config(const ServerGame& game, const ServerConfig& c) {
  gn::NodeConfig nc;
  nc.protocol = game.protocol;
  nc.max_peers = c.maxclients;
  nc.accept = true;
  nc.channels.count = 1;
  nc.channels.delivery[0] = gn::Delivery::reliable;
  nc.keep_foreign = true;  // the LAN's pings arrive on the node's socket
  nc.seed = uint64_t{c.seed} * 1000003u + 17u;
  return nc;
}

int game_seated(const void* ctx) {
  const auto* s = static_cast<const Server*>(ctx);
  return s->game->seated ? s->game->seated(s->state) : 0;
}

}  // namespace

Server::~Server() { server_close(*this); }

void add_server_cvars(cn::Cvars& cvars, ServerConfig& c, const ServerGame& game) {
  using cn::CvarKind;
  const uint32_t info = cn::kServer | cn::kServerInfo;
  const uint32_t latched_info = info | cn::kLatch;
  const uint32_t latched = cn::kServer | cn::kLatch;
  const int most = game.max_clients < kMaxClients ? game.max_clients : kMaxClients;
  if (c.map.empty()) c.map = game.default_map;
  if (c.port == 0) c.port = game.default_port;
  // The fallbacks copied first: binding writes each into the field it is read from.
  const std::string port = std::to_string(c.port), clients = std::to_string(c.maxclients);
  const std::string bots = std::to_string(c.bots), seed = std::to_string(c.seed);
  const std::string name = c.hostname, about = c.description, ip = c.ip, map = c.map;
  const std::string password = c.password, visibility = c.visibility;
  cvars.bind(
      {"sv_hostname", name, info, "the server's name in the browser and the lobby", CvarKind::text},
      &c.hostname);
  cvars.bind({"sv_description", about, info, "a line or two under the name in the browser",
              CvarKind::text},
             &c.description);
  cvars.bind(
      {"net_ip", ip, latched, "the address to listen on (empty: every interface)", CvarKind::text},
      &c.ip);
  cvars.bind({"net_port", port, latched, "the game's UDP port", CvarKind::integer, 1, 65535},
             &c.port);
  cvars.bind({"sv_maxclients", clients, latched_info, "player slots", CvarKind::integer, 1,
              static_cast<double>(most)},
             &c.maxclients);
  cvars.bind({"sv_map", map, info, "the map the server starts on", CvarKind::text}, &c.map);
  cvars.bind({"sv_bots", bots, latched, "slots the game's bots fill at the start",
              CvarKind::integer, 0, static_cast<double>(most)},
             &c.bots);
  cvars.bind({"sv_seed", seed, latched, "the run's seed", CvarKind::integer, 0, 4294967295.0},
             &c.seed);
  cvars.bind(
      {"sv_password", password, cn::kServer, "a password to join (empty: none)", CvarKind::text},
      &c.password);
  cvars.bind({"sv_visibility", visibility, cn::kServer,
              "lan (the LAN only), invite (by its id) or listed (public)", CvarKind::text},
             &c.visibility);
}

bool server_open(Server& s, const ServerGame& game, const ServerConfig& config,
                 gn::Transport* transport, gn::Transport* lan, std::string& error) {
  server_close(s);
  if (!game.create || !game.frame || !game.frame_seconds || !game.hash) {
    error = std::string(game.binary) +
            "'s ServerGame lacks create, frame, frame_seconds or hash; next: fill them "
            "(engine/server/server.hpp)";
    return false;
  }
  const int most = game.max_clients < kMaxClients ? game.max_clients : kMaxClients;
  if (config.maxclients < 1 || config.maxclients > most) {
    error = "sv_maxclients " + std::to_string(config.maxclients) + " is outside 1.." +
            std::to_string(most) + "; next: set it in range";
    return false;
  }
  if (config.bots < 0 || config.bots > config.maxclients) {
    error = std::to_string(config.bots) + " bots do not fit " + std::to_string(config.maxclients) +
            " slots; next: raise sv_maxclients or lower sv_bots";
    return false;
  }
  gn::Visibility vis{};
  if (!gn::visibility_of(config.visibility, vis)) {
    error =
        "sv_visibility '" + config.visibility + "' is not lan, invite or listed; next: pick one";
    return false;
  }
  s.game = &game;
  s.config = config;
  s.state = game.create(config, error);
  if (!s.state) return false;
  s.frame_s = game.frame_seconds(s.state);
  s.frame = 0;
  s.now_us = 0;
  s.networked = transport != nullptr;
  if (!s.networked) return true;

  gn::node_init(s.node, node_config(game, config), *transport);
  gn::SessionConfig sc;
  sc.max_players = config.maxclients;
  sc.need_ready = false;
  sc.host_plays = false;
  sc.seed = uint64_t{config.seed} * 7919u + 3u;
  gn::session_host_init(s.session, sc, s.node, 0);
  gn::HostingConfig hc;
  hc.protocol = game.protocol;
  hc.name = config.hostname;
  hc.visibility = vis;
  hc.locked = !config.password.empty();
  hc.open_port = false;  // the router's forward comes with the rendezvous (M53d)
  hc.seed = uint64_t{config.seed} * 104729u + 11u;
  hc.seated = game_seated;
  hc.seated_ctx = &s;
  gn::hosting_start(s.hosting, hc, s.node, s.session, lan, nullptr, 0);
  return true;
}

void server_frame(Server& s) {
  if (!s.state) return;
  s.game->frame(s.state);
  s.now_us = server_next_us(s);
  s.frame++;
  if (!s.networked) return;
  gn::node_update(s.node, s.now_us);
  gn::session_host_update(s.session, s.now_us);
  gn::hosting_update(s.hosting, s.now_us);
  gn::node_flush(s.node, s.now_us);
}

void server_close(Server& s) {
  if (s.networked) {
    gn::hosting_stop(s.hosting, s.now_us);
    gn::node_flush(s.node, s.now_us);
    s.networked = false;
  }
  if (s.state && s.game && s.game->destroy) s.game->destroy(s.state);
  s.state = nullptr;
}

uint64_t server_digest(const Server& s) { return s.state && s.game ? s.game->hash(s.state) : 0; }

double server_time(const Server& s) { return static_cast<double>(s.now_us) / 1e6; }

int64_t server_next_us(const Server& s) {
  return static_cast<int64_t>(core::num::round(static_cast<double>(s.frame + 1) * s.frame_s * 1e6));
}

std::string server_status(const Server& s) {
  char head[96];
  const int clients = s.networked ? gn::session_player_count(s.session, false) : 0;
  std::snprintf(head, sizeof head, "t=%.1f s frame=%lld seated=%d clients=%d", server_time(s),
                static_cast<long long>(s.frame), s.game && s.game->seated ? game_seated(&s) : 0,
                clients);
  std::string out = head;
  if (s.game && s.game->status) out += " " + s.game->status(s.state);
  char hash[32];
  std::snprintf(hash, sizeof hash, " hash=%016llx",
                static_cast<unsigned long long>(server_digest(s)));
  return out + hash;
}

std::string server_banner(const Server& s) {
  const ServerConfig& c = s.config;
  std::string out = std::string(s.game ? s.game->binary : "server") + ": \"" + c.hostname + "\"";
  if (!c.description.empty()) out += " (" + c.description + ")";
  out += ", map " + c.map + ", " + std::to_string(c.bots) + " of " + std::to_string(c.maxclients) +
         " slots filled by bots, seed " + std::to_string(c.seed);
  if (s.networked)
    out += ", on " + gn::to_string(s.node.transport->local) + " (" + c.visibility + ")";
  else
    out += ", offline";
  return out;
}

}  // namespace ge::server
