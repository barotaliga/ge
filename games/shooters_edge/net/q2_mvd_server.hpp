// QUAKE II'S MULTIVIEW RECORDER ON A LIVE SERVER (porting/shooters-edge/q2-mvd-relay.md § A): the
// server's own recording of every player (`mvdrecord`, a `.mvd2` in the demos folder) and the
// same stream served live over TCP to relays and watchers (`sv_mvd_enable 2`).
//
// THE RECORDER is switched on by a recording or a relay asking for the stream, and off again
// after `sv_mvd_disconnect_time` minutes with neither. While on, it starts each level with its game
// state and adds a block every 10 Hz frame (server/q2_mvd2_live.hpp). It suspends after
// `sv_mvd_suspend_time` minutes with no player in the game (nothing written; a streaming relay is
// sent one empty block) and resumes with a fresh game state. Our rules keep no recorder's dummy
// (the game library never says it knows one), so the stream's observer is -1 and `mvdstuff` is
// refused.
//
// THE TCP SIDE listens on the game's port. A connection is counted against `sv_iplimit` and the
// `sv_mvd_maxclients` slots, must open with the four bytes `MVD2` (echoed), then sends a hello
// (the version 0xED04, its flags, a name, a password, its version): an address on the
// `addgtvhost` list is let in, one on `addgtvban` refused, else `sv_mvd_password` decides. The
// answer is the hello with the flags taken (deflate when asked; string commands never, as
// `sv_mvd_allow_stufftext` is 0 or more), and from then on every byte is one zlib stream when
// deflate was taken, each message flushed whole. Stream start gets its ack and the game state (or
// an empty block while suspended), then a block a frame; stop gets its ack; a ping its pong. A
// viewer whose `sv_mvd_bufsize` × 32 KiB of unsent bytes cannot take a frame is dropped
// ("overflowed"); silence drops it after `timeout` (before its hello, `sv_ghostime`). A dropped
// viewer drains for `zombietime` at most. A level change sends a new game state; a restart the
// reconnect message, a shutdown the disconnect message.
//
// THE WORDS: `mvdrecord [-hz] <filename>`, `mvdstop`, `mvdstuff`, the access lists (`addgtvhost`,
// `delgtvhost`, `listgtvhosts`, `addgtvban`, `delgtvban`, `listgtvbans`), the TCP rows of
// `status`, each in the server's words.
//
// ge:summary Quake II's multiview recorder on a live server: mvdrecord, the relay stream over TCP
// ge:keywords mvd2 mvdrecord mvdstop mvdstuff gtv relay-stream tcp sv_mvd_enable addgtvhost
// ge:keywords addgtvban sv_mvd_password sv_mvd_bufsize sv_mvd_suspend_time sv_mvd_maxsize
// ge:keywords sv_mvd_maxtime sv_mvd_maxmaps stream-start stream-data deflate-stream
// ge:skill net shooter replay
// ge:test tests/shooters_edge_q2_mvd_server_test.cpp
#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "classic/protocol/addr_list.hpp"
#include "core/deflate.hpp"
#include "net/tcp_socket.hpp"
#include "shooters_edge/server/q2_mvd2_live.hpp"

namespace shooters_edge::net {

namespace sv = shooters_edge::server;

// The stream's message ids (porting/shooters-edge/demo-formats-q2.md § 5).
enum : uint8_t {
  kGtvHello = 0,
  kGtvPing = 1,
  kGtvStreamStart = 2,
  kGtvStreamStop = 3,
  kGtvCmd = 4
};
enum : uint8_t {
  kGtsHello = 0,
  kGtsPong = 1,
  kGtsStreamStart = 2,
  kGtsStreamStop = 3,
  kGtsStreamData = 4,
  kGtsError = 5,
  kGtsBadRequest = 6,
  kGtsNoAccess = 7,
  kGtsDisconnect = 8,
  kGtsReconnect = 9,
};
constexpr uint16_t kGtvProtocol = 0xED04;
constexpr uint32_t kGtfDeflate = 1, kGtfStringCmds = 2;
constexpr size_t kGtvClientMost = 256, kGtvServerMost = 32768;

struct Q2MvdConfig {
  int enable = 0;  // sv_mvd_enable: 0 none, 1 recording, 2 the TCP stream too
  int maxclients = 8;
  int bufsize = 2;
  std::string password;
  int64_t maxsize_kb = 0;  // sv_mvd_maxsize, 1000-byte kilobytes; 0 no limit
  int64_t maxtime_min = 0;
  int maxmaps = 1;
  bool noblend = false, nogun = false;
  int suspend_min = 5, disconnect_min = 15;
  int allow_stufftext = 0;
  int iplimit = 3;
  double timeout = 90, ghost = 6, zombie = 2;  // s
  std::string demos;                           // the demos folder (empty: "demos")
  // The dummy spectator (q2_mvd_dummy.hpp): sv_mvd_spawn_dummy (0 never, 1 when the rules know
  // it, which ours do, 2 always), sv_mvd_begincmd, sv_mvd_scorecmd, sv_mvd_capture_flags,
  // sv_mvd_nomsgs and sv_mvd_autorecord (the dummy made at every level and its `record` and
  // `stop` taken).
  int spawn_dummy = 1;
  std::string begincmd = "wait 50; putaway; wait 10; help;";
  std::string scorecmd = "putaway; wait 10; help;";
  int capture_flags = 5;
  bool nomsgs = true, autorecord = false;
};

struct Q2MvdViewer {
  enum class Stage : uint8_t { assigned, connected, primed, sending, zombie };
  ge::net::TcpStream stream;
  Stage stage = Stage::assigned;
  std::string name, version;
  uint32_t flags = 0;
  ge::core::DeflateStream z;
  std::vector<uint8_t> in;
  double heard = 0, dropped_at = 0;
  bool magic = false;
};

struct Q2Mvd {  // NOLINT(clang-analyzer-optin.performance.Padding): one a server
  Q2MvdConfig cfg;
  ge::net::TcpListener listener;
  bool listening = false;
  std::vector<Q2MvdViewer> viewers;
  ge::classic::AddrList hosts, bans;
  // The recorder.
  bool on = false, active = false;  // switched on; not suspended
  double idle_since = 0;            // the last frame with a player in the game, s
  double unused_since = 0;          // the last frame with a recording or a streaming viewer, s
  bool have_level = false;
  sv::Mvd2Live live;
  sv::Mvd2Level level;
  sv::Mvd2Frame last;
  double now = 0;  // the last pump's or frame's time, s
  // The local recording.
  std::FILE* file = nullptr;
  std::string path;
  bool gzip = false;
  ge::core::DeflateStream gz;
  int64_t written = 0, frames = 0;
  int maps = 0;
  // The dummy spectator (q2_mvd_dummy.hpp).
  int dummy = -1;                            // its slot; -1 none
  std::string dummy_cmds;                    // its command buffer
  int dummy_wait = 0;                        // the 10 Hz frames its `wait` still holds the buffer
  double dummy_layout_at = 0;                // the last time its scoreboard was seen, s
  std::vector<std::string> dummy_forwarded;  // relays' commands for it (allow_stufftext below 0)
  bool dummy_failed = false;                 // the last switch-on could not make one
  std::function<void(const std::string&)> log;  // the console's lines
  std::function<int64_t()> unix_time;           // the access lists' clock (null: 0)
};

// The TCP listener opened at `bind` when `sv_mvd_enable` is 2 or more; false (and the console's
// line) when the OS refuses it, `enable` then 1.
bool q2_mvd_listen(Q2Mvd& m, const ge::net::Address& bind);
// Accepts, reads and answers the viewers, drops the silent and drains the dropped (`now` s).
void q2_mvd_pump(Q2Mvd& m, double now);
// A level started: its game state to the recording and every streaming viewer.
void q2_mvd_level(Q2Mvd& m, const sv::RulesCommon& c, const sv::Q2WireOptions& opt,
                  const sv::Mvd2Capture& cap, double now);
// A 10 Hz frame: the suspension's rule, then the block to the recording and the streams.
void q2_mvd_frame(Q2Mvd& m, const sv::RulesCommon& c, const sv::Q2WireOptions& opt,
                  const sv::Mvd2Capture& cap, double now);
// The server stops (`restart`: the viewers told to come back) or the recorder is torn down.
void q2_mvd_shutdown(Q2Mvd& m, bool restart);
// The recorder switched on at `now` (a relay's stream start, a recording, an auto-recording level).
void q2_mvd_switch_on(Q2Mvd& m, double now);
// The dummy could not be made: the recording closed, each streaming relay sent an error and
// dropped (`couldn't create MVD dummy`), the recorder off.
void q2_mvd_dummy_refused(Q2Mvd& m);
// A local recording `name` (in the demos folder, `.mvd2` added; `.gz` with `gzip`) started; its
// path, or false with the line that refuses it in `out`.
bool q2_mvd_record(Q2Mvd& m, std::string name, bool gzip, std::string& path, std::string& out);
// The local recording stopped (false when there was none).
bool q2_mvd_stop(Q2Mvd& m);
// An operator's word (`w` its words, `line` whole); false when it is not the recorder's.
bool q2_mvd_command(Q2Mvd& m, const std::vector<std::string>& w, const std::string& line,
                    bool in_game, std::string& out);
// The TCP viewers' part of `status` (`versions`: the version table).
std::string q2_mvd_status(const Q2Mvd& m, bool versions, double now);

}  // namespace shooters_edge::net
