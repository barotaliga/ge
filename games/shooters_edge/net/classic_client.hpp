// A SCRIPTED CLASSIC CLIENT: the client half the server's tests and the `se-net` lab drive, a
// stand-in for an existing client until our own client speaks the protocol (M18h). It asks for a
// challenge, connects, follows the signon as the server stuffs it (the lists by their pages,
// prespawn, spawn, the skins it lacks downloaded, begin), then plays: one packet a tick with its
// three newest commands from a script, a delta request for the newest entity frame it holds, and
// every message the server sends read and applied (the players, the entities against the frame
// they name, the stats, the prints, the user info).
//
// HOW IT ASKS FOR EXTENSIONS (`ClientExtMode`): `lines` names its families in the connect, as a
// modern client does; `pext` names none and answers the server's `cmd pext` with them; `none`
// answers `pext` with nothing (a base-protocol client). The z set travels in `*z_ext` either way.
//
// VOICE (`ClientConfig::voice`): the client asks for the x2 family's voice bit and, while its
// `talk` script says so, speaks: `speech` when given, else a voice of its own
// (net/voice_trial.hpp's speech off its seed), from where it last stopped, cut into 20 ms frames,
// Opus-coded (net/voice_codec.hpp) and sent as one voice message (clc 83) a tick with the frames
// cut since the last. At its `begin` it asks to hear voice (`unmuteall`) and names its voice's
// target (`ClientConfig::voice_to`, `voicetarg`: net/classic_talk.hpp). It counts the voice
// messages it is sent by talker. With `live_mic` it
// speaks the player's own microphone instead (`mic_in`, handed in by the window), while
// `mic_talking`.
//
// STUFFED LINES: `cmd ...` is sent on; `color`, `team`, `skin`, `name`, `topcolor`, `bottomcolor`
// and `setinfo` change the user info and tell the server (a team mod sets the player's colours
// and drops a player who does not take them); the first `kStuffedKept` lines are kept in
// `stuffed`.
//
// RECORDING (`record`, set after `client_start`): every packet it reads and every command it sends
// go into a ClientRecorder (classic_record.hpp), and its own voice with them in the voice format.
//
// Nothing reads a wall clock: `client_tick` takes the network's time, so a session replays.
//
// ge:summary A scripted classic-protocol client: challenge, connect, signon, downloads, moves
// ge:keywords classic-client scripted-client signon getchallenge connect pext skins download
// ge:keywords nextdl chunked frames delta-request playerinfo packetentities client_tick
// ge:keywords voice talk speech team record client-recording map-checksum stuffed color setinfo
// ge:keywords read_file pmodel emodel heard svc-sound client_move_settings client_net_player
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_server_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "net/voice_codec.hpp"
#include "shooter/predict.hpp"
#include "shooters_edge/client/voice_out.hpp"
#include "shooters_edge/net/classic_channel.hpp"
#include "shooters_edge/net/classic_svc.hpp"

namespace shooters_edge::net {

enum class ClientExtMode : uint8_t { lines, pext, none };

enum class ClientStage : uint8_t {
  challenge,  // asking for a challenge
  connect,    // asking to connect
  signon,     // connected: following the signon
  active,     // begun: playing
  dropped,    // refused or disconnected
};
const char* client_stage_name(ClientStage s);

struct ClientRecorder;

// The script's command at a tick (msec set by the client).
using ClientScript = sh::MoveCmd (*)(int tick, int seed);
sh::MoveCmd default_script(int tick, int seed);
// Whether the client speaks at a tick.
using ClientTalk = bool (*)(int tick, int seed);

inline constexpr size_t kStuffedKept = 256;

struct ClientConfig {
  std::string name = "player";
  std::string skin = "base";
  std::string model;  // the user info's `model`: the player figure in players/<model>/ ("": none)
  std::string team;   // the user info's team ("": none)
  int topcolor = 0, bottomcolor = 0;
  int rate = 25000;
  bool spectator = false;
  bool speedjump = false;  // cl_speedjump: the easy jump asked for (sent only when set)
  std::string password;
  // Through forwarders (classic_forward.hpp): the route after the first one, the server last
  // ("host:port@host:port"); `client_start`'s address is then the first forwarder's. "": direct.
  std::string prx;
  ClientExtMode mode = ClientExtMode::lines;
  Ext ask = first_tier();          // what it understands
  std::vector<std::string> files;  // the files it already has ("skins/base.pcx")
  // THE LOOKS FETCHED: during the signon it asks for each player's skin (`skins/<skin>.pcx`) and
  // named figure (`players/<model>/tris.md2`, else `tris.mdl`, and its `<skin>.pcx`) that it
  // lacks (`files`, `read_file`, what it fetched). Off (`noskins 2`): nothing is asked for.
  bool fetch_looks = true;
  // A file fetched, handed on to be kept (the window writes it into the game's folder);
  // `downloaded` holds it either way.
  std::function<void(const std::string& gamedir, const std::string& path,
                     const std::vector<uint8_t>& bytes)>
      keep_file;
  // The game's file at `path` ("maps/dm4.bsp") in the server's game folder `gamedir`, empty when
  // it has none. The first `prespawn` carries the map's checksum off it (classic_mapsum.hpp); with
  // no reader or no file it carries 0, which a server checking maps refuses. Before it, the client
  // reports its player and eyes models' CRC-16 (`setinfo pmodel`, `emodel`) when it has them.
  std::function<std::vector<uint8_t>(const std::string& gamedir, const std::string& path)>
      read_file;
  ClientScript script = &default_script;
  // A bot at the controls: when set, each move's command is asked of it (the tick) in place of
  // `script`. It may read anything its owner gives it; the client sends what it returns.
  std::function<sh::MoveCmd(int tick)> brain;
  bool voice = false;                          // ask for voice and speak while `talk` says so
  ClientTalk talk = nullptr;                   // null: never speaks
  const std::vector<float>* speech = nullptr;  // 48 kHz mono to speak (null: a voice of its own)
  // A player's own microphone in place of `talk` and `speech`: the samples the window hands in
  // (`ClassicClientEnd::mic_in`), sent while `mic_talking` (the push-to-talk key held).
  bool live_mic = false;
  // Who its voice goes to (`voicetarg`: all, team, near or a slot), told to the server at its
  // `begin`; "" leaves the server's own (team). `classic_voice_target` changes it later.
  std::string voice_to;
  int seed = 1;
  uint16_t qport = 1;
};

// A sound the server started, as the packet carried it (svc_sound or its extended form).
struct ClientSound {
  int ent = 0, channel = 0;
  int sound = 0;  // the sound list's index
  float volume = 1, attenuation = 1;
  sh::Vec3f origin;
};

struct ClientFrame {
  uint32_t sequence = 0;
  bool valid = false;
  std::vector<EntityState> ents;
};

struct ClientDownload {
  std::string name;
  int size = -1;  // -1 until known
  std::vector<uint8_t> bytes;
  std::vector<bool> have;     // chunks held (chunked)
  std::vector<double> asked;  // when each chunk was last asked for (chunked)
  int number = 0;
  bool chunked = false, active = false;
};

struct ClassicClientEnd {  // NOLINT(clang-analyzer-optin.performance.Padding): one a client
  ClientConfig cfg;
  gn::Transport* transport = nullptr;
  gn::Address server;
  std::string refused;  // the server's reason when refused
  int32_t challenge = 0;
  int tick = 0;
  Ext offered;  // what the challenge offered
  Ext ext;      // agreed (serverdata, and the z set)
  Channel ch;
  double now = 0, last_ask = -1;

  // Signon.
  int spawncount = 0;
  int playernum = -1;
  std::string gamedir, level, serverinfo;
  std::vector<std::string> sounds, models;  // [0] ""
  int sound_last = 0, model_last = 0;
  int stufftexts = 0;
  std::vector<std::string> stuffed;  // the first kStuffedKept console lines stuffed, as sent
  ClientStage stage = ClientStage::challenge;
  bool begun = false;
  double connected_at = -1, active_at = -1;  // the network's times, s
  std::vector<std::string> wanted_skins;     // skins left to fetch before begin

  // Play.
  sh::MoveCmd cmds[3];
  std::vector<EntityState> baselines;
  ClientFrame frames[64];
  PlayerState players[32];
  bool player_seen[32] = {};
  std::string userinfos[32];
  int frags[32] = {};
  int32_t stats[kStats] = {};
  std::vector<std::string> prints, centerprints;
  std::vector<std::string> oob_prints;  // out-of-band prints after the connect (rcon's answers)
  std::vector<ClientSound> heard;       // the sounds read this tick (cleared as the tick starts)
  float set_angles[3] = {};             // the server's last turn of the player (setangle), degrees
  float movevars[10] = {800, 100, 320, 500, 10, 10, 10, 4, 4, 1};  // serverdata's, in its order
  sh::MoveCmd sent[64];        // each move's newest command, by its packet's sequence mod 64
  uint32_t sent_seq[64] = {};  // the sequence each `sent` slot holds (0: none)
  double sent_at[64] = {};     // the network's time each `sent` slot went, s
  double player_at[32] = {};   // the network's time each player's newest record arrived, s
  double latency = 0;          // s: the newest acknowledged move's round trip
  double latency_avg = 0;      // s: the round trip smoothed over about the last 16 acknowledged
  int64_t setangles = 0;       // turns read
  int lightstyles = 0, statics = 0, static_sounds = 0;
  int valid_frame = -1;  // the newest frame's sequence held (-1: none)

  ClientDownload dl;
  std::map<std::string, std::vector<uint8_t>> downloaded;

  // Voice.
  client::VoiceOut voice;     // the frames cut and coded (client/voice_out.hpp)
  std::vector<float> speech;  // the voice it speaks, looped
  size_t speech_at = 0;
  std::vector<float> mic_in;         // live_mic: the microphone's samples not yet cut
  bool mic_talking = false;          // live_mic: the talk key held
  std::string voice_to;              // the voice's target the server was last told ("" none)
  std::vector<uint8_t> voice_out;    // this tick's voice message, sent with its move
  int64_t voice_sent = 0;            // frames sent
  int64_t voice_heard[32] = {};      // voice messages received, by talker
  ClientRecorder* record = nullptr;  // a recording running (classic_record.hpp)

  // Counts.
  int64_t svc_counts[256] = {};
  int64_t full_frames = 0, delta_frames = 0, delta_misses = 0, entities_last = 0;
  int64_t datagrams_in = 0, bytes_in = 0, bytes_out = 0, chunks_oob = 0;
  std::string error;  // a message it could not read
};

void client_start(ClassicClientEnd& c, gn::Transport& t, const gn::Address& server,
                  const ClientConfig& cfg, double now);
// One client frame at the network's time: every datagram read, then one packet sent.
void client_tick(ClassicClientEnd& c, int64_t now_us);
// Queues a string command on the reliable stream (a client's console line to the server).
void client_command(ClassicClientEnd& c, const std::string& line);
// Its voice's target (`voicetarg <word>`: all, team, near, a slot) told to the server when it
// differs from the last one told; the word is kept until the client has begun.
void classic_voice_target(ClassicClientEnd& c, const std::string& word);
// A remote console line (`rcon <password> <line>`) sent out of band to the server; its answer
// arrives in `oob_prints`.
void client_rcon(ClassicClientEnd& c, const std::string& password, const std::string& line);

// The move settings serverdata carried (and the easy jump where the server allows it), and a
// player's record as the protocol carried it: what a prediction starts from.
sh::MoveSettings client_move_settings(const ClassicClientEnd& c);
sh::NetPlayer client_net_player(const PlayerState& p);

}  // namespace shooters_edge::net
