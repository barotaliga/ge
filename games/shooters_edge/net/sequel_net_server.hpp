// THE SEQUEL'S NET SERVER (protocol 34, sequel_net.hpp): the sequel's rules
// (server/sequel_server.hpp, through a RulesRunner) served to clients over engine/net's transports,
// each client's view written by the sequel's wire (server/sequel_wire.hpp), the same bytes a
// recording holds.
//
// OUT OF BAND it answers `getchallenge` (one number per address, kept), `connect` (the protocol,
// the challenge, the ban list, the spectator's or the player's password, a free slot; a refusal
// is a print a player reads), `info 34`, `status` (its server info with `maxspectators` and
// `needpass`), `ping` and `rcon` (sequel_admin.hpp). A connect from an address the ban list
// refuses is refused ("Banned."). A second connect from a slot's address and qport while it is in
// use is ignored.
//
// THE SIGNON follows the client's string commands (`new`, `configstrings`, `baselines`, `begin`,
// each with the level's count) and answers each with up to about 700 bytes and the next prompt;
// a wrong count starts it again. `begin` puts the client in the rules' slot.
//
// FRAMES: every host tick (`sequel_net_frame`, at the rules' step, 13 ms) reads every datagram,
// hands the moves to the rules as packets (their three commands, the rules running those a lost
// packet held), and when the rules have run a 10 Hz frame writes each client its frame: a delta
// from the frame its newest move acknowledged when that is the last one written, else whole, and
// holding only the entities the client may see (`cut_to_view`, server/sequel_wire.hpp). The
// frame's prints, centred prints, stuffed lines and changed configstrings go on the reliable
// channel; the frame and its sounds unreliably.
//
// THE RATE: a client's user info `rate` (bytes a second) read as the sequel reads it (none: 5000;
// held to 100..15000) and then to the server's `sv_minrate` and `sv_maxrate` (server/rate.hpp; a
// client out of them told so in the game's words when it enters and set back with `rate N`). The
// sizes of the last 10 frames sent (the frame and its unreliable messages, one slot a frame: a
// second at 10 Hz) are summed before each frame; past the rate, nothing goes that frame (no
// reliable data either), its slot is 0, and the next frame sent carries the count held back as
// its suppress count. The other players' moves between frames (`moves_between`) count in the
// frame's second and wait while the client is past its rate.
//
// A LEVEL CHANGE (`sequel_net_level`) keeps every connection: the count goes up, each client is
// sent `changing` and `reconnect` and asks for the signon again with `new`.
//
// THE GAME'S HOOKS (the logins, sequel_login.hpp): `line_hook` sees each string command but the
// signon's first and takes it (true) before the rules; `name_rule` may change the name a client
// connects or renames with. `sequel_net_print`, `sequel_net_rename` and `sequel_net_drop` answer.
//
// A client is dropped on `disconnect`, after `timeout` seconds of silence, on a message it cannot
// read, or on overflowing its reliable channel; it is sent the disconnect message, and its slot is
// a ZOMBIE for `zombie_time` (sv_zombietime, the sequel's `zombietime`; 2 s) after its last
// packet: the disconnect resent until acknowledged, its packets read for nothing else, `status`
// showing it as `ZMBI`; then the slot is free.
//
// ge:summary The sequel's net server: the rules served to live protocol 34 clients over a transport
// ge:keywords sequel-server protocol-34 live-server getchallenge connect signon configstrings
// ge:keywords baselines begin frame delta ack move checksum status info timeout disconnect
// ge:keywords zombie zombietime sv_zombietime
// ge:keywords sequel_net_start sequel_net_frame sequel_net_stop sequel_net_level level-change
// ge:keywords changelevel changing reconnect
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "server/bans.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/sequel_net.hpp"
#include "shooters_edge/server/rules_runner.hpp"
#include "shooters_edge/server/sequel_native.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/server/sequel_wire.hpp"
#include "vfs/partial.hpp"

namespace shooters_edge::net {

namespace sv = shooters_edge::server;

struct SequelNetConfig {
  std::string hostname = "Shooter's Edge";
  int max_clients = 8;
  std::string password;  // "" or "none": none asked
  // SPECTATORS (user info `spectator` other than 0): with a spectator password set ("" or `none`:
  // none) the key's value must be it ("Spectator password required or incorrect."), and the
  // player password is not asked. `max_spectators` (sv_maxspectators) 0: they take free player
  // slots; N: N slots of their own beside the players' ("Server spectator limit is full.").
  std::string spectator_password;
  int max_spectators = 0;
  int min_rate = 0, max_rate = 0;  // sv_minrate, sv_maxrate (0: no floor, no cap)
  // THE ADMINISTRATOR (sequel_admin.hpp): the remote console's password (empty: every rcon
  // refused), the ban list's mode (sv_banmode, the sequel's `filterban`: 1 the list bans, 0 only
  // the list joins) and file (`writeip`; empty: not saved), and the clock timed bans end by (Unix
  // seconds; unset: 0, so they never end).
  std::string rcon_password;
  int ban_mode = 1;
  std::string ban_file;
  std::function<int64_t()> unix_time;
  double timeout = 125;    // s of silence before a client is dropped
  double zombie_time = 2;  // s a dropped slot stays a zombie after the client's last packet
  // Each client sent only the entities it may see (server/sequel_wire.hpp § the viewer's cut);
  // off: every entity to everyone.
  bool cut_to_view = true;
  // Moves whose checksum byte is wrong are ignored (the frame they acknowledge still counts).
  bool verify_checksum = true;
  sv::SequelWireOptions wire;  // the map's checksum, the sky; the count and the loop set here
  // DOWNLOADS: a file a client asks for (`download <path> [offset]`, then `nextdl` for each next
  // piece of up to 1024 bytes) read from the game's files (`read_file`; unset: every download
  // refused). Refused, as size -1 and percent 0: downloads off, a path holding `..`, starting with
  // `.` or `/` or with no `/`, a file missing, a kind switched off (`players/` skins and figures,
  // `models/`, `sound/`, `maps/`), and a map out of an archive to a client that would keep it loose
  // (any but ours). A client that agreed the download origin (`*ge_ext` bit 4 in its user info)
  // is told each file's archive (`file_origin`) first, as svc 123, and keeps the file in it
  // (engine/vfs/partial.hpp); maps out of archives are sent to it.
  //
  // OTHER PLAYERS' MOVES: a client that agreed them (`*ge_ext` bit 8) gets, after each frame, svc
  // 124: for each other player in the frame and walking (at most 30), the slot, the milliseconds
  // since the server last ran a move of his (at most 255), his state as the sequel's move holds it
  // and that move, so the client runs him forward to its own time as the classic join does
  // (net/sequel_predict.hpp). The bytes: a count, then each player as u8 slot, u8 msec, u8 move
  // type, i16 origin x3, i16 velocity x3 (eighths), u8 flags, u8 timer, i16 gravity, i16 delta
  // angles x3, u8 buttons, i16 angles x3, i16 forward, side and up. A hosted game library's
  // players are not sent (its state is its own). The rules move each player when his move comes,
  // not at their 10 Hz frame, so with `moves_between` the message also goes alone every other host
  // tick between the frames (about 38 a second), when some other player moved since: the client
  // runs the players forward from states 26 ms old at most instead of 100.
  //
  // THE EASY JUMP'S SHARE: a client that agreed it (`*ge_ext` bit 16) is told, reliably as svc 126
  // with its first frame and again whenever it changes, the share of the best strafe's gain its
  // easy jump may take (`pm_speedjump`, a u16 in thousandths; 0 the aid off), as the first game's
  // server info states it (net/classic_remote.hpp).
  bool moves_between = true;
  bool allow_download = true, allow_players = true, allow_models = true, allow_sounds = true;
  bool allow_maps = true;
  std::function<bool(const std::string& path, std::vector<uint8_t>* out)> read_file;
  std::function<bool(const std::string& path, ge::vfs::ArchiveOrigin* out)> file_origin;
};

// A slot's stage. A ZOMBIE is a client dropped: for `zombie_time` seconds after the last packet
// from it the slot still resends what it owes (the disconnect) and reads nothing it sends, then
// it is free. Only `signon` and `active` hold a client (`sequel_held`).
enum class SequelSlot : uint8_t { free, signon, active, zombie };

struct SequelNetClient {
  SequelSlot stage = SequelSlot::free;
  gn::Address address;
  uint16_t qport = 0;
  Channel ch;
  sv::SequelWire wire;
  sv::SequelSignon signon;
  std::string userinfo, name;
  int userid = 0;  // the server's number for whoever took the slot
  bool spectator = false;
  int rate = 5000;             // the bytes a second it is sent, in force (THE RATE)
  int frame_sizes[10] = {};    // the bytes of the last 10 frames, by frame number % 10
  int64_t held = 0;            // frames held back for its rate
  double heard = 0, sent = 0;  // the network's times of the last packet each way, s
  int32_t acked = -1;          // the frame the newest move acknowledged
  int64_t moves = 0, bad_checksums = 0, frames = 0, full_frames = 0;
  bool told_origins = false;        // agreed the download origin (`*ge_ext` bit 4)
  bool told_moves = false;          // agreed the other players' moves (`*ge_ext` bit 8)
  bool told_share = false;          // agreed the easy jump's share (`*ge_ext` bit 16)
  int share_sent = -1;              // the share last told, thousandths (-1: none yet)
  ge::classic::SequelCmd last_cmd;  // the newest move the rules ran of this player, as they ran it
  double last_cmd_at = -1;          // the network's time it came, s (-1: none yet)
  double moves_sent = -1;           // the network's time its newest moves message went, s
  std::vector<uint8_t> download;    // the file being sent
  size_t download_at = 0;           // its bytes sent so far
  bool downloading = false;
  int64_t downloads = 0;  // files sent whole
};

inline bool sequel_held(const SequelNetClient& c) {
  return c.stage == SequelSlot::signon || c.stage == SequelSlot::active;
}

struct SequelNetServer {  // NOLINT(clang-analyzer-optin.performance.Padding): one a server
  SequelNetConfig cfg;
  const sv::RulesRunner* rules = nullptr;
  sv::SequelServer* game = nullptr;
  sv::NativeSequel* native = nullptr;  // a hosted game library's runner (sequel_native.hpp)
  gn::Transport* transport = nullptr;
  SequelNetClient clients[sh::kMaxClients];
  std::map<uint64_t, int32_t> challenges;  // by the address with its port off
  uint32_t next_challenge = 0;
  int32_t count = 0;  // the level's count, as the server data carries it
  int64_t written_frame = -1;
  int64_t ticks = 0;      // host ticks served
  sv::SequelSight sight;  // our rules' entities linked for the cut, each frame
  double now = 0;
  int next_userid = 1;
  std::vector<std::string> log;  // what the server would print on its console (the first 512)
  ge::server::BanList bans;      // the administrator's (sequel_admin.hpp); refused: "Banned."
  int64_t rcon_runs = 0, rcon_refused = 0;
  bool (*line_hook)(void* ctx, int slot, const std::string& line) = nullptr;
  void (*name_rule)(void* ctx, int slot, std::string& name, bool connecting) = nullptr;
  void* hook_ctx = nullptr;
  int64_t datagrams_in = 0, bytes_in = 0, bytes_out = 0, refused = 0;
};

// Serves the started sequel runner `rules` (a map loaded) on `t`. False when `rules` is not the
// sequel's.
bool sequel_net_start(SequelNetServer& s, const sv::RulesRunner& rules, gn::Transport& t,
                      const SequelNetConfig& cfg, std::string& error);
// One host tick at the network's time.
void sequel_net_frame(SequelNetServer& s, int64_t now_us);
// Every client told the server is going, and dropped.
void sequel_net_stop(SequelNetServer& s);
int sequel_net_clients(const SequelNetServer& s);
// The slots clients may hold: the players' and the spectators' own (at most kMaxClients).
int sequel_net_slots(const SequelNetServer& s);
// The rules moved onto another map (their `start` run again) with `wire` the new map's checksum
// and sky: the level's count goes up and every client is told the level changed (`changing`, then
// `reconnect`), and signs on again on the same connection; `begin` seats it on the new map.
void sequel_net_level(SequelNetServer& s, const sv::SequelWireOptions& wire);
// A line printed in the client's console (reliable; level 2, a print a player reads).
void sequel_net_print(SequelNetServer& s, int slot, const std::string& text);
// The client named `name` (its rules' name too, once in the game).
void sequel_net_rename(SequelNetServer& s, int slot, const std::string& name);
// The client dropped (what it was printed before goes with the disconnect); `why` for the log.
void sequel_net_drop(SequelNetServer& s, int slot, const std::string& why);
// An out-of-band packet of `text` to `to` (a remote console's answer: "print\n...").
void sequel_net_oob(SequelNetServer& s, const gn::Address& to, const std::string& text);

}  // namespace shooters_edge::net
