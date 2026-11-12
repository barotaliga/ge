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
#include "core/num_text.hpp"
#include "net/packet.hpp"
#include "server/guards.hpp"

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

// The node's foreign datagrams this frame: status asks answered, the masters' acks taken.
void answer_public(Server& s) {
  bool asked = false;
  gn::Status status;
  uint8_t out[gn::kMaxDatagram];
  for (const gn::Foreign& f : s.node.foreign) {
    const uint8_t* data = s.node.foreign_bytes.data() + f.offset;
    if (gn::master_beat_read(s.beat, data, f.size, f.from, s.now_us)) continue;
    if (!gn::is_status_ask(data, f.size)) continue;
    if (!asked) {
      status = server_public_status(s);
      asked = true;
    }
    const size_t n = gn::status_answer(data, f.size, f.from, s.game->protocol, s.node.secret,
                                       s.now_us, status, out, sizeof out);
    if (n) {
      gn::transport_send(*s.port, f.from, out, n, s.now_us);
      s.status_answers++;
    }
  }
}

// A datagram on an own-protocol game's socket offered first to the core (Transport::on_public):
// a master's ack taken, a hub's status ask answered; anything else left to the game.
bool own_public(void* ctx, const uint8_t* data, size_t size, const gn::Address& from,
                int64_t now_us) {
  Server& s = *static_cast<Server*>(ctx);
  if (gn::master_beat_read(s.beat, data, size, from, now_us)) return true;
  if (!gn::is_status_ask(data, size)) return false;
  uint8_t out[gn::kMaxDatagram];
  const size_t n = gn::status_answer(data, size, from, s.game->protocol, s.public_secret, now_us,
                                     server_public_status(s), out, sizeof out);
  if (n) {
    gn::transport_send(*s.port, from, out, n, now_us);
    s.status_answers++;
  }
  return true;
}

// The beat to sv_master's masters; false with `error` when one is not an address.
bool start_beat(Server& s, const ServerGame& game, const ServerConfig& config, std::string& error) {
  gn::MasterBeatConfig bc;
  if (!gn::master_list(config.masters, bc.masters, error)) {
    error = "sv_master: " + error;
    return false;
  }
  bc.protocol = game.protocol;
  bc.kind = gn::NodeKind::game;
  bc.version = game.version;
  bc.seed = uint64_t{config.seed} * 6151u + 5u;
  gn::master_beat_init(s.beat, bc);
  return true;
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
  const std::string lobby = std::to_string(c.lobby);
  const std::string name = c.hostname, about = c.description, ip = c.ip, map = c.map;
  const std::string password = c.password, visibility = c.visibility;
  const std::string motd = c.motd, admin = c.admin, rotation = c.maprotation;
  const std::string spectators = std::to_string(c.max_spectators);
  const std::string spec_password = c.spectator_password, rendezvous = c.rendezvous, mod = c.mod;
  const std::string masters = c.masters;
  const std::string rcon = c.rcon_password;
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
  cvars.bind({"sv_lobby", lobby, latched_info,
              "1: every map warms up until everyone is ready (ready, break, team, start)",
              CvarKind::integer, 0, 1},
             &c.lobby);
  cvars.bind({"sv_seed", seed, latched, "the run's seed", CvarKind::integer, 0, 4294967295.0},
             &c.seed);
  cvars.bind(
      {"sv_password", password, cn::kServer, "a password to join (empty: none)", CvarKind::text},
      &c.password);
  cvars.bind({"sv_visibility", visibility, cn::kServer,
              "lan (the LAN only), invite (by its id) or listed (public)", CvarKind::text},
             &c.visibility);
  cvars.bind({"sv_motd", motd, cn::kServer, "the message a player sees on joining", CvarKind::text},
             &c.motd);
  cvars.bind({"sv_admin", admin, info, "how to reach the server's owner", CvarKind::text},
             &c.admin);
  cvars.bind({"sv_maprotation", rotation, info, "the maps after the first, space-separated",
              CvarKind::text},
             &c.maprotation);
  cvars.bind({"sv_maxspectators", spectators, latched_info, "spectators beside the players",
              CvarKind::integer, 0, static_cast<double>(kMaxClients)},
             &c.max_spectators);
  cvars.bind({"sv_spectatorpassword", spec_password, cn::kServer,
              "a password to watch (empty: none)", CvarKind::text},
             &c.spectator_password);
  cvars.bind({"rcon_password", rcon, cn::kServer,
              "the remote console's password: `rcon <password> <line>` runs a server console line "
              "from afar (empty: no remote console)",
              CvarKind::text},
             &c.rcon_password);
  cvars.bind({"sv_rendezvous", rendezvous, latched, "the rendezvous to list with (empty: none)",
              CvarKind::text},
             &c.rendezvous);
  cvars.bind({"sv_master", masters, latched,
              "the masters to beat to, space-separated host:port (port 27900 when unsaid; empty: "
              "listed by none)",
              CvarKind::text},
             &c.masters);
  cvars.bind({"sv_mod", mod, cn::kServer | cn::kServerInfo | cn::kReadOnly,
              "the mods' directories over the base, lowest first (--mod)", CvarKind::text},
             &c.mod);
  const std::string timeout = core::num::to_string(c.timeout_s);
  const std::string flood = c.floodprot, flood_msg = c.floodprot_msg;
  cvars.bind({"sv_timeout", timeout, cn::kServer,
              "seconds a client may go unheard before it is dropped", CvarKind::number, 1, 3600},
             &c.timeout_s);
  const std::string countdown = core::num::to_string(c.countdown_s);
  const std::string intermission = core::num::to_string(c.intermission_s);
  cvars.bind(
      {"sv_countdown", countdown, cn::kServer,
       "with sv_lobby: seconds between everyone ready and the match", CvarKind::number, 0, 600},
      &c.countdown_s);
  cvars.bind({"sv_intermission", intermission, cn::kServer,
              "with sv_lobby: seconds the scores stay up after a match before the next map",
              CvarKind::number, 0, 600},
             &c.intermission_s);
  cvars.bind({"sv_floodprot", flood, cn::kServer,
              "\"N S T\": N chat lines in S seconds, else silent for T seconds", CvarKind::text},
             &c.floodprot);
  cvars.bind({"sv_floodprotmsg", flood_msg, cn::kServer,
              "what a player who floods the chat is told (empty: how long the silence is)",
              CvarKind::text},
             &c.floodprot_msg);
  const std::string vote = std::to_string(c.vote);
  const std::string vote_time = core::num::to_string(c.vote_time_s);
  const std::string vote_wait = core::num::to_string(c.vote_wait_s);
  const std::string vote_allow = c.vote_allow, vote_deny = c.vote_deny;
  cvars.bind({"sv_vote", vote, cn::kServer,
              "1: players may vote on settings for the map (vote <name> <value>)",
              CvarKind::integer, 0, 1},
             &c.vote);
  cvars.bind({"sv_vote_time", vote_time, cn::kServer, "seconds a vote stays open", CvarKind::number,
              5, 600},
             &c.vote_time_s);
  cvars.bind({"sv_vote_wait", vote_wait, cn::kServer,
              "seconds a player whose vote failed waits before calling another", CvarKind::number,
              0, 3600},
             &c.vote_wait_s);
  cvars.bind({"sv_vote_allow", vote_allow, cn::kServer,
              "settings players may vote on, * for any characters (empty: all but sv_vote_deny)",
              CvarKind::text},
             &c.vote_allow);
  cvars.bind({"sv_vote_deny", vote_deny, cn::kServer,
              "settings no vote may change, * for any characters", CvarKind::text},
             &c.vote_deny);
  for (int i = 0; i < game.rule_count; i++) {
    const GameRule& r = game.rules[i];
    std::string& field = c.rules[r.name];  // a map's node: its address outlives the inserts
    const std::string fallback = field;
    cvars.bind({r.name, fallback, info, r.help, CvarKind::text}, &field);
  }
}

VoteSettings vote_settings(const ServerConfig& c) {
  VoteSettings v;
  v.enabled = c.vote != 0;
  v.time_s = c.vote_time_s;
  v.wait_s = c.vote_wait_s;
  v.allow = c.vote_allow;
  v.deny = c.vote_deny;
  return v;
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
  FloodRule flood;
  if (!parse_flood_rule(config.floodprot, flood, error)) return false;
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
  if (transport == nullptr) return true;
  if (game.serve) {
    if (!game.net_frame) {
      error = std::string(game.binary) +
              "'s ServerGame serves its own protocol without net_frame; next: fill it "
              "(engine/server/server.hpp)";
      server_close(s);
      return false;
    }
    if (!start_beat(s, game, config, error) || !game.serve(s.state, *transport, error)) {
      server_close(s);
      return false;
    }
    s.networked = s.own_protocol = true;
    s.port = transport;
    // Only with masters to beat to: the hook and the bye touch the socket, which must then outlive
    // the server (a dedicated process's does; a listen game may free its socket first).
    if (!s.beat.config.masters.empty()) {
      s.public_secret = (uint64_t{config.seed} + 0x51a7u) * 0x9e3779b97f4a7c15ull;
      transport->on_public = own_public;
      transport->public_ctx = &s;
    }
    return true;
  }
  s.networked = true;
  s.port = transport;
  if (!start_beat(s, game, config, error)) {
    server_close(s);
    return false;
  }

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
  if (s.own_protocol) {
    s.game->net_frame(s.state, s.now_us);
    if (!s.beat.config.masters.empty()) gn::master_beat_update(s.beat, *s.port, s.now_us);
    s.now_us = server_next_us(s);
    s.frame++;
    return;
  }
  s.game->frame(s.state);
  s.now_us = server_next_us(s);
  s.frame++;
  if (!s.networked) return;
  gn::node_update(s.node, s.now_us);
  answer_public(s);
  gn::session_host_update(s.session, s.now_us);
  gn::hosting_update(s.hosting, s.now_us);
  gn::master_beat_update(s.beat, *s.port, s.now_us);
  gn::node_flush(s.node, s.now_us);
}

void server_close(Server& s) {
  if (s.networked && !s.own_protocol) {
    gn::hosting_stop(s.hosting, s.now_us);
    gn::master_beat_stop(s.beat, *s.port, s.now_us);
    gn::node_flush(s.node, s.now_us);
  }
  if (s.networked && s.own_protocol && !s.beat.config.masters.empty()) {
    gn::master_beat_stop(s.beat, *s.port, s.now_us);
    s.port->on_public = nullptr;
    s.port->public_ctx = nullptr;
  }
  s.networked = false;
  s.own_protocol = false;
  s.port = nullptr;
  if (s.state && s.game && s.game->destroy) s.game->destroy(s.state);
  s.state = nullptr;
}

gn::Status server_public_status(const Server& s) {
  gn::Status st;
  st.kind = gn::NodeKind::game;
  st.protocol = s.game ? s.game->protocol : 0;
  st.version = s.game ? s.game->version : 0;
  st.name = s.config.hostname;
  st.map = s.config.map;
  st.max_players = s.config.maxclients;
  st.max_spectators = s.config.max_spectators;
  st.locked = !s.config.password.empty();
  st.phase = s.own_protocol ? gn::StatusPhase::playing  // the game's fill_status may say more
             : s.session.phase == gn::SessionPhase::playing ? gn::StatusPhase::playing
                                                            : gn::StatusPhase::lobby;
  if (s.game && s.game->fill_status && s.state) s.game->fill_status(s.state, st);
  return st;
}

uint64_t server_digest(const Server& s) { return s.state && s.game ? s.game->hash(s.state) : 0; }

double server_time(const Server& s) { return static_cast<double>(s.now_us) / 1e6; }

int64_t server_next_us(const Server& s) {
  return static_cast<int64_t>(core::num::round(static_cast<double>(s.frame + 1) * s.frame_s * 1e6));
}

std::string server_status(const Server& s) {
  char head[96];
  std::snprintf(head, sizeof head, "t=%.1f s frame=%lld seated=%d", server_time(s),
                static_cast<long long>(s.frame), s.game && s.game->seated ? game_seated(&s) : 0);
  std::string out = head;
  // The core's own session's players (a game on its own protocol counts its clients itself).
  if (s.networked && !s.own_protocol)
    out += " clients=" + std::to_string(gn::session_player_count(s.session, false));
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
  if (s.own_protocol)
    out += ", on " + gn::to_string(s.port->local) + " (the game's own protocol)";
  else if (s.networked)
    out += ", on " + gn::to_string(s.node.transport->local) + " (" + c.visibility + ")";
  else
    out += ", offline";
  return out;
}

}  // namespace ge::server
