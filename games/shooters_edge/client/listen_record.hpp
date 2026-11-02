// RECORDING THE GAME BEING PLAYED: the console's `record <name>` writes the listen server's match
// (listen.hpp) as a multiview recording while it plays, and `stop` ends it. Packed by default:
// `<game folder>/<name>.mvdz`, the recording given to a zstd stream as it is written (the
// `demo_pack` and `demo_pack_level` cvars, record_cvars.hpp); with `demo_pack 0`, the plain
// `<name>.mvd`. Either one plays back with `playdemo <name>` (client/demo.hpp unpacks it).
//
// WHAT A RECORDING HOLDS, the multiview form demo.hpp reads:
//
// - THE SIGNON (`record_listen_begin`): the server data (the multiview tag, the server's time, the
//   map's title), the model and sound lists, the light styles, the static entities and every
//   connected player's user info (his name, team and colours).
// - A FRAME each time the server ran (`record_listen_frame`), stamped with the milliseconds since
//   the last (1..255): every connected player's record (origin, angles, model, skin, effects,
//   frame, dead when his health is 0 or under), the entity list whole (every live entity with a
//   model below number 512, no deltas, so a seek reads any frame on its own), the frags that
//   changed, the explosions and the lines the server printed since the last frame; then a stats
//   record of each player whose health, ammunition, armour, weapon or items changed.
// - `record_listen_end`: a disconnect, then the zstd stream ended.
//
// The bytes go to `ready` as they are made (packed: what the stream gave out, flushed every
// `kListenFlushBytes` of recording or `kListenFlushSeconds` of play, so a crash leaves a file
// that plays to the last flush), and
// the file side (`record_tick`) writes them out and empties it. The recorder reads the server and
// nothing else, so the same session records the same bytes every run.
//
// A MAP CHANGE ends the recording (the file is closed and named in the console): the next map
// is recorded with another `record`.
//
// ge:summary The console's record and stop: the listen game written as a packed multiview demo
// ge:keywords record stop listen-record mvdz mvd multiview packed zstd demo_pack signon frame
// ge:keywords packetentities playerinfo record_listen_begin record_listen_frame record_listen_end
// ge:keywords record_tick add_record_commands ready
// ge:skill shooter replay
// ge:test tests/shooters_edge_listen_record_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/record_cvars.hpp"
#include "vfs/zstd.hpp"

namespace shooters_edge::client {

// A packed recording's stream is flushed after this many plain bytes, or this much of the server's
// time, whichever comes first: a crash loses at most the last few seconds.
inline constexpr int64_t kListenFlushBytes = int64_t{1} << 20;
inline constexpr double kListenFlushSeconds = 2;

struct ListenRecorder {
  bool on = false;
  bool packed = false;
  std::string game_dir;               // the server data's game folder (record_open: the game's)
  std::vector<uint8_t> ready;         // the file's next bytes, for the file side to write
  ge::vfs::ZstdStream pack;           // packed: the stream
  int64_t plain = 0, flushed_at = 0;  // plain bytes recorded, and at the last flush
  int64_t frames = 0;                 // frames recorded
  double last_time = 0;               // the server's time at the last frame, s
  double flushed_time = 0;            // the server's time at the last flush, s
  size_t prints_seen = 0, explosions_seen = 0;
  std::array<int, 32> frags{};
  std::array<std::array<int32_t, 16>, 32> stats{};
  // The file side's (record_tick):
  std::string path;  // the file being written
  std::ofstream file;
  int64_t written = 0;  // bytes written to the file
  ~ListenRecorder();
};

// The recording begun on the server `l` plays (packed at zstd level `pack_level`, 0 plain): the
// signon into `ready`.
void record_listen_begin(ListenRecorder& r, const Listen& l, int pack_level);
// A frame of what the server holds now.
void record_listen_frame(ListenRecorder& r, const Listen& l);
// The recording ended: the disconnect, the stream ended; `on` cleared.
void record_listen_end(ListenRecorder& r);

// The file `<game folder>/<name>.mvdz` (`.mvd` unpacked) opened and the recording begun. False
// with the reason and the next step when the name, the folder or the server will not do.
bool record_open(ListenRecorder& r, const Listen& l, const SearchPath& files,
                 const std::string& name, const RecordSettings& s, std::string& error);
// The recording ended and its file closed; the line to print ("" when none was on).
std::string record_close(ListenRecorder& r);
// After the window's server frames (`frames` of them): a frame recorded and written out, or the
// recording closed when the map changed (the line printed to `console`).
void record_tick(ListenRecorder& r, const Listen& l, int frames, ge::console::Console& console);
// `record <name>` and `stop`, on `listen`'s server, the file in `files`' game folder, packed as
// `s` says.
void add_record_commands(ge::console::Console& console, ListenRecorder& r, const Listen& listen,
                         const SearchPath& files, const RecordSettings& s);

}  // namespace shooters_edge::client
