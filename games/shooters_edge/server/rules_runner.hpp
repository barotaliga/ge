// THE RULES RUNNER: what a host holds to run a game's rules, whatever program form they come in
// (docs/shooters-edge.md § The rules runner). A runner is a table of calls (`RunnerCalls`) and the
// runner's own state behind it; the host, the campaign and the dedicated server call through the
// table and read `RulesCommon`, the part every runner keeps alike: the loop the rules step, the
// lists, the clients' info, the message sink the wire sends from, the console, the map change
// asked for and the rules' world (rules_world.hpp). The QC server (qc_server.hpp) is the first
// runner (`qc_runner`); a later program form is a new table over the same common part, so nothing
// that holds a runner names QC.
//
// A call through the table is one indirect call per entry point (a frame, a connect, a console
// line), never per entity: the frame's path inside the runner is unchanged.
//
// C++ MODS ON TOP (`RulesMods`, docs/mods.md § On the server): the host puts loaded mod tables in
// `RulesCommon::mods` before a map starts; `start` then lays each mod's seam (sim/mod_host.hpp)
// over the program the runner made the loop with, the first named lowest, so every hook runs the
// rules' own function first and each mod's after it in the order named, and runs each mod's
// match_start once the map's entities are spawned. A mod's state lives for one map, as the
// rules' own globals do. Its log lines go to the server console after each call. A runner whose
// rules keep their own entities off the loop's program seam (the sequel's: `program_seam` false in
// its calls) refuses a mod at `start` until its own hooks land (docs/goals/M52g.md).
//
// ge:summary The rules runner: the table of calls a host runs a game's rules through, and the
// ge:summary common part every runner keeps (the loop, the lists, the sink, the world)
// ge:keywords rules-runner runner runner-calls rules-common qc-runner host start connect drop
// ge:keywords frame console-line client-command user-info pause carry map-change program-form
// ge:keywords rules-mods mod-package mods_start rules_hash program-seam
// ge:skill progs shooter
// ge:test tests/shooters_edge_rules_runner_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "console/cvar.hpp"
#include "progs/entity_text.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/rules_world.hpp"
#include "shooters_edge/sim/mod_host.hpp"

namespace ge::brush {
struct Map;
}

namespace shooters_edge::server {

namespace sh = ge::shooter;
namespace pg = ge::progs;

constexpr int kLightStyles = 64;

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
  double entered_at = 0;  // the server's time it connected, s (the scoreboard's minutes)
  ClientBlow blow;        // this frame's (a single-player program's)
};

struct StaticEntity {
  int modelindex = 0;
  float frame = 0, colormap = 0, skin = 0;
  sh::Vec3f origin, angles;
  pg::EntityExtras extras;
};

// A map: its brush models (`[0]` the world) and its entity text, or (the sequel's runner) the
// version 38 map read whole, whose brushes it traces and whose entities it spawns.
struct MapSpec {
  std::string name = "arena";
  std::vector<sh::BrushModel> models;
  std::string entities;
  std::shared_ptr<const ge::brush::Map> brushes;
};

// The C++ mods on top of the rules: the tables the host loaded (it keeps each library open while
// the runner lives), lowest first, and this map's host of each (made at `start`).
struct RulesMods {
  std::vector<const ge_mod_v1*> tables;
  std::vector<std::string> names;  // each table's name, for the console's lines
  uint32_t seed = 1;               // mod k's stream is seeded with seed + k
  std::vector<std::unique_ptr<sim::ModHost>> hosts;
};

// What every runner keeps alike: what the host, the wire and the bots read, and the hooks the
// host sets.
struct RulesCommon {
  sh::Server sv;
  ge::console::Cvars cvars{true};  // the settings the rules register and the console sets

  std::string map_name;
  std::vector<std::string> models;       // the model list; [0] ""
  std::vector<std::string> sounds;       // the sound list; [0] ""
  std::vector<std::string> vwep;         // the visible-weapon list
  std::vector<pg::EntityExtras> extras;  // by entity: alpha and colormod
  std::vector<StaticEntity> statics;
  std::string lightstyles[kLightStyles];
  ClientInfo clients[sh::kMaxClients];
  std::vector<std::pair<std::string, std::string>> serverinfo, localinfo;

  MessageSink out;                   // what the network builtins wrote (messages.hpp)
  std::vector<std::string> console;  // the server console's lines, in order

  std::string stop_reason;  // why the map stopped (error / objerror)
  std::string pending_map;  // a `map`, `changelevel` or `restart` console line: the change made
  MapChange pending_change = MapChange::new_game;  // which of the three `pending_map` is

  // The flags a game carries from map to map (the program's `serverflags`, written at each map's
  // start; a campaign's runes). The host keeps them across a level change.
  float serverflags = 0;

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

  // What the rules show everyone else (rules_world.hpp): filled at the map's start and at the end
  // of each call that changes it.
  RulesWorld world;

  bool map_stopped = false;  // error / objerror: the map is over (`stop_reason` says why)
  bool paused = false;

  RulesMods mods;  // C++ mods on top (none: the rules alone, every hash as before)
};

// The settings every runner of a classic mod registers (the match's rules, the move's and the
// loop's, the server's own), their defaults `loop`'s; and their values written into `set`: every
// one (`live_only` false, a map starting) or the move's alone (between frames).
void add_mod_settings(RulesCommon& s, const sh::ServerSettings& loop);
void settings_from_cvars(const RulesCommon& s, sh::ServerSettings& set, bool live_only);

enum class CommandResult : uint8_t { mod, server, bad };

// The calls a runner answers. `rules` is the runner's own state (`RulesRunner::rules`).
struct RunnerCalls {
  const char* form;  // the program form it runs ("qc")
  // A map loaded, its spawn functions run; false with `error` when the map cannot start.
  bool (*start)(void* rules, const MapSpec& map, std::string& error);
  // A player or spectator takes a slot, carrying `parms` (16) from the last map or null.
  void (*connect)(void* rules, int slot, const std::string& name, bool spectator, bool bot,
                  const float* parms);
  void (*drop)(void* rules, int slot);
  // One server frame over the frame's packets, in slot order.
  void (*frame)(void* rules, const sh::Packet* packets, int count);
  // A client's command line; a console line (false when nothing took it).
  CommandResult (*client_command)(void* rules, int slot, const std::string& line);
  bool (*console)(void* rules, const std::string& line);
  void (*user_info)(void* rules, int slot, const std::string& key, const std::string& value);
  bool (*pause)(void* rules, int slot, bool pause);  // true when the pause changed
  // The players' state kept across a level change (before the next `start`).
  void (*carry)(void* rules);
  void (*destroy)(void* rules);  // null: the runner does not own its state
  // The rules run on the loop's program seam (sv.program), so C++ mods can stack on them.
  bool program_seam = false;
};

// After a runner's own `start`: each mod's seam laid over the loop's program and its
// match_start run. False with `error` when the runner takes no mods or a mod's hooks are refused.
bool start_mods(RulesCommon& s, const RunnerCalls& calls, std::string& error);
// The mods' new log lines put on the server console ("mod <name>: <line>").
void log_mods(RulesCommon& s);
// The loop's digest (`server_hash`), folded with each mod's run when mods play.
uint64_t rules_hash(const RulesCommon& s);

// A runner the host owns: the table, its state and the common part (inside the state). Moved,
// never copied; destroying it destroys the state.
struct RulesRunner {
  const RunnerCalls* calls = nullptr;
  void* rules = nullptr;
  RulesCommon* common = nullptr;

  RulesRunner() = default;
  RulesRunner(const RunnerCalls* c, void* r, RulesCommon* m) : calls(c), rules(r), common(m) {}
  RulesRunner(RulesRunner&& o) noexcept { *this = std::move(o); }
  RulesRunner& operator=(RulesRunner&& o) noexcept;
  RulesRunner(const RulesRunner&) = delete;
  RulesRunner& operator=(const RulesRunner&) = delete;
  ~RulesRunner();

  explicit operator bool() const { return rules != nullptr; }
  RulesCommon& operator*() const { return *common; }
  RulesCommon* operator->() const { return common; }

  bool start(const MapSpec& map, std::string& error) const {
    if (!calls->start(rules, map, error)) return false;
    return common->mods.tables.empty() || start_mods(*common, *calls, error);
  }
  void connect(int slot, const std::string& name, bool spectator, bool bot,
               const float* parms = nullptr) const {
    calls->connect(rules, slot, name, spectator, bot, parms);
  }
  void drop(int slot) const { calls->drop(rules, slot); }
  void frame(const sh::Packet* packets, int count) const {
    calls->frame(rules, packets, count);
    if (!common->mods.hosts.empty()) log_mods(*common);
  }
  CommandResult client_command(int slot, const std::string& line) const {
    return calls->client_command(rules, slot, line);
  }
  bool console(const std::string& line) const { return calls->console(rules, line); }
  void user_info(int slot, const std::string& key, const std::string& value) const {
    calls->user_info(rules, slot, key, value);
  }
  bool pause(int slot, bool on) const { return calls->pause(rules, slot, on); }
  void carry() const { calls->carry(rules); }
};

struct QcServer;

// The QC server as a runner, owning it.
RulesRunner qc_runner(std::unique_ptr<QcServer> s);
// The QC server as a runner over a server the caller owns and keeps alive (a test's, a lab's).
RulesRunner qc_runner_over(QcServer& s);
// The QC server behind a runner, or null when the runner runs another form (the QC-only tools:
// the hook log, the cvars, the profile).
QcServer* qc_of(const RulesRunner& r);

}  // namespace shooters_edge::server
