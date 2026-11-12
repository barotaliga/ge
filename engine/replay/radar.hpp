// THE RADAR TRACK (docs/tv.md's `radar` kind, docs/hub.md § The live map): where everyone is, a few
// times a second, small enough for a phone to draw a whole match as an overhead map. Any game
// writes it beside its own tracks; the hub reads nothing else to draw the map, so it needs no
// level, no model and no game code.
//
// A radar chunk is one of three, by its first byte:
//
//   'R' ROSTER    who the dots are: count, then per player slot, name, team (text). Written at the
//                 start and whenever a name or a team changes; a reader keeps the last.
//   'P' POSITIONS the dots at the chunk's time: count, then per dot slot, flags, x, y (zig-zag
//                 varints, whole units), heading (a byte: 256ths of a turn).
//   'O' OUTLINE   the level as lines, when the game has them: count, then x0, y0, x1, y1 per line.
//                 A game without one writes none, and the map is drawn from where the players went.
//
// THE MAP'S AXES are the game's choice of a ground plane: x to the right and y up the page (north),
// a heading of 0 facing up the page and growing clockwise. Units are the game's; the drawing scales
// to the bounds of everything written.
//
// A FINISHED STREAM GETS A TRACK (`stream_add_track`): the file read block by block and written
// again with the new track's chunks in the block their time falls in, every other track's bytes
// and every block's time and keyframe flag as they were, so a game adds its radar to a recording it
// has already packed without its face knowing.
//
//   radar_roster(players) / radar_positions(dots) / radar_outline(lines)   a chunk's bytes
//   read_radar(chunk, Radar&)                                               one chunk folded in
//   stream_add_track(file, track, chunks, out, error)                       a track added
//
// ge:summary The radar track: rosters, positions and an outline for an overhead map; a track added
// ge:keywords radar track overhead map positions heading roster outline dots stream_add_track
// ge:keywords read_radar radar_frame hub live-map phone
// ge:skill replay
// ge:test tests/replay_radar_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "replay/stream.hpp"

namespace ge::replay {

// RadarDot::flags.
constexpr uint8_t kRadarDead = 1, kRadarBot = 2, kRadarCarrier = 4;

struct RadarPlayer {
  uint8_t slot = 0;
  std::string name, team;
};

struct RadarDot {
  uint8_t slot = 0;
  uint8_t flags = 0;
  int32_t x = 0, y = 0;  // whole units on the map's plane
  uint8_t heading = 0;   // 256ths of a turn, clockwise from up the page
};

struct RadarLine {
  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

struct RadarFrame {
  int64_t time_ms = 0;
  std::vector<RadarDot> dots;
};

// A radar track read whole: the last roster, every positions chunk in time order, the outline.
struct Radar {
  std::vector<RadarPlayer> roster;
  std::vector<RadarFrame> frames;
  std::vector<RadarLine> outline;
  int32_t min_x = 0, min_y = 0, max_x = 0, max_y = 0;  // of every dot and line (0s: none)
  bool any = false;
};

std::vector<uint8_t> radar_roster(std::span<const RadarPlayer> players);
std::vector<uint8_t> radar_positions(std::span<const RadarDot> dots);
std::vector<uint8_t> radar_outline(std::span<const RadarLine> lines);

// One chunk of a radar track (`time_ms` its time) folded into `r`; false for a chunk that is none
// of the three or is cut short.
bool read_radar(std::span<const uint8_t> chunk, int64_t time_ms, Radar& r);
// Every radar chunk of a version 2 file into `r` (the first track of kind radar); false with
// `error` when the file does not read, true with `r.any` false when it has no radar.
bool read_radar_file(std::span<const uint8_t> file, Radar& r, std::string& error);

// `file` (a version 2 stream) written to `out` with `track` added (its id must be new) and its
// `chunks` in the blocks their times fall in (one before the first block goes in the first, one
// after the last in the last), and each of `meta` whose key the header does not have yet added to
// the header's meta. False with `error` when the file does not read or the id is taken.
bool stream_add_track(std::span<const uint8_t> file, const TrackInfo& track,
                      const std::vector<Chunk>& chunks, std::vector<uint8_t>& out,
                      std::string& error, const Meta& meta = {});

}  // namespace ge::replay
