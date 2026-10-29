// THE SERVER'S INPUT LOG: everything a server's run took in, frame by frame, so the run replays
// to the bit. A server is a pure function of its settings and what it received in order
// (shooter/frame.hpp); the log is that order: per server frame, the slots that connected or left
// before it and the packets it ran, as the frame was handed them (a person's and a bot's alike, a
// bot's commands being the ones its source returned), and the `server_hash` after it, so a replay
// names the first frame it leaves the run on.
//
// THE FILE (`write_input_log`, `read_input_log`): little-endian, no padding.
//   "GESL" u32 version (1); the settings: u16 maxclients, u16 max_edicts, u32 seed, the map's
//   name (u8 length, bytes); u32 frames; then per frame: u8 events, u16 packets, u64 hash; each
//   event: u8 kind (1 connect, 2 disconnect), u8 slot, u8 flags (1 spectator, 2 bot); each packet:
//   u8 client, u8 flags, u32 sequence, then its commands, each u8 msec, three f32 angles (their
//   bits), i16 forward, side, up, u8 buttons, u8 impulse: with flag 1 the newest alone (its two
//   older ones are the slot's previous packet's two newer, as for any client that lost none),
//   else all three, oldest first.
// A file that is short, has another magic or version, or names a slot past maxclients is refused
// with what is wrong and where.
//
// The arrays are flat and only grow (frames index into them), so recording a frame allocates
// only when a reserve runs out.
//
// ge:summary The server's input log: per frame its connects and packets in order, and its hash
// ge:keywords input-log server-log replay record packets connects frames hash file gesl
// ge:keywords write_input_log read_input_log deterministic dedicated-server
// ge:skill shooter
// ge:test tests/shooters_edge_host_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooter/client.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

constexpr uint32_t kInputLogVersion = 1;

enum class SlotEventKind : uint8_t { connect = 1, disconnect = 2 };
constexpr uint8_t kSlotSpectator = 1;
constexpr uint8_t kSlotBot = 2;

struct SlotEvent {
  SlotEventKind kind = SlotEventKind::connect;
  uint8_t slot = 0;
  uint8_t flags = 0;  // kSlotSpectator, kSlotBot
};

struct LogFrame {
  uint32_t first_event = 0, events = 0;
  uint32_t first_packet = 0, packets = 0;
  uint64_t hash = 0;  // server_hash after the frame
};

// What a server is created from, as the log keeps it.
struct LogSettings {
  int maxclients = 8;
  int max_edicts = 256;
  uint32_t seed = 1;          // the run's seed (a bot's draws come off it; the server draws none)
  std::string map = "arena";  // the map the server runs ("arena": the test arena)
};

struct InputLog {
  LogSettings settings;
  std::vector<LogFrame> frames;
  std::vector<SlotEvent> events;
  std::vector<sh::Packet> packets;
};

// The log's bytes.
std::vector<uint8_t> write_input_log(const InputLog& log);
// The log from bytes; false with `error` naming what is wrong and where.
bool read_input_log(const std::vector<uint8_t>& bytes, InputLog& log, std::string& error);

}  // namespace shooters_edge::server
