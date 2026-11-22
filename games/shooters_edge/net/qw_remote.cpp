// The window joined to a server (qw_remote.hpp).
//
// ge:summary The window joined to a server: the QuakeWorld client driven by hands, drawn as a demo
// ge:keywords remote_connect remote_step remote_disconnect remote_status
// ge:skill net shooter
#include "shooters_edge/net/qw_remote.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>

#include "net/crypto.hpp"
#include "net/net_commands.hpp"
#include "net/udp_socket.hpp"
#include "shooter/speedjump_assist.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/q1_remote.hpp"
#include "shooters_edge/net/q2_remote.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/net/qw_login.hpp"
#include "shooters_edge/net/qw_remote_macros.hpp"
#include "shooters_edge/net/qw_wire.hpp"

namespace shooters_edge::net {

namespace {

// The game's id in a presence (the game CLI's Spec::game), so a friend's list names it.
constexpr const char* kPresenceGame = "shooters_edge";

// The live recording's reader: a client recording with nothing read yet, as open_demo would set
// one up, its command records known to be the 24-byte struct this client writes.
void open_live(client::Demo& d) {
  d = client::Demo{};
  d.state.format = client::DemoFormat::client;
  d.state.protocol = 28;
  d.state.view_height = 22;
  d.state.players.resize(32);
  d.ring.resize(64);
  d.cmd_size = 24;
  d.named_effects = true;  // ids 120 and 121 the game's own (the server sends them only if agreed)
}

void start(Remote& r, const ClientConfig& cfg) {
  r.kept = false;
  ClientConfig c = cfg;
  c.brain = [&r](int) {
    sh::MoveCmd m = r.hands;
    m.angles = r.view;
    r.hands.impulse = 0;  // an impulse goes once
    // The easy jump: the client strafes for the player off its newest prediction
    // (shooter/speedjump_assist.hpp), at the share the server granted this player alone (his
    // own `*speedjump`, qw_admin.hpp), else its stated pm_speedjump, else kStockShare.
    if (r.end.cfg.speedjump && r.predicted) {
      const int me = r.end.playernum;
      std::string said = me >= 0 && me < 32 ? info_get(r.end.userinfos[me], "*speedjump") : "";
      if (said.empty()) said = info_get(r.end.serverinfo, "pm_speedjump");
      const float share = said.empty() ? kStockShare : static_cast<float>(std::atof(said.c_str()));
      m = sh::assisted_command(m, r.ahead, share, r.speedjump_view, r.jump_aid);
    }
    return m;
  };
  client_start(r.end, *r.transport, r.server, c, 0);
  r.recording = ClientRecorder{};
  record_start(r.recording, false);
  r.recording.live = true;  // read at once by the view (never written to a file)
  r.end.record = &r.recording;
  remote_record_restart(r);  // the level kept for `record` (qw_match_record.hpp)
  open_live(r.demo);
  r.active = true;
  r.now_us = 0;
  r.owed_s = 0;
  r.levels = 0;
  r.setangles = 0;
  r.said = ClientStage::challenge;
  r.why.clear();
  r.world = nullptr;
  r.smooth = sh::Smoothing{};
  r.predicted = false;
  r.replayed = 0;
  r.others = 0;
  r.others_msec = 0;
  r.lerped = 0;
  r.ents_old.clear();
  r.ents_new.clear();
  r.frame_old_s = r.frame_new_s = -1;
  if (r.login) {
    r.login->asked = false;
    r.login->done = false;
    r.login->presence_tried = false;
  }
}

int64_t unix_now(const RemoteLogin& l) { return l.unix_us != nullptr ? l.unix_us() : 0; }

void login_close(RemoteLogin& l) {
  if (l.opened) gn::close_transport(l.own);
  l.opened = false;
}

// Leaving: friends told the player is offline, and the presence socket closed.
void presence_close(RemoteLogin& l) {
  if (l.presence.started) {
    gn::Transport& link = l.link != nullptr ? *l.link : l.presence_own;
    if (l.link != nullptr || l.presence_opened)
      ge::account::client_presence_leave(l.presence, link, unix_now(l));
  }
  l.presence.started = false;
  if (l.presence_opened) gn::close_transport(l.presence_own);
  l.presence_opened = false;
}

// The stage of whichever client the window joined with.
ClientStage stage_of(const Remote& r) {
  if (r.nq) return r.nq->stage;
  return r.q2 ? r.q2->stage : r.end.stage;
}

// While in the game, with the account file's ticket: friends see the player on this server and
// its map, and their invites are said on the console. A file that does not load says nothing
// here (the login says why, on a server that keeps logins).
void presence_step(Remote& r, std::vector<std::string>& said) {
  if (!r.login || !r.active || stage_of(r) != ClientStage::active) return;
  RemoteLogin& l = *r.login;
  if (!l.done) return;  // the login's ask first: a test's shared link would read its answer here
  if (!l.presence_tried) {
    l.presence_tried = true;
    std::error_code ec;
    if (!std::filesystem::exists(l.path, ec)) return;
    ge::account::ClientLogin c;
    std::string why;
    if (!ge::account::client_login_load(c, l.path, unix_now(l) / 1000000, why)) return;
    if (l.link == nullptr && !l.presence_opened) {
      std::string error;
      if (!gn::open_udp_transport(l.presence_own, gn::ipv4(0, 0, 0, 0, 0), &error)) return;
      l.presence_opened = true;
    }
    ge::account::client_presence_start(l.presence, c, l.fresh, l.fresh_ctx);
  }
  if (!l.presence.started) return;
  ge::account::client_presence_set(l.presence, ge::account::PresenceState::playing, kPresenceGame,
                                   r.server_name, r.demo.state.map);
  gn::Transport& link = l.link != nullptr ? *l.link : l.presence_own;
  ge::account::client_presence_update(l.presence, link, unix_now(l));
  for (const ge::account::InviteRow& i : l.presence.got) {
    std::string line = i.from + " invites you to join them on " + i.where;
    line += i.note.empty() ? "." : ": " + i.note;
    said.push_back(line);
    said.push_back("Type connect " + i.where + " to join.");
  }
  l.presence.got.clear();
}

// Once in the game on a server whose info names the account file's master: the ticket traded for
// a code, and the login line sent. Said on the console in a player's words.
void login_step(Remote& r, std::vector<std::string>& said) {
  if (!r.login || !r.active || stage_of(r) != ClientStage::active) return;
  RemoteLogin& l = *r.login;
  if (l.done) return;
  ge::account::ClientLogin& c = l.c;
  if (!l.asked) {
    // Quake II's server says its key in a message of its own (svc 127) once in the game, so
    // the client waits for it there; a server that never says keeps no logins.
    const std::string tag = r.q2 ? r.demo.account_tag : info_get(r.end.serverinfo, kAccountKeyInfo);
    if (tag.empty() && r.q2) return;
    l.done = true;
    if (tag.empty()) return;  // a server that keeps no logins
    std::error_code ec;
    if (!std::filesystem::exists(l.path, ec)) return;  // never signed in here: the server says how
    std::string why;
    if (!ge::account::client_login_load(c, l.path, unix_now(l) / 1000000, why)) {
      said.push_back(why);
      return;
    }
    if (!ge::account::client_login_fits(c, tag)) return;  // another master's server
    if (l.link == nullptr) {
      std::string error;
      if (!gn::open_udp_transport(l.own, gn::ipv4(0, 0, 0, 0, 0), &error)) {
        said.push_back("Couldn't reach the account server to log you in.");
        return;
      }
      l.opened = true;
    }
    gn::Key fresh{};
    if (l.fresh != nullptr)
      l.fresh(l.fresh_ctx, fresh.data(), fresh.size());
    else
      gn::os_random_bytes(fresh.data(), fresh.size());
    ge::account::client_login_ask(c, fresh, unix_now(l));
    said.push_back("Logging in as " + c.file.name + ".");
    l.asked = true;
    l.done = false;
  }
  gn::Transport& link = l.link != nullptr ? *l.link : l.own;
  if (!ge::account::client_login_update(c, link, unix_now(l))) return;
  l.done = true;
  login_close(l);
  switch (c.step) {
    case ge::account::ClientLoginStep::code:
      if (r.q2)
        q2_client_command(*r.q2, "login " + c.file.name + " " + c.code);
      else
        client_command(r.end, "login " + c.file.name + " " + c.code);
      break;
    case ge::account::ClientLoginStep::refused:
      said.push_back(c.words.empty() ? "Couldn't log you in." : c.words);
      break;
    default:
      said.push_back("The account server didn't answer. You can still log in with a code.");
      break;
  }
}

sh::MoveSettings settings_of(const QwClientEnd& c) { return client_move_settings(c); }

// Whether the server says it steps the original move (its sv_playermove in the server info).
bool original_move(const QwClientEnd& c) {
  return info_get(c.serverinfo, "sv_playermove") == "original";
}

sh::NetPlayer net_of(const PlayerState& p) { return client_net_player(p); }

// Whether the level's solids are there to predict over (a level loaded, playing).
bool can_predict(const Remote& r) {
  const QwClientEnd& c = r.end;
  return !r.nopred && r.world != nullptr && !r.world->empty() && c.stage == ClientStage::active &&
         c.playernum >= 0;
}

// The brush models where the last frame put them (its movers).
std::vector<sh::ClientModel> movers_of(const Remote& r) {
  const std::vector<sh::BrushModel>& models = *r.world;
  const client::DemoState& st = r.demo.state;
  std::vector<sh::ClientModel> movers;
  for (const client::DemoEntity& e : st.entities) {
    if (!e.present || e.model <= 0 || static_cast<size_t>(e.model) >= st.models.size()) continue;
    const std::string& name = st.models[static_cast<size_t>(e.model)];
    if (name.size() < 2 || name[0] != '*') continue;
    const int k = std::atoi(name.c_str() + 1);
    if (k > 0 && static_cast<size_t>(k) < models.size())
      movers.push_back({&models[static_cast<size_t>(k)], e.origin});
  }
  return movers;
}

// The player's place `acked`'s reply put him (`own`), with every move sent since run through the
// move, steering round the other players where they are drawn. False when there is nothing to
// predict from.
bool predict_from(Remote& r, const PlayerState& own, uint32_t acked, sh::Vec3f& out, int& replayed,
                  sh::MoveState* full = nullptr) {
  const QwClientEnd& c = r.end;
  if (!can_predict(r) || !c.player_seen[c.playernum] || acked == 0) return false;
  const sh::NetPlayer n = net_of(own);
  sh::MoveCmd cmds[64];
  int count = 0;
  for (uint32_t q = acked + 1; q < c.ch.outgoing_sequence && count < 64; q++)
    if (c.sent_seq[q & 63] == q) cmds[count++] = c.sent[q & 63];
  // The solids where the last frame put them: the movers, then the other players.
  const client::DemoState& st = r.demo.state;
  const std::vector<sh::ClientModel> movers = movers_of(r);
  std::vector<sh::Vec3f> others;
  for (size_t i = 0; r.solid_players && i < st.players.size(); i++)  // cl_solid_players
    if (static_cast<int>(i) != c.playernum && st.players[i].present && !st.players[i].dead)
      others.push_back(st.players[i].origin);
  sh::MoveState ahead;
  if (original_move(c)) {
    // The server steps the original move: the same step on the stand-in, built once a level.
    if (r.original_for != r.world || r.original_level != r.levels) {
      sh::ServerSettings set;
      set.move = settings_of(c);
      ge::classic::original_prediction_level(r.original, *r.world, set);
      r.original_for = r.world;
      r.original_level = r.levels;
    }
    std::vector<ge::classic::ModelAt> at;
    at.reserve(movers.size());
    for (const sh::ClientModel& m : movers)
      at.push_back({static_cast<int>(m.model - r.world->data()), m.origin});
    ge::classic::original_prediction_place(r.original, at.data(), static_cast<int>(at.size()),
                                           others.data(), static_cast<int>(others.size()));
    ahead = ge::classic::predict_player_original(r.original, n, cmds, count);
  } else {
    sh::MoveWorld world;
    sh::client_world((*r.world)[0], movers.data(), static_cast<int>(movers.size()), others.data(),
                     static_cast<int>(others.size()), world);
    ahead = sh::predict_player(n, cmds, count, settings_of(c), world);
  }
  out = ahead.origin;
  if (full != nullptr) *full = ahead;
  replayed = count;
  return true;
}

// The other players run forward to the drawn time `now_s` from their newest records, each with
// its last command, over the world and the movers; their drawn places set in the demo's state.
void predict_others(Remote& r, double now_s) {
  r.others = 0;
  r.others_msec = 0;
  const QwClientEnd& c = r.end;
  if (!r.predict_players || !can_predict(r)) return;
  const std::vector<sh::ClientModel> movers = movers_of(r);
  sh::MoveWorld world;
  sh::client_world((*r.world)[0], movers.data(), static_cast<int>(movers.size()), nullptr, 0,
                   world);
  const sh::MoveSettings set = settings_of(c);
  std::vector<client::DemoEntity>& drawn = r.demo.state.players;
  for (int i = 0; i < 32; i++) {
    const PlayerState& p = c.players[i];
    if (i == c.playernum || !c.player_seen[i] || !p.has_msec || !p.has_command || p.dead || p.gib ||
        static_cast<size_t>(i) >= drawn.size() || !drawn[static_cast<size_t>(i)].present)
      continue;
    const sh::NetPlayer n = net_of(p);
    if (n.type != sh::MoveType::normal && n.type != sh::MoveType::fly) continue;
    const int msec = sh::other_msec(c.player_at[i], p.msec, now_s, c.latency);
    drawn[static_cast<size_t>(i)].origin = sh::predict_other(n, p.command, msec, set, world).origin;
    r.others++;
    if (msec > r.others_msec) r.others_msec = msec;
  }
}

// The entities drawn between the last two frames read, at the drawn time `now_s`.
void lerp_entities(Remote& r, double now_s) {
  r.lerped = 0;
  if (r.nolerp || r.frame_old_s < 0) return;
  const double gap = r.frame_new_s - r.frame_old_s;
  if (gap <= 0) return;
  double f = (now_s - r.frame_new_s) / gap;
  f = f < 0 ? 0 : f > 1 ? 1 : f;
  const auto t = static_cast<float>(f);
  std::vector<client::DemoEntity>& ents = r.demo.state.entities;
  const size_t n = std::min({ents.size(), r.ents_old.size(), r.ents_new.size()});
  for (size_t i = 0; i < n; i++) {
    const client::DemoEntity& a = r.ents_old[i];
    const client::DemoEntity& b = r.ents_new[i];
    if (!a.present || !b.present || a.model != b.model || !ents[i].present) continue;
    const sh::Vec3f d = b.origin - a.origin;
    if (sh::length(d) > kLerpMost || (d.x == 0 && d.y == 0 && d.z == 0)) continue;
    ents[i].origin = a.origin + d * t;
    r.lerped++;
  }
}

}  // namespace

const char* remote_stage_words(ClientStage s) {
  switch (s) {
    case ClientStage::challenge:
    case ClientStage::connect:
      return "connecting to ";
    case ClientStage::signon:
      return "signing on to ";
    case ClientStage::active:
      return "playing on ";
    case ClientStage::dropped:
      return "dropped by ";
  }
  return "";
}

namespace {

std::string size_words(size_t bytes) {
  constexpr size_t kKb = 1024, kMb = kKb * kKb;
  if (bytes < 10 * kKb) return std::to_string(bytes) + " bytes";
  if (bytes < 10 * kMb) return std::to_string(bytes / kKb) + " KB";
  return std::to_string(bytes / kMb) + " MB";
}

std::string coming(const std::string& name, int percent, size_t bytes, double& progress) {
  percent = std::clamp(percent, 0, 100);
  progress = percent / 100.0;
  return name + " " + std::to_string(percent) + "% (" + size_words(bytes) + ")";
}

}  // namespace

const ge::hud::NetHistory* remote_net(const Remote& r) {
  if (!r.active || r.watch || r.nq) return nullptr;
  return r.q2 ? &r.q2->net : &r.end.net;
}

bool remote_net_trouble(const Remote& r) {
  if (!r.active || r.watch || r.q2) return false;
  if (r.nq)
    return r.nq->stage == ClientStage::active && r.nq->heard_us >= 0 &&
           r.now_us - r.nq->heard_us >= 300000;
  const Channel& ch = r.end.ch;
  return r.end.stage == ClientStage::active &&
         ch.outgoing_sequence - 1 - ch.incoming_acknowledged >= 63;
}

int remote_unanswered(const Remote& r) {
  if (!r.active || r.watch || !r.q2 || r.q2->stage != ClientStage::active) return 0;
  return static_cast<int>(r.q2->ch.outgoing_sequence - 1 - r.q2->ch.incoming_acknowledged);
}

std::string remote_foot(const Remote& r, double& progress) {
  progress = -1;
  if (!r.active || r.watch) return {};
  const std::string& map = r.demo.state.map;
  const std::string loading = map.empty() ? "Loading..." : "Loading " + map + "...";
  if (r.nq) return r.nq->stage == ClientStage::signon ? loading : std::string();
  if (r.q2) {
    const Q2ClientEnd& c = *r.q2;
    if (c.stage == ClientStage::active || c.stage == ClientStage::dropped) return {};
    if (!c.fetching.empty())
      return coming(c.fetching, c.fetched_percent, c.fetched_bytes.size(), progress);
    return c.stage == ClientStage::signon ? loading : std::string();
  }
  const QwClientEnd& c = r.end;
  if (c.stage == ClientStage::active || c.stage == ClientStage::dropped) return {};
  if (c.dl.active) {
    int percent = c.dl.percent;
    if (c.dl.chunked && !c.dl.have.empty())
      percent = static_cast<int>(std::count(c.dl.have.begin(), c.dl.have.end(), true) * 100 /
                                 static_cast<std::ptrdiff_t>(c.dl.have.size()));
    return coming(c.dl.name, percent,
                  c.dl.size > 0 ? static_cast<size_t>(c.dl.size) : c.dl.bytes.size(), progress);
  }
  return c.stage == ClientStage::signon ? loading : std::string();
}

namespace {

// The port Quake II's `net_clientport` asks for (-1 any free one; never in Quake).
int wanted_port(const Remote& r) {
  if (!r.setting || !(r.q2_game && r.q2_game())) return -1;
  const std::string v = r.setting("net_clientport");
  const int port = v.empty() ? -1 : std::atoi(v.c_str());
  return port > 0 && port <= 65535 ? port : -1;
}

// The client's socket on `port` (-1 any free one), else on any free port with `r.port_note`
// saying so; false only when no port opens.
bool open_own(Remote& r, int port, std::string& error) {
  r.own_port = port;
  r.port_note.clear();
  if (port > 0) {
    if (gn::open_udp_transport(r.own, gn::ipv4(0, 0, 0, 0, static_cast<uint16_t>(port)), &error))
      return true;
    r.port_note = "Couldn't open port " + std::to_string(port) +
                  " for the client; another program may be using it. Using any free port.";
  }
  return gn::open_udp_transport(r.own, gn::ipv4(0, 0, 0, 0, 0), &error);
}

}  // namespace

bool remote_connect(Remote& r, const std::string& address, const ClientConfig& cfg,
                    std::string& error) {
  remote_disconnect(r);
  std::string text = address;
  if (text.rfind("localhost", 0) == 0) text = "127.0.0.1" + text.substr(9);
  const bool q2 = r.q2_game && r.q2_game();
  const uint16_t port = q2 ? kPortQ2 : kPortServer;
  if (text.find(':') == std::string::npos) text += ":" + std::to_string(port);
  gn::Address to;
  if (std::string why; !gn::parse_address(text, to) && !gn::resolve_ipv4(text, port, to, &why)) {
    error = "'" + address + "' is not an address the client reads (" + why +
            "). Give a host name or an IPv4 address with an optional port (connect "
            "192.168.1.20, connect 192.168.1.20:27500, connect example.org:27500) or localhost.";
    return false;
  }
  if (!open_own(r, q2 ? wanted_port(r) : -1, error)) {
    error =
        "the client's socket did not open: " + error + ". Check that the computer is on a network.";
    return false;
  }
  r.transport = &r.own;
  r.server = to;
  r.server_name = text;
  r.trace.where = text;
  if (q2)
    q2_remote_start(r, cfg);
  else if (to.port == kPortNetQuake)
    nq_remote_start(r, cfg);
  else
    start(r, cfg);
  return true;
}

void remote_login_from(Remote& r, const std::string& path, int64_t (*unix_us)(),
                       gn::Transport* link) {
  if (!r.login) r.login = std::make_unique<RemoteLogin>();
  r.login->path = path;
  r.login->unix_us = unix_us;
  r.login->link = link;
}

bool remote_ranked(Remote& r, const std::string& game, const std::string& mode,
                   const std::string& duo, std::string& said) {
  if (!r.login) {
    said = "Ranked games need an account. Sign in with the game's command line first.";
    return false;
  }
  RemoteLogin& l = *r.login;
  ge::account::ClientLogin c;
  if (!ge::account::client_login_load(c, l.path, unix_now(l) / 1000000, said)) return false;
  if (l.link == nullptr && !l.queue_opened) {
    std::string error;
    if (!gn::open_udp_transport(l.queue_own, gn::ipv4(0, 0, 0, 0, 0), &error)) {
      said = "Couldn't reach the account server.";
      return false;
    }
    l.queue_opened = true;
  }
  ge::account::client_queue_start(l.queue, c, game, mode, duo, l.fresh, l.fresh_ctx);
  said = "Looking for a ranked " + mode + " game as " + c.file.name + ".";
  return true;
}

bool remote_ranked_leave(Remote& r) {
  if (!r.login || r.login->queue.step != ge::account::QueueStep::waiting) return false;
  RemoteLogin& l = *r.login;
  ge::account::client_queue_leave(l.queue, l.link != nullptr ? *l.link : l.queue_own, unix_now(l));
  return true;
}

std::string remote_queue_step(Remote& r, std::vector<std::string>& said) {
  if (!r.login) return "";
  RemoteLogin& l = *r.login;
  ge::account::ClientQueue& q = l.queue;
  if (q.step != ge::account::QueueStep::waiting) return "";
  gn::Transport& link = l.link != nullptr ? *l.link : l.queue_own;
  const bool found = ge::account::client_queue_update(q, link, unix_now(l));
  if (q.step == ge::account::QueueStep::refused) {
    said.push_back(q.words.empty() ? "The matchmaker couldn't take you." : q.words);
    return "";
  }
  if (!found) return "";
  said.push_back(q.words);
  if (l.queue_opened) gn::close_transport(l.queue_own);
  l.queue_opened = false;
  return q.view.address;
}

void remote_queue_frame(Remote& r, ge::console::Console& con) {
  // A server browser's commands from this machine (`localid`), and the id it got wrong cleared.
  if (std::exchange(r.end.localid_refused, false)) con.execute("localid \"\"");
  for (const std::string& line : std::exchange(r.end.local_commands, {})) con.execute(line);
  std::vector<std::string> said;
  const std::string found = remote_queue_step(r, said);
  for (const std::string& line : said) con.print(line);
  if (!found.empty()) con.execute("connect " + found);
  const double wall =  // the wall clock: the match's recording and the rcon answers run on it
      std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  remote_match_frame(r, con, wall);
  said.clear();  // `pingservers`' answers (q2_pingservers.hpp)
  pingservers_step(r.ping, wall, r.now_us, said);
  const std::string called = passive_step(r.passive, r.active, r.now_us, said);  // `passive`
  for (const std::string& line : said) con.print(line);
  if (!called.empty()) con.execute("connect " + called);
  if (!r.away && !r.status_ask) return;
  said.clear();  // the answers to an rcon sent while not joined, and to `serverstatus`
  remote_rcon_step(r, wall, said);
  for (const std::string& line : said) con.print(line);
}

namespace {

std::vector<uint8_t> oob_bytes(const std::string& text) {
  std::vector<uint8_t> b = {0xff, 0xff, 0xff, 0xff};
  b.insert(b.end(), text.begin(), text.end());
  b.push_back(0);
  return b;
}

double steady_now_s() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace

bool remote_rcon(Remote& r, const std::string& address, const std::string& text,
                 std::string& error) {
  const std::vector<uint8_t> b = oob_bytes(text + "\n");
  if (r.active && !r.watch && r.transport != nullptr) {  // joined: to that server
    gn::transport_send(*r.transport, r.server, b.data(), b.size(), r.now_us);
    return true;
  }
  const bool q2 = r.q2_game && r.q2_game();
  if (address.empty()) {
    error = q2 ? "You must be connected, or set rcon_address, to send rcon commands."
               : "You must either be connected or set rcon_address to send rcon commands.";
    return false;
  }
  std::string where = address;
  if (where.rfind("localhost", 0) == 0) where = "127.0.0.1" + where.substr(9);
  const uint16_t port = q2 ? kPortQ2 : kPortServer;
  if (where.find(':') == std::string::npos) where += ":" + std::to_string(port);
  gn::Address to;
  if (std::string why; !gn::parse_address(where, to) && !gn::resolve_ipv4(where, port, to, &why)) {
    error = "Bad address: " + address;
    return false;
  }
  if (!r.away) r.away = std::make_unique<RconAway>();
  RconAway& a = *r.away;
  if (a.link == nullptr && !a.opened) {
    std::string why;
    if (!gn::open_udp_transport(a.own, gn::ipv4(0, 0, 0, 0, 0), &why)) {
      error = "Couldn't open a network socket for rcon.";
      return false;
    }
    a.opened = true;
  }
  a.to = to;
  const double now = a.link != nullptr ? static_cast<double>(r.now_us) * 1e-6 : steady_now_s();
  a.until_s = now + kRconQuietS;
  gn::Transport& link = a.link != nullptr ? *a.link : a.own;
  gn::transport_send(link, to, b.data(), b.size(), r.now_us);
  return true;
}

void remote_rcon_step(Remote& r, double now_s, std::vector<std::string>& said) {
  serverstatus_step(r, now_s, said);  // `serverstatus`'s answer (qw_serverstatus.hpp)
  if (!r.away || r.away->until_s < 0) return;
  RconAway& a = *r.away;
  gn::Transport& link = a.link != nullptr ? *a.link : a.own;
  uint8_t buf[2048];
  gn::Address from;
  for (int n; (n = gn::transport_receive(link, buf, sizeof buf, from, r.now_us)) >= 0;) {
    if (n < 5 || buf[0] != 0xff || buf[1] != 0xff || buf[2] != 0xff || buf[3] != 0xff) continue;
    std::string text(reinterpret_cast<const char*>(buf) + 4,
                     strnlen(reinterpret_cast<const char*>(buf) + 4, static_cast<size_t>(n) - 4));
    if (text.rfind("print\n", 0) == 0)
      text.erase(0, 6);  // Quake II's answer
    else if (!text.empty() && text[0] == 'n')
      text.erase(0, 1);  // QuakeWorld's
    else
      continue;
    a.until_s = now_s + kRconQuietS;
    size_t at = 0;
    while (at < text.size()) {
      size_t end = text.find('\n', at);
      if (end == std::string::npos) end = text.size();
      said.push_back(text.substr(at, end - at));
      at = end + 1;
    }
  }
  if (now_s < a.until_s) return;
  a.until_s = -1;
  if (a.opened) gn::close_transport(a.own);
  a.opened = false;
}

void remote_start(Remote& r, gn::Transport& t, const gn::Address& server, const ClientConfig& cfg) {
  remote_disconnect(r);
  r.transport = &t;
  r.server = server;
  r.server_name = gn::to_string(server);
  r.trace.where = r.server_name;
  if (r.q2_game && r.q2_game())
    q2_remote_start(r, cfg);
  else if (server.port == kPortNetQuake)
    nq_remote_start(r, cfg);
  else
    start(r, cfg);
}

void remote_world(Remote& r, const std::vector<sh::BrushModel>* models,
                  const ge::brush::Map* brushes) {
  r.world = models;
  r.smooth = sh::Smoothing{};
  q2_predict_map(r.q2_ahead, brushes);
}

void remote_disconnect(Remote& r) {
  if (!r.active) return;
  r.left = true;
  if (r.watch) {
    watch_close(*r.watch);
    r.watch.reset();
    r.active = false;
    return;
  }
  if (r.nq) {
    nq_stop(*r.nq, r.now_us);  // the disconnect byte, once
    r.nq.reset();
    if (r.transport == &r.own) gn::close_transport(r.own);
    r.transport = nullptr;
    r.active = false;
    return;
  }
  if (r.q2) {
    q2_client_stop(*r.q2);  // its goodbye, sent at once
    r.q2.reset();
    if (r.transport == &r.own) gn::close_transport(r.own);
    r.transport = nullptr;
    r.active = false;
    return;
  }
  if (r.end.stage == ClientStage::signon || r.end.stage == ClientStage::active) {
    client_command(r.end, "drop");
    for (int i = 0; i < 3; i++) {  // the reliable drop, sent until it is surely out
      r.now_us += kTickUs;
      client_tick(r.end, r.now_us);
    }
  }
  r.end.record = nullptr;
  if (r.login) {
    login_close(*r.login);
    presence_close(*r.login);
  }
  if (r.transport == &r.own) gn::close_transport(r.own);
  r.transport = nullptr;
  r.active = false;
}

void remote_mic(Remote& r, std::span<const float> samples) {
  if (!r.active || r.nq) return;
  if (r.q2 ? !r.q2->cfg.live_mic || !r.q2->voice_on
           : !r.end.cfg.live_mic || !r.end.ext.has(ExtFamily::x2, kX2Voice))
    return;
  std::vector<float>& in = r.q2 ? r.q2->mic_in : r.end.mic_in;
  in.insert(in.end(), samples.begin(), samples.end());
  if (in.size() > static_cast<size_t>(kMicMostSamples))  // a stall: the oldest dropped
    in.erase(in.begin(), in.end() - kMicMostSamples);
}

bool remote_lobby(Remote& r, const std::string& line) {
  if (!r.active || r.watch) return false;
  remote_command(r, line);
  return true;
}

bool remote_rules_word(Remote& r, const std::string& line) {
  if (!r.active || r.watch) return false;
  if (r.nq) {  // a server of protocol 15: the words it takes from a player
    if (!nq_server_word(line.substr(0, line.find_first_of(" \t")))) return false;
    nq_command(*r.nq, line);
    return true;
  }
  if (!r.q2) {  // Quake's client sends on only its server's own words, never any unknown one
    const std::string word = line.substr(0, line.find_first_of(" \t"));
    if (word != "kill" && word != "pause" && word != "serverinfo") return false;
  }
  remote_command(r, line);  // Quake II's: every word the window does not know (`use Railgun`)
  return true;
}

void remote_command(Remote& r, const std::string& line) {
  if (r.nq)
    nq_command(*r.nq, line);
  else if (r.q2)
    q2_client_command(*r.q2, line);
  else
    client_command(r.end, line);
}

bool remote_talking(const Remote& r) {
  if (!r.active || r.nq) return false;
  if (r.q2 ? !r.q2->voice_on : !r.end.cfg.voice) return false;  // our server's only
  if (r.talk != nullptr && ge::console::talk_voice(*r.talk).on) return true;
  return r.voip_key != nullptr && r.voip_key->down();
}

namespace {

// A broadcast watched: its whole frames read, their news kept together, nothing predicted.
RemoteStep watch_step(Remote& r, std::vector<std::string>& said) {
  client::Demo& d = r.demo;
  const bool live = watch_take(*r.watch, d);
  std::string error;
  bool any = false;
  std::vector<client::DemoSound> started;
  std::vector<std::pair<int, int>> stopped;
  std::vector<client::DemoVoice> voice;
  std::vector<client::DemoPrint> prints;
  std::vector<client::DemoEffect> effects;
  std::string centre;
  for (;;) {
    const client::DemoRead got = client::next_frame(d, error);
    if (got == client::DemoRead::error) {
      r.why = "the broadcast's match did not read: " + error;
      said.push_back(r.why);
      remote_disconnect(r);
      return RemoteStep::dropped;
    }
    if (got != client::DemoRead::frame) break;
    any = true;
    const client::DemoState& st = d.state;
    started.insert(started.end(), st.started.begin(), st.started.end());
    stopped.insert(stopped.end(), st.stopped.begin(), st.stopped.end());
    voice.insert(voice.end(), st.voice.begin(), st.voice.end());
    prints.insert(prints.end(), st.prints.begin(), st.prints.end());
    effects.insert(effects.end(), st.effects.begin(), st.effects.end());
    if (!st.centre.empty()) centre = st.centre;
  }
  d.state.started = std::move(started);
  d.state.stopped = std::move(stopped);
  d.state.voice = std::move(voice);
  r.ignore.chat.relay = true;  // a broadcast: Quake II's message triggers wait
  client::drop_ignored(r.ignore, d.state, prints, d.state.time, &centre);  // chat_ignore.hpp
  d.state.prints = std::move(prints);
  d.state.effects = std::move(effects);
  d.state.centre = std::move(centre);
  d.bytes.erase(d.bytes.begin(), d.bytes.begin() + static_cast<std::ptrdiff_t>(d.pos));
  d.pos = 0;
  if (!live || d.ended) {
    r.why = live ? std::string("the broadcast ended") : r.watch->why;
    said.push_back(r.why);
    remote_disconnect(r);
    return RemoteStep::dropped;
  }
  if (!any || d.state.map.empty()) return RemoteStep::waiting;
  if (d.state.levels != r.levels || (r.kept && r.end.begun)) {
    r.levels = d.state.levels;
    if (r.end.begun) r.kept = false;  // what was fetched joining is read with the level
    return RemoteStep::new_level;
  }
  return RemoteStep::frame;
}

}  // namespace

bool remote_watch(Remote& r, const std::string& address, const std::string& name,
                  const std::string& password, std::string& error) {
  remote_disconnect(r);
  std::string text = address;
  if (text.rfind("localhost", 0) == 0) text = "127.0.0.1" + text.substr(9);
  gn::Address to;
  if (std::string why; text.find(':') == std::string::npos ||
                       (!gn::parse_address(text, to) && !gn::resolve_ipv4(text, 0, to, &why))) {
    error = "'" + address +
            "' is not a broadcast's address: give its host and port, as in "
            "watch 192.168.1.20:28000";
    return false;
  }
  auto w = std::make_unique<StreamWatch>();
  if (!watch_open(*w, to, name, password, error)) return false;
  r.watch = std::move(w);
  watch_demo(r.demo);
  r.server = to;
  r.server_name = text;
  r.active = true;
  r.levels = 0;
  r.why.clear();
  return true;
}

namespace {

// A screenshot the server asked for: the window's frame grabbed (800 by 600, JPEG at 70), then
// sent; the upload's lines said.
void snap_step(Remote& r, std::vector<std::string>& said) {
  ClientUpload& u = r.end.up;
  for (std::string& line : u.said) said.push_back(std::move(line));
  u.said.clear();
  if (!u.asked) return;
  if (!r.snap_grabbing) {
    if (!r.snap_grab || !r.snap_take || !r.snap_grab(800, 600, 70)) {
      client_upload_start(r.end, {});  // no frame to grab: the server told no
      return;
    }
    r.snap_grabbing = true;
  }
  std::vector<uint8_t> picture;
  if (!r.snap_take(picture)) return;
  r.snap_grabbing = false;
  client_upload_start(r.end, std::move(picture));
}

// `rate` and `password` as the window's settings now have them, told to the server joined when
// they moved since the join (Quake's `setinfo`, Quake II's whole user info again).
void user_info_step(Remote& r) {
  if (!r.setting) return;
  const std::string rate_text = r.setting("rate");
  const int rate = rate_text.empty() ? 0 : std::atoi(rate_text.c_str());
  const std::string password = r.setting("password");
  if (r.q2) {
    Q2ClientConfig& q = r.q2->cfg;
    if ((rate <= 0 || rate == q.rate) && password == q.password) return;
    if (rate > 0) q.rate = rate;
    q.password = password;
    q2_client_userinfo(*r.q2);
    return;
  }
  ClientConfig& c = r.end.cfg;
  const bool rate_moved = rate > 0 && rate != c.rate;
  const bool password_moved = password != c.password;
  if (rate_moved) c.rate = rate;
  if (password_moved) c.password = password;
  if (rate_moved) client_command(r.end, "setinfo rate " + std::to_string(rate));
  // The colours (`color`, `topcolor`, `bottomcolor`): Quake's server shows them to everyone.
  // Only a change of the setting itself: a team colour the server set is left alone.
  for (const auto& [key, seen, held] :
       {std::tuple{"topcolor", &r.seen_top, &c.topcolor},
        std::tuple{"bottomcolor", &r.seen_bottom, &c.bottomcolor}}) {
    const std::string now = r.setting(key);
    if (now == *seen) continue;
    const bool first = seen->empty();
    *seen = now;
    if (first || now.empty()) continue;
    *held = std::atoi(now.c_str());
    client_command(r.end, std::string("setinfo ") + key + " " + std::to_string(*held));
  }
  if (password_moved) client_command(r.end, "setinfo password \"" + password + "\"");
  // `msg` and `noaim` as they now are.
  if (const std::string m = r.setting("msg"); !m.empty() && std::atoi(m.c_str()) != c.msg) {
    c.msg = std::atoi(m.c_str());
    client_command(r.end, "setinfo msg " + std::to_string(c.msg));
  }
  if (const std::string n = r.setting("noaim"); !n.empty() && (n == "1") != c.noaim) {
    c.noaim = n == "1";
    client_command(r.end, std::string("setinfo noaim ") + (c.noaim ? "1" : "0"));
  }
}

}  // namespace

RemoteStep remote_step(Remote& r, double wall_s, std::vector<std::string>& said) {
  if (!r.active) return RemoteStep::waiting;
  if (r.watch) return watch_step(r, said);
  if (r.nq) return nq_remote_step(r, wall_s, said);
  remote_rates_step(r, wall_s);  // $cl_pps and the other rates (qw_remote_macros.hpp)
  user_info_step(r);
  if (r.q2) {
    // net_clientport moved: the socket opened again on the new port, the connection carrying on
    // (the server knows the client by its qport).
    if (const int port = wanted_port(r); port != r.own_port && r.transport == &r.own) {
      std::string error;
      if (!open_own(r, port, error)) said.emplace_back("Couldn't open a port for the client.");
      if (!r.port_note.empty()) said.push_back(r.port_note);
    }
    const RemoteStep step = q2_remote_step(r, wall_s, said);
    remote_record_flush(r);  // `record`'s file (qw_match_record.hpp)
    login_step(r, said);     // logged in by the account file, and seen by friends, as below
    presence_step(r, said);
    return step;
  }
  if (r.end.cfg.live_mic) {
    r.end.mic_talking = remote_talking(r);
    // The key held names who hears it (everyone, the team, the players near).
    if (r.end.mic_talking && r.talk != nullptr && ge::console::talk_voice(*r.talk).on)
      qw_voice_target(r.end,
                      std::string(ge::console::chat_to_word(ge::console::talk_voice(*r.talk).to)));
  }
  r.owed_s += wall_s;
  // What the player's place was before this frame's packets, for the correction they make.
  const int me = r.end.playernum;
  const PlayerState own_before = me >= 0 ? r.end.players[me] : PlayerState{};
  const uint32_t acked_before = r.end.ch.incoming_acknowledged;
  int ticks = 0;
  r.end.cfg.physfps = r.physfps;  // cl_physfps, live
  r.end.localid = r.localid;      // a command packet with the wrong id clears it
  r.end.cfg.physfps_spectator = r.physfps_spectator;
  r.end.cfg.c2spps = r.c2spps;  // cl_c2spps, live
  r.end.cfg.c2s_impulse_backup = r.c2s_impulse_backup;
  r.end.cfg.allow_snap = r.allow_snap;  // scr_allowsnap, live
  snap_step(r, said);
  for (;;) {
    const double tick_s = 1 / client_move_rate(r.end);
    if (r.owed_s < tick_s) break;
    r.owed_s -= tick_s;
    if (ticks++ >= kMostTicks) continue;  // a stall: the time is dropped, not paid down
    // The network's clock in whole microseconds, the fraction carried.
    r.tick_frac_us += tick_s * 1e6;
    const auto whole = static_cast<int64_t>(r.tick_frac_us);
    r.tick_frac_us -= static_cast<double>(whole);
    r.now_us += whole;
    client_tick(r.end, r.now_us);
  }
  for (std::string& line : r.end.oob_prints) {  // rcon's answers, a line each
    for (size_t at = 0; at < line.size();) {
      const size_t end = std::min(line.find('\n', at), line.size());
      if (end > at) said.push_back(line.substr(at, end - at));
      at = end + 1;
    }
  }
  r.end.oob_prints.clear();
  if (r.end.stage != r.said) {
    r.said = r.end.stage;
    std::string line = remote_stage_words(r.said) + r.server_name;
    if (r.said == ClientStage::dropped && !r.end.refused.empty()) line += ": " + r.end.refused;
    said.push_back(line);
  }
  login_step(r, said);
  presence_step(r, said);
  if (r.end.stage == ClientStage::dropped) {
    r.why = r.end.refused.empty() ? "the connection was lost" : r.end.refused;
    remote_disconnect(r);  // let go: the window is no longer on the server
    return RemoteStep::dropped;
  }
  if (r.end.setangles != r.setangles) {  // the server turned the player
    r.setangles = r.end.setangles;
    r.view = {r.end.set_angles[0], r.end.set_angles[1], 0};
  }
  // The recording read up to its newest packet, and what was read dropped.
  client::Demo& d = r.demo;
  d.bytes.insert(d.bytes.end(), r.recording.bytes.begin(), r.recording.bytes.end());
  r.recording.bytes.clear();
  remote_record_flush(r);  // `record`'s file (qw_match_record.hpp)
  bool any = false;
  std::string error;
  // Every frame read's own news, kept together (next_frame empties it for each frame).
  std::vector<client::DemoSound> started;
  std::vector<std::pair<int, int>> stopped;
  std::vector<client::DemoVoice> voice;
  std::vector<client::DemoPrint> prints;
  std::vector<client::DemoEffect> effects;
  std::string centre;
  for (;;) {
    const client::DemoRead got = client::next_frame(d, error);
    if (got == client::DemoRead::error) {
      r.why = "the server's message did not read: " + error;
      said.push_back("dropped: " + r.why);
      remote_disconnect(r);
      return RemoteStep::dropped;
    }
    if (got != client::DemoRead::frame) break;
    any = true;
    client::DemoState& st = d.state;
    // The entity list is the frame's own (rebuilt each frame), kept before anything drawn moves it.
    r.ents_old.swap(r.ents_new);
    r.ents_new = st.entities;
    r.frame_old_s = r.frame_new_s;
    r.frame_new_s = st.time;
    if (st.levels != r.levels) {  // a new level: nothing to draw between
      r.ents_old.clear();
      r.frame_old_s = -1;
    }
    started.insert(started.end(), st.started.begin(), st.started.end());
    stopped.insert(stopped.end(), st.stopped.begin(), st.stopped.end());
    voice.insert(voice.end(), st.voice.begin(), st.voice.end());
    prints.insert(prints.end(), st.prints.begin(), st.prints.end());
    effects.insert(effects.end(), st.effects.begin(), st.effects.end());
    if (!st.centre.empty()) centre = st.centre;
    if (st.aim_bend >= 0) sh::aim_turn_bent(r.aim_turn, st.aim_bend);
  }
  d.state.started = std::move(started);
  d.state.stopped = std::move(stopped);
  d.state.voice = std::move(voice);
  r.ignore.chat.relay = false;
  client::drop_ignored(r.ignore, d.state, prints, wall_s, &centre);  // chat_ignore.hpp
  d.state.prints = std::move(prints);
  d.state.effects = std::move(effects);
  d.state.centre = std::move(centre);
  if (d.ended) {
    r.why = "the server disconnected";
    said.push_back(r.why);
    remote_disconnect(r);
    return RemoteStep::dropped;
  }
  d.bytes.erase(d.bytes.begin(), d.bytes.begin() + static_cast<std::ptrdiff_t>(d.pos));
  d.pos = 0;
  d.state.angles = r.view;  // the drawn view is the hands', never a tick behind
  d.state.angles.y = d.state.angles.y + r.jump_aid.shown;  // and the easy jump's swing as shown
  // The other players run forward to the drawn time, then the player's own place predicted round
  // them (the last correction decaying over it), then the other entities drawn between frames.
  const double draw_s = static_cast<double>(r.now_us) * 1e-6 + r.owed_s;
  if (!any) {  // nothing new read: the entities back at the newest frame's places before the lerp
    std::vector<client::DemoEntity>& ents = d.state.entities;
    for (size_t i = 0; i < ents.size() && i < r.ents_new.size(); i++)
      ents[i].origin = r.ents_new[i].origin;
  }
  predict_others(r, draw_s);
  sh::Vec3f now;
  r.predicted =
      me >= 0 && me == r.end.playernum &&
      predict_from(r, r.end.players[me], r.end.ch.incoming_acknowledged, now, r.replayed, &r.ahead);
  if (r.predicted) {
    sh::Vec3f before;
    int unused = 0;
    if (r.end.ch.incoming_acknowledged != acked_before &&
        predict_from(r, own_before, acked_before, before, unused))
      sh::smoothing_frame(r.smooth, before, now);
    sh::smoothing_decay(r.smooth, wall_s);
    r.prediction = now;
    const sh::Vec3f drawn = sh::smoothed(r.smooth, now);
    if (static_cast<size_t>(me) < d.state.players.size())
      d.state.players[static_cast<size_t>(me)].origin = drawn;
    if (!d.state.intermission) d.state.eye = {drawn.x, drawn.y, drawn.z + d.state.view_height};
  }
  lerp_entities(r, draw_s);
  if (!any || d.state.map.empty()) return RemoteStep::waiting;
  if (d.state.levels != r.levels || (r.kept && r.end.begun)) {
    r.levels = d.state.levels;
    if (r.end.begun) r.kept = false;  // what was fetched joining is read with the level
    return RemoteStep::new_level;
  }
  return RemoteStep::frame;
}

std::string remote_status(const Remote& r) {
  if (!r.active) return "not connected";
  if (r.watch)
    return (r.watch->state == WatchState::watching ? "watching " : "asking to watch ") +
           r.server_name;
  return remote_stage_words(stage_of(r)) + r.server_name;
}

void remote_hands(Remote& r, const client::Moves& m, const ge::input::InputState& in,
                  const client::ControlSettings& c, double dt) {
  client::turn_view(in, m, c, dt, r.view);
  sh::MoveCmd cmd = client::read_controls(m, in, c);
  // The aim help's turn: toward the player the server last bent a shot onto, while firing.
  const client::DemoState& st = r.demo.state;
  const int target = r.aim_turn.target;
  sh::Vec3f middle;
  const sh::Vec3f* at = nullptr;
  if (target >= 1 && static_cast<size_t>(target - 1) < st.players.size()) {
    const client::DemoEntity& p = st.players[static_cast<size_t>(target - 1)];
    if (p.present && !p.dead) {
      middle = p.origin + sh::Vec3f{0, 0, 4};  // the player's box middle (-24..32 up)
      at = &middle;
    }
  }
  client::aim_turn_view(c, r.aim_turn, r.view, st.eye, at, (cmd.buttons & sh::kButtonAttack) != 0,
                        dt);
  if (cmd.impulse == 0) cmd.impulse = r.hands.impulse;
  r.hands = cmd;
}

void add_remote_commands(ge::console::Console& con, Remote& r) {
  using Con = ge::console::Console;
  using Args = ge::console::Args;
  client::add_qw_ignore(con, r.ignore, r.demo.state);
  add_remote_macros(con, r);  // $cl_server, $cl_ping, $cl_lag and the rates
  add_serverstatus_command(con, r);
  add_q2_download_command(con, r);  // download <file> (q2_remote.hpp)
  add_q2_skins_command(con, r);     // skins
  gn::add_net_commands(con, [&r]() -> const gn::Transport* { return r.transport; });  // dns
  gn::add_client_showip(con, [&r]() -> const gn::Transport* { return r.transport; });
  add_pingservers_command(con, r.ping, steady_now_s);  // pingservers (q2_pingservers.hpp)
  add_passive_command(con, r.passive);                 // passive (the same)
  r.setting = [&con](const std::string& name) {
    if (const ge::console::Cvar* v = con.cvars().find(name)) return v->value;
    if (const ge::console::Synonym* s = con.cvars().synonym(name))
      return con.cvars().synonym_value(*s);
    return std::string();
  };
  const auto join = [&r](Con& c, const std::string& address) {
    ClientConfig cfg;
    const auto cvar = [&c](const char* name, const std::string& fallback) {
      const ge::console::Cvar* v = c.cvars().find(name);
      return v != nullptr && !v->value.empty() ? v->value : fallback;
    };
    cfg.name = cvar("name", "player");
    cfg.skin = cvar("skin", "base");
    cfg.model = cvar("model", "");
    cfg.fetch_looks = cvar("noskins", "0") == "0";  // 1: none drawn; 2: none fetched
    cfg.fetch_lit = cvar("r_loadlit", "1") != "0";
    cfg.fetch_locs = cvar("tp_loadlocs", "1") != "0";
    cfg.keep_file = [&r](const std::string& gamedir, const std::string& path,
                         const std::vector<uint8_t>& bytes, const ge::vfs::ArchiveOrigin* origin) {
      if (r.keep_file) r.keep_file(gamedir, path, bytes, origin);
      r.kept = true;
    };
    cfg.topcolor = std::atoi(cvar("topcolor", "0").c_str());
    cfg.bottomcolor = std::atoi(cvar("bottomcolor", "0").c_str());
    cfg.speedjump = cvar("cl_speedjump", "0") == "1";
    // Quake II's download switches, registered while it is the game (q2_session.cpp).
    cfg.q2_downloads.all = std::atoi(cvar("allow_download", "1").c_str());
    const auto kind = [&cvar](const char* name, const char* fallback) {
      return cvar(name, fallback) != "0";
    };
    cfg.q2_downloads.maps = kind("allow_download_maps", "1");
    cfg.q2_downloads.players = kind("allow_download_players", "1");
    cfg.q2_downloads.models = kind("allow_download_models", "1");
    cfg.q2_downloads.sounds = kind("allow_download_sounds", "1");
    cfg.q2_downloads.textures = kind("allow_download_textures", "1");
    cfg.q2_downloads.pics = kind("allow_download_pics", "1");
    cfg.q2_downloads.others = kind("allow_download_others", "0");
    cfg.q2_nodelta = cvar("cl_nodelta", "0") != "0";
    // Quake II's downloads over HTTP (q2_http.hpp), registered while it is the game.
    cfg.q2_http.downloads = cvar("cl_http_downloads", "1") != "0";
    cfg.q2_http.filelists = cvar("cl_http_filelists", "1") != "0";
    cfg.q2_http.max_connections = std::atoi(cvar("cl_http_max_connections", "2").c_str());
    cfg.q2_http.proxy = cvar("cl_http_proxy", "");
    cfg.q2_http.default_url = cvar("cl_http_default_url", "");
    cfg.q2_http.debug = cvar("cl_http_debug", "0") != "0";
    cfg.rate = std::atoi(cvar("rate", "25000").c_str());  // the loaded game's default (classic)
    cfg.msg = std::atoi(cvar("msg", "1").c_str());
    cfg.noaim = cvar("noaim", "0") == "1";
    cfg.extra_info = r.extra_info;
    cfg.password = cvar("password", "");            // a closed server's password
    cfg.spectator = cvar("spectator", "0") == "1";  // `set spectator 1`: join to watch
    cfg.w_switch = cvar("w_switch", "");            // Quake's switch rules on a pickup
    cfg.b_switch = cvar("b_switch", "");
    // cl_novweps 1: the visible weapons not asked for, so the server sends neither list nor index.
    if (cvar("cl_novweps", "0") != "0") cfg.ask[ExtFamily::z] &= ~kZVwep;
    // The lens Quake II's user info carries (`fov`): cl_fov rounded, 0 (the rig's own) its 90.
    const double fov = std::atof(cvar("cl_fov", "0").c_str());
    cfg.fov = fov > 0 ? static_cast<int>(std::lround(fov)) : 90;
    // Quake II's hand and gender (its layer's; gender_auto picks it from the skin's figure).
    cfg.hand = std::atoi(cvar("hand", "0").c_str());
    cfg.gender = c.cvars().find("gender") != nullptr ? cvar("gender", "male") : "";
    cfg.gender_auto = !cfg.gender.empty() && cvar("gender_auto", "0") == "1";
    cfg.voice = r.voip;
    cfg.live_mic = true;  // the window's microphone (remote_mic), while a voice key is held
    // qport: the number the server knows this client by when a NAT moves its port; 0 one
    // picked from the name, so the same player keeps it from run to run.
    const int qport = std::atoi(cvar("qport", "0").c_str());
    cfg.qport = qport > 0 ? static_cast<uint16_t>(qport)
                          : static_cast<uint16_t>(1 + std::hash<std::string>{}(cfg.name) % 65000);
    cfg.timeout = std::atof(cvar("cl_timeout", "60").c_str());  // 0: never
    cfg.trace = &r.trace;                                       // showpackets, showdrop
    cfg.read_file = r.read_file;
    // Through forwarders (cl_proxyaddr, "fwd1:port@fwd2:port"): the packets to the first, the
    // rest and the server in `prx`, each hop taking its own off the front.
    const std::string chain = cvar("cl_proxyaddr", "");
    std::string first = address;
    if (!chain.empty() && !(r.q2_game && r.q2_game())) {
      const size_t at = chain.find('@');
      first = chain.substr(0, at);
      cfg.prx = (at == std::string::npos ? std::string() : chain.substr(at + 1) + "@") + address;
    }
    std::string error;
    if (!remote_connect(r, first, cfg, error)) {
      c.print("connect: " + error);
      return;
    }
    if (!r.port_note.empty()) c.print(r.port_note);
    if (!cfg.prx.empty()) r.server_name = address;  // reconnect: the server, through the chain
    c.print(remote_status(r));
  };
  con.add_command(
      "connect",
      "connect <host>[:port]: join a server (port 27500 when none is given, 27910 in Quake II; "
      "port 26000 is a server of Quake's own protocol)",
      [join](Con& c, const Args& a) {
        if (a.size() < 2)
          c.print("connect: name a server (connect 192.168.1.20)");
        else
          join(c, a[1]);
      });
  con.add_command(
      "watch", "watch <host>:<port> [password]: watch a broadcast (a server's stream port or a TV)",
      [&r](Con& c, const Args& a) {
        if (a.size() < 2) {
          c.print("watch: name a broadcast (watch 192.168.1.20:28000)");
          return;
        }
        const ge::console::Cvar* v = c.cvars().find("name");
        const std::string name = v != nullptr && !v->value.empty() ? v->value : "viewer";
        std::string error;
        if (!remote_watch(r, a[1], name, a.size() >= 3 ? a[2] : std::string(), error))
          c.print("watch: " + error);
        else
          c.print(remote_status(r));
      });
  con.add_command(
      "ranked",
      "ranked <mode> [duo]: find a ranked game by your rating (1on1, or 2on2 with your duo's "
      "name); ranked leave: stop looking",
      [&r](Con& c, const Args& a) {
        if (a.size() < 2) {
          c.print("ranked: name a mode (ranked 1on1, or ranked 2on2 <your duo's name>)");
          return;
        }
        if (a[1] == "leave") {
          c.print(remote_ranked_leave(r) ? "You stopped looking for a ranked game."
                                         : "You aren't looking for a ranked game.");
          return;
        }
        std::string said;
        remote_ranked(r, kGameFolder, a[1], a.size() > 2 ? a[2] : "", said);
        c.print(said);
      });
  // `user <name|id>`: one player's user info on the QuakeWorld server joined, as the server last
  // told it, a key a line in a column of 20; the first by user id or exact name.
  con.add_command("user", "user <name|id>: one player's user info on the server you joined",
                  [&r](Con& c, const Args& a) {
                    if (a.size() != 2) {
                      c.print("user <name|id>: one player's user info on the server you joined");
                      return;
                    }
                    if (!r.active || r.q2 || r.nq) {
                      c.print("You aren't on a QuakeWorld server.");
                      return;
                    }
                    const int id = std::atoi(a[1].c_str());
                    for (int i = 0; i < 32; i++) {
                      const std::string& info = r.end.userinfos[i];
                      const std::string name = info_get(info, "name");
                      if (name.empty() || (r.end.userids[i] != id && name != a[1])) continue;
                      std::vector<std::pair<std::string, std::string>> pairs;
                      info_pairs(info, pairs);
                      for (const auto& [k, v] : pairs) {
                        std::string line = k;
                        line.append(k.size() < 20 ? 20 - k.size() : 0, ' ');
                        line += v;
                        c.print(line);
                      }
                      return;
                    }
                    c.print("No player by that name or id is on the server.");
                  });
  // `fullinfo <info string>`: every key and value of it made the player's own (a setting's key sets
  // the setting; any other kept for the next join). Nothing is sent now; star keys are refused.
  con.add_command("fullinfo", "fullinfo <\\key\\value...>: your whole user info set at once",
                  [&r](Con& c, const Args& a) {
                    if (a.size() != 2) {
                      c.print("fullinfo <complete info string>");
                      return;
                    }
                    std::string_view s = a[1];
                    if (!s.empty() && s.front() == '\\') s.remove_prefix(1);
                    while (!s.empty()) {
                      const size_t k_end = s.find('\\');
                      const std::string key(s.substr(0, k_end));
                      if (k_end == std::string_view::npos) {
                        c.print("MISSING VALUE");
                        return;
                      }
                      s.remove_prefix(k_end + 1);
                      const size_t v_end = s.find('\\');
                      const std::string value(s.substr(0, v_end));
                      s.remove_prefix(v_end == std::string_view::npos ? s.size() : v_end + 1);
                      std::string low = key;
                      for (char& ch : low)
                        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                      if (low == "pmodel" || low == "emodel") continue;  // the client's own checks
                      if (!key.empty() && key.front() == '*') {
                        c.print("Can't set * keys");
                        continue;
                      }
                      const ge::console::Cvar* v = c.cvars().find(key);
                      if (v != nullptr && (v->flags & ge::console::kUserInfo) != 0) {
                        c.cvars().set(key, value, ge::console::Source::local);
                        continue;
                      }
                      std::erase_if(r.extra_info,
                                    [&key](const auto& kv) { return kv.first == key; });
                      if (!value.empty()) r.extra_info.emplace_back(key, value);
                    }
                  });
  // `version`: the build's own, printed in the window; never asked of a server.
  con.add_command("version", "version: the game's version",
                  [](Con& c, const Args&) { c.print(std::string("Version ") + kProgramVersion); });
  con.add_command("reconnect", "reconnect: join the last server again",
                  [&r, join](Con& c, const Args&) {
                    if (r.server_name.empty())
                      c.print("reconnect: you haven't joined a server yet (use connect <host>)");
                    else
                      join(c, r.server_name);
                  });
  con.add_command("disconnect", "disconnect: leave the server, back to your own game",
                  [&r](Con& c, const Args&) {
                    if (!r.active) {
                      c.print("disconnect: not connected");
                      return;
                    }
                    remote_disconnect(r);
                    c.print("disconnected from " + r.server_name);
                  });
  r.trace.print = [&con](const std::string& line) { con.print(line); };
  add_match_record_commands(con, r);  // match_*, match_save; record while joined
  con.cvars().bind({.name = "cl_nopred",
                    .fallback = "0",
                    .help = "1: on a server joined, draw the player where the server last put him "
                            "(a round trip late); 0: his own moves predicted",
                    .kind = ge::console::CvarKind::boolean},
                   &r.nopred);
  con.cvars().bind({.name = "cl_predict_players",
                    .fallback = "1",
                    .help = "1: on a server joined, the other players run ahead from their last "
                            "command to the drawn time; 0: drawn where the last frame put them",
                    .kind = ge::console::CvarKind::boolean},
                   &r.predict_players);
  con.cvars().bind({.name = "cl_nolerp",
                    .fallback = "0",
                    .help = "1: on a server joined, missiles and movers drawn where the newest "
                            "frame put them; 0: drawn between the last two frames",
                    .kind = ge::console::CvarKind::boolean},
                   &r.nolerp);
  {  // Quake's move rate, its servers' `maxfps` lowered (qw_client.hpp)
    const ge::console::CvarLayer qw(con.cvars(), "classic");
    con.cvars().add({.name = "cl_timeout",
                     .fallback = "60",
                     .flags = ge::console::kArchive,
                     .help = "How long a joined server may send nothing before you leave it, in "
                             "seconds.",
                     .min = 0,
                     .max = 86400,
                     .values = "0: never; 1..86400: that many seconds (60 the default)"});
    con.cvars().add({.name = "qport",
                     .fallback = "0",
                     .help = "The number the server you join knows you by when your router moves "
                             "your port; it applies from the next connect.",
                     .kind = ge::console::CvarKind::integer,
                     .min = 0,
                     .max = 65535,
                     .values = "0: one picked from your name (the default); 1..65535: that "
                               "number"});
    con.cvars().add({.name = "rcon_password",
                     .flags = ge::console::kArchive,
                     .help = "The password rcon sends the server joined.",
                     .kind = ge::console::CvarKind::text,
                     .values = "\"\": none, rcon takes it first on its line; TEXT: the password"});
    con.cvars().bind({.name = "scr_allowsnap",
                      .fallback = "1",
                      .flags = ge::console::kArchive,
                      .help = "Whether a server that asks for a picture of your screen gets one.",
                      .kind = ge::console::CvarKind::boolean,
                      .values = "0: it is told no; 1: yes (the default)"},
                     &r.allow_snap);
    con.cvars().add({.name = "cl_proxyaddr",
                     .flags = ge::console::kArchive,
                     .help = "The forwarders connect goes through on the way to a server, the "
                             "nearest first.",
                     .kind = ge::console::CvarKind::text,
                     .values =
                         "\"\": straight to the server (the default); HOST:PORT[@HOST:PORT...]: "
                         "through those forwarders"});
    con.cvars().add({.name = "rcon_address",
                     .flags = ge::console::kArchive,
                     .help = "The server rcon goes to while you are not joined to one.",
                     .kind = ge::console::CvarKind::text,
                     .values = "\"\": none, rcon needs a server joined; HOST[:PORT]: that server "
                               "(the game's server port when none is given)"});
    con.cvars().bind({.name = "showpackets",
                      .fallback = "0",
                      .help = "Whether each packet to and from the server joined is shown on the "
                              "console.",
                      .kind = ge::console::CvarKind::integer,
                      .min = 0,
                      .max = 3,
                      .values = "0: none (the default); 1: both ways; 2: those sent; 3: those "
                                "received"},
                     &r.trace.packets);
    con.cvars().bind({.name = "showdrop",
                      .fallback = "0",
                      .help = "Whether packets from the server joined that were lost or came out "
                              "of order are shown on the console.",
                      .kind = ge::console::CvarKind::boolean},
                     &r.trace.drop);
    con.cvars().bind(
        {.name = "cl_solid_players",
         .fallback = "1",
         .flags = ge::console::kArchive,
         .help = "Whether the other players block your predicted moves on a QuakeWorld "
                 "server.",
         .kind = ge::console::CvarKind::boolean},
        &r.solid_players);
    con.cvars().bind({.name = "localid",
                      .fallback = "",
                      .help = "The id a server browser on this computer puts on the commands it "
                              "sends the game; one with another id is refused.",
                      .kind = ge::console::CvarKind::text},
                     &r.localid);
    con.cvars().bind({.name = "cl_physfps",
                      .fallback = "0",
                      .flags = ge::console::kArchive,
                      .help = "How many moves a second you send a QuakeWorld server; it may allow "
                              "fewer.",
                      .kind = ge::console::CvarKind::integer,
                      .min = 0,
                      .max = 1000,
                      .values = "0: as many as the server allows (the default); 10..: that many, "
                                "never more than the server allows"},
                     &r.physfps);
    con.cvars().bind({.name = "cl_physfps_spectator",
                      .fallback = "77",
                      .flags = ge::console::kArchive,
                      .help = "How many moves a second you send a QuakeWorld server while you "
                              "watch.",
                      .kind = ge::console::CvarKind::integer,
                      .min = 0,
                      .max = 1000,
                      .values = "77: the default; 0: as many as the server allows; 10..: that "
                                "many, never more than the server allows"},
                     &r.physfps_spectator);
    con.cvars().bind({.name = "cl_c2spps",
                      .fallback = "0",
                      .flags = ge::console::kArchive,
                      .help = "How many packets a second you send a QuakeWorld server; the moves "
                              "between ride in the next one.",
                      .kind = ge::console::CvarKind::integer,
                      .min = 0,
                      .max = 1000,
                      .values = "0: one with every move (the default); 10..72: that many (more is "
                                "72, fewer 10)"},
                     &r.c2spps);
    con.cvars().bind({.name = "cl_c2sImpulseBackup",
                      .fallback = "3",
                      .flags = ge::console::kArchive,
                      .help = "How many of the moves a packet carries are checked for a weapon or "
                              "command key before cl_c2spps may hold one back.",
                      .kind = ge::console::CvarKind::integer,
                      .min = 0,
                      .max = 3,
                      .values = "3: the new move and the two before (the default); 2: the new one "
                                "and the one before; 1: the new one; 0: none"},
                     &r.c2s_impulse_backup);
    con.cvars().bind({.name = "v_gunkick",
                      .fallback = "0",
                      .flags = ge::console::kArchive,
                      .help = "Whether the view kicks back when a QuakeWorld server or a recording "
                              "says your weapon fired.",
                      .kind = ge::console::CvarKind::boolean},
                     &r.gunkick);
  }
  con.cvars().bind({.name = "cl_voip",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "1: on a server joined, ask for voice: hear the players the server "
                            "routes to you and talk while +voip is held (from the next connect)",
                    .kind = ge::console::CvarKind::boolean},
                   &r.voip);
  r.voip_key = &con.add_button(
      "voip",
      "+voip: talk while held, to those the server sends a voice to by default (your team)");
  con.add_command("serverskins",
                  "serverskins: the skins and player models the server joined offers, and "
                  "whether it lets you download them",
                  [&r](Con& c, const Args&) {
                    if (!r.active) {
                      c.print("serverskins: not connected");
                      return;
                    }
                    remote_command(r, "listskins");
                  });
  con.add_command(
      "netstatus", "netstatus: the server joined, its stage and the traffic",
      [&r](Con& c, const Args&) {
        c.print(remote_status(r));
        if (!r.active) return;
        if (r.q2) {
          const Q2ClientEnd& q = *r.q2;
          c.print("  in " + std::to_string(q.datagrams_in) + " packets, " +
                  std::to_string(q.bytes_in) + " bytes; out " + std::to_string(q.bytes_out) +
                  " bytes; " + std::to_string(q.frames) + " frames; " + std::to_string(r.lerped) +
                  " things drawn between frames");
          c.print(r.q2_ahead.predicted
                      ? "  predicting " + std::to_string(r.q2_ahead.replayed) +
                            " moves ahead of the server"
                      : (r.nopred ? "  not predicting (cl_nopred 1)"
                                  : "  not predicting yet (no level loaded or not playing)"));
          return;
        }
        c.print("  in " + std::to_string(r.end.datagrams_in) + " packets, " +
                std::to_string(r.end.bytes_in) + " bytes; out " + std::to_string(r.end.bytes_out) +
                " bytes; " + std::to_string(r.end.ch.dropped) + " dropped");
        if (r.predicted)
          c.print("  predicting " + std::to_string(r.replayed) + " moves ahead of the server, " +
                  std::to_string(static_cast<int>(sh::length(r.smooth.correction))) +
                  " units of correction left");
        else
          c.print(r.nopred ? "  not predicting (cl_nopred 1)"
                           : "  not predicting yet (no level loaded or not playing)");
        c.print("  " + std::to_string(r.others) + " other players run ahead (at most " +
                std::to_string(r.others_msec) + " ms), " + std::to_string(r.lerped) +
                " entities drawn between frames; round trip " +
                std::to_string(static_cast<int>(r.end.latency * 1000)) + " ms");
      });
}

}  // namespace shooters_edge::net
