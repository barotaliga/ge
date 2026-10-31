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
// cut since the last. It counts the voice messages it is sent by talker.
//
// RECORDING (`record`, set after `client_start`): every packet it reads and every command it sends
// go into a ClientRecorder (classic_record.hpp), and its own voice with them in the voice format.
//
// Nothing reads a wall clock: `client_tick` takes the network's time, so a session replays.
//
// ge:summary A scripted classic-protocol client: challenge, connect, signon, downloads, moves
// ge:keywords classic-client scripted-client signon getchallenge connect pext skins download
// ge:keywords nextdl chunked frames delta-request playerinfo packetentities client_tick
// ge:keywords voice talk speech team record client-recording
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_server_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "net/voice_codec.hpp"
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

struct ClientConfig {
  std::string name = "player";
  std::string skin = "base";
  std::string team;  // the user info's team ("": none)
  int topcolor = 0, bottomcolor = 0;
  int rate = 25000;
  bool spectator = false;
  std::string password;
  ClientExtMode mode = ClientExtMode::lines;
  Ext ask = first_tier();          // what it understands
  std::vector<std::string> files;  // the files it already has ("skins/base.pcx")
  ClientScript script = &default_script;
  bool voice = false;                          // ask for voice and speak while `talk` says so
  ClientTalk talk = nullptr;                   // null: never speaks
  const std::vector<float>* speech = nullptr;  // 48 kHz mono to speak (null: a voice of its own)
  int seed = 1;
  uint16_t qport = 1;
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

struct ClassicClientEnd {
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
  int lightstyles = 0, statics = 0, static_sounds = 0;
  int valid_frame = -1;  // the newest frame's sequence held (-1: none)

  ClientDownload dl;
  std::map<std::string, std::vector<uint8_t>> downloaded;

  // Voice.
  std::unique_ptr<gn::VoiceEncoder> encoder;
  std::vector<float> speech;  // the voice it speaks, looped
  size_t speech_at = 0;
  std::vector<float> mic;            // samples not yet a whole frame
  std::vector<uint8_t> voice_out;    // this tick's voice message, sent with its move
  int voice_seq = 0;                 // frames sent, mod 256
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

}  // namespace shooters_edge::net
