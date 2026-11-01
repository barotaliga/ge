// A RECORDED GAME READ BACK, frame by frame: the two recording formats the window plays (the
// `timedemo` command, timedemo.hpp), parsed far enough to know the map, the view and the
// player's status at every frame, and checked so a damaged file is refused with the place it
// broke rather than misread.
//
// THE THREE FORMATS (`DemoFormat`), told apart by their first bytes, never by the file's name (a
// gzip file is unpacked first, so a `.gz` recording opens as the one inside):
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
// - MULTIVIEW (the server's recording of every player, `.mvd`): records of a milliseconds byte, a
//   type byte (the low 3 bits the kind, the high 5 a player) and a body; the messages are the
//   multiplayer protocol's as a client that sees everything would get them, the players as the
//   recorder's own `playerinfo` form (demo_multiview.cpp). A frame ends where the next record's
//   milliseconds are not 0. The view rides one player (`Demo::track`, the lowest one playing by
//   default): his origin and his view's angles, his stats, the messages sent to him alone.
//
// THE AXES are the map's: units, z up, angles in degrees with the pitch down positive and the
// yaw counter-clockwise from +x (view.hpp converts to the engine's).
//
// WHAT IS KEPT: the game folder the server ran (timedemo.hpp's `demo_game_dir`), the precache
// lists, every entity's last origin, angles, model and frame (so a renderer of entities can draw
// them), the static entities and the static (looping) sounds of the level, the stats, each player's
// frags, name and colours (client and multiview), the demo's clock; and, for the frame just read
// only (`DemoState::started`, `stopped`, `voice`: emptied at the next frame), the sounds it started
// and stopped and the voice it carried. Prints and effects are read and skipped.
//
// THE VOICE FORMATS (`.qwdv`, `.mvdv`): a client or multiview recording with the players' voice in
// it is a format of its own, so a player that knows only the plain formats never meets a message
// it cannot read: an 8-byte header (`kVoiceMagic` "GEVR", the version 1, the base format 1 client
// or 2 multiview, the codec `kVoiceOpus`), then the base format's records unchanged with the voice
// messages among their messages. Without the header and the voice messages it is a plain
// recording (net/classic_record.hpp writes either). `DemoState::voice_format` says which was read.
//
// VOICE (`DemoVoice`) is the protocol's voice message (svc 84, under the x2 family's voice bit):
// the talker's slot, a generation byte (the codec in its high nibble, `kVoiceOpus`; the talker's
// stream generation in its low one), the first frame's sequence byte, a u16 length and the
// payload: 20 ms Opus frames of 48 kHz mono, each a u16 length and its bytes. A client recording
// with voice carries the voices its player was sent (the server routes a team's talk to the team
// alone) and the recorder's own, written in as he spoke; a multiview recording with voice carries
// every player's. A plain recording from another program may carry the message too and is read
// the same. demo_voice.hpp decodes them into each talker's sound.
//
// ge:summary Recorded games read frame by frame: the single-player demo and the client recording
// ge:keywords demo recording dem playback timedemo benchmark single-player multiplayer client
// ge:keywords serverinfo serverdata playerinfo packet-entities clientdata view-entity open_demo
// ge:keywords next_frame demo-format demo-state stats intermission multiview mvd gzip track
// ge:keywords voice-format qwdv mvdv voice-header sound-events static-sounds read_voice
// ge:keywords updatefrags frags updateuserinfo player-names dead gib topcolor setinfo gamedir
// ge:keywords print centerprint obituary chat demo-print
// ge:skill shooter
// ge:test tests/shooters_edge_demo_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

enum class DemoFormat : uint8_t { single, client, multiview };

// The voice formats' header (see above).
inline constexpr uint8_t kVoiceMagic[4] = {'G', 'E', 'V', 'R'};
inline constexpr size_t kVoiceHeader = 8;
inline constexpr int kVoiceOpus = 2;  // the voice message's codec (the generation's high nibble)

// The header a voice format of `base` (client or multiview) opens with.
std::vector<uint8_t> voice_header(DemoFormat base);
// The voice format's extension for `base`: `.qwdv` (client) or `.mvdv` (multiview).
std::string voice_extension(DemoFormat base);
// Whether `path` names a voice format by its extension (any case).
bool voice_name(const std::string& path);

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
  bool dead = false, gib = false;  // a multiview player's state this frame (its dead and gib bits)
};

// A sound the frame started: the protocol's sound message.
struct DemoSound {
  int entity = 0, channel = 0;
  int sound = 0;          // the sound list's index (DemoState::sounds)
  float volume = 1;       // 0..1
  float attenuation = 1;  // the protocol's factor: 0 heard everywhere, 1 a normal sound
  sh::Vec3f origin;       // units
};

// The protocol's volume byte (255 full) and attenuation byte (64 a normal sound) as factors.
inline float byte_volume(int b) { return static_cast<float>(b) / 255.0f; }
inline float byte_attenuation(int b) { return static_cast<float>(b) / 64.0f; }

// A looping sound the level placed (spawnstaticsound).
struct DemoStaticSound {
  sh::Vec3f origin;
  int sound = 0;
  float volume = 1, attenuation = 1;
};

// A line the server printed to the player (the protocol's print): deaths, chat, pickups.
struct DemoPrint {
  int level = 2;     // the multiplayer protocol's level: 0 a pickup, 1 a death, 2 high, 3 chat
  std::string text;  // as sent: may hold several lines and the charset's coloured letters
};

// One voice message: a talker's run of 20 ms frames.
struct DemoVoice {
  int sender = 0;             // the talker's slot
  int gen = 0;                // the codec (high nibble) and the talker's stream generation (low)
  int seq = 0;                // the first frame's sequence, mod 256
  std::vector<uint8_t> data;  // the frames: each a u16 length and its bytes
};

struct DemoState {
  DemoFormat format = DemoFormat::single;
  bool voice_format = false;                // a `.qwdv` or `.mvdv` (the voice header)
  int protocol = 0;                         // 15 single, 28 client and multiview
  std::string map;                          // "e1m1" off `maps/e1m1.bsp`; "" before the first level
  int levels = 0;                           // level starts read so far (a new level: load its map)
  double time = 0;                          // the demo's clock, s
  sh::Vec3f eye;                            // the view's eye, units
  sh::Vec3f angles;                         // the view's angles, degrees
  int view_entity = 1;                      // single: the entity the eye rides
  int player = 0;                           // the recording (client) or tracked (multiview) slot
  float view_height = 22;                   // over the origin, units
  bool intermission = false;                // the level ended: the eye is the intermission's
  int32_t stats[kStatCount] = {};           // health, ammo, items ... (DemoStat)
  std::vector<std::string> models, sounds;  // precache lists, index 1 first ([0] is "")
  std::vector<DemoEntity> entities;         // by number
  std::vector<DemoEntity> players;          // client, multiview: by slot (32)
  std::array<int32_t, 32> frags{};          // client, multiview: each slot's frags (updatefrags)
  std::array<std::string, 32> names;        // client, multiview: each slot's name (its userinfo)
  std::array<int, 32> top{}, bottom{};      // client, multiview: each slot's colours (0..13)
  std::string game;  // client, multiview: the game folder the server ran ("fortress"; "" unnamed)
  std::vector<DemoEntity> statics;             // the level's static entities, in order
  std::vector<DemoStaticSound> static_sounds;  // the level's looping sounds, in order
  // This frame's alone (emptied when the next one is read):
  std::vector<DemoSound> started;            // sounds started, in order
  std::vector<std::pair<int, int>> stopped;  // (entity, channel) stopped
  std::vector<DemoVoice> voice;              // voice messages, in order
  std::vector<DemoPrint> prints;             // lines printed (the single-player's at level 2)
  std::string centre;                        // a centre message (the last); empty when none
  int64_t frames = 0;                        // frames read
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
  // The multiview format: the player the view rides (-1: the lowest slot playing), each player's
  // own stats (the stats records write them; the tracked one's are copied to `state.stats`).
  int track = -1;
  std::vector<std::array<int32_t, kStatCount>> player_stats;
  int64_t hidden = 0;   // records no client sees (the recorder's own data), skipped
  int64_t records = 0;  // records read
  int pending = 0;      // records read into the frame not yet finished
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
// The multiview format: whether `bytes` opens as one, and its next frame.
bool is_multiview(std::span<const uint8_t> bytes);
DemoRead next_multiview(Demo& d, std::string& error);

// The voice message's body (after the id byte) into `out`; false when it runs past the end.
bool read_voice(DemoMsg& m, DemoVoice& out);
// A voice message's frames: each frame's bytes in order. False when the lengths overrun.
bool voice_frames(const DemoVoice& v, std::vector<std::span<const uint8_t>>& out);

// `maps/<name>.bsp` -> `<name>`; anything else -> "".
std::string map_of_model(const std::string& model);

}  // namespace shooters_edge::client
