// EVERY MATCH RECORDED BY ITSELF (docs/servers.md § Every match kept): with `sv_autorecord 1` the
// dedicated server starts a recording (record.hpp) at the frame after the match's phase
// (ServerGame::phase) turns to the match, the first its viewers see of it (a map restarted for the
// match shows from there), and finishes it when the phase leaves it, so each match is its own
// replay file from its first frame to its last, with nothing of the warmup or the scores after it.
// A game with no phases is in its match from the start: its one recording runs to the server's
// stop.
//
//   after each server_frame(server):
//     switch (match_record_frame(m, server, error)) {
//       case MatchRecord::started:  open the file;  write record_fresh(m.rec)
//       case MatchRecord::recording:                write record_fresh(m.rec)
//       case MatchRecord::finished: write record_fresh(m.rec); close the file
//     }
//   at the server's stop: match_record_stop(m, server), then write and close as finished
//
// THE FILE'S NAME says the map and when the match began, in the server's own clock's words a person
// can sort: `<map>-<YYYYMMDD>-<HHMMSS>.ged` (`match_record_name`), under the game folder's
// `demos/`.
//
// ge:summary Every match recorded by itself: a recording from the match's start to its end
// ge:keywords autorecord sv_autorecord match-record per-match recording phase-events demos
// ge:keywords match_record_frame match_record_name record-each-match
// ge:skill replay net
// ge:test tests/server_autorecord_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "server/record.hpp"
#include "server/server.hpp"

namespace ge::server {

enum class MatchRecord : uint8_t { none, started, recording, finished, failed };

struct MatchRecorder {
  int keyframe_seconds = 10;  // each match's recording keyframes this often
  Recording rec;
  bool on = false;
  LobbyPhase last = LobbyPhase::warmup;  // the phase the last frame left
  bool due = false;                      // the phase turned to the match: begin at the next frame
  int64_t started_us = 0;                // the server's clock at the match's start
  uint64_t matches = 0;                  // matches recorded whole so far
};

// After each server frame: a recording begun when the phase turned to the match, the frame added
// while one runs, finished when the phase left it. `failed` with `error` when the game has no
// broadcast (the recorder then stays off until the next match).
MatchRecord match_record_frame(MatchRecorder& m, const Server& s, std::string& error);
// The server stops: a match still recording is finished (true when there was one).
bool match_record_stop(MatchRecorder& m, const Server& s);
// `<map>-<YYYYMMDD>-<HHMMSS>.ged` for a match on `map` begun at `unix_s` (UTC).
std::string match_record_name(const std::string& map, int64_t unix_s);

}  // namespace ge::server
