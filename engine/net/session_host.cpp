// The session's host: seats, the leader, teams, the phases, chat relayed, kicks and bans, pings,
// and the away seats kept for their tokens (net/session.hpp states the rules).
//
// ge:summary The session host: seats players, holds the lobby's rules, relays chat, kicks, pings
// ge:keywords net session host lobby seat leader team chat relay rate-limit kick ban token
// ge:keywords reconnect away ping roster dedicated-server listen-server
// ge:skill net
// ge:test tests/net_session_test.cpp
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "net/session.hpp"
#include "net/session_wire.hpp"

namespace ge::net {

namespace {

using namespace session_wire;

constexpr size_t kOut = kMaxMessage;

int team_cap(const SessionConfig& c) {
  if (c.team_count <= 0) return 0;
  return c.team_size > 0 ? c.team_size : (c.max_players + c.team_count - 1) / c.team_count;
}

bool host_seat(const SessionHost& h, int seat) { return h.config.host_plays && seat == 0; }

void send_seat(SessionHost& h, int seat, const uint8_t* data, size_t size) {
  const int peer = h.seat_peer[seat];
  if (peer >= 0) node_send(*h.node, peer, h.config.channel, data, size);
}

// To every present seat with a peer; `except` none when -1.
int send_all(SessionHost& h, const uint8_t* data, size_t size, int except = -1) {
  int sent = 0;
  for (int s = 0; s < h.config.max_players; s++) {
    if (s == except || !h.players[s].used || h.seat_peer[s] < 0) continue;
    send_seat(h, s, data, size);
    sent++;
  }
  return sent;
}

SessionEvent& push(SessionHost& h, SessionEventType type, int player) {
  SessionEvent e;
  e.type = type;
  e.player = player;
  h.events.push_back(e);
  return h.events.back();
}

void announce(SessionHost& h, int seat, uint8_t cause) {
  uint8_t buf[64];
  Writer w = make_writer(buf, sizeof buf);
  write_player(w, seat, cause, h.players[seat]);
  send_all(h, buf, w.size);
}

void send_phase(SessionHost& h, int seat) {
  const uint8_t buf[2] = {kPhase, static_cast<uint8_t>(h.phase)};
  if (seat < 0) {
    send_all(h, buf, 2);
  } else {
    send_seat(h, seat, buf, 2);
  }
}

void send_settings(SessionHost& h, int seat) {
  std::vector<uint8_t> buf(h.settings.size() + 12);
  Writer w = make_writer(buf.data(), buf.size());
  write_u8(w, kSettingsOut);
  write_text(w, h.settings.data(), h.settings.size());
  if (seat < 0) {
    send_all(h, buf.data(), w.size);
  } else {
    send_seat(h, seat, buf.data(), w.size);
  }
}

void refuse(SessionHost& h, int peer, SessionAsk ask, SessionWhy why) {
  const uint8_t buf[3] = {kRefused, static_cast<uint8_t>(ask), static_cast<uint8_t>(why)};
  node_send(*h.node, peer, h.config.channel, buf, 3);
}

// Keep the leader while present; otherwise the longest-seated present player leads.
void elect(SessionHost& h) {
  int now = -1, best = -1;
  for (int s = 0; s < h.config.max_players; s++) {
    const Player& p = h.players[s];
    if (!p.used) continue;
    if (p.leader) now = s;
    if (!p.present) continue;
    if (host_seat(h, s)) {
      best = s;
      break;
    }
    if (best < 0 || p.joined_order < h.players[best].joined_order) best = s;
  }
  if (now >= 0 && h.players[now].present) return;
  if (now == best) return;
  if (now >= 0) {
    h.players[now].leader = false;
    announce(h, now, kChanged);
    push(h, SessionEventType::changed, now);
  }
  if (best >= 0) {
    h.players[best].leader = true;
    announce(h, best, kChanged);
    push(h, SessionEventType::changed, best);
  }
}

void free_seat(SessionHost& h, int seat, SessionWhy why) {
  const int peer = h.seat_peer[seat];
  if (peer >= 0) h.peer_seat[peer] = kNoPlayer;
  h.players[seat] = Player{};
  h.tokens[seat] = 0;
  h.seat_peer[seat] = -1;
  const uint8_t buf[3] = {kGone, static_cast<uint8_t>(seat), static_cast<uint8_t>(why)};
  send_all(h, buf, 3);
  push(h, SessionEventType::left, seat).why = why;
  h.stats.leaves++;
  elect(h);
}

int smallest_team(const SessionHost& h) {
  int best = kNoTeam, best_n = 0;
  for (int t = 0; t < h.config.team_count; t++) {
    int n = 0;
    for (int s = 0; s < h.config.max_players; s++) n += h.players[s].used && h.players[s].team == t;
    if (best == kNoTeam || n < best_n) {
      best = t;
      best_n = n;
    }
  }
  return best;
}

// A fresh token, never 0 (a new player's) and never one in use.
uint64_t new_token(SessionHost& h) {
  for (;;) {
    const auto hi = static_cast<uint64_t>(core::next(h.rng) * 4294967296.0);
    const auto lo = static_cast<uint64_t>(core::next(h.rng) * 4294967296.0);
    const uint64_t t = hi << 32 | lo;
    if (t == 0) continue;
    bool used = false;
    for (int s = 0; s < h.config.max_players; s++) used |= h.tokens[s] == t;
    if (!used) return t;
  }
}

void set_name(Player& p, const uint8_t* name, size_t size, int seat) {
  char tmp[kMaxName + 1] = {};
  size_t n = clean_text(name, size, kMaxName, tmp);
  size_t start = 0;
  while (start < n && tmp[start] == ' ') start++;
  while (n > start && tmp[n - 1] == ' ') n--;
  if (n == start) {
    std::snprintf(p.name, sizeof p.name, "player %d", seat + 1);
    return;
  }
  std::memcpy(p.name, tmp + start, n - start);
  p.name[n - start] = 0;
}

// The seat's welcome: who it is, everyone already seated, the settings and the phase.
void welcome(SessionHost& h, int seat) {
  uint8_t buf[32];
  Writer w = make_writer(buf, sizeof buf);
  write_u8(w, kWelcome);
  write_u8(w, static_cast<uint8_t>(seat));
  write_u64(w, h.tokens[seat]);
  write_u8(w, static_cast<uint8_t>(h.config.max_players));
  write_u8(w, static_cast<uint8_t>(h.config.team_count));
  write_u8(w, static_cast<uint8_t>(h.phase));
  send_seat(h, seat, buf, w.size);
  for (int s = 0; s < h.config.max_players; s++) {
    if (s == seat || !h.players[s].used) continue;
    uint8_t pb[64];
    Writer pw = make_writer(pb, sizeof pb);
    write_player(pw, s, kSnapshot, h.players[s]);
    send_seat(h, seat, pb, pw.size);
  }
  if (!h.settings.empty()) send_settings(h, seat);
  send_phase(h, seat);
}

bool banned(const SessionHost& h, uint64_t token, const Address& from) {
  if (token &&
      std::find(h.banned_tokens.begin(), h.banned_tokens.end(), token) != h.banned_tokens.end())
    return true;
  return std::find(h.banned_addresses.begin(), h.banned_addresses.end(), from) !=
         h.banned_addresses.end();
}

void let_go(SessionHost& h, int peer, int64_t now_us) {
  h.peer_drop_at[peer] = now_us + h.config.linger_us;
}

void on_hello(SessionHost& h, int peer, Reader& r, int64_t now_us) {
  const uint64_t token = read_u64(r);
  size_t name_size = 0;
  const uint8_t* name = read_text(r, 255, name_size);
  if (r.bad) {
    h.stats.malformed++;
    return;
  }
  const Address& from = h.node->peers[peer].address;
  auto refuse_join = [&](SessionWhy why) {
    refuse(h, peer, SessionAsk::join, why);
    let_go(h, peer, now_us);
    h.stats.refused_joins++;
    SessionEvent& e = push(h, SessionEventType::refused, kNoPlayer);
    e.ask = SessionAsk::join;
    e.why = why;
  };
  if (banned(h, token, from)) return refuse_join(SessionWhy::banned);
  // Coming back: the seat the token names, away or still held by a peer not yet timed out.
  if (token) {
    for (int s = 0; s < h.config.max_players; s++) {
      if (!h.players[s].used || h.tokens[s] != token) continue;
      const int old = h.seat_peer[s];
      if (old >= 0 && old != peer) {
        node_disconnect(*h.node, old, now_us);
        h.peer_seat[old] = kNoPlayer;
      }
      h.seat_peer[s] = peer;
      h.peer_seat[peer] = s;
      h.players[s].present = true;
      h.stats.rejoins++;
      welcome(h, s);
      announce(h, s, kRejoined);
      push(h, SessionEventType::rejoined, s);
      elect(h);
      return;
    }
  }
  if (h.phase == SessionPhase::playing && !h.config.join_in_progress)
    return refuse_join(SessionWhy::in_progress);
  int seat = kNoPlayer;
  for (int s = 0; s < h.config.max_players && seat < 0; s++) {
    if (!h.players[s].used) seat = s;
  }
  if (seat < 0) return refuse_join(SessionWhy::full);
  Player& p = h.players[seat];
  p = Player{};
  p.used = true;
  p.present = true;
  p.joined_order = h.next_order++;
  p.team = h.config.team_count > 0 ? smallest_team(h) : kNoTeam;
  set_name(p, name, name_size, seat);
  h.tokens[seat] = new_token(h);
  h.seat_peer[seat] = peer;
  h.peer_seat[peer] = seat;
  h.chat_tokens[seat] = h.config.chat_burst;
  h.chat_at[seat] = now_us;
  h.stats.joins++;
  welcome(h, seat);
  announce(h, seat, kJoined);
  push(h, SessionEventType::joined, seat);
  elect(h);
}

void answer(SessionHost& h, int seat, SessionAsk ask, SessionWhy why) {
  if (why == SessionWhy::none) return;
  if (h.seat_peer[seat] >= 0) refuse(h, h.seat_peer[seat], ask, why);
  SessionEvent& e = push(h, SessionEventType::refused, seat);
  e.ask = ask;
  e.why = why;
}

void on_message(SessionHost& h, int peer, const uint8_t* data, size_t size, int64_t now_us) {
  if (h.peer_drop_at[peer]) return;  // being let go: nothing more it says counts
  Reader r = make_reader(data, size);
  const uint8_t kind = read_u8(r);
  const int seat = h.peer_seat[peer];
  if (kind == kHello) {
    if (seat == kNoPlayer) {
      on_hello(h, peer, r, now_us);
    } else {
      h.stats.malformed++;
    }
    return;
  }
  if (seat == kNoPlayer || r.bad) {
    h.stats.malformed++;
    return;
  }
  switch (kind) {
    case kReady: {
      const uint8_t v = read_u8(r);
      if (!r.bad) answer(h, seat, SessionAsk::ready, session_host_ready(h, seat, v != 0));
      break;
    }
    case kTeam: {
      const int t = seat_of(read_u8(r));
      if (!r.bad) answer(h, seat, SessionAsk::team, session_host_team(h, seat, t));
      break;
    }
    case kChat: {
      const auto scope = static_cast<ChatScope>(read_u8(r));
      const int to = seat_of(read_u8(r));
      size_t n = 0;
      const uint8_t* text = read_text(r, kMaxMessage, n);
      if (r.bad || static_cast<uint8_t>(scope) > static_cast<uint8_t>(ChatScope::direct)) break;
      answer(h, seat, SessionAsk::chat,
             session_host_chat(h, seat, scope, to, reinterpret_cast<const char*>(text), n, now_us));
      return;
    }
    case kKick: {
      const int who = seat_of(read_u8(r));
      if (!r.bad) answer(h, seat, SessionAsk::kick, session_host_kick(h, seat, who, now_us));
      break;
    }
    case kStart:
      answer(h, seat, SessionAsk::start, session_host_start(h, seat));
      return;
    case kBack:
      answer(h, seat, SessionAsk::back, session_host_return(h, seat));
      return;
    case kSettings: {
      size_t n = 0;
      const uint8_t* bytes = read_text(r, kMaxMessage, n);
      if (!r.bad) answer(h, seat, SessionAsk::settings, session_host_settings(h, seat, bytes, n));
      break;
    }
    default:
      r.bad = true;
  }
  if (r.bad) h.stats.malformed++;
}

// Every reliable message queued to the peer has been acked.
bool all_acked(const Node& node, int peer) {
  for (const Connection::Out& o : node.peers[peer].conn.out) {
    if (o.used) return false;
  }
  return true;
}

}  // namespace

void session_host_init(SessionHost& host, const SessionConfig& config, Node& node, int64_t now_us) {
  host = SessionHost{};
  host.config = config;
  host.config.max_players = std::clamp(config.max_players, 1, kMaxPlayers);
  host.config.team_count = std::clamp(config.team_count, 0, host.config.max_players);
  host.config.settings_max = std::min(config.settings_max, kMaxMessage - 8);
  host.config.chat_max = std::min(config.chat_max, kMaxMessage - 8);
  host.node = &node;
  host.rng = core::make_rng(static_cast<uint32_t>(config.seed ^ (config.seed >> 32)));
  for (int s = 0; s < kMaxPlayers; s++) host.seat_peer[s] = -1;
  for (int p = 0; p < kMaxPeers; p++) host.peer_seat[p] = kNoPlayer;
  host.roster_at = now_us;
  if (host.config.host_plays) {
    Player& p = host.players[0];
    p.used = p.present = p.leader = true;
    p.joined_order = host.next_order++;
    p.team = host.config.team_count > 0 ? 0 : kNoTeam;
    const char* name = config.host_name ? config.host_name : "";
    set_name(p, reinterpret_cast<const uint8_t*>(name), std::strlen(name), 0);
    host.tokens[0] = new_token(host);
    host.chat_tokens[0] = host.config.chat_burst;
    host.chat_at[0] = now_us;
  }
}

void session_host_update(SessionHost& h, int64_t now_us) {
  h.events.clear();
  h.event_bytes.clear();
  Node& node = *h.node;
  for (const Event& e : node.events) {
    if (e.peer < 0 || e.peer >= kMaxPeers) continue;
    switch (e.type) {
      case EventType::connected:
        h.peer_seat[e.peer] = kNoPlayer;
        h.peer_since[e.peer] = now_us;
        h.peer_drop_at[e.peer] = 0;
        break;
      case EventType::disconnected: {
        const int seat = h.peer_seat[e.peer];
        h.peer_seat[e.peer] = kNoPlayer;
        h.peer_drop_at[e.peer] = 0;
        if (seat == kNoPlayer) break;
        h.seat_peer[seat] = -1;
        if (e.reason == Reason::remote_closed) {
          free_seat(h, seat, SessionWhy::left);
        } else {
          h.players[seat].present = false;
          h.players[seat].ready = false;
          h.away_since[seat] = now_us;
          announce(h, seat, kAway);
          push(h, SessionEventType::away, seat);
          elect(h);
        }
        break;
      }
      case EventType::message:
        if (e.channel == h.config.channel)
          on_message(h, e.peer, event_data(node, e), e.size, now_us);
        break;
      case EventType::connect_failed:
        break;
    }
  }
  for (int p = 0; p < static_cast<int>(node.peers.size()) && p < kMaxPeers; p++) {
    if (!peer_connected(node, p)) continue;
    if (h.peer_drop_at[p]) {
      // Let go once it has heard why (every reliable message acked), or at the deadline.
      if (now_us >= h.peer_drop_at[p] || all_acked(node, p)) {
        node_disconnect(node, p, now_us);
        h.peer_drop_at[p] = 0;
        h.peer_seat[p] = kNoPlayer;
      }
    } else if (h.peer_seat[p] == kNoPlayer &&
               now_us - h.peer_since[p] > h.config.hello_timeout_us) {
      node_disconnect(node, p, now_us);
    }
  }
  for (int s = 0; s < h.config.max_players; s++) {
    if (h.players[s].used && !h.players[s].present && !host_seat(h, s) &&
        now_us - h.away_since[s] > h.config.reconnect_us)
      free_seat(h, s, SessionWhy::timed_out);
  }
  if (now_us - h.roster_at >= h.config.roster_us) {
    h.roster_at = now_us;
    uint8_t buf[2 + 3 * kMaxPlayers];
    Writer w = make_writer(buf, sizeof buf);
    write_u8(w, kPings);
    int n = 0;
    for (int s = 0; s < h.config.max_players; s++) n += h.players[s].used;
    write_u8(w, static_cast<uint8_t>(n));
    for (int s = 0; s < h.config.max_players; s++) {
      Player& p = h.players[s];
      if (!p.used) continue;
      const int peer = h.seat_peer[s];
      const double rtt = peer >= 0 ? node.peers[peer].conn.stats.rtt_ms : 0;
      p.ping_ms = static_cast<int>(std::min(rtt + 0.5, 65535.0));
      write_u8(w, static_cast<uint8_t>(s));
      write_u16(w, static_cast<uint16_t>(p.ping_ms));
    }
    send_all(h, buf, w.size);
  }
}

SessionWhy session_host_ready(SessionHost& h, int player, bool ready) {
  if (player < 0 || player >= h.config.max_players || !h.players[player].used)
    return SessionWhy::no_player;
  if (h.phase != SessionPhase::lobby) return SessionWhy::wrong_phase;
  if (h.players[player].ready == ready) return SessionWhy::none;
  h.players[player].ready = ready;
  announce(h, player, kChanged);
  push(h, SessionEventType::changed, player);
  return SessionWhy::none;
}

SessionWhy session_host_team(SessionHost& h, int player, int team) {
  if (player < 0 || player >= h.config.max_players || !h.players[player].used)
    return SessionWhy::no_player;
  if (h.config.team_count <= 0 || team < 0 || team >= h.config.team_count)
    return SessionWhy::no_team;
  if (h.phase != SessionPhase::lobby) return SessionWhy::wrong_phase;
  if (h.players[player].team == team) return SessionWhy::none;
  int n = 0;
  for (int s = 0; s < h.config.max_players; s++)
    n += h.players[s].used && h.players[s].team == team;
  if (n >= team_cap(h.config)) return SessionWhy::team_full;
  h.players[player].team = team;
  announce(h, player, kChanged);
  push(h, SessionEventType::changed, player);
  return SessionWhy::none;
}

SessionWhy session_host_start(SessionHost& h, int by) {
  if (by < 0 || by >= h.config.max_players || !h.players[by].leader) return SessionWhy::not_leader;
  if (h.phase != SessionPhase::lobby) return SessionWhy::wrong_phase;
  if (session_player_count(h, true) < h.config.min_players) return SessionWhy::too_few;
  if (h.config.need_ready) {
    for (int s = 0; s < h.config.max_players; s++) {
      const Player& p = h.players[s];
      if (p.used && p.present && !p.ready && !host_seat(h, s)) return SessionWhy::not_ready;
    }
  }
  h.phase = SessionPhase::playing;
  send_phase(h, -1);
  push(h, SessionEventType::phase, by);
  return SessionWhy::none;
}

SessionWhy session_host_return(SessionHost& h, int by) {
  if (by < 0 || by >= h.config.max_players || !h.players[by].leader) return SessionWhy::not_leader;
  if (h.phase != SessionPhase::playing) return SessionWhy::wrong_phase;
  h.phase = SessionPhase::lobby;
  send_phase(h, -1);
  push(h, SessionEventType::phase, by);
  for (int s = 0; s < h.config.max_players; s++) {
    if (!h.players[s].used || !h.players[s].ready) continue;
    h.players[s].ready = false;
    announce(h, s, kChanged);
    push(h, SessionEventType::changed, s);
  }
  return SessionWhy::none;
}

SessionWhy session_host_settings(SessionHost& h, int by, const uint8_t* data, size_t size) {
  if (by < 0 || by >= h.config.max_players || !h.players[by].leader) return SessionWhy::not_leader;
  if (size > h.config.settings_max) return SessionWhy::too_big;
  h.settings.assign(data, data + size);
  send_settings(h, -1);
  push(h, SessionEventType::settings, by);
  return SessionWhy::none;
}

SessionWhy session_host_kick(SessionHost& h, int by, int player, int64_t now_us) {
  if (by < 0 || by >= h.config.max_players || !h.players[by].leader) return SessionWhy::not_leader;
  if (player < 0 || player >= h.config.max_players || !h.players[player].used || player == by ||
      host_seat(h, player))
    return SessionWhy::no_player;
  h.banned_tokens.push_back(h.tokens[player]);
  const int peer = h.seat_peer[player];
  if (peer >= 0) {
    h.banned_addresses.push_back(h.node->peers[peer].address);
    const uint8_t buf[2] = {kKicked, static_cast<uint8_t>(SessionWhy::kicked)};
    node_send(*h.node, peer, h.config.channel, buf, 2);
    let_go(h, peer, now_us);
    // The seat goes now; the peer stays only to hear why.
    h.seat_peer[player] = -1;
    h.peer_seat[peer] = kNoPlayer;
  }
  h.stats.kicks++;
  free_seat(h, player, SessionWhy::kicked);
  return SessionWhy::none;
}

SessionWhy session_host_chat(SessionHost& h, int from, ChatScope scope, int to, const char* text,
                             size_t size, int64_t now_us) {
  if (from < 0 || from >= h.config.max_players || !h.players[from].used)
    return SessionWhy::no_player;
  const Player& sender = h.players[from];
  if (scope == ChatScope::team && sender.team == kNoTeam) {
    h.stats.chat_refused++;
    return SessionWhy::no_team;
  }
  if (scope == ChatScope::direct &&
      (to < 0 || to >= h.config.max_players || to == from || !h.players[to].used)) {
    h.stats.chat_refused++;
    return SessionWhy::no_player;
  }
  // The bucket: refilled by the time since the last line, at most chat_burst.
  double& tokens = h.chat_tokens[from];
  if (h.config.chat_refill_us > 0) {
    tokens += static_cast<double>(now_us - h.chat_at[from]) /
              static_cast<double>(h.config.chat_refill_us);
  }
  tokens = std::min(tokens, static_cast<double>(h.config.chat_burst));
  h.chat_at[from] = now_us;
  if (tokens < 1) {
    h.stats.chat_refused++;
    return SessionWhy::rate;
  }
  tokens -= 1;
  std::vector<char> line(std::min(size, h.config.chat_max) + 1);
  const size_t n =
      clean_text(reinterpret_cast<const uint8_t*>(text), size, h.config.chat_max, line.data());
  std::vector<uint8_t> buf(n + 16);
  Writer w = make_writer(buf.data(), buf.size());
  write_u8(w, kChatOut);
  write_u8(w, static_cast<uint8_t>(from));
  write_u8(w, static_cast<uint8_t>(scope));
  write_u8(w, seat_byte(scope == ChatScope::direct ? to : kNoPlayer));
  write_text(w, line.data(), n);
  int copies = 0;
  for (int s = 0; s < h.config.max_players; s++) {
    const Player& p = h.players[s];
    if (!p.used || h.seat_peer[s] < 0) continue;
    const bool reach = scope == ChatScope::all || s == from ||
                       (scope == ChatScope::team && p.team == sender.team) ||
                       (scope == ChatScope::direct && s == to);
    if (!reach) continue;
    send_seat(h, s, buf.data(), w.size);
    copies++;
  }
  h.stats.chat_relayed++;
  h.stats.chat_copies += static_cast<uint64_t>(copies);
  SessionEvent& e = push(h, SessionEventType::chat, from);
  e.scope = scope;
  e.other = scope == ChatScope::direct ? to : kNoPlayer;
  e.offset = static_cast<uint32_t>(h.event_bytes.size());
  e.size = static_cast<uint32_t>(n);
  h.event_bytes.insert(h.event_bytes.end(), line.data(), line.data() + n);
  h.event_bytes.push_back(0);
  return SessionWhy::none;
}

int session_player_count(const SessionHost& h, bool present_only) {
  int n = 0;
  for (int s = 0; s < h.config.max_players; s++) {
    n += h.players[s].used && (!present_only || h.players[s].present);
  }
  return n;
}

int session_leader(const SessionHost& h) {
  for (int s = 0; s < h.config.max_players; s++) {
    if (h.players[s].used && h.players[s].leader) return s;
  }
  return kNoPlayer;
}

}  // namespace ge::net
