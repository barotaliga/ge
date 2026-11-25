// The window joined to a server (qw_remote.hpp).
//
// ge:summary The window joined to a server: the QuakeWorld client driven by hands, drawn as a demo
// ge:keywords remote_connect remote_step remote_disconnect remote_status
// ge:skill net shooter
#include "shooters_edge/net/qw_remote.hpp"
#include "classic/teleporters.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <utility>

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
#include "shooters_edge/net/qw_remote_movers.hpp"
#include "shooters_edge/net/qw_remote_rulesets.hpp"
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

// The most joins again with another version of a server's map (map_versions.hpp): the player's
// copy, one kept, the server's own.
constexpr int kMostMapTries = 3;

// The server's map as a player names it ("dm3").
std::string map_shown(const QwClientEnd& c) {
  if (c.models.size() < 2) return "its map";
  std::string m = c.models[1];
  if (m.starts_with("maps/")) m.erase(0, 5);
  if (m.ends_with(".bsp")) m.resize(m.size() - 4);
  return m;
}

// The server's turn of the player (a spawn, a teleport: `setangle`) taken by the view once, as
// soon as it is read (a command made after it carries it). A teleport's under the high-lag
// teleport keeps the turning he did since the command that touched it (qw_client.hpp's
// `set_turn`); else the view is the server's.
void take_turn(Remote& r) {
  if (r.end.setangles == r.setangles) return;
  r.setangles = r.end.setangles;
  // The turn of a teleport the prediction foresaw: the view already has it (and his turning since).
  const sh::Vec3f told{r.end.set_angles[0], r.end.set_angles[1], r.end.set_angles[2]};
  if (r.predict_teleports && sh::tele_guess_setangle(r.tele_guess, r.tele.tele, told, r.view))
    return;
  if (r.end.set_kind == 1 && r.end.cfg.lag_turn)
    r.view = {r.end.set_angles[0], r.view.y + r.end.set_turn, 0};
  else
    r.view = {r.end.set_angles[0], r.end.set_angles[1], 0};
}

void start(Remote& r, const ClientConfig& cfg) {
  r.kept = false;
  ClientConfig c = cfg;
  c.brain = [&r](int) {
    take_turn(r);
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

// The player's place `acked`'s reply put him (`own`), with every move sent since run through the
// move, steering round the other players where they are drawn. False when there is nothing to
// predict from. `movers_then` (when given): the movers as an earlier prediction had them, else the
// newest frame's (`remote_movers`); the main pass (`full`) keeps the ones it used in `movers_last`.
bool predict_from(Remote& r, const PlayerState& own, uint32_t acked, sh::Vec3f& out, int& replayed,
                  sh::MoveState* full = nullptr,
                  const std::vector<sh::ClientModel>* movers_then = nullptr) {
  const QwClientEnd& c = r.end;
  if (!can_predict(r) || !c.player_seen[c.playernum] || acked == 0) return false;
  const sh::NetPlayer n = net_of(own);
  sh::MoveCmd cmds[64];
  uint32_t seqs[64];
  int count = 0;
  for (uint32_t q = acked + 1; q < c.ch.outgoing_sequence && count < 64; q++)
    if (c.sent_seq[q & 63] == q) {
      seqs[count] = q;
      cmds[count++] = c.sent[q & 63];
    }
  // The main pass keeps the state after each move (the missiles' places, the jump's sound).
  sh::MoveState* each = full != nullptr ? r.ahead_each : nullptr;
  // The solids where the last frame put them: the movers, then the other players.
  const client::DemoState& st = r.demo.state;
  const std::vector<sh::ClientModel> movers =
      movers_then != nullptr ? *movers_then : remote_movers(r);
  float mover_at[64];
  const float* timed = remote_mover_times(r, acked, seqs, cmds, count, mover_at);
  if (full != nullptr) remote_movers_keep(r, movers, mover_at, count);
  std::vector<sh::Vec3f> others;
  for (size_t i = 0; r.solid_players && i < st.players.size(); i++)  // cl_solid_players
    if (static_cast<int>(i) != c.playernum && st.players[i].present && !st.players[i].dead)
      others.push_back(st.players[i].origin);
  sh::MoveState ahead;
  const sh::Teleporters* tele = r.tele.table(r.predict_teleports, r.predict_pushes);
  sh::TeleHit hit;
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
      at.push_back({static_cast<int>(m.model - r.world->data()), m.origin, m.velocity, m.reach});
    ge::classic::original_prediction_place(r.original, at.data(), static_cast<int>(at.size()),
                                           others.data(), static_cast<int>(others.size()));
    ahead = ge::classic::predict_player_original(r.original, n, cmds, count, each, tele,
                                                 r.tele_guess.skip, &hit, timed);
  } else {
    sh::MoveWorld world;
    sh::client_world((*r.world)[0], movers.data(), static_cast<int>(movers.size()), others.data(),
                     static_cast<int>(others.size()), world);
    ahead = sh::predict_player_teleports(n, cmds, count, settings_of(c), world, each, tele,
                                         r.tele_guess.skip, &hit, movers.data(),
                                         static_cast<int>(movers.size()), timed);
  }
  out = ahead.origin;
  if (full != nullptr) {
    *full = ahead;
    std::copy(seqs, seqs + count, r.ahead_seq);
    r.ahead_count = count;
    r.ahead_from_held = n.jump_held;
    r.tele_hit_cmd = hit.cmd;
    r.tele_hit_index = hit.index;
  }
  replayed = count;
  return true;
}

// The teleport the main prediction went through, if any: the server's answer to an earlier one
// taken (a wrong one's turn undone, shooter/teleport_ahead.hpp), a new one turning the view to the
// far end's facing at once, keeping the turning made since the command that walked in. The drawn
// view follows.
void foresee_teleport(Remote& r, client::DemoState& st) {
  if (!r.predict_teleports || r.tele.tele.empty()) return;
  const QwClientEnd& c = r.end;
  const sh::Vec3f before = r.view;
  sh::tele_guess_acked(r.tele_guess, r.tele.tele, c.ch.incoming_acknowledged,
                       c.players[c.playernum].origin, sh::kPlayerMins, sh::kPlayerMaxs, r.view);
  if (r.tele_hit_cmd >= 0 && r.tele_hit_cmd < r.ahead_count) {
    const uint32_t seq = r.ahead_seq[r.tele_hit_cmd];
    sh::tele_guess_hit(r.tele_guess, r.tele.tele, seq, r.tele_hit_index, c.sent[seq & 63].angles,
                       r.view);
  }
  if (r.view.x != before.x || r.view.y != before.y) {
    st.angles = r.view;
    st.angles.y = st.angles.y + r.jump_aid.shown;
  }
}

// The other players run forward to the drawn time `now_s` from their newest records, each with
// its last command, over the world and the movers; their drawn places set in the demo's state.
void predict_others(Remote& r, double now_s) {
  r.others = 0;
  r.others_msec = 0;
  const QwClientEnd& c = r.end;
  if (!r.predict_players || !can_predict(r)) return;
  const std::vector<sh::ClientModel> movers = remote_movers(r);
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

// THE PLAYER'S OWN JUMP AND MISSILES, at the weapon prediction's shown command: a jump the
// predicted move took on a command shown for the first time plays its sound at once (the server's
// copy on the player dropped, `cl_predict_jump`); each nail, rocket, grenade or pack missile the
// weapon pass fired is thrown ahead of the server (client/own_missiles.hpp,
// `cl_predict_projectiles`) from the player's predicted place and view on its command; then every
// stand-in flies to `now_s`, is handed to the server's copy near it and is drawn into the demo's
// state.
void own_ahead(Remote& r, client::DemoState& d, double now_s) {
  const QwClientEnd& c = r.end;
  const int me = c.playernum;
  const uint32_t acked = c.ch.incoming_acknowledged, next = c.ch.outgoing_sequence;
  if (r.missiles_level != d.levels) {
    client::own_missiles_clear(r.missiles);
    r.missiles_level = d.levels;
  }
  if (static_cast<int32_t>(r.jump_played_to - acked) < 0) r.jump_played_to = acked;
  if (r.weapon.on && r.predict_jump && me >= 0) {
    constexpr std::string_view kJump = "player/plyrjmp8.wav";
    std::erase_if(d.started, [&](const client::DemoSound& s) {
      return s.entity == me + 1 && s.channel == 4 && s.name == nullptr && s.sound > 0 &&
             static_cast<size_t>(s.sound) < d.sounds.size() &&
             d.sounds[static_cast<size_t>(s.sound)] == kJump;
    });
    uint32_t shown = next - 1 - static_cast<uint32_t>(std::max(r.weapon.buffer, 0));
    if (static_cast<int32_t>(shown - (acked + 2)) < 0) shown = next - 1;
    bool held = r.ahead_from_held;
    for (int i = 0; i < r.ahead_count; i++) {
      const uint32_t q = r.ahead_seq[i];
      const bool jumped = !held && r.ahead_each[i].jump_held;
      held = r.ahead_each[i].jump_held;
      if (!jumped || static_cast<int32_t>(q - r.jump_played_to) <= 0 ||
          static_cast<int32_t>(q - shown) > 0)
        continue;
      client::DemoSound snd;
      snd.entity = me + 1;
      snd.channel = 4;
      for (size_t k = 1; k < d.sounds.size() && snd.sound == 0; k++)
        if (d.sounds[k] == kJump) snd.sound = static_cast<int>(k);
      if (snd.sound == 0) snd.name = "player/plyrjmp8.wav";
      snd.attenuation = 0;
      snd.origin = r.ahead_each[i].origin;
      d.started.push_back(snd);
      r.jumps_played++;
    }
    if (static_cast<int32_t>(shown - r.jump_played_to) > 0) r.jump_played_to = shown;
  }
  const int ping = me >= 0 && me < 32 ? c.players[me].weapon.ping_ms : 0;
  const bool deathmatch = info_get(c.serverinfo, "deathmatch") != "0";
  for (const RemoteWeapon::Fired& f : r.weapon.fired) {
    client::OwnShot shot;
    const int throws =
        client::own_shot_kind(f.shot.sound, f.shot.copies, f.weapon, f.rules, shot.kind);
    if (throws == 0) continue;
    shot.deathmatch = deathmatch;
    shot.origin = r.prediction;
    for (int i = 0; i < r.ahead_count; i++)
      if (r.ahead_seq[i] == f.command) shot.origin = r.ahead_each[i].origin;
    shot.angles = c.sent[f.command & 63].angles;
    shot.side = f.shot.side;
    shot.midair = f.midair;
    for (uint32_t q = f.command + 1; static_cast<int32_t>(q - next) < 0; q++)
      if (c.sent_seq[q & 63] == q) shot.age_s += c.sent[q & 63].msec * 0.001;
    for (int k = 0; k < throws; k++) {
      shot.spread = k;
      if (shot.kind == client::OwnMissileKind::laser) shot.side = throws > 1 ? 1 - 2 * k : 0;
      client::own_missile_fire(r.missiles, shot, now_s, c.latency, ping, r.prediction);
    }
  }
  const sh::Hull* world =
      r.world != nullptr && !r.world->empty() ? &(*r.world)[0].hulls[0] : nullptr;
  client::own_missiles_step(r.missiles, world, settings_of(c).gravity, now_s);
  // The copies hidden last step shown as the newest frame has them before they are matched again.
  const auto restore = [&](int n) {
    if (n > 0 && static_cast<size_t>(n) < r.ents_new.size() &&
        static_cast<size_t>(n) < d.entities.size())
      d.entities[static_cast<size_t>(n)].present = r.ents_new[static_cast<size_t>(n)].present;
  };
  for (const client::OwnMissile& o : r.missiles.flying) restore(o.adopted);
  for (const int n : r.missiles.hidden) restore(n);
  client::own_missiles_handover(r.missiles, d);
  client::own_missiles_draw(r.missiles, d, now_s);
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

std::string coming(const std::string& name, int percent, double& progress) {
  progress = std::clamp(percent, 0, 100) / 100.0;
  return name;  // the console draws the bar and the percentage after it
}

// The file the web downloads are fetching and its share (-1 while its size is unknown); "" none.
std::string coming_by_web(const ge::classic::content::WebDownloads& h, double& progress) {
  ge::classic::content::WebProgress p;
  if (!ge::classic::content::web_progress(h, p)) return {};
  progress = p.total > 0
                 ? std::clamp(static_cast<double>(p.got) / static_cast<double>(p.total), 0.0, 1.0)
                 : -1;
  return p.path;
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
    if (std::string web = coming_by_web(c.http, progress); !web.empty()) return web;
    if (!c.fetching.empty()) return coming(c.fetching, c.fetched_percent, progress);
    return c.stage == ClientStage::signon ? loading : std::string();
  }
  const QwClientEnd& c = r.end;
  if (c.stage == ClientStage::active || c.stage == ClientStage::dropped) return {};
  if (std::string web = coming_by_web(c.web, progress); !web.empty()) return web;
  if (c.dl.active) {
    int percent = c.dl.percent;
    if (c.dl.chunked && !c.dl.have.empty())
      percent = static_cast<int>(std::count(c.dl.have.begin(), c.dl.have.end(), true) * 100 /
                                 static_cast<std::ptrdiff_t>(c.dl.have.size()));
    return coming(c.dl.name, percent, progress);
  }
  return c.stage == ClientStage::signon ? loading : std::string();
}

void remote_console_foot(Remote& r, ge::console::ConsoleView& view) {
  double share = -1;
  std::string foot = remote_foot(r, share);  // first: it sets the share the call below reads
  view.set_foot(std::move(foot), share);
  if (const bool joining = r.active && !view.foot().empty(); joining != r.console_held) {
    r.console_held = joining;
    view.set_full(joining);
  }
}

namespace {

// The port `net_clientport` asks for (-1 any free one), in every game.
int wanted_port(const Remote& r) {
  if (!r.setting) return -1;
  const std::string v = r.setting("net_clientport");
  const int port = v.empty() ? -1 : std::atoi(v.c_str());
  return port > 0 && port <= 65535 ? port : -1;
}

// The client's socket on `port` (-1 any free one), else on any free port with `r.port_note`
// saying so; false only when no port opens.
bool open_own(Remote& r, int port, std::string& error) {
  r.own_port = port;
  r.port_note.clear();
  // The address the socket is bound to: any of this machine's, or the one Quake II's `net_ip6`
  // names for an IPv6 server.
  gn::Address any = gn::ipv4(0, 0, 0, 0, 0);
  if (r.own_v6) {
    any = {};
    any.family = gn::Family::ipv6;
    const std::string ip = r.setting ? r.setting("net_ip6") : std::string();
    if (gn::Address named; !ip.empty() && gn::parse_address("[" + ip + "]:0", named)) any = named;
  }
  if (port > 0) {
    gn::Address at = any;
    at.port = static_cast<uint16_t>(port);
    if (gn::open_udp_transport(r.own, at, &error)) return true;
    r.port_note = "Couldn't open port " + std::to_string(port) +
                  " for the client; another program may be using it. Using any free port.";
  }
  any.port = 0;
  return gn::open_udp_transport(r.own, any, &error);
}

// "host", "host:port", "1.2.3.4", "[::1]:port", or a bare IPv6 address ("::1"), with the game's
// port added where the text names none.
std::string with_port(const std::string& text, uint16_t port) {
  const std::string p = ":" + std::to_string(port);
  if (!text.empty() && text[0] == '[')
    return text.find("]:") == std::string::npos ? text + p : text;
  const size_t colons = static_cast<size_t>(std::count(text.begin(), text.end(), ':'));
  if (colons == 0) return text + p;
  return colons == 1 ? text : "[" + text + "]" + p;
}

}  // namespace

bool remote_connect(Remote& r, const std::string& address, const ClientConfig& cfg,
                    std::string& error) {
  remote_disconnect(r);
  std::string text = address;
  if (text.rfind("localhost", 0) == 0) text = "127.0.0.1" + text.substr(9);
  const bool q2 = r.q2_game && r.q2_game();
  const uint16_t port = q2 ? kPortQ2 : kPortServer;
  text = with_port(text, port);
  gn::Address to;
  if (std::string why; !gn::parse_address(text, to) && !gn::resolve_ipv4(text, port, to, &why)) {
    error = "'" + address + "' is not an address the client reads (" + why +
            "). Give a host name or an IPv4 or IPv6 address with an optional port (connect "
            "192.168.1.20, connect 192.168.1.20:27500, connect [2001:db8::7]:27500, connect "
            "example.org:27500) or localhost.";
    return false;
  }
  // Quake II's `net_enable_ipv6` 0 keeps the client on IPv4 (unset: allowed in every game).
  const std::string v6 = r.setting ? r.setting("net_enable_ipv6") : std::string();
  if (to.family == gn::Family::ipv6 && !v6.empty() && std::atoi(v6.c_str()) == 0) {
    error = "IPv6 is turned off; set net_enable_ipv6 to 1 to join " + address + ".";
    return false;
  }
  r.own_v6 = to.family == gn::Family::ipv6;
  if (!open_own(r, q2 ? wanted_port(r) : -1, error)) {
    error =
        "the client's socket did not open: " + error + ". Check that the computer is on a network.";
    return false;
  }
  r.transport = &r.own;
  r.server = to;
  r.server_name = text;
  r.map_tries = 0;
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

void remote_run_stuffed(Remote& r, ge::console::Console& con) {
  const auto source = r.q2 ? ge::console::Source::local : ge::console::Source::server;
  for (const std::string& line : std::exchange(r.stuffed_run, {})) con.execute(line, source);
}

void remote_queue_frame(Remote& r, ge::console::Console& con) {
  // A server browser's commands from this machine (`localid`), and the id it got wrong cleared.
  if (std::exchange(r.end.localid_refused, false)) con.execute("localid \"\"");
  for (const std::string& line : std::exchange(r.end.local_commands, {})) con.execute(line);
  remote_rulesets_frame(r, con);  // the ruleset: its book, locked while joined
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

void remote_teleporters(Remote& r, const ge::brush::Map* level) {
  if (level != nullptr)
    r.tele.fill(ge::classic::level_teleporters(*level), ge::classic::level_pushers(*level));
  else
    r.tele = {};
  r.tele_guess = {};
  r.q2_ahead.tele =
      level != nullptr && level->family == ge::brush::Family::v38 ? r.tele : sh::TeleTables{};
  r.q2_ahead.guess = {};
}

void remote_world(Remote& r, const std::vector<sh::BrushModel>* models,
                  const ge::brush::Map* brushes, const ge::brush::Map* level) {
  if (level != nullptr) remote_teleporters(r, level);
  r.world = models;
  r.smooth = sh::Smoothing{};
  q2_predict_map(r.q2_ahead, brushes);
}

void remote_disconnect(Remote& r) {
  if (!r.active) return;
  r.left = true;
  // The player's own copy of the map read again (another version was drawn while joined).
  if (r.use_map && r.end.models.size() >= 2) r.use_map(r.end.models[1], "");
  if (r.ignore.con != nullptr) r.ignore.con->cvars().hold_cheats(false);  // Quake II's cheat lock
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
    // Our own server's match words (it agreed the ge set's settings bit): its stats.
    const bool ours = r.end.ext.has(ExtFamily::ge, kGeSettings);
    const bool stats_word = word == "stats" || word == "teamstats" || word == "laststats";
    if (word != "kill" && word != "pause" && word != "serverinfo" && !(ours && stats_word))
      return false;
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
  weapon_restore(r.weapon, d.state);  // the server's stats and frame back before its news
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
  if (r.q2) {  // name, skin, hand, fov, msg, gender, rate and password (q2_remote.hpp)
    Q2ClientConfig& q = r.q2->cfg;
    if (const std::string f = r.setting("cl_footsteps"); !f.empty()) {  // told on protocol 36
      q.no_footsteps = std::atoi(f.c_str()) == 0;
      q2_client_settings(*r.q2);
    }
    if (q2_user_info_moved(q, r.setting)) q2_client_userinfo(*r.q2);
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
  // A new name or skin: the server shows it at once.
  for (const auto& [key, held] : {std::pair{"name", &c.name}, std::pair{"skin", &c.skin}}) {
    const std::string now = r.setting(key);
    if (now.empty() || now == *held) continue;
    *held = now;
    client_command(r.end, std::string("setinfo ") + key + " \"" + now + "\"");
  }
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
  for (std::string& line : std::exchange(r.end.web.said, {})) said.push_back(std::move(line));
  for (std::string& line : std::exchange(r.end.to_run, {}))
    r.stuffed_run.push_back(std::move(line));
  // The server refused this version of its map: joined again at once with another (the next one
  // kept, else the server's own fetched; classic/content/map_versions.hpp), while there is one.
  if (r.end.stage == ClientStage::dropped && r.end.map_refused && !r.end.version_unfetched &&
      r.map_tries < kMostMapTries) {
    ClientConfig cfg = r.end.cfg;
    cfg.refused.push_back(r.end.map_sum);
    r.map_tries++;
    said.push_back("The server plays another version of " + map_shown(r.end) +
                   ". Joining again with its version.");
    start(r, cfg);
    return RemoteStep::waiting;
  }
  if (r.end.stage == ClientStage::active && r.said != ClientStage::active &&
      r.end.models.size() >= 2 && r.end.cfg.keep_choice)  // the version this server took
    r.end.cfg.keep_choice(r.server_name, r.end.models[1], r.end.map_sum);
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
  take_turn(r);
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
  weapon_restore(r.weapon, d.state);  // the server's stats and frame back before its news
  for (;;) {
    const client::DemoRead got = client::next_frame(d, error);
    if (got == client::DemoRead::error) {
      // The reader's words are a recording's; a live server's message is the server's.
      if (const size_t cut = error.find(": the recording is damaged"); cut != std::string::npos)
        error.resize(cut);
      r.why = "the server sent a message this game cannot read (" + error +
              "); the server may be newer than the game";
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
      r.movers_track = {};
    }
    sh::mover_track_frame(r.movers_track, st.time);
    for (size_t i = 0; i < st.entities.size(); i++)
      if (st.entities[i].present && client::brush_model(st, st.entities[i].model))
        sh::mover_track_seen(r.movers_track, i, st.entities[i].model, st.entities[i].origin);
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
  // A listing asked for the settings page (qw_remote_sheet.hpp), on the tick clock its asks keep.
  remote_sheet_step(r, prints, static_cast<double>(r.now_us) * 1e-6 + r.owed_s);
  remote_fair_step(r, prints, wall_s);  // f_ruleset answered (qw_remote_rulesets.hpp)
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
  // The movers the last step's prediction ran over: the newest command predicted from the old
  // frame again over them, not over the new frame's (which would count their move as a miss).
  remote_movers_turn(r);
  r.predicted =
      me >= 0 && me == r.end.playernum &&
      predict_from(r, r.end.players[me], r.end.ch.incoming_acknowledged, now, r.replayed, &r.ahead);
  if (r.predicted) {
    sh::Vec3f before;
    int unused = 0;
    // cl_predict_smoothview: 0 snapped; above, the decay's pace by 2 less it (at least 0.1).
    r.smooth.off = r.smoothview <= 0;
    r.smooth.scale = static_cast<float>(std::clamp(2 - r.smoothview, 0.1, 2.0));
    r.smooth.drop = kSmoothDropU;
    if (r.end.ch.incoming_acknowledged != acked_before &&
        predict_from(r, own_before, acked_before, before, unused, nullptr,
                     r.movers_prev_level == r.levels ? &r.movers_prev : nullptr)) {
      sh::smoothing_frame(r.smooth, before, now, r.end.latency);
      if (const float miss = sh::length(before - now); miss > 0) {
        r.misses++;
        r.miss_units = miss;
      }
    }
    sh::smoothing_decay(r.smooth, wall_s);
    r.prediction = now;
    foresee_teleport(r, d.state);
    sh::Vec3f drawn = sh::smoothed(r.smooth, now);
    // The eased place never inside a wall: the player's box swept from the prediction to it.
    if (r.world != nullptr && !r.world->empty() && sh::length(r.smooth.correction) > 0) {
      const sh::HullTrace tr = sh::trace_hull((*r.world)[0].hulls[1], now, drawn);
      if (!tr.allsolid && !tr.startsolid) drawn = tr.endpos;
    }
    if (static_cast<size_t>(me) < d.state.players.size())
      d.state.players[static_cast<size_t>(me)].origin = drawn;
    if (!d.state.intermission) d.state.eye = {drawn.x, drawn.y, drawn.z + d.state.view_height};
  }
  if (r.predicted) {
    std::string w_rank;
    for (const auto& [k, v] : r.extra_info)
      if (k == "w_rank") w_rank = v;
    weapon_predict(r.weapon, r.end, d.state, r.ahead.waterlevel, r.prediction, w_rank);
  } else {
    r.weapon.on = false;
    r.weapon.played_to = r.end.ch.incoming_acknowledged;
    r.weapon.fired.clear();
  }
  remote_lerp_entities(r, draw_s);
  remote_draw_movers_ahead(r);
  if (!r.predicted) r.ahead_count = 0;
  own_ahead(r, d.state, draw_s);
  if (!any || d.state.map.empty()) return RemoteStep::waiting;
  // The level is loaded once the files it needs are here: while its map or models still come
  // (the content fetched before prespawn) the window shows the console and the download's bar.
  if (r.end.prespawn_held) return RemoteStep::waiting;
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
  add_remote_macros(con, r);    // $cl_server, $cl_ping, $cl_lag and the rates
  add_remote_rulesets(con, r);  // ruleset, rulesets, f_ruleset (qw_remote_rulesets.hpp)
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
    cfg.q2_protocol = std::atoi(cvar("cl_protocol", "0").c_str());
    cfg.q2_msglen = std::atoi(cvar("net_maxmsglen", "1390").c_str());
    cfg.q2_chantype = std::atoi(cvar("net_chantype", "1").c_str());
    cfg.q2_batch = cvar("cl_batchcmds", "1") != "0";
    cfg.q2_packetdup = std::atoi(cvar("cl_packetdup", "1").c_str());
    cfg.q2_maxpackets = std::atoi(cvar("cl_c2spps", "30").c_str());  // its cl_maxpackets
    cfg.q2_instantpacket = cvar("cl_instantpacket", "1") != "0";
    // Quake II's downloads over HTTP (classic/content/web_downloads.hpp), registered while it is
    // the game.
    cfg.web.downloads = cvar("cl_http_downloads", "1") != "0";
    cfg.web.filelists = cvar("cl_http_filelists", "1") != "0";
    cfg.web.max_connections = std::atoi(cvar("cl_http_max_connections", "2").c_str());
    cfg.web.proxy = cvar("cl_http_proxy", "");
    cfg.web.default_url = cvar("cl_http_default_url", "");
    cfg.web.debug = cvar("cl_http_debug", "0") != "0";
    cfg.web.insecure = cvar("cl_http_insecure", "0") != "0";
    cfg.web.max_rate_kb = std::atoll(cvar("cl_http_max_rate", "0").c_str());
    cfg.web.max_size_mb = std::atoll(cvar("cl_http_max_size", "512").c_str());
    cfg.rate = std::atoi(cvar("rate", "25000").c_str());  // the loaded game's default (classic)
    cfg.msg = std::atoi(cvar("msg", "1").c_str());
    cfg.noaim = cvar("noaim", "0") == "1";
    cfg.extra_info = r.extra_info;
    cfg.ruleset = cvar("ruleset", "default");  // the competition ruleset (qw_remote_rulesets.hpp)
    cfg.password = cvar("password", "");       // a closed server's password
    cfg.spectator = cvar("spectator", "0") == "1";  // `set spectator 1`: join to watch
    cfg.w_switch = cvar("w_switch", "");            // Quake's switch rules on a pickup
    cfg.b_switch = cvar("b_switch", "");
    // cl_novweps 1: the visible weapons not asked for, so the server sends neither list nor index.
    if (cvar("cl_novweps", "0") != "0") cfg.ask[ExtFamily::z] &= ~kZVwep;
    // cl_pext_weaponprediction 0: the weapon's state not asked for (qw_weapon.hpp).
    if (cvar("cl_pext_weaponprediction", "1") == "0") cfg.ask[ExtFamily::mvd] &= ~kMvdWeaponPred;
    // cl_pext_lagteleport 0: the teleport's kind not asked for, the moves in flight left as made.
    cfg.lag_turn = cvar("cl_pext_lagteleport", "1") != "0";
    if (!cfg.lag_turn) cfg.ask[ExtFamily::mvd] &= ~kMvdTeleport;
    // cl_predict_teleport 1: the teleport foreseen, its moves made the new way from the first, so
    // the server must not turn them again: the high-lag teleport is not asked for.
    if (cvar("cl_predict_teleport", "1") != "0") cfg.ask[ExtFamily::mvd] &= ~kMvdTeleport;
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
    // Read at call time: a `+connect` on the command line joins before the window wires the files.
    cfg.read_file = [&r](const std::string& gamedir, const std::string& path) {
      return r.read_file ? r.read_file(gamedir, path) : std::vector<uint8_t>{};
    };
    cfg.map_versions = [&r](const std::string& map) {
      return r.map_versions ? r.map_versions(map)
                            : std::vector<std::pair<std::string, std::vector<uint8_t>>>{};
    };
    cfg.keep_version = [&r](const std::string& map, uint32_t sum,
                            const std::vector<uint8_t>& bytes) {
      return r.keep_version ? r.keep_version(map, sum, bytes) : std::string();
    };
    cfg.use_map = [&r](const std::string& map, const std::string& version) {
      if (r.use_map) r.use_map(map, version);
    };
    cfg.remembered = [&r](const std::string& map) {
      return r.remembered ? r.remembered(r.server_name, map) : std::optional<uint32_t>();
    };
    cfg.keep_choice = [&r](const std::string& server, const std::string& map, uint32_t sum) {
      if (r.keep_choice) r.keep_choice(server, map, sum);
    };
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
      [join, &r](Con& c, const Args& a) {
        if (a.size() < 2) {
          c.print("connect: name a server (connect 192.168.1.20)");
          return;
        }
        if (a.size() > 2 && r.q2_game && r.q2_game())  // Quake II's protocol is a setting
          c.print("The second argument to connect is ignored. Set the protocol with cl_protocol.");
        else if (a.size() > 2)
          c.print("Second argument to connect is ignored.");
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
  // The held weapon predicted (qw_weapon.hpp).
  con.cvars().bind({.name = "cl_nopred_weapon",
                    .fallback = "0",
                    .help = "1: on a server joined, your weapon's shots, frames and ammunition "
                            "shown as the server sends them (a round trip late); 0: predicted",
                    .kind = ge::console::CvarKind::boolean},
                   &r.weapon.off);
  con.cvars().bind({.name = "cl_predict_weaponsound",
                    .fallback = "1",
                    .help = "0: your weapon's sounds heard from the server; 1: every one played "
                            "as you fire; above 1, the weapons left to the server, 2 to the power "
                            "of the weapon's key (Quake: 2 axe, 4 shotgun, 8 super shotgun, 16 "
                            "nailgun, 32 super nailgun, 64 grenade launcher, 128 rocket "
                            "launcher, 256 lightning gun; Quake II: 2 blaster .. 512 railgun, "
                            "1024 BFG10K)",
                    .min = 0,
                    .max = 2047},
                   &r.weapon.sound_mask);
  con.cvars().bind({.name = "cl_predict_buffer",
                    .fallback = "1",
                    .help = "How many commands behind the newest your predicted weapon is shown "
                            "(0 the newest)",
                    .min = 0,
                    .max = 10},
                   &r.weapon.buffer);
  con.cvars().bind({.name = "cl_predict_projectiles",
                    .fallback = "1",
                    .help = "1: on a server joined, your own nails, bolts, rockets and grenades "
                            "fly as you fire, before the server's arrive; 0: only the server's",
                    .kind = ge::console::CvarKind::boolean},
                   &r.missiles.on);
  con.cvars().bind({.name = "cl_predict_jump",
                    .fallback = "1",
                    .help = "1: on a server joined, your jump heard as you jump; 0: when the "
                            "server's sound arrives",
                    .kind = ge::console::CvarKind::boolean},
                   &r.predict_jump);
  con.cvars().bind({.name = "cl_predict_teleport",
                    .fallback = "1",
                    .help = "1: on a server joined, a teleporter takes you to its far end and "
                            "turns you the moment you walk in; 0: when the server's word arrives",
                    .kind = ge::console::CvarKind::boolean},
                   &r.predict_teleports);
  con.cvars().bind({.name = "cl_predict_push",
                    .fallback = "1",
                    .help = "1: on a server joined, a push (a wind tunnel, a jump pad) throws you "
                            "the moment you step in; 0: when the server's word arrives",
                    .kind = ge::console::CvarKind::boolean},
                   &r.predict_pushes);
  con.cvars().bind({.name = "cl_predict_movers",
                    .fallback = "1",
                    .help = "1: on a server joined, doors, lifts and trains are run on at their "
                            "speed in your predicted move, so one you ride or walk through never "
                            "jolts you; 0: where the last frame showed them",
                    .kind = ge::console::CvarKind::boolean},
                   &r.predict_movers);
  con.cvars().bind({.name = "cl_predict_smoothview",
                    .fallback = "1",
                    .help = "How a correction of your predicted place is eased out on a server "
                            "joined: 0 snapped at once, 1 the usual pace, up to 2 slower",
                    .min = 0,
                    .max = 2},
                   &r.smoothview);
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
    // Downloads from a web address (classic/content/web_downloads.hpp), read at each `connect`;
    // the server list's web lists read `cl_http_blocking_timeout` (menu/servers_page.hpp).
    con.cvars().add({.name = "cl_http_downloads",
                     .fallback = "1",
                     .flags = ge::console::kArchive,
                     .help = "Whether joining fetches files from the web address a server names.",
                     .kind = ge::console::CvarKind::boolean,
                     .values = "1: from the web first (the default); 0: from the server alone"});
    con.cvars().add(
        {.name = "cl_http_filelists",
         .fallback = "1",
         .flags = ge::console::kArchive,
         .help = "Whether a server's web lists of further files are fetched and followed.",
         .kind = ge::console::CvarKind::boolean,
         .values = "1: followed (the default); 0: not"});
    con.cvars().add({.name = "cl_http_max_connections",
                     .fallback = "2",
                     .flags = ge::console::kArchive,
                     .help = "How many files are fetched from the web at once.",
                     .kind = ge::console::CvarKind::integer,
                     .min = 1,
                     .max = 4,
                     .values = "2: two at once (the default); 1 to 4: that many"});
    con.cvars().add({.name = "cl_http_proxy",
                     .fallback = "",
                     .flags = ge::console::kArchive,
                     .help = "The web proxy downloads go through.",
                     .kind = ge::console::CvarKind::text,
                     .values = "\"\": none (the default); TEXT: host:port"});
    con.cvars().add(
        {.name = "cl_http_default_url",
         .fallback = "",
         .flags = ge::console::kArchive,
         .help = "Your own web address files are fetched from when a server names none.",
         .kind = ge::console::CvarKind::text,
         .values = "\"\": none (the default); TEXT: an address such as http://host/q2/"});
    con.cvars().add({.name = "cl_http_insecure",
                     .fallback = "0",
                     .flags = ge::console::kArchive,
                     .help = "Whether a secure web address's certificate goes unchecked.",
                     .kind = ge::console::CvarKind::boolean,
                     .values = "0: checked (the default); 1: not"});
    con.cvars().add({.name = "cl_http_max_rate",
                     .fallback = "0",
                     .flags = ge::console::kArchive,
                     .help = "The fastest web downloads come, in kilobytes a second, all together.",
                     .kind = ge::console::CvarKind::integer,
                     .min = 0,
                     .max = 10000000,
                     .values = "0: as fast as the server sends (the default); N: that many kB a "
                               "second"});
    con.cvars().add({.name = "cl_http_max_size",
                     .fallback = "512",
                     .flags = ge::console::kArchive,
                     .help = "The largest file taken from the web, in megabytes.",
                     .kind = ge::console::CvarKind::integer,
                     .min = 1,
                     .max = 100000,
                     .values = "512: half a gigabyte (the default); N: that many megabytes"});
    con.cvars().add({.name = "cl_http_debug",
                     .fallback = "0",
                     .help = "Whether each web download's address is said as it starts.",
                     .kind = ge::console::CvarKind::boolean,
                     .values = "0: not said (the default); 1: said"});
    con.cvars().add({.name = "cl_http_blocking_timeout",
                     .fallback = "15",
                     .flags = ge::console::kArchive,
                     .help = "Seconds of silence before a web list of servers is given up.",
                     .kind = ge::console::CvarKind::number,
                     .min = 1,
                     .values = "15: fifteen seconds (the default); a number: that many"});
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
