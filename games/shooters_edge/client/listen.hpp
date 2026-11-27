// THE LISTEN SERVER: Shooter's Edge as one player plays it on his own machine. A match is the
// engine's server core (engine/server) on the game's server (`ListenOptions::game`, the game
// the dedicated server runs) opened in this process with the window's program (the baseline
// mod, the arena's rules in C++: `baseline_progs`) and map
// (`ListenHost`): the local player is client slot 0, a local seat whose commands go into each
// frame as packets that are never lost (no prediction: what he sees is the server's own state
// after each frame), and others join on `ListenOptions::port` (`sv_port`) as they would a
// dedicated server.
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
// THE PARTY (a campaign in coop: coop_party.hpp): companions seated by `addbot` or
// `ListenOptions::companions`, carried from map to map by the campaign host, their packets in each
// frame after the player's; `removebot` lets one go. With a port, others join it (`CampaignDoor`).
//
// THE LOBBY (`ListenOptions::lobby`, a match only): the server's (net/qw_game.hpp). Every
// map opens in warmup with the player and the bots seated in `room` (lobby_room.hpp); the player is
// on the map only once he joins the warmup (`warmup`, F4) or the match starts, and off it again
// when he goes back to the lobby's screen (`sitout`), the bots fighting on meanwhile; the lobby's
// lines are printed like the server's; when the countdown runs out the server restarts the map
// with every seat on it (`restarted`), and a map change goes back to warmup. `listen_lobby` runs
// the player's lobby words. THE EASY JUMP: a player with `cl_speedjump 1` has the aid strafe
// for him (`listen_frame`, shooter/speedjump_assist.hpp) at the server's `pm_speedjump` (the
// window's 0.8, ListenHost::speedjump; a dedicated one only when set).
//
// THE VIEW: the player's angles are the client's (`Listen::view`, degrees: pitch down positive,
// yaw counter-clockwise from +x), sent with every command; when the server turns him (a spawn, a
// teleport: the mod's forced view) the client takes the server's angles (`forced`).
//
// ge:summary The listen server: a match on the server core in the window's process
// ge:keywords listen-server local-player client slot usercmd packet listen_start listen_frame
// ge:keywords listen_advance listen_change loaded-map load_named_map baseline_progs map-change
// ge:keywords listen_stop listen-host listenhost sv_port server-core hosting
// ge:keywords forced-view mintic player_state missiles rocket-jump lobby warmup listen_lobby
// ge:keywords match-restart restarted listen_lobby_view on_lobby_screen lobby-screen
// ge:keywords listen_campaign campaign single-player map_changed level-change
// ge:keywords listen_save listen_load savegame listen_save_text listen_load_text classic-save
// ge:keywords coop party companions campaign-door coop-join coop-over-network companion-coop
// ge:skill shooter progs
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "net/transport.hpp"
#include "progs/file.hpp"
#include "server/accounts.hpp"
#include "server/server.hpp"
#include "shooter/aim_turn.hpp"
#include "shooter/pmove.hpp"
#include "shooter/speedjump_assist.hpp"
#include "shooters_edge/client/campaign_view.hpp"
#include "shooters_edge/client/coop_party.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/lobby_room.hpp"
#include "shooters_edge/client/voice_out.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;
namespace sv = shooters_edge::server;

struct RemixFiles;  // remix.hpp

inline constexpr int kMostFrames = 8;             // server frames a wall frame may run
inline constexpr size_t kMostListenSounds = 256;  // sounds kept for the window between its plays

// The baseline mod (the arena's rules in C++, server/baseline_mod.hpp) as the window runs it.
bool baseline_progs(ge::progs::Progs& out, std::string& error);

// A map as the listen server plays it and the client draws it. Never copied once read: the spec's
// models point into `bsp`.
struct LoadedMap {
  std::string name;
  sv::MapSpec spec;
  sv::BspMap bsp;
  bool from_file = false;     // false: the built-in arena (nothing for the world draw)
  std::vector<uint8_t> file;  // the map file's bytes
  // Classic remix (client/remix.hpp): the content profile id of the other game whose packs held
  // the map ("" the played game's own), and whether a map of brushes plays on the programs'
  // rules (the played game's), its models traced through their brushes, instead of the rules
  // that trace brushes.
  std::string from_game;
  bool program_rules = false;
};

// `maps/<name>.bsp` read from bytes.
std::unique_ptr<LoadedMap> map_from_bytes(std::span<const uint8_t> bytes, const std::string& name,
                                          std::string& error);
// `arena` (built in) or `maps/<name>.bsp` from the search path.
std::unique_ptr<LoadedMap> load_named_map(const SearchPath& files, const std::string& name,
                                          std::string& error);

// A LISTEN SERVER ON THE CORE: a match is a `ge::server::Server` the listen opens on
// `ListenOptions::game` (net/qw_game.hpp's `qw_game()`, which the client cannot name:
// it sits above it), handing it this in `ServerConfig::host`. The listen fills the first block;
// the game's `create` fills the second and keeps it current through the run (a map change swaps
// `rules` and `map`), so the listen reads the game playing without knowing its type.
struct ListenHost {
  const ge::progs::Progs* progs = nullptr;  // the program (outlives the server)
  // A mod's compiled module (its qwprogs.qvm) that plays in place of `progs`; null or empty: none.
  const std::vector<uint8_t>* module = nullptr;
  // A mod's game library of Quake II's interface (server/q2_native.hpp) that plays in place
  // of `progs` and `module`, on any map; empty: none.
  std::string native_library;
  std::shared_ptr<LoadedMap> map;     // the first map, read
  const SearchPath* files = nullptr;  // where the next map is read (null: the arena only)
  std::vector<std::string> lines;     // server lines before each map's rules start
  // The remix's lines the match started with (`ListenOptions::arsenal`, `over_mod`, `remix_info`):
  // a map change after the remix changed starts the match afresh, its items the remix's new ones.
  std::vector<std::string> worn, worn_over, worn_info;
  bool lobby = false;
  // The bots' nav built on a thread of its own (bots/bot_seats.hpp's `background`): the window's.
  bool bots_background = false;
  sv::LobbySettings lobby_settings;
  std::string player = "player";  // slot 0, the window's own player: a local seat
  float speedjump = 0;            // pm_speedjump on each map's server (the easy jump allowed)
  // The player's side of every frame: the rules' frame before the endpoint routes it (prints,
  // effects, blows), and a line printed to him (the lobby's and the room's answers).
  void (*tap)(void* ctx, const sv::RulesCommon& game) = nullptr;
  void (*print)(void* ctx, int level, const std::string& text) = nullptr;  // the print's level
  // A voice the server relayed to him (the match's `sv_voip`): the talker's slot, the protocol's
  // generation and first sequence, the frames (demo.hpp's `DemoVoice`).
  void (*heard)(void* ctx, int talker, int gen, int seq,
                const std::vector<uint8_t>& data) = nullptr;
  void* ctx = nullptr;

  void* game = nullptr;  // the game's state, for the calls below
  sv::RulesRunner* rules = nullptr;
  Room* room = nullptr;  // null: no lobby and no bots
  int changes = 0;       // maps the game moved to (a lobby's restart too)
  std::string asked;     // a map the mod asked for that the window reads (then `change`)
  // A save of Quake II's game (server/q2_save.hpp) the next map's rules load, then cleared,
  // and its name for a refusal.
  std::vector<uint8_t> load;
  std::string load_name;
  // The player's words the game takes (the room's `addbot`, `removebot`; the lobby's): false
  // when none of them.
  bool (*words)(void* game, const std::string& line) = nullptr;
  // The player's move for the next frame.
  void (*move)(void* game, const sh::Packet& p) = nullptr;
  // The game moved onto `map` (read by the window): false with `error`, the game on the old map.
  bool (*change)(void* game, std::shared_ptr<LoadedMap> map, std::string& error) = nullptr;
  // A key of the player's user info, told to every client.
  void (*set_info)(void* game, const std::string& key, const std::string& value) = nullptr;
  // The player's voice message (voice_out.hpp's frames), relayed to those who hear him.
  void (*speak)(void* game, int gen, int seq, const std::vector<uint8_t>& data) = nullptr;
  // The administrator's words on the window's own server (net/qw_admin.hpp: `users`,
  // `status`, `speedjump <id> on|off|<share>`): false when the line is none of them, the answer
  // in `out`.
  bool (*admin)(void* game, const std::string& line, std::string& out) = nullptr;
  // The player's line to the server as a client's string command (`say`, `say_team`,
  // `say_near`, `voicetarg`): routed by the server as a joined client's is.
  void (*command)(void* game, const std::string& line) = nullptr;
};

// THE CAMPAIGN'S DOOR: a coop campaign served to others over the classic protocol, so they join
// the window's coop game as they would a server (net/qw_coop.hpp's `qw_coop()`, which
// the client cannot name: it sits above it). The window's player and the companions are its local
// seats; a client who joins enters the campaign (carried from map to map as the player is), and
// the endpoint follows the campaign onto each next map.
struct CampaignDoor {
  // The endpoint opened on the campaign's map over `t` (which outlives it), `most` players at
  // most; `tap` sees each frame's rules before the endpoint routes them. Null with `error`.
  void* (*open)(sv::Campaign& c, ge::net::Transport& t, const std::string& hostname, uint32_t seed,
                int most, void (*tap)(void* ctx, const sv::RulesCommon& game), void* ctx,
                std::string& error) = nullptr;
  // A seat the window plays (its player, `bot` false; a companion) shown to every client.
  bool (*seat)(void* door, int slot, const std::string& name, bool bot,
               std::string& error) = nullptr;
  void (*unseat)(void* door, int slot) = nullptr;
  // One frame at the network's time with the local seats' moves, then the change it asked for
  // (the endpoint follows the campaign onto the next map). False with `error` when that map did
  // not load.
  bool (*frame)(void* door, const sh::Packet* local, int count, int64_t now_us,
                std::string& error) = nullptr;
  // The campaign moves maps outside a frame (a new game by hand): let go before, follow after.
  void (*unbind)(void* door) = nullptr;
  bool (*rebind)(void* door, std::string& error) = nullptr;
  // The logins on the coop game (server/login.hpp), over `c`'s accounts (`accounts`,
  // `account_link`, `unix_us`, `login`), which outlive the door. False with `error`.
  bool (*logins)(void* door, const ge::server::ServerConfig& c, std::string& error) = nullptr;
  int (*joined)(const void* door) = nullptr;  // clients that joined over the network, playing
  void (*close)(void* door) = nullptr;
};

struct ListenOptions {
  // In the order that packs it (the lint holds the padding).
  const SearchPath* files = nullptr;  // where a campaign's next map is read (outlives the listen)
  // The server a match runs on (net/qw_game.hpp's `qw_game()`); a match does not start
  // without one. A campaign runs on its own host.
  const ge::server::ServerGame* game = nullptr;
  // Or the socket others join on, opened by the caller (a test's simulated network): it outlives
  // the listen, and `port` is not opened.
  ge::net::Transport* transport = nullptr;
  // The logins on a match others join (the window's `sv_login`, `sv_accountmaster`, ...; server/
  // login.hpp): with sv_login on and no master, the window's own accounts in `accounts_dir`; with
  // a master, a socket to ask it on. `accounts`, when set, is the service instead (a test's, on
  // the simulated network). `unix_us` is the wall clock (null: the computer's).
  ge::account::Service* accounts = nullptr;
  int64_t (*unix_us)() = nullptr;
  // A campaign in coop is served to others on `port` (or `transport`) through this door
  // (net/qw_coop.hpp's `qw_coop()`); null: a coop game is the window's alone.
  const CampaignDoor* door = nullptr;
  // Classic remix's files and choices (remix.hpp), or null: a campaign's own change of map (a
  // level's exit, the start map's skill gates) reads the next map through it, so its monsters are
  // `cr_monsters`'s as a map the console asked for is (outlives the listen).
  const RemixFiles* remix = nullptr;
  // Called before a campaign's own change of map reads the next one: the window takes the cr_
  // settings changed during the map here (remix.hpp's `remix_waits`), as it does before a map
  // the console asked for. Null: nothing to take.
  std::function<void()> before_map;
  // A mod's compiled module (GameContent's `module`) that plays a match in place of the program;
  // null or empty: the program (outlives the listen).
  const std::vector<uint8_t>* module = nullptr;
  // A mod folder's game library of Quake II's interface (GameContent's `native_library`) that
  // plays a match in place of the program; null or empty: none.
  const std::string* native_library = nullptr;
  std::vector<std::string> console = {"deathmatch 1"};  // server lines before each map loads
  std::vector<std::string> item_names;  // a match's server lines before `console` (GameContent's)
  std::vector<std::string>
      arsenal;  // the game's weapons and armour as server lines (GameContent's)
  // Lines a mod's own rules take too (classic remix's `cr_over_mods`: `cr_physics`'s), after the
  // window's own; `arsenal` is left out while a mod plays.
  std::vector<std::string> over_mod;
  // The remix the match plays as server info lines (classic remix's `remix_info_lines`), for every
  // client that joins, a mod's rules or not; our client follows them.
  std::vector<std::string> remix_info;
  std::string player = "player";
  std::string hostname = "Shooter's Edge";
  std::string accounts_dir;
  // The window's lobby: two seats (a bot counts) before a match may start, and the player waits
  // on the lobby's own screen until he joins the warmup (F4).
  sv::LobbySettings lobby_settings = {.min_players = 2, .warm_on_join = false};
  sv::EpisodeOrder episodes;  // a campaign's episodes in order (GameContent's)
  ge::server::LoginSettings login;
  uint32_t seed = 1;
  // Others join the match on this UDP port (0: nobody: the match is the window's alone).
  int port = 0;
  int maxclients = 32;
  // A campaign in coop starts with this many companions beside the player (coop_party.hpp).
  int companions = 0;
  bool registered = false;  // a campaign's content is the full game (gfx/pop.lmp)
  // A match opens every map in the lobby's warmup (lobby_room.hpp): ready, teams, bots, the
  // countdown, then the map restarted for the match. Off: the match starts with the map.
  bool lobby = false;
  // The bots' nav built on a thread of its own, so adding a bot or a map change with bots in never
  // holds a frame while it builds (the window's; off, a run is the same frame by frame: tests).
  bool bots_background = false;
  // A match relays its players' voices (`sv_voip 1`), the window's player among them
  // (`listen_mic`, `Listen::voices`).
  bool voice = true;
  sb::AimSkill companion_skill = sb::AimSkill::average;
  // The next map starts a new campaign on these options (`newgame`, `coopgame`, `sv_coop 1`),
  // not the running one's change of map; cleared once it has started.
  bool new_game = false;
};

// What the console set on the window's listen (`from`: the lobby, its settings, sv_port,
// maxclients, sv_hostname, sv_login and the rest) copied onto the options a new game opens with.
inline void take_console_settings(ListenOptions& to, const ListenOptions& from) {
  to.lobby = from.lobby;
  to.lobby_settings = from.lobby_settings;
  to.port = from.port;
  to.maxclients = from.maxclients;
  to.hostname = from.hostname;
  to.login = from.login;
}

// A blow the player took (a single-player program's damage record, client_data.hpp): its armour
// and blood, 0..255 each, and where it came from, map units.
struct Blow {
  int armor = 0, blood = 0;
  sh::Vec3f from;
};

struct Listen {
  const ge::progs::Progs* progs = nullptr;
  ListenOptions opt;
  std::shared_ptr<LoadedMap> map;     // the map playing (a match's is the server's too)
  sv::RulesRunner* rules = nullptr;   // the rules playing: the match server's, or the campaign's
  sv::RulesCommon* server = nullptr;  // what they keep (`*rules`'s): the loop, the lists, the sink
  // A match: the engine's server on `opt.game` and the window's content it was opened with; its
  // socket (closed when nobody may join).
  std::unique_ptr<ge::server::Server> match;
  std::unique_ptr<ListenHost> host;
  std::unique_ptr<ge::net::Transport> port;
  std::unique_ptr<ge::server::ServerAccounts> accounts;  // a match's logins' (`opt.login`)
  std::unique_ptr<sv::Campaign> campaign_host;           // a campaign's host (null in a match)
  std::unique_ptr<LoadedMap> next;  // a map read ahead for the campaign's loader
  bool map_changed = false;  // the campaign moved to another map in the last frames (the window
                             // draws the new one and clears it)
  size_t maps_started = 0;   // of the campaign's history: the maps the listen has entered
  std::string failed;        // the campaign's next map did not load: why
  CampaignView campaign;     // the program's client messages (campaign_view.hpp)
  double entered_at = 0;     // the server's time the player entered this map, s
  uint32_t sequence = 0;
  sh::MoveCmd cmds[3];  // the three newest, oldest first
  double owed = 0;      // wall seconds not yet run, s
  int64_t frames = 0;   // server frames run on this map
  // Quake II's rules frame whose items back and teleporter pads were last thrown (-1 none).
  int64_t q2_fx_frame = -1;
  sh::Vec3f view;        // the player's view angles, degrees (his aim)
  sh::AimTurn aim_turn;  // the aim help's turn of the view (shooter/aim_turn.hpp)
  // The easy jump (shooter/speedjump_assist.hpp): the aid strafing for him while his slot asks
  // (`cl_speedjump 1`), at the server's pm_speedjump; `speedjump_view` of its swing is drawn.
  sh::SpeedJumpAid jump_aid;
  float speedjump_view = 0.5f;      // cl_speedjump_view
  bool forced = false;              // the server turned the view in the last frame
  bool load_menu_asked = false;     // the rules asked his client for the load menu (a death)
  int impulse = 0;                  // waiting for a frame
  std::string pending_map;          // the mod or the console asked for this map
  std::string cinematic;            // Quake II's movies before it (cinematic.hpp), for the window
  std::vector<std::string> prints;  // what the server printed to the player, in order
  // Each print's level (0 low .. 3 chat) by its index; past the end, or a line of the window's
  // own, 2 (high).
  std::vector<int> print_levels;
  std::vector<std::string> centres;  // its centre prints, in order ("" clears the last)
  // The lines a single-player program stuffed into his console other than `bf` (counted in
  // `bonuses`) and the load menu's, for the window's console to run.
  std::vector<std::string> stuffed;
  // A save of Quake II's game the next match started loads (ListenHost::load), and its name.
  std::vector<uint8_t> load;
  std::string load_name;
  // Where the mod's explosions went off on this map (its temporary-entity message), map units,
  // in order.
  std::vector<sh::Vec3f> explosions;
  // Quake II's: each one's kind byte by index (q2_model_fx.hpp; 0 any other), as long as the last
  // one with a kind (the rest are 0).
  std::vector<uint8_t> explosion_codes;
  std::vector<Blow> blows;  // the blows the player took on this map, in order
  int64_t bonuses = 0;      // the items he took (the server's `bf`), since the window opened
  double cshift[4] = {130, 80, 50, 0};  // v_cshift: the tint in open air (red, green, blue, of 255)
  // The sounds the server started for the player (the protocol's sound message to slot 0) since
  // the window last played them (listen_sound.hpp empties it), and the map's looping sounds.
  std::vector<DemoSound> sounds;
  std::vector<DemoStaticSound> static_sounds;
  // A match's voices: what the server relayed to the player since the window last played them
  // (listen_sound.hpp empties it), and his own microphone cut into frames (`listen_mic`).
  std::vector<DemoVoice> voices;
  VoiceOut voice;
  // A campaign's effects on this map (its temporary entities and particle bursts), in order; its
  // explosions are in `explosions` too.
  std::vector<Effect> effects;
  // The lobby (a match with `ListenOptions::lobby`; null otherwise) and whether the last frames
  // restarted the map for the match (the window clears what it keeps of the old server's
  // entities, then this).
  Room* room = nullptr;  // the match server's
  bool restarted = false;
  bool voice_tried = false;  // `voice`'s encoder was opened (or failed to) for this game
  int changes = 0;           // the match server's map changes the listen has caught up with
  int entries = 0;           // maps entered (a new one, the next level, a restart, a load), ever
  // A campaign's companions (coop_party.hpp; null when none was ever seated).
  std::unique_ptr<Party> party;
  // The game was single player and turned cooperative for a companion (`addbot`; server/
  // campaign.hpp's `campaign_set_coop`): the last companion's leaving turns it back, and saving
  // waits until then.
  bool companion_coop = false;
  // A coop campaign's door open to others (`ListenOptions::door`; null: nobody joins) and the
  // network's time it runs on, s.
  void* door = nullptr;
  const CampaignDoor* door_calls = nullptr;
  double net_s = 0;
};

// The server made for `progs` (which must outlive it) on `map`, the player connected.
bool listen_start(Listen& l, const ge::progs::Progs& progs, std::unique_ptr<LoadedMap> map,
                  const ListenOptions& opt, std::string& error);
// The same progs on another map (the spawn parms saved first), the player connected again; in a
// campaign, a new game on it.
bool listen_change(Listen& l, std::unique_ptr<LoadedMap> map, std::string& error);
// The game ended and let go (its servers, then the maps they view, then the program), the options
// kept: another game folder may be opened, and the next start is `listen_start`'s.
void listen_stop(Listen& l);
// `quit [reason]` and `recycle [reason]` in the window while it hosts a match: each player who
// joined over the network told "Server quit" or "Server restarted" (": <reason>" after it when
// one was given) and dropped, or told to reconnect (`restart`), by the match's game
// (ServerGame::quit). Nothing when no match is hosted.
void listen_farewell(Listen& l, const std::string& reason, bool restart);
// `net_restart` in the window: a hosted match's socket opened again on `port` (sv_port) on every
// interface, else on the port it had (`port` set back to it), the game told
// (ServerGame::reopened); what went wrong in `said`. False when no match takes players over the
// network (nothing to open) or no port opened.
bool listen_net_restart(Listen& l, int& port, std::string& said);
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
// The save `bytes` (named `name`) of Quake II's game (server/q2_save.hpp) loaded as the game
// now playing: onto Quake II's game the window plays (its next map the save's), or a new one
// started on `opt`; its map read from `opt.files`. False with `error` when the save or its map
// does not load.
bool listen_load_q2(Listen& l, const ListenOptions& opt, std::span<const uint8_t> bytes,
                    std::string_view name, std::string& error);
// The autosave Quake II's rules made as the last level was entered (null when none is waiting),
// taken: the caller keeps it.
std::vector<uint8_t> listen_take_autosave(Listen& l);
// The same as the classic text save (server/q1_save.hpp), and one loaded: less is kept, so
// the game plays on as the program's state says rather than bit for bit.
bool listen_save_text(const Listen& l, std::string& out, std::string& error);
bool listen_load_text(Listen& l, const ge::progs::Progs& progs, const ListenOptions& opt,
                      std::string_view text, std::string_view name, std::string& error,
                      std::unique_ptr<LoadedMap> map = nullptr);
// A brush model file's bounds read from the search path `ctx` (a `const SearchPath*`): the mod
// server's ModelBounds (qc_server.hpp), so a single-player program's exploding box has its size.
bool search_path_model_bounds(void* ctx, const std::string& name, ge::shooter::Vec3f& mins,
                              ge::shooter::Vec3f& maxs);
// Whether a program is a single-player one (its layout's form).
bool single_player(const ge::progs::Progs& progs);
// Whether the server's lines ask for a single-player game: the last `deathmatch` set is 0 (the
// menu's SINGLE PLAYER ▸ NEW GAME sets it so, a match's rules set it to 1 or more).
bool wants_campaign(const ListenOptions& opt);
// The map's title: the world's message, else the map's name.
std::string map_title(const Listen& l);
// The server's frame length now: the match's `sv_mintic`, or the campaign's step, s.
double listen_tic(const Listen& l);

// A lobby line from the player (slot 0): the lobby's words (server/lobby.hpp: ready, break,
// notready, toggleready, team, start, warmup, sitout, togglewarmup, lobby), `addbot
// [novice|average|expert]` and `removebot`, and the vote's (`vote ...`, `yes`, `no`: any match). In
// a campaign the last two seat and let go a companion (coop on: `coopgame` starts one so). False
// when the line is none of them, or when no lobby runs (a line saying so is printed).
bool listen_lobby(Listen& l, std::string_view line);
// The player's chat line (`say "..."` or `say_team "..."`) on the window's own server, as slot 0
// says it: false when no game is playing.
bool listen_say(Listen& l, const std::string& line);
// True when the window's own game runs Quake II's rules (server/q2_server.hpp).
bool listen_is_q2(const Listen& l);
// True when the window's own game fires Quake II's weapons whole (classic remix's `cr_items q2` on
// the classic rules, rules/common_q2_arms.cpp): their hits, blasts and trails are drawn as Quake
// II's.
bool listen_q2_arms(const Listen& l);
// The rules draw their own screens (a scoreboard, an inventory): the window draws none of its own.
bool listen_own_screens(const Listen& l);
// A console line the window does not know, given to the rules of a game whose client passes its
// own commands to its server (Quake II's: `use`, `drop`, `weapnext`, `weapprev`, `help`, `kill`
// ...) as slot 0 says it; false when no such game is playing or its rules refuse it.
bool listen_rules_word(Listen& l, const std::string& line);
// A console line the window does not know, in a single-player program's game: the original's
// cheats and `kill` (god, notarget, noclip, fly, give, kill) go to the server as slot 0's own
// command, its level commands (restart, changelevel) to the server's console; false for any other
// line or game. On any Quake server (deathmatch too), `tell` and `ping` go to it as slot 0's.
bool listen_campaign_word(Listen& l, const std::string& line);
// A line the server printed to the player at `level` (0 low .. 3 chat), its level kept beside it.
void listen_print(Listen& l, int level, const std::string& text);
// The people playing in the game the window hosts (its own player and those who joined; never
// a bot, so a game with companions alone still pauses under the menu).
int listen_players(const Listen& l);
// An administrator's line on the window's own server (`users`, `status`, `speedjump`: ListenHost's
// `admin`): false when no match is hosted or the line is none of them, the answer in `out`.
bool listen_admin(Listen& l, const std::string& line, std::string& out);

// What the lobby's banner and screen show (lobby_room.hpp's `lobby_view` for the player), each
// seat's frags read off the server when it is on the map, and the map's name. A view with no rows
// when no lobby runs.
hud::LobbyView listen_lobby_view(const Listen& l);
// Whether the player is on the lobby's screen: a lobby runs and he is not on the map (he has not
// joined the warmup, and the match is not on).
bool on_lobby_screen(const Listen& l);

// The player's microphone since the last call (48 kHz mono): while `talking` (his talk key held)
// on a match with voice, sent to those who hear him. Nothing in a campaign or with no match.
void listen_mic(Listen& l, std::span<const float> samples, bool talking);
// Whether the game playing carries the player's voice (a match with `ListenOptions::voice`).
bool listen_voice(const Listen& l);
// The player's line to his own match's server as a joined client's (ListenHost's `command`: a
// chat line, a voice target): false when no match is hosted (a campaign, nothing loaded).
bool listen_command(Listen& l, const std::string& line);

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
  bool noclip = false;
  bool dead = false;
  float health = 0;
  // The rules work the view out themselves (Quake II's: RulesWorld's `view_set`): `view_ofs` is
  // their eye with its bob, `kick` the angles they add (degrees), `v_angle` the dead view's.
  bool rules_view = false;
  sh::Vec3f kick;
  float fov = 0;  // the field of view the rules hold the view to, degrees (0: the player's own)
  // Quake II's rules run a frame every tenth: the frame's number, the seconds since it (-1: no
  // such rules) and how far its movers carried the player in it.
  int64_t rules_frame = -1;
  double rules_age = -1;
  sh::Vec3f pushed;
};
PlayerState player_state(const Listen& l);

// The aim help's turn for the player (slot 0): his eye, and the middle of the player `target`
// (an entity) when that one is alive in the world; false when he is gone.
bool aim_turn_points(const Listen& l, int target, sh::Vec3f& eye, sh::Vec3f& middle);

// A missile in flight (an entity of class rocket, grenade or spike), map units.
struct Missile {
  int entity = 0;
  sh::Vec3f origin;
  bool rocket = false;   // a rocket or a grenade: something that explodes
  bool grenade = false;  // a grenade (bounces, smokes) rather than a rocket
  // Neither: a nail, or one of Quake II's glowing shots, by the kind of shot whoever fired it
  // (a player or a monster): a blaster's bolt, or the BFG's ball.
  bool bolt = false, bfg = false;
  bool streak = false;  // Quake II's blaster bolt (its blaster bit): specks behind it
  // Quake II's other marks by its effect bits (q2_look.hpp's Q2Look::Missile as a number: a
  // flag, the tag token, an ion ripper's, a blue or a plasma bolt); 0 none of these.
  uint8_t q2_mark = 0;
  // Quake II's spinning lights: a red light 64 units ahead along its spin from its `yaw` (degrees).
  bool q2_spin = false;
  float yaw = 0;
};
void missiles_of(const Listen& l, std::vector<Missile>& out);
// Quake II's areas the player's eye sees into, one bit an area (server/q2_map.hpp's
// `map_area_bits`); empty when the game is not Quake II's or its map has no areas.
void listen_area_bits(const Listen& l, std::vector<uint8_t>& out);

// True when the entity is drawn: a model index and a model text (a program hides a taken item by
// emptying its text; server::model_shown). Without a QC program: a model index alone.
bool model_shown(const Listen& l, int entity);

// A float field of an entity by its name in the progs (0 when the progs has no such field).
float field_named(const Listen& l, int entity, const char* name);
// The string field `name` of `entity` (empty when the program has none).
std::string_view string_field_named(const Listen& l, int entity, const char* name);
// A vector field by name into `out`; false (and `out` untouched) when the program has none.
bool vector_field_named(const Listen& l, int entity, const char* name, float out[3]);
// Sets that field (nothing when the progs has no such field): a lab hands the player its weapons.
void set_field_named(Listen& l, int entity, const char* name, float value);
// A float global of the program by its name (0, and `found` false, when it has no such global).
float global_named(const Listen& l, const char* name, bool* found = nullptr);

}  // namespace shooters_edge::client
