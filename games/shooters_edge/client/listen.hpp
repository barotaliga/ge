// THE LISTEN SERVER: Shooter's Edge as one player plays it on his own machine. The mod server
// (server/qc_server.hpp) runs a progs (the baseline deathmatch mod, compiled at start from the
// sources built into the game: `baseline_progs`) on a map, and the local player is client slot 0,
// his commands handed to the server's frame directly as packets that are never lost (no network,
// no prediction: what he sees is the server's own state after each frame).
//
// A MAP (`LoadedMap`): a map file from the search path (`maps/<name>.bsp`, read by
// server/bsp_map.hpp: its hulls, its entity text, its visibility and hearing sets) or the
// built-in test arena (`arena`, with the baseline mod's entities). The map's bytes are kept for
// the world draw.
//
// THE CLOCK: the server steps in frames of `sv_mintic` (13 ms), each running the player's command
// of that length. `listen_advance` turns the window's wall time into the frames that are due
// (at most `kMostFrames` a call, the rest dropped, so a stall never runs the world on fast), so a
// session of scripted commands steps the same server on any machine. An impulse (a weapon key)
// waits until a frame takes it.
//
// A CAMPAIGN (`listen_campaign`): a single-player program (its layout's form) played on the
// campaign host (server/campaign.hpp) in place of one server: its maps in a chain, each read from
// `ListenOptions::files` by name when the program changes level (`map_changed` says so, for the
// window to draw the new map), the frames on the host's 1/72 s clock. The program's client
// messages (the intermission, the finale, the statistics: campaign_view.hpp) are read off the
// reliable streams after every frame into `campaign`. `map` in a campaign is a new game.
//
// A SAVE (`listen_save`, `listen_load`): the campaign's whole running game (server/
// campaign_save.hpp) written to bytes and read back as the game playing, so it plays on as if
// never stopped.
//
// THE LOBBY (`ListenOptions::lobby`, a match only): every map opens in warmup with the player and
// the bots seated in `room` (lobby_room.hpp); the bots' packets go into each frame after his; the
// lobby's lines are printed like the server's; when the countdown runs out the map is restarted
// with every seat on it (`restarted`), and a map change goes back to warmup. `listen_lobby` runs
// the player's lobby words.
//
// THE VIEW: the player's angles are the client's (`Listen::view`, degrees: pitch down positive,
// yaw counter-clockwise from +x), sent with every command; when the server turns him (a spawn, a
// teleport: the mod's forced view) the client takes the server's angles (`forced`).
//
// ge:summary The listen server: the mod server and its local player in one process, no network
// ge:keywords listen-server local-player client slot usercmd packet listen_start listen_frame
// ge:keywords listen_advance listen_change loaded-map load_named_map baseline_progs map-change
// ge:keywords forced-view mintic player_state missiles rocket-jump lobby warmup listen_lobby
// ge:keywords match-restart restarted
// ge:keywords listen_campaign campaign single-player map_changed level-change
// ge:keywords listen_save listen_load savegame listen_save_text listen_load_text classic-save
// ge:keywords find_program qwprogs program-file mod-program
// ge:skill shooter progs
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "progs/file.hpp"
#include "shooter/pmove.hpp"
#include "shooters_edge/client/campaign_view.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/lobby_room.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

inline constexpr int kMostFrames = 8;  // server frames a wall frame may run

// The baseline mod compiled from the sources built into the game. False with every diagnostic.
bool baseline_progs(ge::progs::Progs& out, std::string& error);

// A map as the listen server plays it and the client draws it. Never copied once read: the spec's
// models point into `bsp`.
struct LoadedMap {
  std::string name;
  sv::MapSpec spec;
  sv::BspMap bsp;
  bool from_file = false;     // false: the built-in arena (nothing for the world draw)
  std::vector<uint8_t> file;  // the map file's bytes
};

// `maps/<name>.bsp` read from bytes.
std::unique_ptr<LoadedMap> map_from_bytes(std::span<const uint8_t> bytes, const std::string& name,
                                          std::string& error);
// `arena` (built in) or `maps/<name>.bsp` from the search path.
std::unique_ptr<LoadedMap> load_named_map(const SearchPath& files, const std::string& name,
                                          std::string& error);

struct ListenOptions {
  uint32_t seed = 1;
  std::vector<std::string> console = {"deathmatch 1"};  // server lines before each map loads
  std::string player = "player";
  const SearchPath* files = nullptr;  // where a campaign's next map is read (outlives the listen)
  bool registered = false;            // a campaign's content is the full game (gfx/pop.lmp)
  // A match opens every map in the lobby's warmup (lobby_room.hpp): ready, teams, bots, the
  // countdown, then the map restarted for the match. Off: the match starts with the map.
  bool lobby = false;
  sv::LobbySettings lobby_settings;
};

// A blow the player took (a single-player program's damage record, client_data.hpp): its armour
// and blood, 0..255 each, and where it came from, map units.
struct Blow {
  int armor = 0, blood = 0;
  sh::Vec3f from;
};

struct Listen {
  const ge::progs::Progs* progs = nullptr;
  ListenOptions opt;
  std::unique_ptr<LoadedMap> map;
  sv::QcServer* server = nullptr;       // the one playing: `match`'s, or the campaign's map's
  std::unique_ptr<sv::QcServer> match;  // a match's server (null in a campaign)
  std::unique_ptr<sv::Campaign> campaign_host;  // a campaign's host (null in a match)
  std::unique_ptr<LoadedMap> next;              // a map read ahead for the campaign's loader
  bool map_changed = false;  // the campaign moved to another map in the last frames (the window
                             // draws the new one and clears it)
  size_t maps_started = 0;   // of the campaign's history: the maps the listen has entered
  std::string failed;        // the campaign's next map did not load: why
  CampaignView campaign;     // the program's client messages (campaign_view.hpp)
  double entered_at = 0;     // the server's time the player entered this map, s
  uint32_t sequence = 0;
  sh::MoveCmd cmds[3];              // the three newest, oldest first
  double owed = 0;                  // wall seconds not yet run, s
  int64_t frames = 0;               // server frames run on this map
  sh::Vec3f view;                   // the player's view angles, degrees
  bool forced = false;              // the server turned the view in the last frame
  int impulse = 0;                  // waiting for a frame
  std::string pending_map;          // the mod or the console asked for this map
  std::vector<std::string> prints;  // what the server printed to the player, in order
  // Where the mod's explosions went off on this map (its temporary-entity message), map units,
  // in order.
  std::vector<sh::Vec3f> explosions;
  std::vector<Blow> blows;  // the blows the player took on this map, in order
  // A campaign's effects on this map (its temporary entities and particle bursts), in order; its
  // explosions are in `explosions` too.
  std::vector<Effect> effects;
  // The lobby (a match with `ListenOptions::lobby`; null otherwise) and whether the last frames
  // restarted the map for the match (the window clears what it keeps of the old server's
  // entities, then this).
  std::unique_ptr<Room> room;
  bool restarted = false;
};

// The server made for `progs` (which must outlive it) on `map`, the player connected.
bool listen_start(Listen& l, const ge::progs::Progs& progs, std::unique_ptr<LoadedMap> map,
                  const ListenOptions& opt, std::string& error);
// The same progs on another map (the spawn parms saved first), the player connected again; in a
// campaign, a new game on it.
bool listen_change(Listen& l, std::unique_ptr<LoadedMap> map, std::string& error);
// A new game of the single-player program `progs` (which must outlive the listen) on `map`, the
// match before it (if any) ended. `opt.console`'s lines are the campaign's settings.
bool listen_campaign(Listen& l, const ge::progs::Progs& progs, std::unique_ptr<LoadedMap> map,
                     const ListenOptions& opt, std::string& error);
// Why the game cannot be saved now ("" when it can): only a campaign saves, never at the level's
// end (the intermission or a finale the program's messages started), and never what the campaign
// host refuses (server/campaign_save.hpp).
std::string listen_save_problem(const Listen& l);
// The game saved into `out` (`saved_unix` the caller's clock). False with `error` when it cannot
// be.
bool listen_save(const Listen& l, std::vector<uint8_t>& out, std::string& error,
                 int64_t saved_unix = 0);
// The save `bytes` (named `name` in a refusal) of the single-player program `progs` (which must
// outlive the listen) loaded as the game now playing, the match or campaign before it ended; its
// map read from `opt.files` (or `map`, when it is that map already read), `map_changed` set. The
// player's view and command sequence go on from the save's. False with `error`, the game before it
// left playing, when the save does not load.
bool listen_load(Listen& l, const ge::progs::Progs& progs, const ListenOptions& opt,
                 std::span<const uint8_t> bytes, std::string_view name, std::string& error,
                 std::unique_ptr<LoadedMap> map = nullptr);
// The same as the classic text save (server/classic_save.hpp), and one loaded: less is kept, so
// the game plays on as the program's state says rather than bit for bit.
bool listen_save_text(const Listen& l, std::string& out, std::string& error);
bool listen_load_text(Listen& l, const ge::progs::Progs& progs, const ListenOptions& opt,
                      std::string_view text, std::string_view name, std::string& error,
                      std::unique_ptr<LoadedMap> map = nullptr);
// Whether a program is a single-player one (its layout's form).
bool single_player(const ge::progs::Progs& progs);
// The first of `names` the search path holds whose program loads and is of the single-player
// form when `single` (else of another form), loaded into `out` and named in `found`. False when
// none is: `why` then names each file passed over and why ("" when none of them is there).
bool find_program(const SearchPath& files, std::span<const std::string_view> names, bool single,
                  ge::progs::Progs& out, std::string& found, std::string& why);
// Whether the server's lines ask for a single-player game: the last `deathmatch` set is 0 (the
// menu's SINGLE PLAYER ▸ NEW GAME sets it so, a match's rules set it to 1 or more).
bool wants_campaign(const ListenOptions& opt);
// The map's title: the world's message, else the map's name.
std::string map_title(const Listen& l);
// The server's frame length now: the match's `sv_mintic`, or the campaign's step, s.
double listen_tic(const Listen& l);

// A lobby line from the player (slot 0): the lobby's words (server/lobby.hpp: ready, break,
// notready, toggleready, team, start, lobby), `addbot [novice|average|expert]` and `removebot`.
// False when the line is none of them, or when no lobby runs (a line saying so is printed).
bool listen_lobby(Listen& l, std::string_view line);

// One server frame with the player's command `cmd` (its angles are `l.view`, its length the
// server's frame; an impulse waiting goes with it).
void listen_frame(Listen& l, sh::MoveCmd cmd);
// The frames `wall_s` more wall time makes due, each with `cmd`; returns how many ran.
int listen_advance(Listen& l, double wall_s, const sh::MoveCmd& cmd);

// What the camera and the HUD read of the player, in map units.
struct PlayerState {
  int entity = 1;
  sh::Vec3f origin, velocity, view_ofs, v_angle;
  sh::Vec3f punch;        // the program's recoil kick on the view, degrees (0 without one)
  float ideal_pitch = 0;  // the pitch the view drifts to (a single-player program's), degrees
  bool on_ground = false;
  bool dead = false;
  float health = 0;
};
PlayerState player_state(const Listen& l);

// A missile in flight (an entity of class rocket, grenade or spike), map units.
struct Missile {
  int entity = 0;
  sh::Vec3f origin;
  bool rocket = false;  // a rocket or a grenade: something that explodes
};
void missiles_of(const Listen& l, std::vector<Missile>& out);

// A float field of an entity by its name in the progs (0 when the progs has no such field).
float field_named(const Listen& l, int entity, const char* name);

}  // namespace shooters_edge::client
