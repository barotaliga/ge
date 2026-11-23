// QUAKE II'S NET SERVER (protocol 34, q2_net.hpp): Quake II's rules
// (server/q2_server.hpp, through a RulesRunner) served to clients over engine/net's transports,
// each client's view written by Quake II's wire (server/q2_wire.hpp), the same bytes a
// recording holds.
//
// OUT OF BAND it answers `getchallenge` (one number per address, kept), `connect` (the protocol,
// the challenge, the ban list, the spectator's or the player's password, a free slot; a refusal
// is a print a player reads), `info 34`, `status` (its server info with `maxspectators` and
// `needpass`), `ping` and `rcon` (q2_admin.hpp). A connect from an address the ban list
// refuses is refused ("Banned."). A second connect from a slot's address and qport while it is in
// use is ignored.
//
// THE SIGNON follows the client's string commands (`new`, `configstrings`, `baselines`, `begin`,
// each with the level's count) and answers each with up to about 700 bytes and the next prompt;
// a wrong count starts it again. `begin` puts the client in the rules' slot.
//
// FRAMES: every host tick (`q2_net_frame`, at the rules' step, 13 ms) reads every datagram,
// hands the moves to the rules as packets (their three commands, the rules running those a lost
// packet held), and when the rules have run a 10 Hz frame writes each client its frame: a delta
// from the frame its newest move acknowledged when that is the last one written, else whole, and
// holding only the entities the client may see (`cut_to_view`, server/q2_wire.hpp). The
// frame's prints, centred prints, stuffed lines and changed configstrings go on the reliable
// channel; the frame and its sounds unreliably.
//
// THE RATE: a client's user info `rate` (bytes a second) read as Quake II reads it (none: 5000;
// 100 the least) and then held to the server's `sv_minrate` and `sv_maxrate` (15000 and 60000 by
// default; server/rate.hpp; a client out of them told so in the game's words when it enters and
// set back with `rate N`; `sv_lanrate` 1 holds a client on the LAN to neither). The
// sizes of the last 10 frames sent (the frame and its unreliable messages, one slot a frame: a
// second at 10 Hz) are summed before each frame; past the rate, nothing goes that frame (no
// reliable data either), its slot is 0, and the next frame sent carries the count held back as
// its suppress count. The other players' moves between frames (`moves_between`) count in the
// frame's second and wait while the client is past its rate.
//
// A LEVEL CHANGE (`q2_net_level`) keeps every connection: the count goes up, each client is
// sent `changing` and `reconnect` and asks for the signon again with `new`.
//
// THE GAME'S HOOKS (the logins, q2_login.hpp): `line_hook` sees each string command but the
// signon's first and takes it (true) before the rules; `name_rule` may change the name a client
// connects or renames with. `q2_net_print`, `q2_net_rename` and `q2_net_drop` answer.
//
// THE CONNECT'S CHECKS: the user info held to the strict rule (classic/protocol/info.hpp), a
// name with something to show, cut to 15; `mvdspec` and an asked password taken out, `ip` forced.
// A repeat connect before the first `new` is answered again, later the old client is dropped. At
// the first `new` the client is asked its version (no answer: dropped at `begin`) and run the
// `connect` stuff list; `sv_forcereconnect` sends a newcomer back with a setting to bring back.
//
// A client is dropped on `disconnect`, `timeout` seconds of silence, a message it cannot read or
// a full reliable channel; its slot is then a ZOMBIE for `zombie_time` (sv_zombietime; 2 s): the
// disconnect resent until acknowledged, `status` showing it as `ZMBI`; then the slot is free.
//
// ge:summary Quake II's net server: the rules served to live protocol 34 clients over a transport
// ge:keywords q2-server protocol-34 live-server getchallenge connect signon configstrings baselines
// ge:keywords begin frame delta ack move checksum status info timeout disconnect zombie zombietime
// ge:keywords sv_zombietime q2_net_start q2_net_frame q2_net_stop q2_net_level level-change
// ge:keywords changelevel changing reconnect version-probe force-reconnect sv_forcereconnect
// ge:keywords userinfo-check malformed-userinfo name-cut packetdup nogamedata stopdl sinfo lag
// ge:keywords timed-out q2_net_shutdown killserver
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "server/bans.hpp"
#include "server/cmdfilter.hpp"
#include "server/idle.hpp"
#include "server/limit.hpp"
#include "server/lrcon.hpp"
#include "server/names.hpp"
#include "server/operator.hpp"
#include "server/settingban.hpp"
#include "server/skew.hpp"
#include "server/status.hpp"
#include "server/web_files.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/q2_mvd_server.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_ext.hpp"
#include "shooters_edge/net/qw_antilag.hpp"
#include "shooters_edge/net/qw_voice_route.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_wire.hpp"
#include "shooters_edge/server/rules_runner.hpp"
#include "vfs/partial.hpp"

namespace shooters_edge::net {

namespace sv = shooters_edge::server;

struct Q2NetConfig {  // NOLINT(clang-analyzer-optin.performance.Padding): one a server
  std::string hostname = "Shooter's Edge";
  // The server info's `version`, and the game folder its `gamedir` names ("" the base folder).
  std::string version, game_dir;
  // The server-info keys an administrator added (`sets`), as `\key\value` pairs; null: none.
  std::function<std::string()> admin_info;
  Q2MvdConfig mvd;  // the multiview recorder and its relay stream (q2_mvd_server.hpp)
  int max_clients = 8;
  std::string password;  // "" or "none": none asked
  // SPECTATORS (user info `spectator` other than 0): with a spectator password set ("" or `none`:
  // none) the key's value must be it ("Spectator password required or incorrect."), and the
  // player password is not asked. `max_spectators` (sv_maxspectators) 0: they take free player
  // slots; N: N slots of their own beside the players' ("Server spectator limit is full.").
  std::string spectator_password;
  int max_spectators = 0;
  int min_rate = 15000, max_rate = 60000;  // sv_minrate, sv_maxrate (0: no floor, no cap)
  bool lan_rate = false;  // sv_lanrate: a LAN client held to neither (server/rate.hpp)
  int ip_limit = 3;      // sv_iplimit: the most slots one address holds (0: none; server/admission)
  bool locked = false;   // sv_locked: no new clients but one taking back its slot (admission)
  std::string redirect;  // sv_redirect: where a newcomer is sent when full ("!" first: everyone)
  std::string ruleset;   // sv_ruleset: the ruleset a player keeps to ("" any; server/admission)
  // SLOTS KEPT FOR VIPS (server/admission.hpp): the players' last `vip_slots` (sv_vipslots, the
  // modern server's `sv_reserved_slots`) only for a client whose `password` is one of
  // `vip_passwords` (sv_vippassword, `sv_reserved_password`; it is asked no other password) or the
  // server's own password: "Server and reserved slots are full." to anyone else.
  int vip_slots = 0;
  ge::server::SkewRule skew;  // sv_speedwindow, sv_speedwarn, sv_speedkick (server/skew.hpp)
  double idle_time = 0;  // sv_idletime: s of no buttons, move or chat before a drop (server/idle)
  ge::server::NameRule names;  // sv_namelimit, sv_shownames (server/names.hpp)
  // sv_rconrate, sv_passwordrate (server/limit.hpp): one a second, five at once, a good password's
  // try given back; a try past them is not answered.
  ge::server::TryLimit rcon_rate{1, 1, 5}, password_rate{1, 1, 5};
  ge::server::LimitedRcon lrcon;  // lrcon_password and its allowed lines (server/lrcon.hpp)
  // sv_status, sv_statusrate (15 a second, 5 at once), sv_uptime (server/status.hpp).
  ge::server::StatusRule status{.show = 2, .rate = {15, 1, 5}, .uptime = 0};
  std::vector<std::string> vip_passwords;
  // THE ADMINISTRATOR (q2_admin.hpp): the remote console's password (empty: every rcon
  // refused), the ban list's mode (sv_banmode, Quake II's `filterban`: 1 the list bans, 0 only
  // the list joins) and file (`writeip`; empty: not saved), and the clock timed bans end by (Unix
  // seconds; unset: 0, so they never end).
  std::string rcon_password;
  int ban_mode = 1;
  std::string ban_file;
  std::function<int64_t()> unix_time;
  double timeout = 90;     // s of silence before a client is dropped
  double zombie_time = 2;  // s a dropped slot stays a zombie after the client's last packet
  // sv_ghosttime: s a slot that passed the challenge may stay silent before it is freed (0: never).
  double ghost_time = 6;
  // sv_nodelta: 1 sends whole frames whenever asked; 0 drops a client that asks for more than 64.
  bool allow_nodelta = true;
  // net_maxmsglen: the largest packet a client of protocol 35 or 36 gets (512..4086; 0: what it
  // asks; a client on this machine is never held to it). q2_net_ext.hpp.
  int net_maxmsglen = 1390;
  // sv_pingmethod: 0 no ping is shown; 1 the average of the last 16 round trips; 2 their least.
  int ping_method = 2;
  // Each client sent only the entities it may see (server/q2_wire.hpp § the viewer's cut);
  // off: every entity to everyone.
  bool cut_to_view = true;
  // Moves whose checksum byte is wrong are ignored (the frame they acknowledge still counts).
  bool verify_checksum = true;
  sv::Q2WireOptions wire;  // the map's checksum, the sky; the count and the loop set here
  // DOWNLOADS: a file a client asks for (`download <path> [offset]`, then `nextdl` for each next
  // piece, each filling the packet, paced by the rate: q2_net_download.hpp) read from the game's
  // files (`read_file`; unset: every download refused). Refused, as size -1 and percent 0:
  // downloads off, a path holding `..`, starting with
  // `.` or `/` or with no `/`, a file missing, a kind switched off (`players/` skins and figures,
  // `models/` and `sprites/`, `sound/`, `maps/`, `textures/` and `env/`, `pics/`, anything else),
  // a file past `max_download_size`, and a map out of an archive to a client that would keep it
  // loose (any but ours). A resume past the file's end is told "File size differs from server."
  // before the refusal. A client that agreed the download origin (`*ge_ext` bit 4 in its user info)
  // is told each file's archive (`file_origin`) first, as svc 123, and keeps the file in it
  // (engine/vfs/partial.hpp); maps out of archives are sent to it.
  //
  // OTHER PLAYERS' MOVES: a client that agreed them (`*ge_ext` bit 8) gets, after each frame, svc
  // 124: for each other player in the frame and walking (at most 30), the slot, the milliseconds
  // since the server last ran a move of his (at most 255), his state as Quake II's move holds it
  // and that move, so the client runs him forward to its own time as the QuakeWorld join does
  // (net/q2_predict.hpp). The bytes: a count, then each player as u8 slot, u8 msec, u8 move
  // type, i16 origin x3, i16 velocity x3 (eighths), u8 flags, u8 timer, i16 gravity, i16 delta
  // angles x3, u8 buttons, i16 angles x3, i16 forward, side and up. A hosted game library's
  // players are not sent (its state is its own). The rules move each player when his move comes,
  // not at their 10 Hz frame, so with `moves_between` the message also goes alone every other host
  // tick between the frames (about 38 a second), when some other player moved since: the client
  // runs the players forward from states 26 ms old at most instead of 100.
  //
  // THE EASY JUMP'S SHARE: a client that agreed it (`*ge_ext` bit 16) is told, reliably as svc 126
  // with its first frame and again whenever it changes, the share of the best strafe's gain its
  // easy jump may take (`pm_speedjump`, a u16 in thousandths; 0 the aid off), as Quake's
  // server info states it (net/qw_remote.hpp).
  bool moves_between = true;
  // VOICE (sv_voip, off unless set; sv_voipecho; sv_earshot, as on Quake's server): a
  // client that agreed it (`*ge_ext` bit 128) may talk and hear. Its voice
  // message (clc 83: Quake's, net/qw_voice_route.hpp) is relayed as svc 121 with
  // the talker's slot, on the next host tick, to each other client that agreed the bit, sent
  // `unmuteall`, ignores no talker so named and is reached by the talker's target (`voicetarg`:
  // all, team by the rules' teams, near within `earshot`, one slot); never back to the talker
  // unless `voice_echo`. The client learns the server takes its voice from an empty svc 121 (the
  // talker 255, no frames) sent reliably as it enters, and talks only after it; a client that sent
  // voice where it had none is not dropped for it. `say_near` prints to the players the near voice
  // reaches, as Quake's server does. A stock client is never offered it and never sent it.
  bool voice = false;
  bool voice_echo = false;
  int earshot = 1000;  // sv_earshot: how near `say_near` and the near voice reach, map units
  // LAG COMPENSATION (sv_antilag 1 or 2, off unless set; sv_antilagexact, as on
  // Quake's server, net/qw_antilag.hpp): a client's bullets and rail meet each other client's
  // player where its screen showed him, box and all (a crouch): his place at the network time the
  // frame its newest move acknowledged was sent, plus 20 ms (or 1/77 s under a 20 ms ping;
  // `antilag_exact`: no lead), less a frame (1 / sv_fps) with `antilag_interp` (sv_antilag_interp:
  // the client draws the others a frame behind), and never more than `antilag_max_ms` back
  // (sv_antilag_max, 250 by default). With sv_antilag 2 and sv_antilag_projectiles his rockets,
  // grenades and bolts meet them there too. A client whose user info says `cl_antilag 0` is not
  // rewound, and is told when that changes. Bots and our rules' own shots are never rewound; a
  // hosted game library is served as it is.
  int antilag = 0;
  int antilag_max_ms = 250;
  bool antilag_exact = false;
  bool antilag_projectiles = false;
  bool antilag_interp = false;
  bool allow_download = true, allow_players = true, allow_models = true, allow_sounds = true;
  bool allow_maps = true, allow_textures = true, allow_pics = true, allow_others = false;
  // sv_max_download_size: the largest file sent, bytes (0 or past 128 MiB and 4 KiB: that cap).
  int64_t max_download_size = 8388608;
  // sv_downloadserver: the web address a client may fetch content from instead (` dlserver=` in
  // the connect answer); empty: none.
  std::string download_server;
  // The server's own web files (engine/server/web_files.hpp, sv_http_*): their settings, and once
  // served the address told when sv_downloadserver is not set (`{server}` for the host the client
  // joined) and their certificate's digest (` dlpin=` beside it; "": none).
  ge::server::WebFilesConfig web;
  std::string web_url, web_pin;
  // THE OPERATOR (q2_admin.hpp): `kick`, `kickban`, `dumpuser`, `stuff` and `stuffcvar` also take
  // a player's name in any case or a unique part of it (sv_enhanced_setplayer).
  bool enhanced_setplayer = false;
  // sv_packetdup_hack: the most extra copies of each frame a client may ask for (`packetdup <n>`).
  int packetdup_most = 0;
  // sv_force_reconnect (latched: from the next map): the address every newcomer is first sent to,
  // with a random setting it must bring back (THE FORCED RECONNECT); "" none.
  std::string force_reconnect;
  std::function<bool(const std::string& path, std::vector<uint8_t>* out)> read_file;
  std::function<bool(const std::string& path, ge::vfs::ArchiveOrigin* out)> file_origin;
  // The file as the raw deflate stream it is stored as in a `.pkz`, false for any other: sent as
  // it lies (message 22) to a 36 client at minor 1021 or later that has zlib, from its start.
  std::function<bool(const std::string& path, std::vector<uint8_t>* out)> read_deflated;
};

// A slot's stage. A ZOMBIE is a client dropped: for `zombie_time` seconds after the last packet
// from it the slot still resends what it owes (the disconnect) and reads nothing it sends, then
// it is free. Only `signon` and `active` hold a client (`q2_held`). A DUMMY is the multiview
// recorder's own spectator (q2_mvd_dummy.hpp): in the rules, with no connection, sent nothing.
enum class Q2Slot : uint8_t { free, signon, active, zombie, dummy };

struct Q2NetClient {  // NOLINT(clang-analyzer-optin.performance.Padding): one a slot
  Q2Slot stage = Q2Slot::free;
  gn::Address address;
  uint16_t qport = 0;
  Channel ch;
  sv::Q2Wire wire;
  // The last frames sent, by their number % 16 (the client keeps as many): the next frame is a
  // delta from the newest one it acknowledged, however many were sent since, and a full frame only
  // when that one is gone (a round trip past 16 frames, or a loss).
  struct SentFrame {
    int32_t number = -1;
    std::vector<sv::Q2EntityState> entities;
    sv::Q2PlayerState player;
  };
  SentFrame sent_frames[16];
  sv::Q2Signon signon;
  std::string userinfo, name;
  int userid = 0;  // the server's number for whoever took the slot
  bool spectator = false;
  int rate = 5000;  // the bytes a second it is sent, in force (THE RATE)
  // The bytes of the last second's frames, by frame number % the frames a second (10, or
  // `sv_fps` for a client told every frame).
  int frame_sizes[60] = {};
  int64_t held = 0;            // frames held back for its rate
  double heard = 0, sent = 0;  // the network's times of the last packet each way, s
  int32_t acked = -1;          // the frame the newest move acknowledged
  // The last 64 frames sent it, by frame number & 63: the frame's number and the network's time it
  // went, s; and the round trip its newest move showed, s (lag compensation).
  int64_t sent_frame[64] = {};
  double sent_at[64] = {};
  double ping_s = 0;
  // The round trips of its last 16 moves, s, and the ping they make by `sv_pingmethod` (ms):
  // shown in the scores, `status` and status answers.
  double trips[16] = {};
  int trip_count = 0;
  int ping_ms = 0;
  int nodelta = 0;          // the whole frames it asked for (a move acknowledging none)
  bool heard_once = false;  // a packet came since it connected (sv_ghosttime)
  int64_t moves = 0, bad_checksums = 0, frames = 0, full_frames = 0;
  bool told_origins = false;       // agreed the download origin (`*ge_ext` bit 4)
  bool told_moves = false;         // agreed the other players' moves (`*ge_ext` bit 8)
  bool told_share = false;         // agreed the easy jump's share (`*ge_ext` bit 16)
  int share_sent = -1;             // the share last told, thousandths (-1: none yet)
  bool told_tag = false;           // agreed the logins' key tag (`*ge_ext` bit 32)
  bool tag_sent = false;           // and was told it
  bool told_feel = false;          // agreed the move's feel (`*ge_ext` bit 64)
  std::vector<uint8_t> feel_sent;  // the feel's message last sent (empty: none yet)
  bool told_voice = false;         // agreed voice (`*ge_ext` bit 128)
  bool told_frames = false;        // agreed every rules frame (`*ge_ext` bit 256)
  bool told_hits = false;          // agreed the damage it dealt (`*ge_ext` bit 1024)
  TalkState talk;                  // its voice's target, mutes (qw_voice_route.hpp)
  std::vector<uint8_t> voice_out;  // voices relayed to it this tick (svc 121 each), unreliable
  int64_t voice_in = 0, voice_heard = 0;  // its voice messages taken; voices relayed to it
  bool told_back = false;                 // agreed the items coming back (`*ge_ext` bit 512)
  bool told_arsenal = false;              // agreed its weapons and ammunition (`*ge_ext` bit 4096)
  std::vector<uint8_t> arsenal_sent;      // the arsenal's message last sent (empty: none yet)
  std::vector<double> back_told;          // the rules' time each item told of comes back; -1 none
  ge::classic::Q2Cmd last_cmd;    // the newest move the rules ran of this player, as they ran it
  double last_cmd_at = -1;        // the network's time it came, s (-1: none yet)
  ge::server::IdleWatch idle;     // sv_idletime: when he last pressed, moved or spoke
  ge::server::SkewWatch skew;     // sv_speedwarn, sv_speedkick: his move time against ours
  ge::server::NameWatch renames;  // sv_namelimit: his last renames
  double moves_sent = -1;         // the network's time its newest moves message went, s
  std::vector<uint8_t> download;  // the file being sent
  size_t download_at = 0;         // its bytes sent so far
  bool downloading = false;
  bool download_deflated = false;  // `download` is a deflate stream the client inflates (22)
  int64_t downloads = 0;           // files sent whole
  bool download_owed = false;      // a piece asked for and not yet sent (q2_net_download.hpp)
  double download_wait = 0;        // the network's time the pause after the last piece ends, s
  int64_t download_pieces = 0;     // pieces sent
  std::string download_name;       // the file being sent, as asked for
  std::vector<uint8_t> last_out;   // the last datagram sent it (`packetdup` sends it again)
  // THE PROBES AND QUERIES (q2_admin.hpp): its first `new` sent (an assigned slot before), when;
  // its answer to the version probe; `download http` said; `nogamedata`; `packetdup`; the settings
  // `stuffcvar` asked it and it has not answered yet.
  bool newed = false;
  double new_at = -1;
  std::string version;
  bool version_heard = false;
  bool http_download = false;
  bool no_game_data = false;
  int packet_dup = 0;
  int cvars_asked = 0;
  // THE FORCED RECONNECT: the random setting's name and value it must bring back; brought back.
  std::string reconnect_var, reconnect_value;
  bool reconnected = false;
  // THE LAG (`lag`, `sinfo`, `status l`): frames whose acknowledgement came; the round trip's
  // least and most (ms) and its samples every 10 s; the moves run, made a rate every 10 s; the
  // move time it claimed against the real time over the last 16 s window, and that window's scale.
  int64_t frames_acked = 0;
  int rtt_min = 9999, rtt_max = 0;
  double rtt_sum = 0;
  int rtt_samples = 0;
  double rtt_next = -1;
  int64_t cmds_run = 0, cmds_counted = 0;
  // A LATER PROTOCOL's client (35 or 36, q2_net_ext.hpp): its protocol, minor version, packet
  // length, compression, its client settings (clc 5, 14 of them; the frame rate starts at 10),
  // the sequence its commands are handed to the rules by (one command a packet), the last command
  // (run again for each packet lost past what a move carries), the frame's flags for 36, and the
  // signon waiting for room on the old channel.
  int protocol = 34, minor = 0, msglen = 1390;
  bool zlib = false;
  int16_t asked[14] = {0, 0, 0, 0, 10};
  uint32_t run_sequence = 0;
  Q2Cmd last_run;
  uint8_t frame_flags = 0;
  std::vector<std::vector<uint8_t>> signon_queue;
  int move_fps = 0;
  double fps_next = -1;
  double scale = 0;
  int64_t scale_ms = 0;
  double scale_from = -1;
};

inline bool q2_held(const Q2NetClient& c) {
  return c.stage == Q2Slot::signon || c.stage == Q2Slot::active;
}

struct Q2NetServer {  // NOLINT(clang-analyzer-optin.performance.Padding): one a server
  Q2NetConfig cfg;
  const sv::RulesRunner* rules = nullptr;
  sv::Q2Server* game = nullptr;
  sv::NativeQ2* native = nullptr;  // a hosted game library's runner (q2_native.hpp)
  gn::Transport* transport = nullptr;
  Q2NetClient clients[sh::kMaxClients];
  Q2Mvd mvd;               // the multiview recorder (sv_mvd_enable)
  int64_t mvd_frame = -1;  // the rules frame last recorded
  bool mvd_level = true;   // a level waits for its game state
  // The challenges (1024 kept, one an address and port, each used once; the oldest
  // replaced when full): by address, and the addresses oldest first.
  std::map<uint64_t, int32_t> challenges;
  std::vector<uint64_t> challenge_order;
  uint32_t next_challenge = 0;
  int32_t count = 0;           // the level's count, as the server data carries it
  int64_t written_frame = -1;  // the tenth last sent
  int64_t written_fast = -1;   // the rules frame last sent (every frame, `*ge_ext` bit 256)
  int64_t ticks = 0;           // host ticks served
  sv::Q2Sight sight;           // our rules' entities linked for the cut, each frame
  double now = 0;
  int next_userid = 1;
  std::vector<std::string> log;  // what the server would print on its console (the first 512)
  // A client's `nextserver <count>` (a cinematic or picture level watched to its end, while
  // `cfg.wire.show` names one): the game decides (q2_game.cpp); null: ignored.
  std::function<void(int32_t count)> nextserver;
  // The tag of the key the logins' tickets are signed with (q2_login.hpp), once known: told to
  // each client that agreed it (`*ge_ext` bit 32) as svc 127, so our client logs itself in.
  std::string account_tag;
  AntilagHistory antilag;                 // each player's places by the network's time (sv_antilag)
  ge::server::BanList bans;               // the administrator's (q2_admin.hpp); refused: "Banned."
  ge::server::BanList blackholes;         // addresses never heard (blackhole_addip)
  ge::server::CmdFilters cmd_filters;     // client commands refused (filtercmd_add)
  ge::server::SettingBans cvar_bans;      // clients' settings judged (cvarban_add)
  ge::server::SettingBans userinfo_bans;  // clients' user info judged (userinfoban_add)
  int64_t rcon_runs = 0, rcon_refused = 0;
  ge::server::TryBucket rcon_tries, password_tries;  // sv_rconrate's and sv_passwordrate's
  ge::server::TryBucket status_tries;                // sv_statusrate's
  double started = -1;                               // the network's time of the first frame, s
  bool (*line_hook)(void* ctx, int slot, const std::string& line) = nullptr;
  // Each move a client's packet brings, before the rules run it (the match's prewar holds fire
  // and the jump: q2_match.hpp); null: none.
  void (*move_hook)(void* ctx, int slot, sh::MoveCmd& cmd) = nullptr;
  void* move_ctx = nullptr;
  void (*name_rule)(void* ctx, int slot, std::string& name, bool connecting) = nullptr;
  void* hook_ctx = nullptr;
  int64_t datagrams_in = 0, bytes_in = 0, bytes_out = 0, refused = 0;
  int64_t redirected = 0;              // newcomers sent elsewhere (sv_redirect)
  int64_t blackholed = 0;              // datagrams dropped unread from a blackholed address
  int64_t filtered = 0;                // client commands refused by a filter
  int64_t setting_banned = 0;          // setting and user info ban entries that matched
  ge::server::StuffLists stuff_lists;  // `addstuffcmd`: run at connect and at begin
  std::string force_reconnect;         // sv_force_reconnect as the level started with it
  bool killed = false;                 // `killserver`: nothing answered until the next map
  std::vector<gn::Address> acks;       // masters' acknowledgements, for the game to read
  uint32_t junk = 0x2545F491u;         // the forced reconnect's random strings' state
};

// Serves the started Quake II runner `rules` (a map loaded) on `t`. False when `rules` is not the
// Quake II's.
bool q2_net_start(Q2NetServer& s, const sv::RulesRunner& rules, gn::Transport& t,
                  const Q2NetConfig& cfg, std::string& error);
// One host tick at the network's time.
void q2_net_frame(Q2NetServer& s, int64_t now_us);

// The arsenal's message (svc 118 after its id, to a client that agreed `*ge_ext` bit 4096): the
// weapons `body` holds as a 32-bit mask by kSqWeapons' row, the count of ammunition kinds (`none`
// included) as a byte, then its ammunition of each kind from 1 (shells, bullets, grenades,
// rockets, cells, slugs, then the mission packs' kinds) as 16-bit counts, little-endian.
std::vector<uint8_t> q2_arsenal_message(const sv::SqBody& body);
// Every client told the server is going, and dropped.
void q2_net_stop(Q2NetServer& s);
int q2_net_clients(const Q2NetServer& s);
// The status text a `status` request is answered with (the server info, then a line a player):
// what a heartbeat carries to the masters.
std::string q2_status_text(const Q2NetServer& s);
// The slots clients may hold: the players' and the spectators' own (at most kMaxClients).
int q2_net_slots(const Q2NetServer& s);
// The rules moved onto another map (their `start` run again) with `wire` the new map's checksum
// and sky: the level's count goes up and every client is told the level changed (`changing`, then
// `reconnect`), and signs on again on the same connection; `begin` seats it on the new map.
void q2_net_level(Q2NetServer& s, const sv::Q2WireOptions& wire);
// A line printed in the client's console (reliable; level 2, a print a player reads).
void q2_net_print(Q2NetServer& s, int slot, const std::string& text);
// The client named `name` (its rules' name too, once in the game).
void q2_net_rename(Q2NetServer& s, int slot, const std::string& name);
// The client dropped (what it was printed before goes with the disconnect); `why` for the log.
void q2_net_drop(Q2NetServer& s, int slot, const std::string& why);
// Client `slot` asked for each setting a setting ban names (`cvarban_check`,
// server/settingban.hpp).
void q2_net_ask_settings(Q2NetServer& s, int slot);
// An out-of-band packet of `text` to `to` (a remote console's answer: "print\n...").
void q2_net_oob(Q2NetServer& s, const gn::Address& to, const std::string& text);
// A line the client runs (reliable), as written: add its newline.
void q2_net_stuff(Q2NetServer& s, int slot, const std::string& text);
// THE SHUTDOWN (`killserver`, `quit`, `recycle`): every client not a zombie sent at once, twice,
// `text` printed (level 2) then the disconnect, or with `reconnect` the reconnect message; every
// slot freed. `killserver` then answers nothing until the next map (`killed`).
void q2_net_shutdown(Q2NetServer& s, const std::string& text, bool reconnect);
// THE LAG's lines about client `slot` (`sinfo`, `dumpuser`'s misc block): a label padded to 21.
std::string q2_misc_lines(const Q2NetServer& s, int slot);
// `lag`'s five lines about client `slot`.
std::string q2_lag_lines(const Q2NetServer& s, int slot);
// The server info as `info` and `serverinfo` list it: a key padded to 20, a space, the value.
std::string q2_info_lines(const Q2NetServer& s);
// The players' slots `maxclients` shows outside: less the slots kept for VIPs.
int q2_public_slots(const Q2NetServer& s);
// THE LAG's numbers: the loss toward the client and from it (percent), the round trip's average
// (ms), and a span as `mm.ss`, `h:mm.ss` or `d+h:mm.ss`.
double q2_loss_down(const Q2NetClient& c);
double q2_loss_up(const Q2NetClient& c);
int q2_rtt_avg(const Q2NetClient& c);
std::string q2_span_text(double seconds);
// Client `slot` kicked: `<name> was kicked` to it and everyone in the game, then dropped; with
// `logged` the console's `<name>[<address>] was kicked` too.
void q2_net_kick(Q2NetServer& s, int slot, bool logged = true);
// Client `slot` asked for its setting `name` (`stuffcvar`): its answer printed on the console as
// `<name>[<address>]: "<setting>" is "<value>"`.
void q2_net_ask_setting(Q2NetServer& s, int slot, const std::string& name);

}  // namespace shooters_edge::net
