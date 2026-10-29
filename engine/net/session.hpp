// SESSIONS ROUND THE GAME: the lobby a game is set up in, the players in it, their teams and
// their chat, over a node's reliable channel. The host's process is the authority: it seats each
// player, keeps the roster, relays every chat line and holds every rule below; a client asks and
// mirrors what the host tells it.
//
// - PLAYERS. A player is a SEAT (0..max_players-1), not a peer slot, so a player who drops and
//   comes back keeps the seat, the name, the team and the readiness. A joining client says hello
//   with its name and, coming back, the TOKEN the host gave it (64 random bits off the host's
//   stream). A player whose peer drops is AWAY for `reconnect_us`; a hello with the token in that
//   time takes the seat back (`rejoined`), after it the seat is freed (`left`). A client that
//   loses its host tries again every `retry_us` until `reconnect_us` has passed.
// - THE LEADER. One player may change the game's settings, kick, start and return to the lobby:
//   the host's own player when the host plays (a listen server), otherwise the first to join (a
//   dedicated server). When the leader leaves, the longest-seated player present leads.
// - TEAMS. `team_count` teams (0: none) of at most `team_size` (0: as many as fit evenly). A new
//   player joins the smallest team; a player asks to move and is refused into a full team or
//   once the game has started.
// - THE GAME'S SETTINGS are the game's bytes (a map, a mode, a skier), at most `settings_max`;
//   the host keeps the leader's last and sends them to everyone who joins.
// - PHASES. `lobby` → `playing` when the leader starts (refused below `min_players`, or while a
//   player is not ready when `need_ready`), and back to `lobby` when the leader returns (every
//   player unready again). A new player joins a game in progress only when `join_in_progress`;
//   a returning one always may.
// - CHAT to everyone, to the sender's team, to one player (DIRECT) or to the players NEAR the
//   sender in the game (proximity chat, whom `hears` says can hear them: the game knows where
//   everyone is and how far a voice carries). The host strips control bytes, caps a line at
//   `chat_max` bytes, and holds each player to a token bucket (`chat_burst` lines, one more every
//   `chat_refill_us`); a line refused is answered with why. A direct line reaches only its target.
// - KICK. The leader removes a player: the host tells them why, drops their peer once the word is
//   acked (or `linger_us` has passed), and refuses their token and their address for the rest of
//   the session.
// - PINGS. Every `roster_us` the host sends each player's round trip as it measures it, so every
//   client's player list shows everyone's ping.
//
// A frame's use, after node_update and before the game reads the node's events (the game skips
// `channel`, which is the session's):
//   session_host_update(host, now)      or  session_client_update(client, now)
//   for (const SessionEvent& e : host.events) ...  (session_text for a chat line's bytes)
// The session queues on the node; the game's node_flush sends it.
//
// The clock is the caller's, the tokens come off `seed`: a session over the simulated network
// replays to the bit (the `net-session` lab).
//
// ge:summary Sessions: the lobby, players by seat, teams, chat, kick, start, reconnect by token
// ge:keywords net session lobby player roster seat team ready leader host-player dedicated-server
// ge:keywords listen-server chat private-message direct-message whisper team-chat rate-limit
// ge:keywords proximity-chat nearby say-prox earshot
// ge:keywords kick ban start return phase settings reconnect token away ping player-list
// ge:keywords join-in-progress multiplayer
// ge:skill net
// ge:test tests/net_session_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "net/node.hpp"

namespace ge::net {

constexpr int kMaxPlayers = 32;
constexpr size_t kMaxName = 24;  // bytes of a player's name
constexpr int kNoPlayer = -1;
constexpr int kNoTeam = -1;

// Whether seat `listener` hears seat `speaker` in the game (proximity chat). The game's: it knows
// where both are and how far a voice carries (its own range setting).
using HearsFn = bool (*)(void* ctx, int speaker, int listener);

struct SessionConfig {
  int channel = 0;  // a RELIABLE channel of the node's ChannelConfig, the session's alone
  int max_players = kMaxPlayers;
  int min_players = 1;  // the fewest the leader may start with
  int team_count = 0;   // 0: no teams
  int team_size = 0;    // 0: max_players / team_count, rounded up
  bool need_ready = true;
  bool join_in_progress = false;
  bool host_plays = false;  // a listen server: the host is seat 0 and leads
  const char* host_name = "host";
  int64_t reconnect_us = 30000000;     // an away seat is kept this long
  int64_t hello_timeout_us = 5000000;  // a peer that says no hello by then is dropped
  int64_t roster_us = 1000000;         // pings sent this often
  int64_t linger_us = 2000000;         // the longest a refused or kicked peer is kept to hear why
  size_t chat_max = 200;               // bytes of one chat line
  int chat_burst = 5;                  // lines a player may send at once
  int64_t chat_refill_us = 1000000;    // one more line this often
  size_t settings_max = 1024;
  HearsFn hears = nullptr;  // proximity chat's earshot (null: this session has none)
  void* hears_ctx = nullptr;
  uint64_t seed = 0;  // the tokens' stream (a real host passes os_entropy())
};

enum class SessionPhase : uint8_t { lobby, playing };
enum class ChatScope : uint8_t { all, team, direct, nearby };

// Why the host refused or removed something.
enum class SessionWhy : uint8_t {
  none,
  full,         // every seat taken
  in_progress,  // the game has started and takes no new players
  banned,       // kicked earlier this session
  not_leader,   // only the leader may
  team_full,
  no_team,      // a team that does not exist, or this session has none
  not_ready,    // a player not ready (need_ready)
  too_few,      // fewer than min_players
  rate,         // chat faster than the bucket allows
  no_player,    // a direct line to an empty seat
  too_big,      // settings past settings_max
  kicked,       // removed by the leader
  left,         // said goodbye (the peer disconnected)
  timed_out,    // away past reconnect_us
  wrong_phase,  // start while playing, return in the lobby, teams moved in a game
  no_nearby,    // proximity chat in a session whose game gave no earshot (SessionConfig::hears)
};
const char* why_name(SessionWhy w);

// What a client asked, named in a refusal.
enum class SessionAsk : uint8_t { join, ready, team, chat, kick, start, back, settings };
const char* ask_name(SessionAsk a);
const char* phase_name(SessionPhase p);
const char* scope_name(ChatScope s);

enum class SessionEventType : uint8_t {
  welcome,   // client: seated (`player` is this client's seat)
  joined,    // a player took a seat
  rejoined,  // an away player came back to their seat
  away,      // a player's peer dropped; the seat is kept
  left,      // a seat freed (`why`: left, kicked, timed_out)
  changed,   // a player's team, readiness or leadership changed
  chat,      // a line (`player` from, `other` the target of a direct line)
  settings,  // the game's settings changed (session_settings for the bytes)
  phase,     // the phase changed
  refused,   // client: the host refused this client's ask (`why`); host: it refused a player
  kicked,    // client: this client was removed (`why`)
  lost,      // client: the host is gone for good (reconnect window over, or refused)
};
const char* session_event_name(SessionEventType t);

struct SessionEvent {
  SessionEventType type;
  int player = kNoPlayer;
  int other = kNoPlayer;
  ChatScope scope = ChatScope::all;
  SessionWhy why = SessionWhy::none;
  SessionAsk ask = SessionAsk::join;  // refused: what was asked
  uint32_t offset = 0, size = 0;      // chat: the line's bytes in the session's event buffer
};

// One seat as the host keeps it and every client mirrors it.
struct Player {
  bool used = false;
  bool present = false;  // connected now (false: away)
  bool ready = false;
  bool leader = false;
  int team = kNoTeam;
  int ping_ms = 0;
  uint32_t joined_order = 0;  // the leader passes to the lowest present
  char name[kMaxName + 1] = {};
};

// What the host holds and did, for the labs and the scoreboard.
struct SessionStats {
  uint64_t joins = 0, rejoins = 0, leaves = 0, kicks = 0, refused_joins = 0;
  uint64_t chat_relayed = 0;  // lines relayed (a line to n players counts once)
  uint64_t chat_copies = 0;   // copies sent
  uint64_t chat_refused = 0;  // lines refused (rate, no player)
  uint64_t asks_refused = 0;  // other asks refused (team, start, kick, settings)
  uint64_t malformed = 0;     // session messages that did not read
};

struct SessionHost {
  SessionConfig config;
  Node* node = nullptr;
  core::Rng rng{0};
  SessionPhase phase = SessionPhase::lobby;
  Player players[kMaxPlayers];
  // Host-only per seat.
  uint64_t tokens[kMaxPlayers] = {};
  int seat_peer[kMaxPlayers] = {};  // the seat's peer, or -1 (away, or the host's own seat)
  int64_t away_since[kMaxPlayers] = {};
  double chat_tokens[kMaxPlayers] = {};
  int64_t chat_at[kMaxPlayers] = {};
  // Per peer slot: its seat (or -1 before its hello), when it connected, and whether it is being
  // let go (refused or kicked) and by when.
  int peer_seat[kMaxPeers] = {};
  int64_t peer_since[kMaxPeers] = {};
  int64_t peer_drop_at[kMaxPeers] = {};  // 0: kept
  uint32_t next_order = 0;
  std::vector<uint8_t> settings;
  std::vector<uint64_t> banned_tokens;
  std::vector<Address> banned_addresses;
  int64_t roster_at = 0;
  std::vector<SessionEvent> events;
  std::vector<uint8_t> event_bytes;
  SessionStats stats;
};

enum class ClientState : uint8_t { idle, joining, seated, reconnecting, lost };
const char* client_state_name(ClientState s);

struct SessionClient {
  int channel = 0;
  int64_t reconnect_us = 30000000;
  int64_t retry_us = 1000000;
  Node* node = nullptr;
  ClientState state = ClientState::idle;
  Address host;
  int peer = -1;
  int me = kNoPlayer;
  uint64_t token = 0;
  char name[kMaxName + 1] = {};
  int max_players = 0, team_count = 0;
  SessionPhase phase = SessionPhase::lobby;
  Player players[kMaxPlayers];
  std::vector<uint8_t> settings;
  int64_t lost_at = 0, tried_at = 0;
  std::vector<SessionEvent> events;
  std::vector<uint8_t> event_bytes;
  uint64_t malformed = 0;
};

// HOST. The node must accept (NodeConfig::accept) and carry `config.channel` as reliable; with
// `host_plays` the host's own player is seated at once as seat 0 and leads.
void session_host_init(SessionHost& host, const SessionConfig& config, Node& node, int64_t now_us);
void session_host_update(SessionHost& host, int64_t now_us);
// The leader's acts and the host's own player's, from the host's process (a dedicated server's
// console, a listen server's own lobby). `by` is the seat acting: the checks are a client's.
SessionWhy session_host_kick(SessionHost& host, int by, int player, int64_t now_us);
SessionWhy session_host_start(SessionHost& host, int by);
SessionWhy session_host_return(SessionHost& host, int by);
SessionWhy session_host_settings(SessionHost& host, int by, const uint8_t* data, size_t size);
SessionWhy session_host_team(SessionHost& host, int player, int team);
SessionWhy session_host_ready(SessionHost& host, int player, bool ready);
SessionWhy session_host_chat(SessionHost& host, int from, ChatScope scope, int to, const char* text,
                             size_t size, int64_t now_us);
int session_player_count(const SessionHost& host, bool present_only);
int session_leader(const SessionHost& host);
// The seat whose peer this is, or kNoPlayer.
inline int session_seat_of_peer(const SessionHost& host, int peer) {
  return peer >= 0 && peer < kMaxPeers ? host.peer_seat[peer] : kNoPlayer;
}

// CLIENT. The node connects to `host` itself; `token` 0 is a new player, a token kept from an
// earlier welcome comes back to its seat.
void session_client_init(SessionClient& client, Node& node, int channel);
bool session_client_join(SessionClient& client, const Address& host, const char* name,
                         uint64_t token, int64_t now_us);
// Join over a peer the node has already connected (reach.hpp crossed the NATs to the host): the
// hello goes at once. False when the peer is not connected.
bool session_client_attach(SessionClient& client, int peer, const char* name, uint64_t token,
                           int64_t now_us);
void session_client_update(SessionClient& client, int64_t now_us);
// Asks; each queues one message to the host (false: not seated, or the queue refused it). What
// the host decided comes back as events.
bool session_client_ready(SessionClient& client, bool ready);
bool session_client_team(SessionClient& client, int team);
bool session_client_chat(SessionClient& client, ChatScope scope, int to, const char* text,
                         size_t size);
bool session_client_kick(SessionClient& client, int player);
bool session_client_start(SessionClient& client);
bool session_client_return(SessionClient& client);
bool session_client_settings(SessionClient& client, const uint8_t* data, size_t size);
// Leave for good: the peer is disconnected and the seat freed at once (no away time).
void session_client_leave(SessionClient& client, int64_t now_us);

inline const char* session_text(const std::vector<uint8_t>& event_bytes, const SessionEvent& e) {
  return reinterpret_cast<const char*>(event_bytes.data() + e.offset);
}

}  // namespace ge::net
