// THE WINDOW JOINED TO A SERVER: the `shooters_edge` window playing on a server somewhere else over
// the classic protocol (`connect <host>[:port]` in the console), as a client of it rather than the
// player of its own listen server.
//
// HOW IT IS MADE: the protocol is the classic client's (classic_client.hpp: the challenge, the
// connect, the signon, the moves, the downloads), driven by the window's hands in place of a
// script; and what the server sends is read as a client recording of it (client/demo.hpp), so the
// window draws a game it plays exactly as it draws one it watches: the map, the entities, the
// other players, the status, the sounds and the printed lines all come off the `DemoState` the
// recording's reader keeps. The recording is the client's own (classic_record.hpp's
// `ClientRecorder` on its `record`), fed to the reader as it grows and dropped as it is read.
//
// THE CLOCK: the client sends one move a tick, 77 a second (13 ms, the server's frame), so the
// window's wall time is turned into the ticks that are due (at most `kMostTicks` a call, the rest
// dropped, so a stall never floods the server). The network's time is that tick clock: nothing
// here reads a wall clock, so a session over the simulated network replays.
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
// alone (his own `*speedjump`, net/classic_admin.hpp), else at the server's `pm_speedjump` where
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
// player are kept in the live recording (classic_record.hpp's `live`) and heard as a demo's are,
// his own left out. While `+voip` is held he speaks the microphone the window hands to
// `remote_mic`.
//
// ge:summary The window joined to a server: the classic client driven by the hands, drawn as a demo
// ge:keywords remote-client connect disconnect join-server classic-client live-demo network-play
// ge:keywords remote_connect remote_step remote_disconnect setangle tick-clock reconnect
// ge:keywords remote_hands add_remote_commands netstatus prediction predict cl_nopred
// ge:keywords remote_world smoothing replayed predict_other other-players cl_predict_players
// ge:keywords interpolation lerp cl_nolerp entity-lerp draw-clock round-trip
// ge:keywords cl_speedjump pm_speedjump easy-jump assisted_command kstockshare
// ge:keywords voice voice-chat cl_voip push-to-talk microphone remote_mic autologin accountkey
// ge:keywords client-presence invites
// ge:skill net shooter
// ge:test tests/shooters_edge_remote_test.cpp
#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "account/client_login.hpp"
#include "account/client_presence.hpp"
#include "account/client_queue.hpp"
#include "console/console.hpp"
#include "console/talk.hpp"
#include "input/state.hpp"
#include "net/transport.hpp"
#include "shooter/aim_turn.hpp"
#include "shooter/predict.hpp"
#include "shooter/predict_original.hpp"
#include "shooter/speedjump_assist.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_record.hpp"
#include "shooters_edge/net/classic_watch.hpp"

namespace shooters_edge::net {

inline constexpr int kTickUs = 13000;    // a move's length: the server's frame
inline constexpr int kMostTicks = 8;     // ticks a call may run; a longer stall drops the rest
inline constexpr float kLerpMost = 200;  // units: an entity moved further between two frames snaps
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

struct Remote {
  // In the order that packs it (the lint holds the padding).
  gn::Transport* transport = nullptr;             // what it talks over (`own`, or remote_start's)
  const ge::console::Button* voip_key = nullptr;  // `+voip` (add_remote_commands)
  const ge::console::Talk* talk = nullptr;        // the window's +voice keys (console/talk.hpp)
  std::unique_ptr<StreamWatch> watch;             // watching a broadcast (`watch`), not playing
  std::unique_ptr<RemoteLogin> login;             // logging in by the account file, when named
  int64_t now_us = 0;                             // the tick clock
  double owed_s = 0;                              // wall time not yet a tick
  int64_t setangles = 0;                          // the server's turns taken
  double frame_old_s = -1, frame_new_s = -1;  // lerp: the last two frames' times (network clock)
  const std::vector<sh::BrushModel>* world = nullptr;  // prediction: the level's models ([0] world)
  std::string server_name;                             // as asked ("host:port")
  std::string why;                                     // why it dropped
  // Lerp: the last two frames' entities as read (by number), before anything drawn moved them.
  std::vector<client::DemoEntity> ents_old, ents_new;
  gn::Transport own;  // remote_connect's UDP socket
  ClientRecorder recording;
  client::Demo demo;  // the recording read back: what the window draws
  ClassicClientEnd end;
  int levels = 0;             // level starts drawn
  int replayed = 0;           // prediction: the moves it ran past the acknowledged one
  int others = 0;             // other players predicted the last step
  int others_msec = 0;        // the most milliseconds one of them was run forward the last step
  int lerped = 0;             // entities drawn between two frames the last step
  sh::Vec3f view;             // the player's angles, degrees (sent with every move)
  sh::AimTurn aim_turn;       // the aim help's turn of the view (shooter/aim_turn.hpp)
  sh::Vec3f prediction;       // the player's predicted origin, the correction not added
  sh::MoveState ahead;        // the player's whole predicted state (the easy jump's help reads it)
  sh::SpeedJumpAid jump_aid;  // the easy jump's memory (shooter/speedjump_assist.hpp)
  float speedjump_view = 0.5f;  // cl_speedjump_view: the share of its swing drawn
  sh::Smoothing smooth;         // the prediction's correction
  // Prediction on the original move (the server's `sv_playermove original`): the stand-in
  // server, built for the level `original_level` counts and the models `original_for` points at.
  sh::OriginalPrediction original;
  const std::vector<sh::BrushModel>* original_for = nullptr;
  int original_level = -1;
  sh::MoveCmd hands;  // the window's command, sent at the next tick (an impulse held to it)
  // The game's files as the server's game folder holds them (ClientConfig::read_file: the map's
  // checksum and the model checks a server asks for); unset, they are sent as 0 and unsent.
  std::function<std::vector<uint8_t>(const std::string& gamedir, const std::string& path)>
      read_file;
  // A skin or player figure fetched while joining (ClientConfig::keep_file): the window keeps it in
  // the game's folder. `kept`: one was, so once the game is begun the level is read again
  // (RemoteStep::new_level) and the players are drawn in what was fetched.
  std::function<void(const std::string& gamedir, const std::string& path,
                     const std::vector<uint8_t>& bytes)>
      keep_file;
  bool kept = false;
  gn::Address server;
  bool active = false;
  ClientStage said = ClientStage::challenge;  // the stage last reported
  bool nopred = false;                        // cl_nopred: draw the server's place
  bool predicted = false;                     // the last step drew a prediction
  bool predict_players = true;                // cl_predict_players: the other players run ahead
  bool nolerp = false;                        // cl_nolerp: entities at the newest frame's places
  bool voip = true;                           // cl_voip: ask for voice when joining
};

// Watches the broadcast at `address` ("host:port": a stream port, our TV node's or a relay's;
// classic_watch.hpp) as `name`: drawn as a joined game is, its frames as they come, no hands.
// False with the reason when the address does not read or cannot be reached.
bool remote_watch(Remote& r, const std::string& address, const std::string& name,
                  const std::string& password, std::string& error);

// Joins `address` ("host", "host:port"; the classic server port when none) over a UDP socket of
// its own. False with the reason when the address or the socket does not open.
bool remote_connect(Remote& r, const std::string& address, const ClientConfig& cfg,
                    std::string& error);
// Joins over a transport already open (the simulated network's, in tests and labs).
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
// The window's frame of it: its news printed on the console, and `connect` run once found.
void remote_queue_frame(Remote& r, ge::console::Console& con);

void remote_login_from(Remote& r, const std::string& path, int64_t (*unix_us)(),
                       gn::Transport* link = nullptr);
// The level's brush models the prediction moves through ([0] the world), the window's once it
// has loaded the level `remote_step` named (nullptr: nothing predicted).
void remote_world(Remote& r, const std::vector<sh::BrushModel>* models);
// Leaves: the server told (a disconnect, three times), the socket closed, the state cleared.
void remote_disconnect(Remote& r);
// One window frame of `wall_s` s: the ticks due run (each sending `r.hands` with `r.view`), every
// packet read, the drawn state brought up to the newest. `said` takes the stages reached; what
// every frame read carried (prints, sounds started and stopped, voice, a centre message) is kept
// together in `demo.state` for the window to hear and show, as a played demo's one frame's is. An
// impulse in `r.hands` is cleared once sent.
RemoteStep remote_step(Remote& r, double wall_s, std::vector<std::string>& said);
// The stage, as the console says it ("connecting to host:27500", "playing on host:27500").
std::string remote_status(const Remote& r);

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

// `connect <host>[:port]`, `disconnect`, `reconnect` (the last server again), `netstatus`, the
// `+voip` button (talk while held) and the `cl_nopred`, `cl_predict_players`, `cl_nolerp` and
// `cl_voip` cvars; the player's `name`, `topcolor`, `bottomcolor` and `cl_speedjump` cvars are read
// when a connect is made.
void add_remote_commands(ge::console::Console& con, Remote& r);

}  // namespace shooters_edge::net
