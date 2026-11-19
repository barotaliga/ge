// A SCRIPTED SEQUEL CLIENT (protocol 34, sequel_net.hpp): the client half the sequel's net server's
// tests and the `se-sequel-net` lab drive, and the one a watcher or a bridge joins a live server
// with. It asks for a challenge, connects, follows the signon as the server stuffs it
// (`cmd configstrings ...`, `cmd baselines ...`, `precache <n>` answered with `begin <n>`), then
// sends a move every tick: the newest frame it read whole (so the server deltas from it) and its
// three newest commands from a script or a brain.
// With `fetch` on it asks, before `begin`, for the files it lacks (the map, a list) and keeps
// each with the archive our server says it came out of (`keep_file`).
//
// WHAT IT READS: every in-band payload is one block of the sequel's own recording format, appended
// to `demo.bytes` with its length and read by the recording's reader (client/demo.hpp,
// `next_sequel`), so the client's state is `demo.state` and its recording is `demo.bytes` (end it
// with `sequel_client_stop`; a length of -1 closes it). A level change (`changing`, then
// `reconnect`) sends `new` again on the same connection; the server's reconnect message starts a
// fresh connect. Through forwarders (`cfg.via`) the route is dialled before the challenge.
//
// Nothing reads a wall clock: `sequel_client_tick` takes the network's time, so a session replays.
//
// ge:summary A scripted sequel-protocol client: challenge, connect, signon, frames and moves
// ge:keywords sequel-client protocol-34 scripted-client signon getchallenge connect configstrings
// ge:keywords baselines precache begin move ack-frame spectator recording dm2 reconnect
// ge:keywords sequel_client_start sequel_client_tick sequel_client_stop sequel_client_command
// ge:keywords download nextdl download-origin fetch
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "forward/forward.hpp"
#include "net/transport.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/sequel_net.hpp"
#include "vfs/partial.hpp"

namespace shooters_edge::net {

struct SequelClientConfig {
  std::string name = "player";
  std::string skin = "male/grunt";
  int rate = 15000;
  bool spectator = false;
  std::string password;
  std::string spectator_password;  // a spectator's: sent as the `spectator` key's value ("": 1)
  uint16_t qport = 1;
  int seed = 1;
  int send_every = 1;  // ticks between packets in the game (a slower client sends fewer)
  // Seconds between spells of 150 ms in which the moves acknowledge no frame (0: never), so the
  // server sends its next frame whole: a relay of this recording starts a late viewer there.
  double whole_every = 0;
  // Through forwarders (engine/forward): the hops in order, the server after them. The route is
  // dialled first (`ge::forward::Dial`), then the game's own handshake goes to the first hop.
  std::vector<gn::Address> via;
  // The command at a tick (msec set by the client); null: stands still.
  std::function<sh::MoveCmd(int tick)> brain;
  // How long the server may send nothing once connected before the client gives it up, s: a
  // server that quit or crashed, or a link gone, ends the game with a reason (0: never).
  double timeout = 60;
  // DOWNLOADS: with `fetch` on, at `precache` and before `begin` it asks the server, one file at a
  // time, for the level's map when `read_file` lacks it and for each of `want` it lacks. A file
  // fetched is kept in `downloaded` and handed to `keep_file` with the archive the server said it
  // came out of (null: none said), as the window keeps it (classic/content/downloads.hpp); a
  // refused one is skipped. `ask_origins` puts the download origin (`*ge_ext` bit 4) in its user
  // info, so our server tells each file's archive.
  bool fetch = false;
  bool ask_origins = true;
  // With `fetch_level` on (the window's), every model, sound and player figure the level names
  // that `read_file` lacks is fetched too, as a player's client fetches them.
  bool fetch_level = false;
  // `ask_moves` asks our server for the other players' moves after each frame (`*ge_ext` bit 8),
  // which the window runs them forward by (sequel_predict.hpp); a stock server ignores it.
  bool ask_moves = false;
  // The view's width in degrees and the hand the weapon is held in (0 right, 1 left, 2 centred),
  // as the user info carries them (`fov`, `hand`): the window fills them from its settings.
  int fov = 90, hand = 0;
  // `cl_speedjump 1`: the easy jump asked for, said in the user info (as the first game's join
  // does); the client asks our server for its share (`*ge_ext` bit 16, net/sequel_remote.hpp).
  bool speedjump = false;
  std::vector<std::string> want;
  std::function<std::vector<uint8_t>(const std::string& path)> read_file;
  std::function<void(const std::string& path, const std::vector<uint8_t>& bytes,
                     const ge::vfs::ArchiveOrigin* origin)>
      keep_file;
  // THE WINDOW'S JOIN (classic_remote.hpp): what the server sends is read into `into` (the
  // window's drawn recording) in place of the client's own `demo`, the read bytes dropped as they
  // are read, and each payload's prints, effects and sounds kept in `news` until the window takes
  // them, however many payloads come between its frames.
  client::Demo* into = nullptr;
};

// What the payloads read since the window last took it carried (`SequelClientConfig::into`).
struct SequelNews {
  std::vector<client::DemoPrint> prints;
  std::vector<client::DemoEffect> effects;
  std::vector<client::DemoSound> started;
  std::vector<std::pair<int, int>> stopped;
  std::string centre;
};

struct SequelClientEnd {  // NOLINT(clang-analyzer-optin.performance.Padding): one a client
  SequelClientConfig cfg;
  gn::Transport* transport = nullptr;
  gn::Address server;
  ClientStage stage = ClientStage::challenge;
  std::string refused;  // the server's reason when refused or dropped
  int32_t challenge = 0;
  Channel ch;
  double now = 0, last_ask = -1, last_tick = -1, last_whole = -1;
  ge::forward::Dial dial;  // the route through `cfg.via`, when it has hops
  bool dialing = false;
  int tick = 0;

  client::Demo demo;  // what the server sent, as the sequel's recording, read as it comes
  bool recording = true;
  std::vector<std::string> stuffed;          // every line stuffed, in order (the first 256)
  std::vector<std::string> prints;           // every print's text, in order (the first 256)
  int signon_steps = 0;                      // signon commands answered
  int levels = 0;                            // server data read
  double connected_at = -1, active_at = -1;  // the network's times, s

  // The downloads (SequelClientConfig::fetch): the files still to ask for, the one coming, its
  // bytes and archive, the `begin` held back until the last, and every file fetched.
  std::vector<std::string> fetch_queue;
  std::string fetching;
  std::vector<uint8_t> fetched_bytes;
  bool fetched_origin = false;
  ge::vfs::ArchiveOrigin fetching_origin;
  std::string held_begin;
  std::map<std::string, std::vector<uint8_t>> downloaded;
  int downloads_refused = 0;

  SequelCmd cmds[3];  // the three newest, oldest first
  // Every move sent, by its packet's outgoing sequence (its low 6 bits), as the player's hands made
  // it: what a prediction runs again past the move the server acknowledged (sequel_predict.hpp).
  sh::MoveCmd sent[64] = {};
  double sent_at[64] = {};  // the network's time each was sent, s
  double latency = 0;       // the newest acknowledged move's round trip, s
  double moves_at = -1;     // the network's time the newest other players' moves came (-1: none)
  int64_t moves_read = 0;   // the other players' moves read so far
  int64_t frames = 0, moves = 0, datagrams_in = 0, bytes_in = 0, bytes_out = 0;
  std::string error;  // a payload it could not read
  SequelNews news;    // with `cfg.into`: what the window has not yet taken
};

// The recording the client reads into: the window's (`cfg.into`) or its own.
inline client::Demo& sequel_demo(SequelClientEnd& c) {
  return c.cfg.into != nullptr ? *c.cfg.into : c.demo;
}

void sequel_client_start(SequelClientEnd& c, gn::Transport& t, const gn::Address& server,
                         const SequelClientConfig& cfg, double now);
// One client frame at the network's time: every datagram read, then one packet sent.
void sequel_client_tick(SequelClientEnd& c, int64_t now_us);
// Queues a string command on the reliable channel (`say hello`).
void sequel_client_command(SequelClientEnd& c, const std::string& line);
// Says goodbye (`disconnect`, three times) and closes the recording.
void sequel_client_stop(SequelClientEnd& c);

}  // namespace shooters_edge::net
