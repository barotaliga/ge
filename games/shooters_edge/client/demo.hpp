// A RECORDED GAME READ BACK, frame by frame: the two recording formats the window plays (the
// `timedemo` command, timedemo.hpp), parsed far enough to know the map, the view and the
// player's status at every frame, and checked so a damaged file is refused with the place it
// broke rather than misread.
//
// THE FOUR FORMATS (`DemoFormat`), told apart by their first bytes, never by the file's name (a
// packed or archived file is unpacked first, so it opens as the one inside: demo_pack.hpp):
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
// - SEQUEL (the sequel's client recording, `.dm2`): blocks of an i32 length and one server
//   packet's messages, ended by -1; a block with a `frame` is one of the game's 10 Hz frames,
//   deltas off a ring of 16 (demo_sequel.cpp).
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
// ge:summary Recorded games read frame by frame: the single-player demo and the client recording
// ge:keywords demo recording dem playback timedemo benchmark single-player multiplayer client
// ge:keywords serverinfo serverdata playerinfo packet-entities clientdata view-entity open_demo
// ge:keywords next_frame demo-format demo-state stats intermission multiview mvd gzip track
// ge:keywords voice-format qwdv mvdv voice-header sound-events static-sounds read_voice
// ge:keywords updatefrags frags updateuserinfo player-names dead gib topcolor setinfo gamedir
// ge:keywords print centerprint obituary chat demo-print sequel dm2 sequel-demo configstring
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
#include "vfs/unpack.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

enum class DemoFormat : uint8_t { single, client, multiview, sequel };

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
  kStatWeapon = 2,  // the view model's index (0: nothing in hand)
  kStatAmmo = 3,
  kStatArmour = 4,
  kStatShells = 6,
  kStatNails = 7,
  kStatRockets = 8,
  kStatCells = 9,
  kStatActiveWeapon = 10,
  kStatTotalSecrets = 11,   // the level's secrets
  kStatTotalMonsters = 12,  // the level's monsters
  kStatSecrets = 13,        // found so far
  kStatMonsters = 14,       // killed so far
  kStatItems = 15,
  kStatCount = 32,
};

struct DemoEntity {
  sh::Vec3f origin, angles;  // units, degrees
  int model = 0, frame = 0, skin = 0, effects = 0;
  int weapon_frame = 0;  // a player's held weapon's frame (client, multiview; single: DemoState's)
  float alpha = 1;       // how much of it shows (the transparency byte; 1 solid)
  float scale = 1;       // its size (the scale byte)
  float colormod[3] = {1, 1, 1};  // its light's tint (the colour-mod bytes)
  bool present = false;           // updated this frame (single) or in the last entity list (client)
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

// The extended protocol's look bytes. An entity's transparency: 254 and 255 (unset) solid, else
// b / 254 (0 gone). A player's: b / 255 (255 solid). An entity's scale b / 16 (0 whole), a
// player's b / 50. A colour-mod byte b / 32 (32 32 32: no tint).
inline float byte_alpha(int b) { return b >= 254 ? 1.0f : static_cast<float>(b) / 254.0f; }
inline float player_byte_alpha(int b) { return static_cast<float>(b) / 255.0f; }
inline float byte_scale(int b) { return b <= 0 ? 1.0f : static_cast<float>(b) / 16.0f; }
inline float player_byte_scale(int b) { return b <= 0 ? 1.0f : static_cast<float>(b) / 50.0f; }
inline float byte_colormod(int b) { return static_cast<float>(b) / 32.0f; }

// The protocol's volume byte (255 full) and attenuation byte (64 a normal sound) as factors.

inline float byte_volume(int b) { return static_cast<float>(b) / 255.0f; }
inline float byte_attenuation(int b) { return static_cast<float>(b) / 64.0f; }

// A looping sound the level placed (spawnstaticsound).
struct DemoStaticSound {
  sh::Vec3f origin;
  int sound = 0;
  float volume = 1, attenuation = 1;
};

// A line the server printed to the player (the protocol's print): deaths, chat, pickups. `say` and
// `say_team` both arrive as level 3 to whoever may read them (a single-player recording's chat
// is known by its leading byte). Bytes from 128 are the font's second (gold) half.
struct DemoPrint {
  int level = 2;     // the multiplayer protocol's level: 0 a pickup, 1 a death, 2 high, 3 chat
  std::string text;  // as sent: may hold several lines and the charset's coloured letters
};

// One voice message, a talker's run of 20 ms frames: the protocol's voice message (svc 84, under
// the x2 family's voice bit): the talker's slot, a generation byte (the codec in its high nibble,
// `kVoiceOpus`; the talker's stream generation in its low one), the first frame's sequence byte, a
// u16 length and the payload: 20 ms Opus frames of 48 kHz mono, each a u16 length and its bytes. A
// client recording with voice carries the voices its player was sent (the server routes a team's
// talk to the team alone) and the recorder's own, written in as he spoke; a multiview recording
// with voice carries every player's. A plain recording from another program may carry the message
// too and is read the same. demo_voice.hpp decodes them into each talker's sound.
struct DemoVoice {
  int sender = 0;             // the talker's slot
  int gen = 0;                // the codec (high nibble) and the talker's stream generation (low)
  int seq = 0;                // the first frame's sequence, mod 256
  std::vector<uint8_t> data;  // the frames: each a u16 length and its bytes
};

// A temporary entity the frame sent: a hit, a spike, an explosion, a beam, a splash, blood.
// `named` and `named_trail`: the game's own named effects (svc 120, 121; `Demo::named_effects`).
enum class DemoEffectKind : uint8_t {
  spike,
  gunshot,
  explosion,
  beam,
  lava,
  teleport,
  blood,
  named,
  named_trail
};
struct DemoEffect {
  DemoEffectKind kind = DemoEffectKind::gunshot;
  int code = 0;       // the message's kind byte
  int count = 1;      // a gunshot's pellets, blood's drops (the multiplayer protocol's); else 1
  int entity = 0;     // a beam's owner
  sh::Vec3f at, end;  // units; a beam's start and end (`end` is `at` for the rest); a trail's
  sh::Vec3f way;      // a named effect's way
  std::string name;   // a named effect's
};

struct DemoState {
  DemoFormat format = DemoFormat::single;
  bool voice_format = false;                // a `.qwdv` or `.mvdv` (the voice header)
  int protocol = 0;                         // 15 single, 28 client and multiview, 34 sequel
  std::string map;                          // "e1m1" off `maps/e1m1.bsp`; "" before the first level
  int levels = 0;                           // level starts read so far (a new level: load its map)
  double time = 0;                          // the demo's clock, s
  sh::Vec3f eye;                            // the view's eye, units
  sh::Vec3f angles;                         // the view's angles, degrees
  sh::Vec3f kick;                           // sequel: the server's kick angles on the view, degrees
  int view_entity = 1;                      // single: the entity the eye rides
  int max_clients = 1;                      // single: the players, entities 1 to this
  int player = 0;                           // the recording (client) or tracked (multiview) slot
  float view_height = 22;                   // over the origin, units
  int weapon_frame = 0;                     // single: the held weapon's frame (client data)
  bool intermission = false;                // the level ended: the eye is the intermission's
  int32_t stats[kStatCount] = {};           // health, ammo, items ... (DemoStat)
  std::vector<std::string> models, sounds;  // precache lists, index 1 first ([0] is "")
  std::vector<std::string> images;          // sequel: the pictures it names (`pics/<name>.pcx`)
  std::array<std::string, 32> looks;        // sequel: each slot's model and skin ("male/grunt")
  std::vector<DemoEntity> entities;         // by number
  std::vector<DemoEntity> players;          // client, multiview: by slot (32)
  std::array<int32_t, 32> frags{};          // client, multiview: each slot's frags (updatefrags)
  std::array<std::string, 32> names;        // client, multiview: each slot's name (its userinfo)
  std::array<std::string, 32> teams;        // client, multiview: each slot's team ("": none)
  std::array<bool, 32> spectator{};         // client, multiview: the slot watches (`*spectator`)
  std::array<int, 32> top{}, bottom{};      // client, multiview: each slot's colours (0..13)
  std::array<std::string, 32> skins;        // client, multiview: each slot's `skin` ("": none)
  std::string game;  // client, multiview: the game folder the server ran ("fortress"; "" unnamed)
  // Client, multiview: the server's info keys in the order first seen (the signon's
  // `fullserverinfo`, then each `serverinfo` change): `hostname`, `teamplay`, `maxclients` ...
  std::vector<std::pair<std::string, std::string>> serverinfo;
  std::vector<DemoEntity> statics;             // the level's static entities, in order
  std::vector<DemoStaticSound> static_sounds;  // the level's looping sounds, in order
  // This frame's alone (emptied when the next one is read):
  std::vector<DemoSound> started;            // sounds started, in order
  std::vector<std::pair<int, int>> stopped;  // (entity, channel) stopped
  std::vector<DemoVoice> voice;              // voice messages, in order
  std::vector<DemoPrint> prints;             // lines printed (a multiview: the tracked one's)
  std::vector<DemoEffect> effects;           // temporary entities, in order
  std::string centre;       // a centre message or the finale's text (the last); empty when none
  uint32_t set_angles = 0;  // client, multiview: a bit a slot whose view the server set (setangle)
  int64_t frames = 0;       // frames read
};

// The sequel format's player state, as its frames carry it (whole 1/8 units, 1/4 units, angle16).
struct SequelView {
  int32_t frame = -1;  // the server frame it is of (-1: none kept in this slot)
  int16_t origin[3] = {}, velocity[3] = {}, delta[3] = {}, angles[3] = {};
  int8_t offset[3] = {}, kick[3] = {}, gun_offset[3] = {}, gun_angles[3] = {};
  uint8_t blend[4] = {};
  int pm_type = 0, pm_time = 0, pm_flags = 0, gravity = 0;
  int gun_model = 0, gun_frame = 0, fov = 0, rdflags = 0;
  int16_t stats[32] = {};
};

struct Demo {
  std::vector<uint8_t> bytes;
  std::vector<ge::vfs::Wrapping> layers;  // what open_demo peeled, outermost first
  std::string inner_name;                 // the recording's name inside an archive ("": none)
  bool truncated = false;                 // a packed recording cut short: plays to its last flush
  bool replay_v2 = false;                 // it came out of a replay file version 2 (.ged)
  size_t pos = 0;
  DemoState state;
  std::vector<DemoEntity> baselines;  // by entity number
  // The client format's packet-entity frames by the low 6 bits of their packet's sequence.
  std::vector<std::vector<std::pair<int, DemoEntity>>> ring;
  uint32_t incoming = 0;              // the last packet's sequence
  uint32_t ext_fte = 0, ext_mvd = 0;  // the client format's protocol extensions
  int cmd_size = 0;                   // the client format's usercmd struct: 24 or 28 (0: unknown)
  int player_model = 0;  // the client format's `progs/player.mdl` index: a playerinfo's default
  bool ended = false;
  // The stream is a live connection's that agreed the game's own named effects (the ge set): its
  // ids 120 and 121 are read as them. Off (every file), they are ids this reader does not know.
  bool named_effects = false;
  // The multiview format: the player the view rides (-1: the lowest slot playing), each player's
  // own stats (the stats records write them; the tracked one's are copied to `state.stats`).
  int track = -1;
  std::vector<std::array<int32_t, kStatCount>> player_stats;
  int64_t hidden = 0;   // records no client sees (the recorder's own data), skipped
  int64_t records = 0;  // records read
  int pending = 0;      // records read into the frame not yet finished
  // The sequel format: each kept frame's player state (by frame number mod 16, `ring` beside it
  // holding the entity list), the players the server takes, and the first frame read.
  std::array<SequelView, 16> views;
  int max_clients = 0;
  bool framed = false;
  // The sequel format read off a live connection (net/sequel_client.hpp): the lines the server
  // stuffed (each with its newline; the reader appends, its owner takes them), the server's
  // reconnect message seen, the server data's count, and the newest frame read whole (-1: none,
  // or the last one deltaed from a frame this reader did not hold).
  std::vector<std::string> stuffed;
  bool reconnect = false;
  int32_t server_count = 0;
  int32_t last_frame = -1;
};

// `bytes` as a demo of any format (detected), unpacked first when packed or archived
// (demo_pack.hpp: `layers`, `inner_name` and `truncated` say what was peeled), positioned at its
// first frame. False with the reason and the next step when it is none.
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
// The sequel format (demo_sequel.cpp): whether `bytes` opens as one (its first block's length and
// the server data's protocol; the rest may still be on its way), and its next frame.
bool is_sequel_demo(std::span<const uint8_t> bytes);
DemoRead next_sequel(Demo& d, std::string& error);
// `d` set up to read the sequel format from blocks appended to `d.bytes` as they come (a live
// connection's payloads); open_demo does the same for a file.
void sequel_stream_begin(Demo& d);
inline constexpr size_t kSequelStuffedKept = 256;

// The voice message's body (after the id byte) into `out`; false when it runs past the end.
bool read_voice(DemoMsg& m, DemoVoice& out);
// A voice message's frames: each frame's bytes in order. False when the lengths overrun.
bool voice_frames(const DemoVoice& v, std::vector<std::span<const uint8_t>>& out);

// `maps/<name>.bsp` -> `<name>`; anything else -> "".
std::string map_of_model(const std::string& model);

}  // namespace shooters_edge::client
