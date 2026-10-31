// THE MULTIPLAYER MOD SERVER: a loaded progs running a match on the shooter loop with every
// builtin, hook and extension field the modern multiplayer server gives a mod (catalogue.hpp is the
// table). It owns the loop's server (plugins/shooter), the QC adapter (sim/qc_program.hpp), the
// settings (engine/console cvars), the precache lists, the clients' info and spawn parms, the
// message sink the network builtins write into (messages.hpp), the static entities and light
// styles, and the pause state. Everything it does is a function of its map, its settings and what
// it is told in order (packets, client commands, console lines): no wall clock, no platform random.
// The date a mod asks for (calltimeofday, the long frag log) comes from `clock`, a game-supplied
// source that a replay records.
//
// A MAP (`load_map`): the server is made over the map's brush models; the model list starts "",
// the map's own name, then "*1", "*2", ... for its inline brush models, so a brush entity's loop
// model index is its list index; the entity text is read (plugins/progs/entity_text.hpp): the
// first block fills the world, each later one is a newly allocated entity; in deathmatch an
// entity with spawnflag 2048 is freed, otherwise skill 0 frees 256, skill 1 512, skill 2 and up
// 1024 (each counted as inhibited); an entity with no classname, or whose classname names no
// function, is freed with a message; else the function runs with `self` the entity. Precaching
// and the signon are allowed while it loads; then the world is locked (ADDRESS of a world field
// faults) and the match runs.
//
// THE HOOKS (when each is called, with self, time and arguments: commands.hpp for those a client
// or the console starts): StartFrame, PlayerPreThink, PlayerPostThink and SpectatorThink by the
// loop; ClientConnect / SpectatorConnect then PutClientInServer by `connect`; SetNewParms before
// them, its parm1..16 saved as the client's spawn parms (setspawnparms reads them back);
// ClientDisconnect / SpectatorDisconnect by `disconnect`; SetChangeParms by `save_change_parms`;
// GE_PausedTic each frame while paused (p0 the seconds paused so far). Every named hook call is
// counted and kept in order in `hook_log` (the lab's timeline).
//
// ge:summary The multiplayer mod server: a progs running a match with the full builtin catalogue
// ge:keywords qc progs mod server builtins hooks extensions cvars precache signon clients
// ge:keywords spawn-parms setnewparms map entity-text inhibit deathmatch skill spawnflags
// ge:keywords classname worldspawn load_map connect disconnect qc_frame hook_log clock-source
// ge:keywords timeofday static-entities lightstyle pause
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_server_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "console/cvar.hpp"
#include "progs/entity_text.hpp"
#include "progs/extensions.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/sim/qc_program.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;
namespace pg = ge::progs;

constexpr int kMaxModels = 256;    // the model list (the precache)
constexpr int kMaxSounds = 256;    // the sound list
constexpr int kMaxVwep = 32;       // the visible-weapon list
constexpr int kMaxStatics = 2048;  // static entities
constexpr int kLightStyles = 64;
constexpr int kStuffBytes = 256;    // a client's pending stuffcmd text
constexpr int kMaxHookLog = 20000;  // hook calls kept in order (more are counted only)

// A date and time of day, as the clock source gives it.
struct DateTime {
  int year = 2026, month = 1, day = 1;  // month 1..12
  int hour = 0, minute = 0, second = 0;
};
// The game's clock source: the date at a server time. The default is a fixed date plus the
// server's time, so a run is reproducible; a live server passes the local time and records it.
using ClockSource = DateTime (*)(void* ctx, double server_time);
DateTime fixed_clock(void* ctx, double server_time);
// "Mon Jan 02, 15:04:05 2006": the weekday, the month, the day of the month, the time, the year.
std::string date_text(const DateTime& d);

// The engine's hooks a progs may have, by name (-1 when absent; the loop's own are in
// `QcProgram::hooks`).
struct NamedHooks {
  int32_t user_cmd = 0, client_command = 0, chat_message = 0, user_info_changed = 0;
  int32_t localinfo_changed = 0, console_cmd = 0, paused_tic = 0, should_pause = 0;
  int32_t timeofday = 0;
};

enum class HookId : uint8_t {
  start_frame,
  player_pre_think,
  player_post_think,
  spectator_think,
  client_connect,
  spectator_connect,
  put_client_in_server,
  client_disconnect,
  spectator_disconnect,
  set_new_parms,
  set_change_parms,
  client_kill,
  user_cmd,
  client_command,
  chat_message,
  user_info_changed,
  localinfo_changed,
  console_cmd,
  should_pause,
  paused_tic,
  timeofday,
  spawn_function,  // a map entity's classname function
  count,
};
const char* hook_name(HookId h);

struct HookCall {
  double time = 0;  // the server's time
  HookId hook = HookId::start_frame;
  int self = 0;
};

// The change a console line asks for: `map` a new game, `changelevel` the next map with the
// players' parms carried (SetChangeParms), `restart` the same map with the parms it was entered
// with.
enum class MapChange : uint8_t { new_game, change_level, restart };

// What the server keeps of one client (the player is entity slot + 1).
// The blow a player took this frame (client_data.hpp): its blood and armour, each 0..255, and
// where it came from.
struct ClientBlow {
  bool hit = false;
  int armor = 0, blood = 0;
  sh::Vec3f from;
};

struct ClientInfo {
  bool active = false;
  bool spectator = false;
  bool bot = false;
  int userid = 0;
  std::vector<std::pair<std::string, std::string>> userinfo;  // in the order set
  float spawn_parms[16] = {};
  std::string stuff;  // stuffcmd text waiting for its newline
  bool drop = false;  // a stuffed "disconnect": dropped after the current call
  std::string address = "loopback";
  int ping_ms = 0;
  ClientBlow blow;  // this frame's (a single-player program's)
};

struct StaticEntity {
  int modelindex = 0;
  float frame = 0, colormap = 0, skin = 0;
  sh::Vec3f origin, angles;
  pg::EntityExtras extras;
};

// A map: its brush models (`[0]` the world) and its entity text.
struct MapSpec {
  std::string name = "arena";
  std::vector<sh::BrushModel> models;
  std::string entities;
};

struct QcServerOptions {
  sh::ServerSettings loop;  // the loop's settings (first_dynamic, max_edicts set by the server)
  uint32_t seed = 1;        // the generator random, randomvec and movetogoal draw from
  ClockSource clock = &fixed_clock;
  void* clock_ctx = nullptr;
};

struct QcServer {
  QcServerOptions opt;
  const pg::Progs* progs = nullptr;
  sh::Server sv;
  sim::QcProgram q;
  ge::console::Cvars cvars{true};
  pg::ExtensionRegistry extensions;
  NamedHooks named;

  std::string map_name;
  std::vector<std::string> models;       // the model list; [0] ""
  std::vector<std::string> sounds;       // the sound list; [0] ""
  std::vector<std::string> vwep;         // the visible-weapon list
  std::vector<pg::EntityExtras> extras;  // by entity: alpha and colormod
  std::vector<StaticEntity> statics;
  std::string lightstyles[kLightStyles];
  ClientInfo clients[sh::kMaxClients];
  std::vector<std::pair<std::string, std::string>> serverinfo, localinfo;

  MessageSink out;                        // what the network builtins wrote (messages.hpp)
  std::vector<std::string> console;       // the server console's lines, in order
  std::vector<std::string> frag_log;      // logfrag's lines
  std::string command_buffer;             // localcmd's text, run at the next frame
  std::vector<std::string> map_messages;  // the entity text's messages ("x is not a field")

  std::string stop_reason;  // why the map stopped (error / objerror)
  std::string pending_map;  // a `map`, `changelevel` or `restart` console line: the change made
  MapChange pending_change = MapChange::new_game;  // which of the three `pending_map` is
  // The flags a game carries from map to map (the program's `serverflags`, written at each map's
  // start; a campaign's runes). The host keeps them across a level change.
  float serverflags = 0;

  // Statement tracing (traceon / traceoff) into the console, at most `trace_lines` lines.
  pg::TraceHooks trace_hooks;

  // The map's visibility and hearing sets: whether `to` is in the set of `from` (`see` false: the
  // hearing set). Null until the map carries them (M18b): every point is in every set.
  bool (*vis)(void* ctx, sh::Vec3f from, sh::Vec3f to, bool see) = nullptr;
  void* vis_ctx = nullptr;

  // Lag compensation: called before (`begin`) and after a trace a player shoots (traceline or
  // tracebox whose pass entity, or its owner, is a client) so the protocol's server can move the
  // other players to where the shooter saw them and back. Null: traces see the players as they
  // are (games/shooters_edge/net/classic_antilag.hpp).
  void (*lag_rewind)(void* ctx, int shooter, bool begin) = nullptr;
  void* lag_ctx = nullptr;

  std::vector<HookCall> hook_log;
  int64_t hook_counts[static_cast<int>(HookId::count)] = {};

  double paused_seconds = 0;
  double check_time = -1;  // checkclient: when the candidate last moved on
  int64_t trace_lines = 0;
  int check_client = 0;  // checkclient: the candidate
  int inhibited = 0;
  int next_userid = 1;

  bool loading = false;      // spawn functions running: precache and the signon allowed
  bool map_stopped = false;  // error / objerror: the map is over (`stop_reason` says why)
  bool changelevel_queued = false;
  bool paused = false;
  bool tracing = false;
};

// A server for `progs` (which must outlive it). False with `error` when the progs cannot drive
// the loop. The settings registered, the catalogue installed, the hooks found.
bool create_qc_server(QcServer& s, const pg::Progs& progs, const QcServerOptions& opt,
                      std::string& error);
// A new server on the heap (it is not movable: cvars and the loop point into it).
std::unique_ptr<QcServer> make_qc_server(const pg::Progs& progs, const QcServerOptions& opt,
                                         std::string& error);

// Loads a map: the server made over its models, the precache seeded, the entities spawned.
// False with `error` when the entity text does not parse or a key stops the map.
bool load_map(QcServer& s, const MapSpec& map, std::string& error);

// A client takes slot `slot`: its user info (`name`), its entity cleared in the VM with that name
// as its netname, SetNewParms (or the carried `parms`), then ClientConnect and PutClientInServer (a
// spectator's SpectatorConnect). `parms` (16) are the spawn parms the client carries from the last
// map: written where the program reads them in place of SetNewParms.
void connect(QcServer& s, int slot, const std::string& name, bool spectator, bool bot,
             const float* parms = nullptr);
void disconnect(QcServer& s, int slot);
// SetChangeParms for every client (before a map change), saving their parms.
void save_change_parms(QcServer& s);

// One server frame: while paused, GE_PausedTic and nothing moves; otherwise the loop's frame (the
// packets in order, then physics). Then the extension fields' per-frame work (visclients), the
// clients a stuffed "disconnect" dropped, and the command buffer.
void qc_frame(QcServer& s, const sh::Packet* packets, int count);

// The console's line (prints, a mod's error messages, coredump, traces).
void console_print(QcServer& s, const std::string& line);
// A player entity's netname (its user info's name): at connect, and when the name changes.
void set_netname(QcServer& s, int e, const std::string& name);

// The server-side half of a builtin that reaches the world: the server a builtin runs under.
QcServer& server_of(pg::Vm& vm);
// The number of a client entity (1..32 in use), or -1.
int client_slot_of(const QcServer& s, int entity);
// Records a named hook's call (time, self) for the timeline and the counts.
void note_hook(QcServer& s, HookId h, int self);
// A user info or server info value; "" when unset.
std::string info_value(const std::vector<std::pair<std::string, std::string>>& info,
                       const std::string& key);
// Sets one; true when the value changed (the old one in `old`).
bool set_info_value(std::vector<std::pair<std::string, std::string>>& info, const std::string& key,
                    const std::string& value, std::string& old);
// The cvars' values the loop reads (sv_gravity, sv_maxspeed, ...) copied into its settings.
void apply_settings(QcServer& s);

}  // namespace shooters_edge::server
