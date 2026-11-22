// QUAKE II'S MULTIVIEW RELAY (porting/shooters-edge/q2-mvd-relay.md § B): a server turned relay
// watches other servers' multiview streams (`mvdconnect`, over TCP: q2_mvd_server.hpp's other end)
// and plays multiview recordings (`mvdplay`), each one a CHANNEL, served to unmodified Quake II
// clients that connect to it as to any server.
//
// THE UPSTREAM: a connection sends the magic and its hello (the version 0xED04, string commands
// asked; no deflate: the relay reads the stream as it is), asks for the stream while the relay is
// active (a viewer did something within `mvd_suspend_time` minutes) and stops it while not. Faults
// either DROP it (a retry after a backoff: 15 s more each time before the connect completed, 5 s
// once the server's hello came, at most 5 hours) or DESTROY it. Every line the console prints is
// the server's words, "[<name>] -=- ..." for progress, "=X=" for an end.
//
// A CHANNEL holds what came in a delay buffer (`mvd_buffer_size` × 32 KiB) and starts playing once
// `mvd_wait_delay` seconds of frames are in or the buffer is `mvd_wait_percent` full, then plays a
// block every 10 Hz frame; running dry it waits again (50 frames, 5 more for each time before). A
// played file is read at the same pace, gzipped or not, and may be paused, skipped and sought
// (back through a snapshot kept every `mvd_snaps` seconds). A channel is read by one reader that
// keeps the match as it stands, and a VIEW of one player is forked off it when a viewer chases him
// (client/demo_convert.hpp's `convert_fork`): every view is a TV face (q2_tv_face.hpp) on the
// relay's one port, and a viewer moves between faces as a client moves between levels.
//
// THE WAITING ROOM is where a viewer lands: `mvd_default_map` from above its intermission spot, the
// Channel Chooser on the screen (`invnext`, `invprev`, `invuse`). With one channel a viewer goes
// straight to it. The viewer's words: `follow`/`chase`, `autofollow`, `observe`, `inven`/`menu`,
// `invnext`, `invprev`, `invuse`, `help`/`score`, `oldscore`, `putaway`, `channels`, `clients`,
// `join`, `leave`, `commands`, `say`, `say_team`, `!mvdadmin`, `fwd`; chat stays among the
// channel's viewers, held to the flood rule (`flood_msgs`, `flood_persecond`, `flood_waitdelay`,
// `flood_mute`).
//
// WHAT IT DOES NOT: a viewer flies free only by following no one (a view needs a player); stats
// and intermission rewrites (`mvd_stats_score`, `mvd_stats_hack`, `mvd_freeze_hack`) are not made;
// a stream with the extended limits is refused by the reader.
//
// ge:summary Quake II's multiview relay: mvdconnect, mvdplay, channels, the Waiting Room, viewers
// ge:keywords mvd-relay mvdconnect mvdplay mvdchannels mvdservers mvdkill mvdspawn mvdseek
// ge:keywords mvdskip mvdpause mvdcontrol mvdisconnect waiting-room channel-chooser gtv-client
// ge:keywords mvd_wait_delay mvd_buffer_size mvd_suspend_time chase autofollow mvdadmin
// ge:skill net shooter replay
// ge:test tests/shooters_edge_q2_mvd_relay_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/deflate.hpp"
#include "net/tcp_socket.hpp"
#include "net/transport.hpp"
#include "shooters_edge/client/demo_convert.hpp"
#include "shooters_edge/net/q2_tv_face.hpp"

namespace shooters_edge::net {

struct Q2RelayConfig {
  std::string hostname = "Shooter's Edge TV";
  std::string version = "Shooter's Edge";  // the relay's version, in its hello and on the chooser
  int max_viewers = 32;
  std::string password;                      // what a viewer must bring
  double timeout = 90;                       // mvd_timeout, s
  int suspend_min = 5;                       // mvd_suspend_time, minutes (0: always active)
  int wait_delay = 20;                       // mvd_wait_delay, s
  int wait_percent = 50;                     // mvd_wait_percent
  int buffer_size = 8;                       // mvd_buffer_size, 32 KiB
  std::string username = "unnamed";          // mvd_username
  std::string upstream_password;             // mvd_password
  int snaps = 10;                            // mvd_snaps, s
  std::string admin_password;                // mvd_admin_password
  int part_filter = 0;                       // mvd_part_filter
  int stats_score = 0;                       // mvd_stats_score: 0 none, 1 the channel, else frags
  bool filter_version = false;               // mvd_filter_version
  std::string default_map = "q2dm1";         // mvd_default_map
  std::string chase_prefix = "xv 0 yb -64";  // mvd_chase_prefix
  int flood_msgs = 4;
  double flood_persecond = 4, flood_waitdelay = 10;
  bool flood_mute = false;
  double viewer_timeout = 90;  // s of a viewer's silence
  std::string demos;           // the demos folder (empty: "demos")
};

enum class Q2GtvState : uint8_t {
  disconnected,
  connecting,
  preparing,
  connected,
  resuming,
  waiting,
  reading,
  suspending
};

struct Q2GtvConn {
  int id = 0;
  std::string name, typed;  // the channel's name; the address as typed
  ge::net::Address addr;
  ge::net::TcpStream stream;
  Q2GtvState state = Q2GtvState::disconnected;
  std::string user, pass;
  uint32_t flags = 0;
  bool magic = false, hello = false, suspended = false;
  double backoff = 0, retry_at = 0, heard = 0, sent = 0;
  std::vector<uint8_t> in;
  uint64_t bytes_in = 0;
};

enum class Q2ChanState : uint8_t { dead, wait, read };

// One viewer's view of a channel (or the Waiting Room's, shared): its conversion and the face it
// watches through.
struct Q2RelayView {
  int pov = -1;  // the player followed (-1: the lowest playing)
  client::DemoConvert conv;
  Q2TvFace face;
  ge::net::Transport port;  // a pipe onto the relay's port
  std::deque<std::pair<ge::net::Address, std::vector<uint8_t>>> inbox;  // datagrams routed here
  struct Q2Relay* relay = nullptr;
};

struct Q2Snap {
  size_t pos = 0;     // the file's position after the block
  int64_t frame = 0;  // the channel's frame
  client::DemoConvert base;
};

struct Q2Channel {
  int id = 0;
  std::string name;
  Q2ChanState state = Q2ChanState::dead;
  int conn = -1;  // the connection's id; -1 a played file
  // The delay buffer: whole blocks waiting to be played.
  std::deque<std::vector<uint8_t>> buffer;
  size_t buffered = 0, most = 0;
  int target = 0, underflows = 0;
  double next = 0;           // the next 10 Hz frame, s
  client::DemoConvert base;  // reads every block: the state a new view forks from
  bool have_state = false;
  std::string map;
  std::string oldscore;  // the last level's scoreboard, as a layout
  int64_t frame = 0, level_frame = 0;
  // A played file.
  std::vector<std::string> files;
  size_t file_at = 0;
  int loops = 1, looped = 0;
  std::vector<uint8_t> data;
  size_t pos = 0;
  bool paused = false;
  int skip = 0;
  std::vector<Q2Snap> snaps;
  // A recording of it.
  std::FILE* rec = nullptr;
  std::string rec_path;
  bool rec_gz = false;
  ge::core::DeflateStream gz;
};

enum class Q2Layout : uint8_t { none, chooser, menu, clients, score, oldscore };

struct Q2RelayViewer {
  ge::net::Address addr;
  uint16_t qport = 0;
  std::unique_ptr<Q2RelayView> view;  // its view of its channel; null in the Waiting Room
  std::string name;
  int chan = -1;    // the channel's id; -1 the Waiting Room
  int target = -1;  // the player chased (-1: the view's own choice)
  int prev = -1;
  bool admin = false;
  int uf = 0;
  Q2Layout layout = Q2Layout::none;
  int cursor = 0;
  double entered = -100, talked = -1e9, mute_until = 0;
  std::array<double, 16> said{};
  int said_at = 0;
  std::vector<int> marks;  // autofollow's slots
  bool autochase = false;
  char powerup = 0;
};

struct Q2Relay {
  Q2RelayConfig cfg;
  ge::net::Transport* port = nullptr;
  bool spawned = false;
  bool active = false;
  double now = 0, activity = -1e18;
  int next_id = 0;
  std::vector<Q2GtvConn> conns;
  std::vector<std::unique_ptr<Q2Channel>> channels;
  std::unique_ptr<Q2RelayView> room;  // the Waiting Room
  double room_next = 0;
  int32_t room_frame = 0;
  std::map<uint64_t, Q2RelayViewer> viewers;  // by host and qport
  std::function<void(const std::string&)> log;
  // The Waiting Room's map: its intermission spot (origin and angles) and its checksum; false
  // when it does not load (the reason in `error`).
  std::function<bool(const std::string& map, float origin[3], float angles[3], int32_t& checksum,
                     std::string& error)>
      spot;
  float room_origin[3] = {}, room_angles[3] = {};
  int32_t room_checksum = 0;
  std::string room_map;
  // Moves asked by viewers' words, made once every face has read its datagrams.
  struct Move {
    uint64_t key = 0;
    int chan = -1;      // the channel to enter (-1: the Waiting Room)
    int target = -1;    // the player to follow there
    bool same = false;  // the same channel: the view follows `target` now
  };
  std::vector<Move> moves;
};

// Relay mode switched on at `port` (the server's own, taken over): the Waiting Room opened.
void q2_relay_spawn(Q2Relay& r, ge::net::Transport& port, double now);
// The connections read, the channels played, the viewers answered (`now_us`).
void q2_relay_frame(Q2Relay& r, int64_t now_us);
// Every viewer told the relay is going, every connection and channel ended.
void q2_relay_stop(Q2Relay& r);
// The server going away while it relays: every viewer told `text` with the disconnect (or the
// reconnect), then the relay stopped.
void q2_relay_farewell(Q2Relay& r, const std::string& text, bool reconnect);
// An operator's word (`w` its words); false when it is not the relay's. `port` is where relay mode
// starts when the word starts it.
bool q2_relay_command(Q2Relay& r, const std::vector<std::string>& w, const std::string& line,
                      ge::net::Transport* port, double now, std::string& out);
int q2_relay_viewers(const Q2Relay& r);

// THE TWO HALVES' SEAM: the upstream and the channels (q2_mvd_relay.cpp) and the viewers
// (q2_mvd_relay_viewers.cpp) call each other through these.
namespace relay {
Q2Channel* channel(Q2Relay& r, int id);
// A console line, as the relay's console prints it.
void say(Q2Relay& r, const std::string& line);
// "[MVD] <text>" to every viewer of channel `chan` (-1: the Waiting Room).
void tell(Q2Relay& r, int chan, const std::string& text, int level = 2);
// The block played on `c`: its reader, its views and their faces.
void feed(Q2Relay& r, Q2Channel& c, const std::vector<uint8_t>& block);
// The block (in a file's form) to every view of `c` and their faces.
void feed_views(Q2Relay& r, Q2Channel& c, const std::vector<uint8_t>& bytes);
// Every view of `c` started again off its reader (after a seek): its viewers sent the signon.
void refork(Q2Relay& r, Q2Channel& c);
// Every viewer of `c` sent to the Waiting Room (the channel is going).
void evict(Q2Relay& r, Q2Channel& c);
// The channel list changed: the choosers drawn again.
void listed(Q2Relay& r);
void open_room(Q2Relay& r);
// The port's datagrams to the faces, the faces' frames, the Waiting Room's frame.
void viewers_frame(Q2Relay& r, int64_t now_us);
void close_viewers(Q2Relay& r);
}  // namespace relay

}  // namespace shooters_edge::net
