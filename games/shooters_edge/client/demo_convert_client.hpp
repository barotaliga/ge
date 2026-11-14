// THE CLIENT RECORDING WRITTEN FOR THE OLDEST CLIENTS (demo_convert.hpp's pairs to the client
// recording, protocol 28 with no extension), shared by every source that converts to it, and the
// wire pieces the converters share (the byte writer, an entity's state and its delta).
//
// WHAT THE OLDEST CLIENT ASKS OF A RECORDING (porting/shooters-edge/demo-write-client.md):
// - A packet record is at most 1450 bytes with its 8-byte channel header: the writer keeps a
//   frame's packet to 1400 and a signon packet to about 725, the size the client's own recorder
//   flushes at. A message that does not fit a frame's packet waits for the next frame's.
// - Both words of the channel header are the packet's sequence, rising by one a packet: the
//   entities are kept under the first and the players under the second.
// - Every frame is a whole list (`packetentities`, never a delta: the client finds no frame to
//   delta from while it plays a recording), at most 64 entities (the nearest the view are kept),
//   every entity a delta from the baseline the writer sent for it, or written whole when it has
//   none. Every player in view is in every frame's packet; another player carries his command,
//   whose angles the client draws him with.
// - The view's angles come only from the command records, and the client moves the view only
//   when a command record stands one sequence past the newest packet: after each signon a
//   sequences record puts the commands one sequence ahead of the packets, and every frame's
//   packet follows a command record at its time with its angles (the milliseconds since the frame
//   before, the moves the view's velocity in its own axes, so the client's prediction carries the
//   view the way it went).
// - The signon: the server data (the view's slot as the player number), the server's info stuffed
//   as `fullserverinfo`, the sound list before the model list (completing the sound list clears
//   the models), the model list completed a moment later than the server's info so it is read
//   first, then the source's own signon (the statics, the baselines, the light styles, the names),
//   then a stuffed `skins`.
// - The end is a disconnect inside a packet (the out-of-band end some recorders write is not one
//   the oldest client reads).
//
// ge:summary The client recording written for the oldest clients, shared by every converter to it
// ge:keywords demo-convert client-recording protocol-28 old-client packetentities 64-entities
// ge:keywords command-record sequences-record signon-packet conv-out conv-ent entity-delta
// ge:skill shooter
// ge:test tests/shooters_edge_demo_convert_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "shooters_edge/client/demo_convert.hpp"

namespace shooters_edge::client {

// Bytes written little-endian.
struct ConvOut {
  std::vector<uint8_t> b;
  void u8(int v) { b.push_back(static_cast<uint8_t>(v)); }
  void i16(int v);
  void i32(int32_t v);
  void f32(float f);
  void str(const std::string& s);
  void bytes(std::span<const uint8_t> s) { b.insert(b.end(), s.begin(), s.end()); }
  bool empty() const { return b.empty(); }
  size_t size() const { return b.size(); }
};

// An entity's state as the wire carries it (coordinates in eighths, angles in 1/256 of a turn).
struct ConvEnt {
  int model = 0, frame = 0, colormap = 0, skin = 0, effects = 0;
  std::array<int16_t, 3> origin{};
  std::array<int8_t, 3> angles{};
  bool operator==(const ConvEnt&) const = default;
};

// The 16-bit angle of `deg` degrees.
int conv_angle16(float deg);
// A baseline's or a static's body: model, frame, colormap, skin, then origin and angle by axis.
void conv_write_spawn(ConvOut& o, const ConvEnt& e);
// An entity's delta from `from` to `to` under number `q` in the entity list's form; nothing
// when nothing changed and `always` is off; every field when `whole`.
void conv_write_delta(ConvOut& o, int q, const ConvEnt& from, const ConvEnt& to, bool always,
                      bool whole = false);

// The messages of one stretch, with where each message ends, so a packet is cut between them.
struct ConvMessages {
  ConvOut out;
  std::vector<size_t> ends;
  void mark() {
    if (ends.empty() || ends.back() != out.size()) ends.push_back(out.size());
  }
  void clear() {
    out.b.clear();
    ends.clear();
  }
};

// A level's start as the client recording writes it.
struct ClientLevel {
  std::string game, title, server_info;  // server_info: "\\key\\value..."
  int player = 0;                        // the view's slot
  int server_count = 1;
  std::vector<std::string> models, sounds;  // from slot 1
  std::array<float, 10> movevars{800, 100, 320, 500, 10, 10, 10, 4, 4, 1};
};

// A player in a frame.
struct ClientPlayer {
  bool present = false;
  std::array<int16_t, 3> origin{};    // eighths
  std::array<int16_t, 3> velocity{};  // units a second
  std::array<float, 3> angles{};      // degrees; the view's for the view's slot
  int frame = 0, model = 0, skin = 0, effects = 0, weapon_frame = 0;
  bool dead = false, gib = false;
};

// One frame.
struct ClientFrame {
  double time = 0;              // seconds, never going back
  std::array<float, 3> view{};  // the view's angles, degrees
  std::array<ClientPlayer, 32> players{};
  std::map<int, ConvEnt> entities;  // by number, 33 up
  std::array<int32_t, 32> stats{};
};

class ClientRecording {
 public:
  // A level starts: the server data, the server's info and the lists, at `time`.
  void level(std::vector<uint8_t>& out, double time, const ClientLevel& l, ConvertNotes& notes);
  // A baseline the source's signon sends for entity `q` (also written by `signon`'s messages).
  void baseline(int q, const ConvEnt& e) { baselines_[q] = e; }
  // Messages before the level's first frame (statics, baselines, light styles, names).
  void signon(std::vector<uint8_t>& out, double time, const ConvMessages& m);
  // A frame and the messages it brought.
  void frame(std::vector<uint8_t>& out, const ClientFrame& f, const ConvMessages& events,
             ConvertNotes& notes);
  // The end: what is still waiting, then a disconnect.
  void end(std::vector<uint8_t>& out, double time, bool disconnect, ConvertNotes& notes);

 private:
  void packet(std::vector<uint8_t>& out, double time, const ConvOut& body);
  void flush_signon(std::vector<uint8_t>& out, double time);
  void command(std::vector<uint8_t>& out, double time, int msec, const std::array<float, 3>& view,
               const ClientPlayer& p);
  void player_info(ConvOut& m, int slot, const ClientPlayer& p) const;

  uint32_t seq_ = 0;  // the last packet's sequence
  bool in_signon_ = false;
  int player_ = 0;
  double last_time_ = 0;
  bool timed_ = false;
  ConvOut pending_;                            // the signon packet being filled
  std::vector<std::vector<uint8_t>> waiting_;  // messages a full frame packet left for the next
  std::map<int, ConvEnt> baselines_;
  std::array<int32_t, 32> stats_written_{};
  bool stats_any_ = false;
};

}  // namespace shooters_edge::client
