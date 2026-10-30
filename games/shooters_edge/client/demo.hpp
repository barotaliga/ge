// A RECORDED GAME READ BACK, frame by frame: the two recording formats the window plays (the
// `benchmark` command, timedemo.hpp), parsed far enough to know the map, the view and the
// player's status at every frame, and checked so a damaged file is refused with the place it
// broke rather than misread.
//
// THE TWO FORMATS (`DemoFormat`), told apart by their first bytes, never by the file's name:
//
// - SINGLE (`.dem`): the single-player protocol (version 15) as a client received it. The file
//   opens with the music track as text and a newline (`-1\n`, `2\n`), then blocks: i32 length,
//   three f32 view angles (degrees), the message bytes. One block is one frame. The messages are
//   the single-player protocol's (demo_single.cpp): `serverinfo` names the map (the first model,
//   `maps/<name>.bsp`), `setview` the entity the eye rides, the entity updates move it from its
//   baseline, `clientdata` carries the view height and the status.
// - CLIENT (the multiplayer client's recording): records of f32 time, a type byte and its body:
//   0 a command (the usercmd struct, 24 bytes, or 28 from a client that pads it; detected once,
//   then three f32 view angles), 1 a packet as received (i32 length, the 8-byte channel header,
//   the multiplayer protocol's messages, version 28: demo_client.cpp), 2 the sequences (two
//   i32). A frame ends at each packet. The eye is the recording player's own `playerinfo` origin.
//
// THE AXES are the map's: units, z up, angles in degrees with the pitch down positive and the
// yaw counter-clockwise from +x (view.hpp converts to the engine's).
//
// WHAT IS KEPT: the precache lists, every entity's last origin, angles, model and frame (so a
// renderer of entities can draw them), the stats, the demo's clock. Sounds, prints and effects
// are read and skipped.
//
// ge:summary Recorded games read frame by frame: the single-player demo and the client recording
// ge:keywords demo recording dem playback timedemo benchmark single-player multiplayer client
// ge:keywords serverinfo serverdata playerinfo packet-entities clientdata view-entity open_demo
// ge:keywords next_frame demo-format demo-state stats intermission
// ge:skill shooter
// ge:test tests/shooters_edge_demo_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

enum class DemoFormat : uint8_t { single, client };

// The stats a frame carries (the single-player `clientdata`, the multiplayer `updatestat`): the
// classic stat numbers, so the HUD reads one table for both.
enum DemoStat : uint8_t {
  kStatHealth = 0,
  kStatAmmo = 3,
  kStatArmour = 4,
  kStatShells = 6,
  kStatNails = 7,
  kStatRockets = 8,
  kStatCells = 9,
  kStatActiveWeapon = 10,
  kStatItems = 15,
  kStatCount = 32,
};

struct DemoEntity {
  sh::Vec3f origin, angles;  // units, degrees
  int model = 0, frame = 0, skin = 0, effects = 0;
  bool present = false;  // updated this frame (single) or in the last entity list (client)
};

struct DemoState {
  DemoFormat format = DemoFormat::single;
  int protocol = 0;                         // 15 single, 28 client
  std::string map;                          // "e1m1" off `maps/e1m1.bsp`; "" before the first level
  int levels = 0;                           // level starts read so far (a new level: load its map)
  double time = 0;                          // the demo's clock, s
  sh::Vec3f eye;                            // the view's eye, units
  sh::Vec3f angles;                         // the view's angles, degrees
  int view_entity = 1;                      // single: the entity the eye rides
  int player = 0;                           // client: the recording player's slot
  float view_height = 22;                   // over the origin, units
  bool intermission = false;                // the level ended: the eye is the intermission's
  int32_t stats[kStatCount] = {};           // health, ammo, items ... (DemoStat)
  std::vector<std::string> models, sounds;  // precache lists, index 1 first ([0] is "")
  std::vector<DemoEntity> entities;         // by number
  std::vector<DemoEntity> players;          // client: by slot (32)
  int64_t frames = 0;                       // frames read
};

struct Demo {
  std::vector<uint8_t> bytes;
  size_t pos = 0;
  DemoState state;
  std::vector<DemoEntity> baselines;  // by entity number
  // The client format's packet-entity frames by the low 6 bits of their packet's sequence.
  std::vector<std::vector<std::pair<int, DemoEntity>>> ring;
  uint32_t incoming = 0;              // the last packet's sequence
  uint32_t ext_fte = 0, ext_mvd = 0;  // the client format's protocol extensions
  int cmd_size = 0;                   // the client format's usercmd struct: 24 or 28 (0: unknown)
  bool ended = false;
};

// `bytes` as a demo of either format (detected), positioned at its first frame. False with the
// reason and the next step when it is neither.
bool open_demo(std::vector<uint8_t> bytes, Demo& out, std::string& error);

enum class DemoRead : uint8_t { frame, end, error };

// The next frame into `d.state`. `end` once the file (or a disconnect) ends it; `error` with the
// byte offset, the message and the next step in `error` when a message does not read.
DemoRead next_frame(Demo& d, std::string& error);

// --- shared by the two formats' readers (demo_single.cpp, demo_client.cpp) ---

// A message's bytes, read little-endian. Reading past the end sets `bad` and returns zeros.
struct DemoMsg {
  std::span<const uint8_t> b;
  size_t at = 0;
  bool bad = false;
  int u8();
  int i8();
  int i16();
  int u16();
  int32_t i32();
  float f32();
  std::string str();
  void skip(size_t n);
  bool done() const { return at >= b.size(); }
};

// One message stream (a single-player block, a client packet's payload) into `d.state`. False
// with the reason in `error`.
bool read_single_messages(Demo& d, DemoMsg& m, std::string& error);
bool read_client_messages(Demo& d, DemoMsg& m, std::string& error);

// `maps/<name>.bsp` -> `<name>`; anything else -> "".
std::string map_of_model(const std::string& model);

}  // namespace shooters_edge::client
