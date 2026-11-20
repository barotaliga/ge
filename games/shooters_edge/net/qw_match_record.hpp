// RECORDING A QUAKEWORLD SERVER JOINED: `record`, `stop` and `easyrecord` on the window's join
// (client/listen_record.hpp's `JoinedRecord`), and each match recorded on its own.
//
// - **The level kept**: from each level's server data on, the client feeds a plain recording of
//   its own (`QwClientEnd::level_record`, qw_record.hpp: the packets and the moves sent), trimmed
//   to the newest level every frame. `record <name>` writes the level so far to
//   `<game folder>/<name>.qwd` and then each frame's new bytes, across later levels too, until
//   `stop`, a disconnect or another recording.
// - **The match on its own** (`match_auto_record`): a match starts when the server info's
//   `status` leaves `standby` (in any case) outside an intermission, or at `match_forcestart`; the
//   match's name is taken then (client/match_name.hpp). Watching (`spectator 1`) records nothing
//   unless `match_auto_spectating` is 1, and a recording begun by hand is left alone. It goes to
//   `<game folder>/temp/_match_.qwd` and ends 4 s into the intermission, so the scores are in it.
//   With 2 it is then moved to `<game folder>/<name>_NNN.qwd` (the first number free); with 1 it
//   waits for `match_save`. A match cut short (`status` back to `standby`, a new level, leaving the
//   server) ends as one that ended when its 4 s were already running; else with 2 it is saved only
//   when it ran longer than `match_auto_minlength` seconds. `stop` ends it as a match's end does.
//   A recording waiting is written over by the next match.
// - Quake II's servers: not yet (its client keeps no level recording).
//
// ge:summary Recording a QuakeWorld server joined: record, stop, easyrecord, matches on their own
// ge:keywords record stop easyrecord match_auto_record match_save match_forcestart auto-record
// ge:keywords match_auto_minlength match_auto_spectating level-record qwd temp-recording
// ge:skill net replay
// ge:test tests/shooters_edge_remote_test.cpp
#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "console/console.hpp"
#include "shooters_edge/client/listen_record.hpp"
#include "shooters_edge/client/match_name.hpp"
#include "shooters_edge/net/qw_record.hpp"

namespace shooters_edge::net {

struct Remote;

inline constexpr double kMatchEndHoldS = 4;  // the recording's end after the intermission begins

struct JoinedFile {
  ClientRecorder level;  // the level so far (plain)
  std::ofstream out;
  std::filesystem::path path;  // the file written ("" when none)
  size_t written = 0;          // of `level.bytes`
  int64_t bytes = 0;           // written to the file
};

struct MatchAuto {
  bool standby = false;  // the server's `status` was standby at the last frame
  bool running = false;  // the match recorded now
  bool waiting = false;  // a finished recording waits in the temporary file for match_save
  int mode = 0;          // match_auto_record when it began
  int level = -1;        // the level it watches (Remote::levels)
  double began_s = 0, end_at_s = -1;
  std::string name;           // the match's name, taken at its start
  std::filesystem::path dir;  // the game folder it began in
};

// The joined server's facts for the match's name, the local time now.
client::MatchFacts match_facts(const Remote& r);
// The level's recording opened into `file` (the level so far written). False with the reason.
bool remote_record_open(Remote& r, const std::filesystem::path& file, std::string& error);
// The recording ended; the line to print ("" when none was on). An automatic one ends as a match.
std::string remote_record_close(Remote& r, ge::console::Console& con, double now_s);
// Each frame after the packets: the new bytes written, the old levels let go.
void remote_record_flush(Remote& r);
// Each frame at `now_s` (any clock in seconds): the match's start and end found and acted on.
void remote_match_frame(Remote& r, ge::console::Console& con, double now_s);
// `match_save`, `match_forcestart`, the `match_*` settings and the window's `JoinedRecord`.
void add_match_record_commands(ge::console::Console& con, Remote& r);

}  // namespace shooters_edge::net
