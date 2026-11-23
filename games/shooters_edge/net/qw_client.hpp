// A SCRIPTED QUAKEWORLD CLIENT: the client half the server's tests and the `se-net` lab drive, a
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
// target (`ClientConfig::voice_to`, `voicetarg`: net/qw_talk.hpp). It counts the voice
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
// go into a ClientRecorder (qw_record.hpp), and its own voice with them in the voice format.
//
// Nothing reads a wall clock: `client_tick` takes the network's time, so a session replays.
//
// ge:summary A scripted qw-protocol client: challenge, connect, signon, downloads, moves
// ge:keywords qw-client scripted-client signon getchallenge connect pext skins download
// ge:keywords nextdl chunked frames delta-request playerinfo packetentities client_tick
// ge:keywords cl_physfps cl_physfps_spectator move-rate maxfps msec-carry client_move_rate
// ge:keywords voice talk speech team record client-recording map-checksum stuffed color setinfo
// ge:keywords net-graph net-history
// ge:keywords read_file pmodel emodel heard svc-sound client_move_settings client_net_player
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_server_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "classic/content/web_downloads.hpp"
#include "hud/net_graph.hpp"
#include "net/transport.hpp"
#include "net/voice_codec.hpp"
#include "server/sheet.hpp"
#include "shooter/predict.hpp"
#include "shooter/usercmd.hpp"
#include "shooters_edge/client/voice_out.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/q2_downloads.hpp"
#include "shooters_edge/net/qw_svc.hpp"
#include "vfs/partial.hpp"

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
  // Keys the player set by `fullinfo` that no setting of the window holds, sent as given.
  std::vector<std::pair<std::string, std::string>> extra_info;
  // The competition ruleset in force (classic/rulesets.hpp), sent as `*rsn` unless "default".
  std::string ruleset;
  std::string name = "player";
  std::string skin = "base";
  std::string model;  // the user info's `model`: the player figure in players/<model>/ ("": none)
  std::string team;   // the user info's team ("": none)
  std::string w_switch, b_switch;  // the switch rules on a pickup ("": not sent)
  std::string gender;              // Quake II's user info `gender` ("": not sent)
  int topcolor = 0, bottomcolor = 0;
  int rate = 25000;
  int msg = 1;             // the user info's `msg`: the lowest print level wanted
  bool noaim = false;      // the user info's `noaim`: no aim help
  int fov = 90, hand = 0;  // Quake II's user info carries them (q2_client.hpp)
  bool spectator = false;
  bool gender_auto = false;  // Quake II's: `gender` picked from the skin's figure
  bool speedjump = false;    // cl_speedjump: the easy jump asked for (sent only when set)
  Q2Downloads q2_downloads;  // Quake II's `allow_download` switches (q2_downloads.hpp)
  bool q2_nodelta = false;   // Quake II's `cl_nodelta`: every frame asked for whole
  std::string password;
  // Through forwarders (qw_forward.hpp): the route after the first one, the server last
  // ("host:port@host:port"); `client_start`'s address is then the first forwarder's. "": direct.
  std::string prx;
  ClientExtMode mode = ClientExtMode::lines;
  Ext ask = first_tier();          // what it understands
  std::vector<std::string> files;  // the files it already has ("skins/base.pcx")
  // THE LOOKS FETCHED: during the signon it asks for each player's skin (`skins/<skin>.pcx`) and
  // named figure (`players/<model>/tris.md2`, else `tris.mdl`, and its `<skin>.pcx`) that it
  // lacks (`files`, `read_file`, what it fetched). Off (`noskins 2`): nothing is asked for.
  bool fetch_looks = true;
  // THE CONTENT FETCHED before `prespawn`: the map and every model and sound of the lists the game
  // lacks (`read_file`, `files`), asked for one at a time; one the server refuses is passed over.
  bool fetch_content = true;
  // THE MAP'S VERSIONS (classic/content/map_versions.hpp): the other versions of a map kept from
  // servers before (path in the game, bytes), and the one the first `prespawn` sends: the version
  // this server took last (`remembered`), else the player's own copy, else the next kept one; a
  // version whose checksum is in `refused` (a server that said its map does not match) is passed
  // over, and with none left the server's own is fetched (`keep_version` keeps it hidden beside
  // the player's and returns its path) and sent. `use_map` tells the window which version to draw
  // ("" the player's own copy).
  std::function<std::vector<std::pair<std::string, std::vector<uint8_t>>>(const std::string& map)>
      map_versions;
  std::function<std::string(const std::string& map, uint32_t sum,
                            const std::vector<uint8_t>& bytes)>
      keep_version;
  std::function<void(const std::string& map, const std::string& version)> use_map;
  std::vector<uint32_t> refused;
  // The checksum of the version of `map` this server took last (none: never joined it with one),
  // and the version it took this time, remembered once the client is in the game.
  std::function<std::optional<uint32_t>(const std::string& map)> remembered;
  std::function<void(const std::string& server, const std::string& map, uint32_t sum)> keep_choice;
  // THE MAP'S EXTRAS FETCHED, after the looks, only when lacking and only while what they serve is
  // on: its coloured light (`lits/<map>.lit`; none when `maps/<map>.lit` or `maps/lits/<map>.lit`
  // is there; the window's `r_loadlit`) and its place names (`locs/<map>.loc`; `tp_loadlocs`).
  // Off unless asked (the window's join turns them on by those settings).
  bool fetch_lit = false;
  bool fetch_locs = false;
  bool q2_batch = true, q2_instantpacket = true;  // cl_batchcmds, cl_instantpacket (below)
  // A file fetched, handed on to be kept (the window writes it into the game's folder, or into a
  // partial of the archive `origin` names when the server told it: engine/vfs/partial.hpp);
  // `downloaded` holds it either way. `origin` is null for a loose file or a server that did not.
  std::function<void(const std::string& gamedir, const std::string& path,
                     const std::vector<uint8_t>& bytes, const ge::vfs::ArchiveOrigin* origin)>
      keep_file;
  // The game's file at `path` ("maps/dm4.bsp") in the server's game folder `gamedir`, empty when
  // it has none. The first `prespawn` carries the map's checksum off it (qw_mapsum.hpp); with
  // no reader or no file it carries 0, which a server checking maps lets in. Before it, the client
  // reports its player and eyes models' CRC-16 (`setinfo pmodel`, `emodel`) when it has them.
  std::function<std::vector<uint8_t>(const std::string& gamedir, const std::string& path)>
      read_file;
  ClientScript script = &default_script;
  // A bot at the controls: when set, each move's command is asked of it (the tick) in place of
  // `script`. It may read anything its owner gives it; the client sends what it returns.
  std::function<sh::MoveCmd(int tick)> brain;
  bool voice = false;                          // ask for voice and speak while `talk` says so
  int q2_maxpackets = 30;                      // Quake II's cl_maxpackets (below)
  ClientTalk talk = nullptr;                   // null: never speaks
  const std::vector<float>* speech = nullptr;  // 48 kHz mono to speak (null: a voice of its own)
  // A player's own microphone in place of `talk` and `speech`: the samples the window hands in
  // (`QwClientEnd::mic_in`), sent while `mic_talking` (the push-to-talk key held).
  bool live_mic = false;
  // scr_allowsnap: a screenshot the server asks for ("cmd snap") is sent (the window grabs it,
  // `ClientUpload`); false: the server is told no.
  bool allow_snap = true;
  // Who its voice goes to (`voicetarg`: all, team, near or a slot), told to the server at its
  // `begin`; "" leaves the server's own (team). `qw_voice_target` changes it later.
  std::string voice_to;
  ge::classic::content::WebConfig web;  // `cl_http_*`: downloads from a web address
  int seed = 1;
  uint16_t qport = 1;
  // How long the server may send nothing once connected before the client gives it up, s: a
  // server that quit or crashed, or a link gone, ends the game with a reason (0: never).
  double timeout = 60;
  // The packets shown (`showpackets`, `showdrop`): the channel prints through it (null: none).
  sp::ChannelTrace* trace = nullptr;
  // THE MOVE RATE (`client_move_rate`): `cl_physfps`, moves a second (0: the server's `maxfps`),
  // held to the server's and to at least 10; `cl_physfps_spectator` in its place while watching.
  int physfps = 0;
  int physfps_spectator = 77;
  // THE PACKET RATE (`cl_c2spps`): packets a second (0: one a move; else held to 10..72). A move
  // over the rate is held back, its sequence spent, and rides in the next packet as a backup:
  // never more than two in a row, never one when a move it would carry has an impulse
  // (`cl_c2sImpulseBackup`: 1 the new move, 2 the one before too, 3 the one before that; 0 none).
  int c2spps = 0;
  int c2s_impulse_backup = 3;
  // Quake II's later protocols (q2_client.hpp): cl_protocol, net_maxmsglen, net_chantype,
  // cl_packetdup (cl_maxpackets beside `voice`).
  int q2_protocol = 0, q2_msglen = 1390, q2_chantype = 1, q2_packetdup = 1;
};

// A SCREENSHOT THE SERVER ASKED FOR (a stuffed "cmd snap"): `asked` until the window hands the
// picture to `client_upload_start`; then sent in blocks of kUploadBlock bytes, each with the
// share done, the next when the server stuffs "nextul"; "stopul" stops it. `said`: the lines the
// player reads ("Upload starting of ...", "Upload completed").
constexpr size_t kUploadBlock = 1350;
struct ClientUpload {
  bool asked = false, sending = false;
  std::vector<uint8_t> data;
  size_t at = 0;
  std::vector<std::string> said;
};

// The server's rate when its info names none, moves a second.
inline constexpr double kServerMovesDefault = 72;
inline constexpr double kLeastMoves = 10;  // the slowest move rate
// `cl_c2spps`'s range, packets a second, and the most moves held back in a row (a packet carries
// two backups); the send balance's bound, s.
inline constexpr int kLeastPackets = 10, kMostPackets = 72, kMostHeld = 2;
inline constexpr double kPacketBalanceS = 0.1;

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
  int percent = 0;  // the server's share sent so far (not chunked)
  bool chunked = false, active = false;
  bool has_origin = false;          // the server told the archive it comes out of (svc::ge_origin)
  ge::vfs::ArchiveOrigin origin{};  // that archive
};

struct QwClientEnd {  // NOLINT(clang-analyzer-optin.performance.Padding): one a client
  ClientConfig cfg;
  gn::Transport* transport = nullptr;
  gn::Address server;
  std::string refused;  // the server's reason when refused
  // A server that sent it elsewhere (a stuffed `connect <address>`, the server's `sv_redirect`):
  // the address it connects to next tick, and how many times it was sent on (at most 3).
  std::string sent_on;
  int redirects = 0;
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
  // The server's settings sheet as our server tells it (svc ge_sheet; net/qw_sheet.hpp), asked
  // for with `ge_sheet` while the server settings page is up.
  ge::server::SettingsSheet sheet;
  std::vector<std::string> sounds, models;  // [0] ""
  int sound_last = 0, model_last = 0;
  int stufftexts = 0;
  std::vector<std::string> stuffed;  // the first kStuffedKept console lines stuffed, as sent
  ClientStage stage = ClientStage::challenge;
  bool begun = false;
  double connected_at = -1, active_at = -1;  // the network's times, s
  std::vector<std::string> wanted_skins;     // skins left to fetch before begin
  // The map, models and sounds the lists named that the game lacks, fetched before `prespawn`
  // (held while `prespawn_held`), so a server's map the player has not got is played.
  std::vector<std::string> wanted_content;
  bool prespawn_held = false;
  // The map's version sent: its path ("" the player's own copy), its checksum; `fetch_version`
  // while the server's own is being fetched. `map_refused`: the server dropped the client because
  // its map did not match (the window joins again with another version, ClientConfig::refused).
  std::string map_version;
  uint32_t map_sum = 0;
  bool fetch_version = false;
  bool version_unfetched = false;  // the server would not hand out its version
  bool web_version = false;        // the server's version of the map asked from its web address
  bool map_refused = false;
  ClientUpload up;  // a screenshot the server asked for

  // Play.
  sh::MoveCmd cmds[3];
  sh::CmdClock clock;       // the moves' milliseconds, the fraction carried (shooter/usercmd.hpp)
  double send_balance = 0;  // `cl_c2spps`: s owed a packet (moves' time in, a packet's out)
  int held = 0;             // moves held back in a row
  int64_t moves_held = 0;   // all moves held back
  bool tmove = false;       // a spectator's teleport to `tmove_to` goes with the next move
  sh::Vec3f tmove_to;       // (qw_follow.hpp)
  std::vector<EntityState> baselines;
  ClientFrame frames[64];
  PlayerState players[32];
  bool player_seen[32] = {};
  std::string userinfos[32];
  int userids[32] = {};  // each slot's user id (updateuserinfo)
  int frags[32] = {};
  int32_t stats[kStats] = {};
  std::vector<std::string> prints, centerprints;
  size_t packet_prints = 0;             // `prints` before the packet being read: its own are after
  std::vector<std::string> oob_prints;  // out-of-band prints after the connect (rcon's answers)
  // A server browser's command packets from this machine (`localid`): the id they must carry
  // (the caller sets it each frame), whether one may still run without it (until the first
  // accept), and the commands accepted, for the caller to run.
  std::string localid;
  bool commands_unchecked = true;
  bool localid_refused = false;  // a wrong id cleared `localid` (the caller clears its setting)
  std::vector<std::string> local_commands;
  std::vector<ClientSound> heard;  // the sounds read this tick (cleared as the tick starts)
  float set_angles[3] = {};        // the server's last turn of the player (setangle), degrees
  float movevars[10] = {800, 100, 320, 500, 10, 10, 10, 4, 4, 1};  // serverdata's, in its order
  sh::MoveCmd sent[64];        // each move's newest command, by its packet's sequence mod 64
  uint32_t sent_seq[64] = {};  // the sequence each `sent` slot holds (0: none)
  double sent_at[64] = {};     // the network's time each `sent` slot went, s
  double player_at[32] = {};   // the network's time each player's newest record arrived, s
  double latency = 0;          // s: the newest acknowledged move's round trip
  double latency_avg = 0;      // s: the round trip smoothed over about the last 16 acknowledged
  int64_t setangles = 0;       // turns read
  int lightstyles = 0, statics = 0, static_sounds = 0;
  int valid_frame = -1;     // the newest frame's sequence held (-1: none)
  ge::hud::NetHistory net;  // each packet as it came, or not (the net graph, r_netgraph)

  ClientDownload dl;
  int downloads_started =
      0;  // the download number of the last one asked for (each starts one more)
  std::map<std::string, std::vector<uint8_t>> downloaded;
  // From a web address (classic/content/web_downloads.hpp): with the server's `*dlserver` or the
  // player's `cl_http_default_url`, the files lacked are fetched from it first, `begin` held, the
  // channel taking what it gives back; its lines for the player in `web.said`.
  ge::classic::content::WebDownloads web;

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
  ClientRecorder* record = nullptr;  // a recording running (qw_record.hpp)
  // The level so far, plain (qw_record.hpp), for `record` while joined (qw_remote.hpp): fed as
  // `record` is; `level_from` its byte where the newest level's server data begins.
  ClientRecorder* level_record = nullptr;
  size_t level_from = 0;

  // Counts.
  int64_t svc_counts[256] = {};
  int64_t full_frames = 0, delta_frames = 0, delta_misses = 0, entities_last = 0;
  int64_t datagrams_in = 0, bytes_in = 0, bytes_out = 0, chunks_oob = 0;
  std::string error;  // a message it could not read
};

void client_start(QwClientEnd& c, gn::Transport& t, const gn::Address& server,
                  const ClientConfig& cfg, double now);
// One client frame at the network's time: every datagram read, then one packet sent. A move's
// `msec` is its share of a second at `client_move_rate` in whole milliseconds, the fraction
// carried to the next (77 a second: one move of 12 ms, then 76 of 13, a second each 77).
void client_tick(QwClientEnd& c, int64_t now_us);
// Moves a second: the server's `maxfps` (72 when its info names none), lowered by `physfps` (or
// `physfps_spectator` while watching) when set, never under 10.
double client_move_rate(const QwClientEnd& c);
// `cl_c2spps`: true when the move just put in `cmds[2]`, `move_s` long, is held back rather than
// sent (the balance gains the move's time, a packet sent costs one over the rate).
bool client_hold_move(QwClientEnd& c, double move_s);
// Queues a string command on the reliable stream (a client's console line to the server).
void client_command(QwClientEnd& c, const std::string& line);
// The picture the server asked for (`ClientUpload`): its first block sent; an empty one (the
// frame could not be packed) is the server told no.
void client_upload_start(QwClientEnd& c, std::vector<uint8_t> picture);
// Its voice's target (`voicetarg <word>`: all, team, near, a slot) told to the server when it
// differs from the last one told; the word is kept until the client has begun.
void qw_voice_target(QwClientEnd& c, const std::string& word);
// A remote console line (`rcon <password> <line>`) sent out of band to the server; its answer
// arrives in `oob_prints`.
void client_rcon(QwClientEnd& c, const std::string& password, const std::string& line);

// The move settings serverdata carried (and the easy jump where the server allows it), and a
// player's record as the protocol carried it: what a prediction starts from.
sh::MoveSettings client_move_settings(const QwClientEnd& c);
sh::NetPlayer client_net_player(const PlayerState& p);

}  // namespace shooters_edge::net
