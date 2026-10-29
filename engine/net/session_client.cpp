// The session's client: the hello, the roster mirrored from the host's word, the asks, and
// coming back by token when the host is lost for a while; and the session's names
// (net/session.hpp states the rules).
//
// ge:summary The session client: joins with a hello, mirrors the roster, asks, reconnects by token
// ge:keywords net session client lobby join hello welcome roster mirror reconnect token ask
// ge:keywords chat kick start ready team settings
// ge:skill net
// ge:test tests/net_session_test.cpp
#include <cstring>

#include "net/session.hpp"
#include "net/session_wire.hpp"

namespace ge::net {

namespace {

using namespace session_wire;

SessionEvent& push(SessionClient& c, SessionEventType type, int player) {
  SessionEvent e;
  e.type = type;
  e.player = player;
  c.events.push_back(e);
  return c.events.back();
}

bool ask(SessionClient& c, const uint8_t* data, size_t size) {
  if (c.state != ClientState::seated) return false;
  return node_send(*c.node, c.peer, c.channel, data, size);
}

void send_hello(SessionClient& c) {
  uint8_t buf[64];
  Writer w = make_writer(buf, sizeof buf);
  write_u8(w, kHello);
  write_u64(w, c.token);
  write_text(w, c.name, std::strlen(c.name));
  node_send(*c.node, c.peer, c.channel, buf, w.size);
}

void lose(SessionClient& c, SessionWhy why) {
  c.state = ClientState::lost;
  c.token = 0;
  push(c, SessionEventType::lost, c.me).why = why;
}

void on_player(SessionClient& c, Reader& r) {
  const int seat = seat_of(read_u8(r));
  const uint8_t cause = read_u8(r);
  const uint8_t flags = read_u8(r);
  const int team = seat_of(read_u8(r));
  size_t n = 0;
  const uint8_t* name = read_text(r, kMaxName, n);
  if (r.bad || seat < 0 || seat >= kMaxPlayers) {
    c.malformed++;
    return;
  }
  Player& p = c.players[seat];
  const int ping = p.ping_ms;
  p = Player{};
  p.used = true;
  p.present = flags & kFlagPresent;
  p.ready = flags & kFlagReady;
  p.leader = flags & kFlagLeader;
  p.team = team;
  p.ping_ms = ping;
  std::memcpy(p.name, name, n);
  p.name[n] = 0;
  switch (cause) {
    case kJoined:
      push(c, SessionEventType::joined, seat);
      break;
    case kRejoined:
      push(c, SessionEventType::rejoined, seat);
      break;
    case kAway:
      push(c, SessionEventType::away, seat);
      break;
    case kChanged:
      push(c, SessionEventType::changed, seat);
      break;
    default:
      break;
  }
}

void on_message(SessionClient& c, const uint8_t* data, size_t size) {
  Reader r = make_reader(data, size);
  const uint8_t kind = read_u8(r);
  switch (kind) {
    case kWelcome: {
      const int me = read_u8(r);
      const uint64_t token = read_u64(r);
      const int max_players = read_u8(r);
      const int teams = read_u8(r);
      const auto phase = static_cast<SessionPhase>(read_u8(r));
      if (r.bad || me >= kMaxPlayers) break;
      for (Player& p : c.players) p = Player{};
      c.me = me;
      c.token = token;
      c.max_players = max_players;
      c.team_count = teams;
      c.phase = phase;
      c.state = ClientState::seated;
      push(c, SessionEventType::welcome, me);
      return;
    }
    case kPlayer:
      on_player(c, r);
      return;
    case kGone: {
      const int seat = seat_of(read_u8(r));
      const auto why = static_cast<SessionWhy>(read_u8(r));
      if (r.bad || seat < 0 || seat >= kMaxPlayers) break;
      c.players[seat] = Player{};
      push(c, SessionEventType::left, seat).why = why;
      return;
    }
    case kPings: {
      const int n = read_u8(r);
      for (int i = 0; i < n && !r.bad; i++) {
        const int seat = read_u8(r);
        const int ms = read_u16(r);
        if (!r.bad && seat < kMaxPlayers) c.players[seat].ping_ms = ms;
      }
      if (r.bad) break;
      return;
    }
    case kChatOut: {
      const int from = seat_of(read_u8(r));
      const auto scope = static_cast<ChatScope>(read_u8(r));
      const int to = seat_of(read_u8(r));
      size_t n = 0;
      const uint8_t* text = read_text(r, kMaxMessage, n);
      if (r.bad || static_cast<uint8_t>(scope) > static_cast<uint8_t>(ChatScope::nearby)) break;
      SessionEvent& e = push(c, SessionEventType::chat, from);
      e.scope = scope;
      e.other = to;
      e.offset = static_cast<uint32_t>(c.event_bytes.size());
      e.size = static_cast<uint32_t>(n);
      c.event_bytes.insert(c.event_bytes.end(), text, text + n);
      c.event_bytes.push_back(0);
      return;
    }
    case kSettingsOut: {
      size_t n = 0;
      const uint8_t* bytes = read_text(r, kMaxMessage, n);
      if (r.bad) break;
      c.settings.assign(bytes, bytes + n);
      push(c, SessionEventType::settings, kNoPlayer);
      return;
    }
    case kPhase: {
      const auto phase = static_cast<SessionPhase>(read_u8(r));
      if (r.bad) break;
      if (phase != c.phase) {
        c.phase = phase;
        push(c, SessionEventType::phase, kNoPlayer);
      }
      return;
    }
    case kKicked: {
      const auto why = static_cast<SessionWhy>(read_u8(r));
      if (r.bad) break;
      push(c, SessionEventType::kicked, c.me).why = why;
      lose(c, why);
      return;
    }
    case kRefused: {
      const auto what = static_cast<SessionAsk>(read_u8(r));
      const auto why = static_cast<SessionWhy>(read_u8(r));
      if (r.bad) break;
      SessionEvent& e = push(c, SessionEventType::refused, c.me);
      e.ask = what;
      e.why = why;
      if (what == SessionAsk::join) lose(c, why);
      return;
    }
    default:
      break;
  }
  c.malformed++;
}

}  // namespace

void session_client_init(SessionClient& client, Node& node, int channel) {
  client = SessionClient{};
  client.node = &node;
  client.channel = channel;
}

bool session_client_join(SessionClient& c, const Address& host, const char* name, uint64_t token,
                         int64_t now_us) {
  c.host = host;
  c.token = token;
  const size_t n =
      clean_text(reinterpret_cast<const uint8_t*>(name), std::strlen(name), kMaxName, c.name);
  c.name[n] = 0;
  c.me = kNoPlayer;
  c.lost_at = c.tried_at = now_us;
  c.peer = node_connect(*c.node, host, now_us);
  c.state = token ? ClientState::reconnecting : ClientState::joining;
  return c.peer >= 0;
}

bool session_client_attach(SessionClient& c, int peer, const char* name, uint64_t token,
                           int64_t now_us) {
  if (!peer_connected(*c.node, peer)) return false;
  c.host = c.node->peers[static_cast<size_t>(peer)].address;
  c.token = token;
  const size_t n =
      clean_text(reinterpret_cast<const uint8_t*>(name), std::strlen(name), kMaxName, c.name);
  c.name[n] = 0;
  c.me = kNoPlayer;
  c.lost_at = c.tried_at = now_us;
  c.peer = peer;
  c.state = token ? ClientState::reconnecting : ClientState::joining;
  send_hello(c);
  return true;
}

void session_client_update(SessionClient& c, int64_t now_us) {
  c.events.clear();
  c.event_bytes.clear();
  for (const Event& e : c.node->events) {
    if (e.peer != c.peer || c.peer < 0) continue;
    switch (e.type) {
      case EventType::connected:
        if (c.state == ClientState::joining || c.state == ClientState::reconnecting) send_hello(c);
        break;
      case EventType::message:
        if (e.channel == c.channel) on_message(c, event_data(*c.node, e), e.size);
        break;
      case EventType::disconnected:
      case EventType::connect_failed:
        c.peer = -1;
        if (c.state == ClientState::lost || c.state == ClientState::idle) break;
        if (e.reason == Reason::remote_closed || c.token == 0) {
          // The host closed the session, or a first join never got in.
          lose(c, SessionWhy::none);
        } else if (c.state != ClientState::reconnecting) {
          c.state = ClientState::reconnecting;
          c.lost_at = c.tried_at = now_us;
          push(c, SessionEventType::away, c.me);
        }
        break;
    }
  }
  if (c.state == ClientState::reconnecting) {
    if (now_us - c.lost_at > c.reconnect_us) {
      // The window is over: give up, a connect still trying included.
      if (c.peer >= 0) node_disconnect(*c.node, c.peer, now_us);
      c.peer = -1;
      lose(c, SessionWhy::timed_out);
    } else if (c.peer < 0 && now_us - c.tried_at >= c.retry_us) {
      c.tried_at = now_us;
      c.peer = node_connect(*c.node, c.host, now_us);
    }
  }
}

bool session_client_ready(SessionClient& c, bool ready) {
  const uint8_t buf[2] = {kReady, static_cast<uint8_t>(ready ? 1 : 0)};
  return ask(c, buf, 2);
}

bool session_client_team(SessionClient& c, int team) {
  const uint8_t buf[2] = {kTeam, seat_byte(team)};
  return ask(c, buf, 2);
}

bool session_client_chat(SessionClient& c, ChatScope scope, int to, const char* text, size_t size) {
  if (size > kMaxMessage - 16) return false;
  uint8_t buf[kMaxMessage];
  Writer w = make_writer(buf, sizeof buf);
  write_u8(w, kChat);
  write_u8(w, static_cast<uint8_t>(scope));
  write_u8(w, seat_byte(to));
  write_text(w, text, size);
  return !w.overflow && ask(c, buf, w.size);
}

bool session_client_kick(SessionClient& c, int player) {
  const uint8_t buf[2] = {kKick, seat_byte(player)};
  return ask(c, buf, 2);
}

bool session_client_start(SessionClient& c) {
  const uint8_t buf[1] = {kStart};
  return ask(c, buf, 1);
}

bool session_client_return(SessionClient& c) {
  const uint8_t buf[1] = {kBack};
  return ask(c, buf, 1);
}

bool session_client_settings(SessionClient& c, const uint8_t* data, size_t size) {
  if (size > kMaxMessage - 16) return false;
  uint8_t buf[kMaxMessage];
  Writer w = make_writer(buf, sizeof buf);
  write_u8(w, kSettings);
  write_text(w, data, size);
  return !w.overflow && ask(c, buf, w.size);
}

void session_client_leave(SessionClient& c, int64_t now_us) {
  if (c.peer >= 0) node_disconnect(*c.node, c.peer, now_us);
  c.peer = -1;
  c.state = ClientState::idle;
  c.token = 0;
  c.me = kNoPlayer;
  for (Player& p : c.players) p = Player{};
}

const char* why_name(SessionWhy w) {
  switch (w) {
    case SessionWhy::none:
      return "none";
    case SessionWhy::full:
      return "full";
    case SessionWhy::in_progress:
      return "in_progress";
    case SessionWhy::banned:
      return "banned";
    case SessionWhy::not_leader:
      return "not_leader";
    case SessionWhy::team_full:
      return "team_full";
    case SessionWhy::no_team:
      return "no_team";
    case SessionWhy::not_ready:
      return "not_ready";
    case SessionWhy::too_few:
      return "too_few";
    case SessionWhy::rate:
      return "rate";
    case SessionWhy::no_player:
      return "no_player";
    case SessionWhy::too_big:
      return "too_big";
    case SessionWhy::kicked:
      return "kicked";
    case SessionWhy::left:
      return "left";
    case SessionWhy::timed_out:
      return "timed_out";
    case SessionWhy::wrong_phase:
      return "wrong_phase";
    case SessionWhy::no_nearby:
      return "no_nearby";
  }
  return "?";
}

const char* ask_name(SessionAsk a) {
  switch (a) {
    case SessionAsk::join:
      return "join";
    case SessionAsk::ready:
      return "ready";
    case SessionAsk::team:
      return "team";
    case SessionAsk::chat:
      return "chat";
    case SessionAsk::kick:
      return "kick";
    case SessionAsk::start:
      return "start";
    case SessionAsk::back:
      return "back";
    case SessionAsk::settings:
      return "settings";
  }
  return "?";
}

const char* phase_name(SessionPhase p) { return p == SessionPhase::lobby ? "lobby" : "playing"; }

const char* scope_name(ChatScope s) {
  switch (s) {
    case ChatScope::all:
      return "all";
    case ChatScope::team:
      return "team";
    case ChatScope::direct:
      return "direct";
    case ChatScope::nearby:
      return "nearby";
  }
  return "?";
}

const char* session_event_name(SessionEventType t) {
  switch (t) {
    case SessionEventType::welcome:
      return "welcome";
    case SessionEventType::joined:
      return "joined";
    case SessionEventType::rejoined:
      return "rejoined";
    case SessionEventType::away:
      return "away";
    case SessionEventType::left:
      return "left";
    case SessionEventType::changed:
      return "changed";
    case SessionEventType::chat:
      return "chat";
    case SessionEventType::settings:
      return "settings";
    case SessionEventType::phase:
      return "phase";
    case SessionEventType::refused:
      return "refused";
    case SessionEventType::kicked:
      return "kicked";
    case SessionEventType::lost:
      return "lost";
  }
  return "?";
}

const char* client_state_name(ClientState s) {
  switch (s) {
    case ClientState::idle:
      return "idle";
    case ClientState::joining:
      return "joining";
    case ClientState::seated:
      return "seated";
    case ClientState::reconnecting:
      return "reconnecting";
    case ClientState::lost:
      return "lost";
  }
  return "?";
}

}  // namespace ge::net
