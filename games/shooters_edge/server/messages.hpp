// WHAT THE NETWORK BUILTINS WRITE: the message sink a mod's output lands in until the protocol
// (M18g) sends it. Two kinds of record, both kept in order so a test reads exactly what a mod
// sent and to whom:
//
// - THE BUFFERS the Write builtins (#52..#59) append to, by destination: 0 the unreliable
//   broadcast, 1 one client's reliable stream (the client in `msg_entity`), 2 the reliable
//   broadcast, 3 the signon (only while the map loads), 4 the multicast buffer (sent by multicast,
//   #82, which copies it into a `multicast` event and clears it). Values are encoded here, every
//   number truncated from the float first: byte u8, char i8, short i16, long i32 (little endian),
//   coord an i16 of the value times 8 (the protocol's 1/8 grid), angle a u8 of the value times
//   256 / 360, string its bytes and a NUL, entity an i16 of its number. M18g holds these to the
//   classic protocol's own bytes and changes them here if a capture says otherwise.
// - THE EVENTS: every other effect a builtin has on clients, typed (a sound, a stuffed command, a
//   print, a centred print, a light style, a particle burst, an ambient sound, a static entity, a
//   multicast, a pause), with its recipients as a bit per client slot and whether it is reliable.
//
// Nothing here allocates once the vectors have grown; `clear_frame` empties the per-frame parts.
//
// ge:summary The message sink: the Write buffers by destination and every network effect as events
// ge:keywords qc progs message sink writebyte writecoord writeangle writestring writeentity
// ge:keywords multicast unreliable reliable signon broadcast events recipients sound stuffcmd
// ge:keywords bprint sprint centerprint lightstyle particle ambientsound makestatic encode
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_messages_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

enum class MsgDest : uint8_t { unreliable = 0, one = 1, all = 2, init = 3, multicast = 4 };

enum class WriteKind : uint8_t { byte_, char_, short_, long_, coord, angle, string, entity };

// One value encoded onto `buf`.
void encode(std::vector<uint8_t>& buf, WriteKind kind, float number, const char* text);

enum class EventKind : uint8_t {
  sound,
  stuffcmd,
  drop,  // a stuffed "disconnect\n": the client is dropped
  print,
  centerprint,
  lightstyle,
  particle,
  ambient,
  static_entity,
  multicast,
  pause,
};
const char* event_kind_name(EventKind k);

struct NetEvent {
  EventKind kind = EventKind::print;
  uint32_t to = 0;  // a bit per client slot (bit 0 the slot of entity 1)
  bool reliable = false;
  double time = 0;  // the server's time
  int ent = 0;      // the entity it is about (a sound's source, a static's model index, ...)
  int level = 0;    // a print's level; a sound's or a light style's channel or style
  sh::Vec3f origin;
  float values[4] = {0, 0, 0, 0};  // a sound's volume and attenuation, a particle's numbers
  std::string text;                // the print, the command, the sample, the pattern
  std::vector<uint8_t> bytes;      // a multicast's buffer
};

struct MessageSink {
  std::vector<uint8_t> unreliable, all, init, multicast;
  std::vector<uint8_t> one[32];  // by client slot
  std::vector<NetEvent> events;
  int64_t events_total = 0;
};

// Empties the unreliable broadcast and the events (the reliable streams and the signon stay).
void clear_frame(MessageSink& m);

// A short text for an event (the lab's tables, a test's message).
std::string event_text(const NetEvent& e);

}  // namespace shooters_edge::server
