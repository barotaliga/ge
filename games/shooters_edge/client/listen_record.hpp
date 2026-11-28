// RECORDING THE GAME BEING PLAYED: the console's `record <name>` writes the listen server's game
// (listen.hpp) as a demo series, `<game folder>/<name>.geds` (engine/replay/series.hpp): each level
// recorded as a multiview recording while it plays (Quake II's maps: its own recording from the
// player's view), packed as a replay file (`.ged`, demo_v2.hpp) when the level ends and added to
// the series' file then. A map change or the match's restart ends the level and the next one goes
// on in the same file; `stop`, a disconnect or quitting the game end the last level and close the
// file. `playdemo <name>` plays the levels in order (demo.hpp), and `secli demos split` gives the
// levels back as replay files. A crash loses the level being played: the levels before it are in
// the file whole.
//
// WHAT A RECORDING HOLDS, the multiview form demo.hpp reads:
//
// - THE SIGNON (`record_listen_begin`): the server data (the multiview tag, the server's time, the
//   map's title), the model and sound lists, the light styles, the static entities, every
//   connected player's user info (his name, team and colours) and the map's place names as
//   stuffed `//loc` lines (`places`), which another client's console reads as comments. The
//   Quake II's recording carries the same lines in its signon.
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
// ge:summary The console's record and stop: the listen game, level after level, as a demo series
// ge:keywords record stop listen-record mvdz mvd multiview packed zstd demo_pack signon frame
// ge:keywords packetentities playerinfo record_listen_begin record_listen_frame record_listen_end
// ge:keywords record_tick add_record_commands ready recorded-places loc-lines geds demo-series
// ge:keywords record-across-levels
// ge:skill shooter replay
// ge:test tests/shooters_edge_listen_record_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "classic/locs.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/record_cvars.hpp"
#include "shooters_edge/server/q2_wire.hpp"
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
  std::string path;  // the series being written
  std::ofstream file;
  int64_t written = 0;         // bytes written to the file
  std::vector<uint8_t> level;  // the level's recording so far (plain)
  std::string level_map;       // the level's map
  int entries = 0;             // levels added to the series
  int64_t total_frames = 0;    // frames of the levels added
  // Quake II's rules playing: its own recording (server/q2_wire.hpp) from the player's
  // view, kept whole and written as a replay file (`.ged`) at the stop.
  std::unique_ptr<sv::Q2Recorder> q2;
  // The map's place names as they are when the recording begins, kept in it (`//loc` lines,
  // plugins/classic/locs.hpp) so it plays back with them; null: none kept.
  const ge::classic::Locs* places = nullptr;
  ~ListenRecorder();
};

// The recording begun on the server `l` plays (packed at zstd level `pack_level`, 0 plain): the
// signon into `ready`.
void record_listen_begin(ListenRecorder& r, const Listen& l, int pack_level);
// A frame of what the server holds now.
void record_listen_frame(ListenRecorder& r, const Listen& l);
// The recording ended: the disconnect, the stream ended; `on` cleared.
void record_listen_end(ListenRecorder& r);

// The series `<game folder>/<name>.geds` opened and the first level's recording begun. False
// with the reason and the next step when the name, the folder or the server will not do.
bool record_open(ListenRecorder& r, const Listen& l, const SearchPath& files,
                 const std::string& name, const RecordSettings& s, std::string& error);
// The level being recorded added to the series and the file closed; the line to print ("" when
// none was on).
std::string record_close(ListenRecorder& r);
// After the window's server frames (`frames` of them): a frame recorded; at a map change or a
// restart the level added to the series and the next begun; the game gone, the file closed (the
// lines printed to `console`).
void record_tick(ListenRecorder& r, const Listen& l, int frames, ge::console::Console& console);
// RECORDING A GAME JOINED (net/qw_match_record.hpp fills it): while a server is joined,
// `record` and `stop` reach it, and `easyrecord [name]` names the file by the match
// (client/match_name.hpp) with the first free `_NNN`; the file `<game folder>/<name>.qwd` (Quake
// II's `.dm2`) holds the level from its start.
struct JoinedRecord {
  std::function<bool()> joined;
  std::function<bool(const std::filesystem::path& file, std::string& error)> open;
  std::function<std::string()> close;      // the line to print ("" when not recording)
  std::function<std::string()> path;       // the file being written ("" when none)
  std::function<int64_t()> bytes;          // what has gone into it so far (before any gzip)
  std::function<std::string()> extension;  // the joined game's recording: qwd, or Quake II's dm2
  std::function<bool()> automatic;         // the match is being recorded on its own
  std::function<std::string()> match_name;
  std::function<std::string()> folder;  // the game folder (add_record_commands sets it)
  // A screenshot of the next frame named `name` (the window's screenshots); false with `why`.
  std::function<bool(const std::string& name, std::string& why)> screenshot;
};

// A recording's size in Quake II's words: "1.2 MB", "345 kB", "12 bytes" (decimal units).
std::string q2_demo_size(int64_t bytes);

// `record <name>`, `stop` and `easyrecord`, on `listen`'s server or the server `joined` says, the
// file in `files`' game folder, packed as `s` says.
void add_record_commands(ge::console::Console& console, ListenRecorder& r, const Listen& listen,
                         const SearchPath& files, const RecordSettings& s,
                         JoinedRecord* joined = nullptr);

}  // namespace shooters_edge::client
