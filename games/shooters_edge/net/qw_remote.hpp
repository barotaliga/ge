// THE WINDOW JOINED TO A SERVER: the `shooters_edge` window playing on a server somewhere else over
// the QuakeWorld protocol (`connect <host>[:port]` in the console), as a client of it rather than
// the player of its own listen server.
//
// HOW IT IS MADE: the protocol is the QuakeWorld client's (qw_client.hpp: the challenge, the
// connect, the signon, the moves, the downloads), driven by the window's hands in place of a
// script; and what the server sends is read as a client recording of it (client/demo.hpp), so the
// window draws a game it plays exactly as it draws one it watches: the map, the entities, the
// other players, the status, the sounds and the printed lines all come off the `DemoState` the
// recording's reader keeps. The recording is the client's own (qw_record.hpp's
// `ClientRecorder` on its `record`), fed to the reader as it grows and dropped as it is read.
//
// THE CLOCK: one move a tick at the server's `maxfps` or `cl_physfps` under it (client_move_rate),
// its msec whole milliseconds, the fraction carried; at most `kMostTicks` a call, so a stall never
// floods the server. The network's time is that tick clock: nothing here reads a wall clock, so a
// session over the simulated network replays.
//
// THE VIEW is the client's own (`Remote::view`, degrees as the protocol's: pitch down positive,
// yaw counter-clockwise from +x), turned by the window's mouse every frame and sent with every
// move; the drawn view takes it every frame, so the mouse is never a tick behind. When the server
// turns the player (a spawn, a teleport: `setangle`) the view takes the server's angles.
//
// PREDICTION: the player's own moves are run ahead of the server (plugins/shooter/predict.hpp):
// from the newest state the server sent of the player (the reply to the move it acknowledged),
// every move sent since is run through the same player move as the server's, over the level's
// solids (its brush models, `remote_world`, the movers where the last frame put them, the other
// players' boxes), with the move settings serverdata carried. A prediction the next frame proves
// wrong by 4 to 64 units is not snapped but folded into a correction that decays (`Smoothing`).
// The drawn eye and the player's own place are the prediction; `cl_nopred 1` draws the server's.
// THE EASY JUMP: with `cl_speedjump 1` when joining, the user info says so (the server, its admins
// and the players see the help is on), and the client strafes for the player on every server
// (shooter/speedjump_assist.hpp) off its newest prediction: at the share the server granted him
// alone (his own `*speedjump`, net/qw_admin.hpp), else at the server's `pm_speedjump` where
// its server info states one (ours always do; 0 turns it off), else at `kStockShare` of the best
// strafe's gain. The drawn view turns by `Remote::speedjump_view` of the swing
// (`cl_speedjump_view`). It needs `cl_nopred 0`. Changed later, `cl_speedjump` acts next join.
//
// THE OTHER PLAYERS are run forward from their newest record (plugins/shooter/predict.hpp's
// `predict_other`) to the drawn time (the window's clock less the newest move's round trip plus
// 0.02 s), their last command run for the difference (at most 255 ms) over the level's solids, so
// they move every window frame; `cl_predict_players 0` draws them where the last frame put them.
// THE OTHER ENTITIES (missiles, movers, gibs) are drawn between the last two frames read
// (`cl_nolerp 1`: the newest's places), at the newest place when absent from the older frame,
// their model changed or moved more than `kLerpMost` units (a teleport, a respawn).
//
// VOICE (`cl_voip 1` when joining): the client asks for voice; the voices the server routes to the
// player are kept in the live recording (qw_record.hpp's `live`) and heard as a demo's are,
// his own left out. While `+voip` is held he speaks the microphone the window hands to
// `remote_mic`. Our server of Quake II alike (net/q2_client.hpp's voice).
//
// ge:summary The window joined to a server: the QuakeWorld client driven by hands, drawn as a demo
// ge:keywords remote-client connect disconnect join-server qw-client live-demo network-play
// ge:keywords remote_connect remote_step remote_disconnect setangle tick-clock reconnect
// ge:keywords remote_hands add_remote_commands netstatus prediction predict cl_nopred
// ge:keywords remote_world smoothing replayed predict_other other-players cl_predict_players
// ge:keywords interpolation lerp cl_nolerp entity-lerp draw-clock round-trip
// ge:keywords cl_speedjump pm_speedjump easy-jump assisted_command kstockshare
// ge:keywords voice voice-chat cl_voip push-to-talk microphone remote_mic autologin accountkey
// ge:keywords client-presence invites q2-join remote_command cl_physfps move-rate
// ge:skill net shooter
// ge:test tests/shooters_edge_remote_test.cpp
#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "account/client_login.hpp"
#include "account/client_presence.hpp"
#include "account/client_queue.hpp"
#include "classic/content/downloads.hpp"
#include "classic/predict_original.hpp"
#include "classic/protocol/stuff_filter.hpp"
#include "classic/rulesets.hpp"
#include "console/console.hpp"
#include "console/ruleset.hpp"
#include "console/talk.hpp"
#include "console/view.hpp"
#include "input/state.hpp"
#include "net/rates.hpp"
#include "net/transport.hpp"
#include "shooter/aim_turn.hpp"
#include "shooter/predict.hpp"
#include "shooter/speedjump_assist.hpp"
#include "shooters_edge/client/chat_ignore.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/own_missiles.hpp"
#include "shooters_edge/net/q1_end.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_pingservers.hpp"
#include "shooters_edge/net/q2_predict.hpp"
#include "shooters_edge/net/q2_weapon.hpp"
#include "shooters_edge/net/qw_client.hpp"
#include "shooters_edge/net/qw_match_record.hpp"
#include "shooters_edge/net/qw_record.hpp"
#include "shooters_edge/net/qw_remote_sheet.hpp"
#include "shooters_edge/net/qw_serverstatus.hpp"
#include "shooters_edge/net/qw_watch.hpp"
#include "shooters_edge/net/qw_weapon.hpp"

namespace shooters_edge::net {

inline constexpr int kTickUs = 13000;                         // a move's length: the server's frame
inline constexpr int kQ2LeastMoves = 10, kQ2MostMoves = 125;  // Quake II's cl_maxfps range
inline constexpr int kMostTicks = 8;     // ticks a call may run; a longer stall drops the rest
inline constexpr float kLerpMost = 200;  // units: an entity moved further between two frames snaps
// units/s: a correction of the predicted place left smaller than this times the step is dropped.
inline constexpr float kSmoothDropU = 220;
inline constexpr float kStockShare = 0.8f;         // the easy jump's share on a server without it
inline constexpr int kMicMostSamples = 48000 / 4;  // the microphone's samples a frame may hold

enum class RemoteStep : uint8_t {
  waiting,    // nothing new to draw yet (connecting, or no packet since the last call)
  frame,      // draw `demo.state`
  new_level,  // load `demo.state.map`, then draw
  dropped,    // the server refused or dropped the client: `Remote::why`
};

// LOGGING IN (docs/accounts.md § From our own client): when the window names the player's account
// file (`remote_login_from`: the one the game CLI's `account signin` writes) and a server's info
// carries `*accountkey` naming the same master's key, the client trades the file's ticket with
// the master for a one-time code on a socket of its own once it is in the game, and sends the
// server `login <name> <code>`, as a player would type it. Another master's server, or one
// without logins, is joined as before; a ticket run out is said on the console.
struct RemoteLogin {
  ge::account::ClientLogin c;
  gn::Transport own;                     // the socket to the master, opened for the ask
  gn::Transport* link = nullptr;         // where the ask goes (`own`, or a test's)
  std::string path;                      // the account file
  int64_t (*unix_us)() = nullptr;        // the wall clock: the ask's and a ticket's life
  ge::account::FreshFn fresh = nullptr;  // the ask's exchange secret (nullptr: the OS's)
  void* fresh_ctx = nullptr;
  bool asked = false, done = false;  // this join's
  bool opened = false;               // `own` open
  // PRESENCE AND INVITES (docs/accounts.md § Friends): while in the game, with the same file's
  // ticket, friends see this player playing on this server and its map; invites friends send are
  // said on the console. On a socket of its own (`presence_own`, or `link` in a test).
  ge::account::ClientPresence presence;
  gn::Transport presence_own;
  bool presence_tried = false;   // this join's: the file read for it
  bool presence_opened = false;  // `presence_own` open
  // A RANKED GAME asked of the matchmaker (`ranked <mode>`), in the menus or in a game, on a
  // socket of its own (`queue_own`, or `link` in a test): when it is found, the window connects.
  ge::account::ClientQueue queue;
  gn::Transport queue_own;
  bool queue_opened = false;
};

// RCON WHILE NOT JOINED (`rcon_address`): a socket of its own to that server, and its answers
// said on the console until it has been quiet for `kRconQuietS`. `link`, when given, is the socket
// to send over instead (the simulated network's, in tests).
inline constexpr double kRconQuietS = 5;
struct RconAway {
  gn::Transport own;
  gn::Transport* link = nullptr;
  gn::Address to;
  double until_s = -1;  // when it closes (-1: closed)
  bool opened = false;
};

struct Remote {
  // In the order that packs it (the lint holds the padding).
  gn::Transport* transport = nullptr;               // what it talks over (`own`, or remote_start's)
  const ge::console::Button* voip_key = nullptr;    // `+voip` (add_remote_commands)
  const ge::console::Talk* talk = nullptr;          // the window's +voice keys (console/talk.hpp)
  std::unique_ptr<StreamWatch> watch;               // watching a broadcast (`watch`), not playing
  std::unique_ptr<RemoteLogin> login;               // logging in by the account file, when named
  std::unique_ptr<RconAway> away;                   // rcon to `rcon_address`, not joined
  std::unique_ptr<StatusAsk> status_ask;            // `serverstatus` waiting for its answer
  std::unique_ptr<ge::console::Rulesets> rulesets;  // the competition ruleset (qw_remote_rulesets)
  PingSweep ping;                                   // `pingservers`' asks (q2_pingservers.hpp)
  PassiveListen passive;                            // `passive` waiting for a server's call
  JoinedFile file;  // `record` while joined: the level kept and the file (qw_match_record.hpp)
  MatchAuto match;  // the match recorded on its own (match_auto_record)
  client::JoinedRecord joined_record;  // the window's record commands reach the join through it
  // `rerecord <name>`: the file the next join records to from its start ("": none waiting).
  std::filesystem::path rerecord;
  // `fullinfo`'s keys no window setting holds, sent in the user info at the next join.
  std::vector<std::pair<std::string, std::string>> extra_info;
  std::unique_ptr<Q2ClientEnd> q2;  // joined to a server of Quake II (q2_remote)
  std::unique_ptr<NqClientEnd> nq;  // joined to a Quake server of protocol 15 (nq_remote)
  Q2Predict q2_ahead;               // Quake II's own moves run ahead (q2_predict.hpp)
  // The window's game is Quake II: `connect` speaks its protocol (port 27910 when none is given).
  std::function<bool()> q2_game;
  int64_t now_us = 0;                         // the tick clock
  double owed_s = 0;                          // wall time not yet a tick
  int64_t setangles = 0;                      // the server's turns taken
  double frame_old_s = -1, frame_new_s = -1;  // lerp: the last two frames' times (network clock)
  const std::vector<sh::BrushModel>* world = nullptr;  // prediction: the level's models ([0] world)
  std::string server_name;                             // as asked ("host:port")
  std::string why;                                     // why it dropped
  ge::classic::FairFacts fair_facts;  // what an f_ruleset answer says (qw_remote_rulesets.hpp)
  ge::classic::FairReplies fair;      // when the last answer was said
  std::string fakeshaft_seen;         // cl_fakeshaft as last seen: a change in a match is said
  // The window's `topcolor` and `bottomcolor` last seen: a change of them is told the server.
  std::string seen_top, seen_bottom;
  sp::ChannelTrace trace;  // showpackets, showdrop: the channel's lines (add_remote_commands)
  ge::net::Rates rates;    // $cl_pps, $cl_mps, $net_uprate, $net_dnrate (qw_remote_macros.hpp)
  // Lerp: the last two frames' entities as read (by number), before anything drawn moved them.
  std::vector<client::DemoEntity> ents_old, ents_new;
  std::vector<client::DemoEntity> players_old, players_new;  // Quake II's players, the same way
  int64_t q2_frames = 0;                                     // Quake II's frames read, drawn
  gn::Transport own;                                         // remote_connect's UDP socket
  ClientRecorder recording;
  client::Demo demo;        // the recording read back: what the window draws
  client::QwIgnore ignore;  // the chat ignore lists over its players (chat_ignore.hpp)
  QwClientEnd end;
  int levels = 0;             // level starts drawn
  int replayed = 0;           // prediction: the moves it ran past the acknowledged one
  int others = 0;             // other players predicted the last step
  int others_msec = 0;        // the most milliseconds one of them was run forward the last step
  int lerped = 0;             // entities drawn between two frames the last step
  int map_tries = 0;          // joins again with another version of the server's map
  sh::Vec3f view;             // the player's angles, degrees (sent with every move)
  sh::AimTurn aim_turn;       // the aim help's turn of the view (shooter/aim_turn.hpp)
  sh::Vec3f prediction;       // the player's predicted origin, the correction not added
  sh::MoveState ahead;        // the player's whole predicted state (the easy jump's help reads it)
  sh::SpeedJumpAid jump_aid;  // the easy jump's memory (shooter/speedjump_assist.hpp)
  float speedjump_view = 0.5f;  // cl_speedjump_view: the share of its swing drawn
  sh::Smoothing smooth;         // the prediction's correction
  double smoothview = 1;        // cl_predict_smoothview: 0 snapped, 1 the decay's own rate, 2 slow
  RemoteWeapon weapon;          // the held weapon predicted (qw_weapon.hpp)
  Q2Weapon q2_weapon;           // Quake II's held weapon predicted (q2_weapon.hpp)
  // The moves the last prediction ran past the acknowledged one: each one's command and the state
  // after it (`ahead_count` of them), for the missiles' launch places and the jump's sound.
  uint32_t ahead_seq[64] = {};
  sh::MoveState ahead_each[64];
  int ahead_count = 0;
  bool console_held =
      false;  // the console held over the screen while joining (remote_console_foot)
  bool ahead_from_held = false;  // the acknowledged state's jump button was held
  client::OwnMissiles missiles;  // cl_predict_projectiles (client/own_missiles.hpp)
  int missiles_level = -1;       // the level they fly in (DemoState::levels)
  bool predict_jump = true;      // cl_predict_jump: the jump's sound played as predicted
  uint32_t jump_played_to = 0;   // the newest command whose jump has played
  int jumps_played = 0;          // jumps whose sound the prediction played (for the labs)
  // Prediction on the original move (the server's `sv_playermove original`): the stand-in
  // server, built for the level `original_level` counts and the models `original_for` points at.
  ge::classic::OriginalPrediction original;
  const std::vector<sh::BrushModel>* original_for = nullptr;
  int original_level = -1;
  sh::MoveCmd hands;  // the window's command, sent at the next tick (an impulse held to it)
  // The game's files as the server's game folder holds them (ClientConfig::read_file: the map's
  // checksum and the model checks a server asks for); unset, they are sent as 0 and unsent.
  std::function<std::vector<uint8_t>(const std::string& gamedir, const std::string& path)>
      read_file;
  // A console setting's value by name, for a server's setting bans (Q2ClientConfig::setting);
  // set by add_remote_commands to the window's console.
  std::function<std::string(const std::string& name)> setting;
  // A skin or player figure fetched while joining (ClientConfig::keep_file): the window keeps it in
  // the game's folder, or in a partial of the archive it came out of (`origin`, null when loose).
  // `kept`: one was, so once the game is begun the level is read again (RemoteStep::new_level) and
  // the players are drawn in what was fetched.
  std::function<void(const std::string& gamedir, const std::string& path,
                     const std::vector<uint8_t>& bytes, const ge::vfs::ArchiveOrigin* origin)>
      keep_file;
  // A map's other versions (ClientConfig::map_versions, classic/content/map_versions.hpp): those
  // kept, one kept, the one drawn, and which this server took (by its name as asked).
  std::function<std::vector<std::pair<std::string, std::vector<uint8_t>>>(const std::string& map)>
      map_versions;
  std::function<std::string(const std::string& map, uint32_t sum,
                            const std::vector<uint8_t>& bytes)>
      keep_version;
  std::function<void(const std::string& map, const std::string& version)> use_map;
  std::function<std::optional<uint32_t>(const std::string& server, const std::string& map)>
      remembered;
  std::function<void(const std::string& server, const std::string& map, uint32_t sum)> keep_choice;
  // A Quake II download cut short, held in the game's folder to be resumed (Q2ClientConfig::held).
  ge::classic::content::HeldDownloads held;
  // What a server stuffed that the client does not answer itself (net/q2_client.hpp's and
  // net/qw_client.hpp's `to_run`), for the window's console to run as the server's; taken each
  // frame.
  std::vector<std::string> stuffed_run;
  // A screenshot the server asked for (qw_client.hpp `ClientUpload`): the window's frame grabbed
  // in memory, scaled and packed (`snap_grab(width, height, quality)`, false when it can't) and
  // handed back later (`snap_take`). Unset: the server is told no, as with scr_allowsnap 0.
  std::function<bool(int width, int height, int quality)> snap_grab;
  std::function<bool(std::vector<uint8_t>& out)> snap_take;
  bool snap_grabbing = false;
  bool allow_snap = true;  // scr_allowsnap
  gn::Address server;
  bool active = false;
  bool left = false;  // a server was left (dropped or let go) since the window last took this
  ClientStage said = ClientStage::challenge;  // the stage last reported
  bool nopred = false;                        // cl_nopred: draw the server's place
  bool predicted = false;                     // the last step drew a prediction
  bool predict_players = true;                // cl_predict_players: the other players run ahead
  bool nolerp = false;                        // cl_nolerp: entities at the newest frame's places
  bool solid_players = true;                  // cl_solid_players: others block the prediction
  bool voip = true;                           // cl_voip: ask for voice when joining
  bool kept = false;
  // cl_physfps, cl_physfps_spectator: the moves a second asked for (qw_client.hpp's
  // `client_move_rate`), taken by the client every step; the tick clock's fraction of a
  // microsecond.
  int physfps = 0, physfps_spectator = 77;
  int c2spps = 0, c2s_impulse_backup = 3;  // cl_c2spps, cl_c2sImpulseBackup (qw_client.hpp)
  bool gunkick = false;  // v_gunkick: the server's kicks drawn on the view (client/demo_eyes.hpp)
  int q2_maxfps = 62;    // Quake II's own cl_maxfps: its moves a second (q2_remote.hpp)
  // Quake II's cl_ignore_stufftext and its whitelist file (read at each join; "" none).
  ge::classic::StuffFilter stuff;
  std::filesystem::path stuff_file;
  int q2_shownet = 0;  // Quake II's cl_shownet: each message's size printed (q2_client.hpp)
  bool q2_showpackets = false;  // Quake II's cl_showpackets: each packet sent shown (`trace.sent`)
  double tick_frac_us = 0;
  RemoteSheet sheet;  // the server settings page's sheet (qw_remote_sheet.hpp)
  // Quake II's `net_clientport`: the port `own` is bound to (-1 any free one), opened again on
  // another the frame the setting moves; `port_note` says a port that would not open (the join
  // prints it), the socket then on any free port.
  int own_port = -1;
  bool own_v6 = false;  // the client's socket is IPv6's (an IPv6 server joined)
  std::string port_note;
  std::string localid;  // localid: the id a server browser's command packets carry (qw_client.hpp)
};

// Watches the broadcast at `address` ("host:port": a stream port, our TV node's or a relay's;
// qw_watch.hpp) as `name`: drawn as a joined game is, its frames as they come, no hands.
// False with the reason when the address does not read or cannot be reached.
bool remote_watch(Remote& r, const std::string& address, const std::string& name,
                  const std::string& password, std::string& error);

// Joins `address` ("host", "host:port"; the QuakeWorld server port when none, Quake II's when
// `q2_game` says the window plays Quake II: q2_remote.hpp) over a UDP socket of its own. Port 26000
// (`kPortNetQuake`) in Quake is a server of Quake's own protocol 15 (q1_remote.hpp).
// False with the reason when the address or the socket does not open.
bool remote_connect(Remote& r, const std::string& address, const ClientConfig& cfg,
                    std::string& error);
// Joins over a transport already open (the simulated network's, in tests and labs); as Quake II
// when `r.q2_game` says so.
void remote_start(Remote& r, gn::Transport& t, const gn::Address& server, const ClientConfig& cfg);
// Logs in by the account file at `path` on every server whose master it names (the window's: the
// game CLI's `<state>/account.txt`). `unix_us` is the wall clock (the ask runs on it, apart from
// the game's tick clock); `link`, when given, is the socket the master is asked over (the simulated
// network's), else one is opened for each ask.
// `ranked <mode> [duo]`: the matchmaker asked for a ranked game of `game` (its ladder's: the
// game folder) with the account file's ticket. False with `said` (why, in a player's words).
bool remote_ranked(Remote& r, const std::string& game, const std::string& mode,
                   const std::string& duo, std::string& said);
// `ranked leave`: out of the queue. False when it was not looking.
bool remote_ranked_leave(Remote& r);
// Every frame, connected or not: the queue's asks; its news said; the address to connect to once
// the game is found ("" otherwise).
std::string remote_queue_step(Remote& r, std::vector<std::string>& said);
// The window's frame of it: its news printed on the console, and `connect` run once found; then the
// match recorded on its own (qw_match_record.hpp) and the answers to an rcon sent while not joined.
void remote_queue_frame(Remote& r, ge::console::Console& con);
// What the server stuffed (`stuffed_run`) run in `con`: a Quake II server's as typed
// (cl_ignore_stufftext filtered it), a QuakeWorld server's as the server's (console.hpp: only the
// commands a game lets a server run, and only the server's own settings).
void remote_run_stuffed(Remote& r, ge::console::Console& con);

void remote_login_from(Remote& r, const std::string& path, int64_t (*unix_us)(),
                       gn::Transport* link = nullptr);
// The level's brush models the prediction moves through ([0] the world), the window's once it
// has loaded the level `remote_step` named (nullptr: nothing predicted); `brushes`, the level's
// brushes, are what a server of Quake II's prediction traces.
void remote_world(Remote& r, const std::vector<sh::BrushModel>* models,
                  const ge::brush::Map* brushes = nullptr);
// Leaves: the server told (a disconnect, three times), the socket closed, the state cleared.
void remote_disconnect(Remote& r);
// One window frame of `wall_s` s: the ticks due run (each sending `r.hands` with `r.view`), every
// packet read, the drawn state brought up to the newest. `said` takes the stages reached; what
// every frame read carried (prints, sounds started and stopped, voice, a centre message) is kept
// together in `demo.state` for the window to hear and show, as a played demo's one frame's is. An
// impulse in `r.hands` is cleared once sent.
RemoteStep remote_step(Remote& r, double wall_s, std::vector<std::string>& said);
// `rcon`: `text` ("rcon <password> <line>") out of band to the server joined, else to `address`
// (`rcon_address`; the game's server port when none is given), whose answers `remote_rcon_step`
// says. False with `error` (a player's words) when not joined and the address is empty or wrong.
bool remote_rcon(Remote& r, const std::string& address, const std::string& text,
                 std::string& error);
// Every frame at `now_s`, joined or not: the answers to an rcon sent while not joined, said.
void remote_rcon_step(Remote& r, double now_s, std::vector<std::string>& said);
// The stage, as the console says it ("connecting to host:27500", "playing on host:27500").
std::string remote_status(const Remote& r);
// The words before the server's name for a stage ("playing on ").
const char* remote_stage_words(ClientStage s);
// The console's foot while a game is joined but not yet played (console/view.hpp's set_foot): the
// file coming and its share ("maps/dm2.bsp", `progress` 0.4: the console draws the charset's bar
// and "40%" after the name), or "Loading dm2..." while signing on; "" (and `progress` -1) once
// playing, watching or not joined.
std::string remote_foot(const Remote& r, double& progress);
// The foot set on the console's view each frame, and the console held over the whole screen while
// a server is joined and not yet played, as the classic client showed a connection; let go once in.
void remote_console_foot(Remote& r, ge::console::ConsoleView& view);
// The joined server's packets as they came (the net graph and Quake II's lag graph), whichever
// protocol it speaks; null when nothing is joined or a broadcast is watched.
const ge::hud::NetHistory* remote_net(const Remote& r);
// Whether the classic net icon shows: a Quake server silent 0.3 s or more, or a QuakeWorld one
// with 63 or more of the client's packets unacknowledged; never while watching or signing on.
bool remote_net_trouble(const Remote& r);
// The moves sent to a joined Quake II server it has not answered yet (0: none, or not Quake II).
int remote_unanswered(const Remote& r);
// A line for the server joined (`say hello`, `vote map dm2`), whichever protocol it speaks.
void remote_command(Remote& r, const std::string& line);
// A line the console does not know, sent to a server of Quake II joined (its game's own words:
// `use Railgun`, `inven`, `drop`); false when none is joined.
bool remote_rules_word(Remote& r, const std::string& line);

// The window's hands this frame: the mouse and the turn keys turn `r.view`, the moves make the
// command the next tick sends (an impulse not yet sent is kept until one is).
void remote_hands(Remote& r, const client::Moves& m, const ge::input::InputState& in,
                  const client::ControlSettings& c, double dt);
// The microphone's samples since the last call (48 kHz mono), sent at the next ticks while `+voip`
// is held. Dropped when not joined, when the server has no voice, or past `kMicMostSamples` unsent.
void remote_mic(Remote& r, std::span<const float> samples);
// True while the player is joined with voice and holds a voice key (`+voice`, `+voice_team`,
// `+voice_near` of `Remote::talk`, or `+voip`): the window opens the microphone the first time,
// and shows that he is talking. The key held names the voice's target (`voicetarg`), told to the
// server as it changes.
bool remote_talking(const Remote& r);
// A lobby word (`ready`, `team switch`, ...) to the server joined, whose lobby it is; false when
// none is joined (or it is a broadcast watched), so the window's own lobby takes it.
bool remote_lobby(Remote& r, const std::string& line);

// `connect <host>[:port]`, `disconnect`, `reconnect` (the last server again), `netstatus`, the
// `+voip` button (talk while held) and the `cl_nopred`, `cl_predict_players`, `cl_nolerp` and
// `cl_voip` cvars; the player's `name`, `topcolor`, `bottomcolor` and `cl_speedjump` cvars are read
// when a connect is made.
void add_remote_commands(ge::console::Console& con, Remote& r);

}  // namespace shooters_edge::net
