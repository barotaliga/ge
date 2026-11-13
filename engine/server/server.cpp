// The server core, defined: the settings' cvars, opening a server (the game, then the node, the
// session host and the hosting round it), the frame, the status and banner lines.
//
// ge:summary The server core's definitions: cvars, open, the frame, close, status, banner
// ge:keywords server server_open server_frame server_close add_server_cvars status banner
// ge:skill net
// ge:test tests/server_core_test.cpp
#include "server/server.hpp"

#include <cstdio>
#include <limits>

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
  // Each setting's words (console/words.hpp): one sentence, then what each value does.
  auto bind = [&](std::string_view id, std::string_view fallback, uint32_t flags,
                  std::string_view help, CvarKind kind, double min, double max,
                  std::string_view values, auto* target) {
    cvars.bind({.name = id,
                .fallback = fallback,
                .flags = flags,
                .help = help,
                .kind = kind,
                .min = min,
                .max = max,
                .values = values},
               target);
  };
  constexpr double kAny = std::numeric_limits<double>::infinity();
  const std::string slots_values = "1.." + std::to_string(most) + ": that many players";
  const std::string bots_values =
      "0: no bots (the default); 1.." + std::to_string(most) + ": that many slots";
  bind("sv_hostname", name, info, "The server's name in the server browser and the lobby.",
       CvarKind::text, -kAny, kAny, "TEXT: the name shown", &c.hostname);
  bind("sv_description", about, info, "A line or two shown under the name in the browser.",
       CvarKind::text, -kAny, kAny, "\"\": none; TEXT: the line shown", &c.description);
  bind("net_ip", ip, latched, "The local address the server listens on.", CvarKind::text, -kAny,
       kAny, "\"\": every interface (the default); TEXT: that address only", &c.ip);
  bind("net_port", port, latched, "The UDP port the game listens on.", CvarKind::integer, 1, 65535,
       "1..65535: that port", &c.port);
  bind("sv_maxclients", clients, latched_info, "The most players the server seats at once.",
       CvarKind::integer, 1, static_cast<double>(most), slots_values, &c.maxclients);
  bind("sv_map", map, info, "The map the server starts on.", CvarKind::text, -kAny, kAny,
       "TEXT: the map's name", &c.map);
  bind("sv_bots", bots, latched, "How many player slots the game's bots fill at the start.",
       CvarKind::integer, 0, static_cast<double>(most), bots_values, &c.bots);
  bind("sv_lobby", lobby, latched_info,
       "Whether every map warms up in a lobby until everyone is ready.", CvarKind::integer, 0, 1,
       "0: the match starts at once (the default); 1: a warm-up until all say ready", &c.lobby);
  bind("sv_seed", seed, latched, "The seed every random choice of the run starts from.",
       CvarKind::integer, 0, 4294967295.0, "0..4294967295: that seed", &c.seed);
  bind("sv_password", password, cn::kServer, "The password a player must give to join.",
       CvarKind::text, -kAny, kAny, "\"\": none (the default); TEXT: that password", &c.password);
  bind("sv_visibility", visibility, cn::kServer, "Who can find the server.", CvarKind::text, -kAny,
       kAny,
       "lan: only the local network (the default); invite: those given its id; listed: everyone, "
       "in the public list",
       &c.visibility);
  bind("sv_motd", motd, cn::kServer, "The message a player sees on joining.", CvarKind::text, -kAny,
       kAny, "\"\": none; TEXT: the message", &c.motd);
  bind("sv_admin", admin, info, "How to reach the server's owner, shown in the server info.",
       CvarKind::text, -kAny, kAny, "\"\": not said; TEXT: a name, an address or a link", &c.admin);
  const std::string country = c.country, city = c.city, coords = c.coords, addr = c.public_addr;
  bind("sv_country", country, info, "The country the server is in, for browsers to show.",
       CvarKind::text, -kAny, kAny, "\"\": not said; TEXT: a two-letter country code (se)",
       &c.country);
  bind("sv_city", city, info, "The city the server is in, for browsers to show.", CvarKind::text,
       -kAny, kAny, "\"\": not said; TEXT: the city", &c.city);
  bind("sv_coords", coords, info, "Where the server is on the globe, for maps of servers.",
       CvarKind::text, -kAny, kAny, "\"\": not said; TEXT: \"latitude,longitude\" in degrees",
       &c.coords);
  bind("sv_publicaddr", addr, info, "The address a browser should show for this server.",
       CvarKind::text, -kAny, kAny,
       "\"\": the address it is reached at; TEXT: \"host:port\" to show instead", &c.public_addr);
  bind("sv_maprotation", rotation, info, "The maps played after the first, in turn.",
       CvarKind::text, -kAny, kAny,
       "\"\": the first map again; TEXT: map names with spaces between", &c.maprotation);
  bind("sv_maxspectators", spectators, latched_info,
       "How many spectator slots the server has beside the players'.", CvarKind::integer, 0,
       static_cast<double>(kMaxClients),
       "0: spectators take free player slots (the default); 1..32: that many of their own",
       &c.max_spectators);
  bind("sv_spectatorpassword", spec_password, cn::kServer,
       "The password a spectator must give to watch.", CvarKind::text, -kAny, kAny,
       "\"\": none (the default); TEXT: that password", &c.spectator_password);
  bind("rcon_password", rcon, cn::kServer,
       "The password that lets a remote console run server console lines from afar.",
       CvarKind::text, -kAny, kAny,
       "\"\": no remote console (the default); TEXT: that password, given as rcon <password> "
       "<line>",
       &c.rcon_password);
  bind("sv_rendezvous", rendezvous, latched,
       "The rendezvous server that lists this server and helps players reach it.", CvarKind::text,
       -kAny, kAny, "\"\": none (the default); TEXT: its host:port", &c.rendezvous);
  bind("sv_master", masters, latched, "The master servers this server reports itself to.",
       CvarKind::text, -kAny, kAny,
       "\"\": none, listed by no master (the default); TEXT: host:port addresses with spaces "
       "between (port 27900 when left out)",
       &c.masters);
  bind("sv_mod", mod, cn::kServer | cn::kServerInfo | cn::kReadOnly,
       "The mods' game directories laid over the base game, set by --mod.", CvarKind::text, -kAny,
       kAny, "\"\": the base game alone; TEXT: directories, lowest first, commas between", &c.mod);
  const std::string timeout = core::num::to_string(c.timeout_s);
  const std::string flood = c.floodprot, flood_msg = c.floodprot_msg;
  bind("sv_timeout", timeout, cn::kServer,
       "How long a client may go unheard before it is dropped, in seconds.", CvarKind::number, 1,
       3600, "1..3600: that many seconds", &c.timeout_s);
  const std::string countdown = core::num::to_string(c.countdown_s);
  const std::string intermission = core::num::to_string(c.intermission_s);
  bind("sv_countdown", countdown, cn::kServer,
       "With the lobby, the seconds between everyone being ready and the match.", CvarKind::number,
       0, 600, "0: the match starts at once; 1..600: that many seconds", &c.countdown_s);
  bind("sv_intermission", intermission, cn::kServer,
       "With the lobby, the seconds the scores stay up before the next map.", CvarKind::number, 0,
       600, "0: the next map at once; 1..600: that many seconds", &c.intermission_s);
  bind("sv_floodprot", flood, cn::kServer,
       "How much a player may chat before being silenced for a while.", CvarKind::text, -kAny, kAny,
       "N N N: N lines in any S seconds, else silent for T seconds (\"10 1 1\" the default)",
       &c.floodprot);
  bind("sv_floodprotmsg", flood_msg, cn::kServer, "What a player who floods the chat is told.",
       CvarKind::text, -kAny, kAny,
       "\"\": how long the silence lasts (the default); TEXT: that message instead",
       &c.floodprot_msg);
  const std::string vote = std::to_string(c.vote);
  const std::string vote_time = core::num::to_string(c.vote_time_s);
  const std::string vote_wait = core::num::to_string(c.vote_wait_s);
  const std::string vote_allow = c.vote_allow, vote_deny = c.vote_deny;
  bind("sv_vote", vote, cn::kServer, "Whether players may vote to change a setting for the map.",
       CvarKind::integer, 0, 1, "0: no votes; 1: vote <name> <value> calls one (the default)",
       &c.vote);
  bind("sv_vote_time", vote_time, cn::kServer, "How long a vote stays open, in seconds.",
       CvarKind::number, 5, 600, "5..600: that many seconds", &c.vote_time_s);
  bind("sv_vote_wait", vote_wait, cn::kServer,
       "How long a player whose vote failed waits before calling another, in seconds.",
       CvarKind::number, 0, 3600, "0: no wait; 1..3600: that many seconds", &c.vote_wait_s);
  bind("sv_vote_allow", vote_allow, cn::kServer, "The settings players may vote on.",
       CvarKind::text, -kAny, kAny,
       "\"\": all but sv_vote_deny's (the default); TEXT: names with spaces between, * for any "
       "characters",
       &c.vote_allow);
  bind("sv_vote_deny", vote_deny, cn::kServer, "The settings no vote may change.", CvarKind::text,
       -kAny, kAny, "TEXT: names with spaces between, * for any characters", &c.vote_deny);
  // The logins (login.hpp).
  const std::string login = std::to_string(c.login.login);
  const std::string login_time = core::num::to_string(c.login.time_s);
  const std::string login_name = std::to_string(c.login.name);
  const std::string welcome = c.login.welcome, master = c.login.master, key = c.login.key;
  const std::string reserved = std::to_string(c.login.reserved);
  const std::string account_port = std::to_string(c.login.port);
  bind("sv_login", login, latched, "Whether players must log in to an account to play.",
       CvarKind::integer, 0, 2,
       "0: nobody is asked, a login still counts (the default); 1: players must log in, "
       "spectators may watch as guests; 2: everyone must",
       &c.login.login);
  bind("sv_logintime", login_time, cn::kServer,
       "How long a player who must log in has to do it, in seconds.", CvarKind::number, 10, 3600,
       "10..3600: that many seconds", &c.login.time_s);
  bind("sv_loginname", login_name, cn::kServer,
       "Whether a logged-in player is shown by the account's name.", CvarKind::integer, 0, 1,
       "0: by the name the player picks; 1: by the account's, and renaming is refused (the "
       "default)",
       &c.login.name);
  bind("sv_loginwelcome", welcome, cn::kServer,
       "A line shown before the login prompt, such as where to get a login code.", CvarKind::text,
       -kAny, kAny, "\"\": none (the default); TEXT: that line", &c.login.welcome);
  bind("sv_accountmaster", master, latched,
       "The account master the server's logins are checked by.", CvarKind::text, -kAny, kAny,
       "\"\": the server is its own master, its accounts kept in its game folder (the default); "
       "TEXT: the master's host[:port]",
       &c.login.master);
  bind("sv_accountkey", key, latched, "The account master's public key, which signs its tickets.",
       CvarKind::text, -kAny, kAny,
       "\"\": learned from the master at the first answer and printed; TEXT: 64 hex digits",
       &c.login.key);
  bind("sv_reservednames", reserved, cn::kServer,
       "Whether guests may not use a registered player's name.", CvarKind::integer, 0, 1,
       "0: any name; 1: a registered name is the account's alone (the default)", &c.login.reserved);
  bind("sv_accountport", account_port, latched,
       "The UDP port the server's own account service answers on, when it is its own master.",
       CvarKind::integer, 0, 65535, "0: 27901 (the default); 1..65535: that port", &c.login.port);
  for (int i = 0; i < game.rule_count; i++) {
    const GameRule& r = game.rules[i];
    std::string& field = c.rules[r.name];  // a map's node: its address outlives the inserts
    if (field.empty() && r.fallback != nullptr) field = r.fallback;
    const std::string fallback = field;
    cvars.bind({.name = r.name,
                .fallback = fallback,
                .flags = info,
                .help = r.help,
                .kind = CvarKind::text,
                .values = r.values != nullptr ? r.values : ""},
               &field);
  }
  for (int i = 0; i < game.name_count; i++) {
    const SettingName& n = game.names[i];
    if (!cvars.add_synonym(n.name, n.setting, n.part))
      std::fprintf(stderr,
                   "error: the game's name %s for %s is refused: it is a setting of its own, or "
                   "there is no such setting. Fix its row in the game's ServerGame::names.\n",
                   n.name, n.setting);
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
            " is built wrong: its ServerGame lacks create, frame, frame_seconds or hash";
    return false;
  }
  const int most = game.max_clients < kMaxClients ? game.max_clients : kMaxClients;
  if (config.maxclients < 1 || config.maxclients > most) {
    error = "sv_maxclients " + std::to_string(config.maxclients) + " is outside 1.." +
            std::to_string(most);
    return false;
  }
  if (config.bots < 0 || config.bots > config.maxclients) {
    error = std::to_string(config.bots) + " bots do not fit " + std::to_string(config.maxclients) +
            " slots: raise sv_maxclients or lower sv_bots";
    return false;
  }
  FloodRule flood;
  if (!parse_flood_rule(config.floodprot, flood, error)) return false;
  gn::Visibility vis{};
  if (!gn::visibility_of(config.visibility, vis)) {
    error = "sv_visibility '" + config.visibility + "' must be lan, invite or listed";
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
              " is built wrong: its ServerGame serves its own protocol without net_frame";
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
