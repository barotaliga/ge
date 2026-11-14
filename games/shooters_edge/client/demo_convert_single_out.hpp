// THE SINGLE-PLAYER DEMO (`.dem`, protocol 15) WRITTEN FROM A MULTIPLAYER MATCH seen from one
// player's view, for the oldest single-player client and current ones (the facts:
// porting/shooters-edge/demo-write-single.md). It takes what demo_convert_client.hpp's writer
// takes (a level, the signon's messages, frames of players and entities, the frame's messages, all
// in the multiplayer protocol's form) and writes the other protocol:
// - The music line, then blocks of at most 8000 bytes, each with the view's angles in its header
//   (the only angles a playing client takes).
// - The signon: the server info (16 player slots, deathmatch or co-op from the server's info, the
//   model and sound lists of at most 255, the map first), the CD track, the view's entity, stage 1;
//   the baselines, statics, light styles, names and colours, stage 2; the time, stage 3. The first
//   frame's updates start the game.
// - A frame a block: the time first, the view's status (clientdata), an update for every entity in
//   view (each a delta from its baseline: an entity not updated in a frame is hidden), then the
//   frame's messages rewritten into the single-player protocol. Frames are at most 0.1 s apart (a
//   longer gap is filled with the last frame again: the client skips what lies past 0.1 s).
// - Players: the view's player first, then the others by slot, as slots 0..15 (entity k+1, its own
//   colours); a match's players past 16 are drawn as ordinary entities with no name. Other
//   entities move up by 16 slots' worth: number n becomes n - 16.
// - The end: a disconnect.
//
// ge:summary The single-player demo written from a multiplayer match seen from one player's view
// ge:keywords demo-convert single-out dem protocol-15 serverinfo clientdata fast-update signonnum
// ge:keywords mvd-to-dem client-to-dem 16-slots temp-entity-rewrite
// ge:skill shooter
// ge:test tests/shooters_edge_demo_convert_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "shooters_edge/client/demo_convert_client.hpp"

namespace shooters_edge::client {

class SingleRecording {
 public:
  // A level starts: written at its first frame, once the signon's lists are known.
  void level(std::vector<uint8_t>& out, double time, const ClientLevel& l, ConvertNotes& notes);
  void baseline(int q, const ConvEnt& e) { baselines_[q] = e; }
  // The signon's messages (the lists, the server's info, baselines, statics, styles, names).
  void signon(std::vector<uint8_t>& out, double time, const ConvMessages& m);
  void flush(std::vector<uint8_t>&, double) {}
  void frame(std::vector<uint8_t>& out, const ClientFrame& f, const ConvMessages& events,
             ConvertNotes& notes);
  void end(std::vector<uint8_t>& out, double time, bool disconnect, ConvertNotes& notes);

 private:
  void block(std::vector<uint8_t>& out, const std::array<float, 3>& view, const ConvOut& body);
  void write_level(std::vector<uint8_t>& out, const ClientFrame& f, ConvertNotes& notes);
  // One multiplayer message rewritten into `o` (nothing when it has no place there).
  void message(ConvOut& o, std::span<const uint8_t> m, bool in_signon, ConvertNotes& notes);
  void user_info(ConvOut& o, int slot, const std::string& info, ConvertNotes& notes);
  int slot_of(int source_slot);     // -1: past the 16
  int entity_of(int source) const;  // the entity a source number becomes; -1 none
  void frame_body(ConvOut& o, const ClientFrame& f, ConvertNotes& notes);

  bool header_ = false;   // the music line written
  bool pending_ = false;  // a level not yet written
  bool written_ = false;  // its signon written: frames follow
  ClientLevel level_;
  int view_slot_ = 0;
  bool deathmatch_ = true;
  std::vector<std::string> models_, sounds_;
  int cd_track_ = 0;
  ConvOut signon_;  // the signon's rewritten messages, in order
  std::vector<size_t> signon_ends_;
  std::map<int, ConvEnt> baselines_;  // by source number
  std::array<int, 32> slots_{};       // source slot -> slot + 1 (0: none yet)
  int slots_used_ = 0;
  std::array<std::string, 32> infos_;  // each source slot's info
  std::array<int32_t, 32> stats_{};
  bool stats_any_ = false;
  bool intermission_ = false;  // its spot and view, from the source's intermission message
  std::array<int16_t, 3> intermission_origin_{};
  std::array<int8_t, 3> intermission_angles_{};
  double last_time_ = 0;
  bool timed_ = false;
  std::array<float, 3> last_view_{};
  ConvOut last_body_;  // the last frame's block after its time, for a long gap
  std::vector<std::vector<uint8_t>> waiting_;
  ConvertNotes dropped_notes_;  // what the signon dropped, told at the level
};

}  // namespace shooters_edge::client
