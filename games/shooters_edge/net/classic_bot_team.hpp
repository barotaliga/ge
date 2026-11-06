// A TEAM OF BOTS ON ANY CLASSIC SERVER: N fighters (bots/fighter.hpp), each at the controls of its
// own classic client (classic_client.hpp) on its own UDP socket, joining one team on a server this
// process cannot read (an existing server running a match mod, or ours) and playing it. One process
// is one team; two processes can play the two teams of a bots-only match, neither knowing the
// other.
//
// WHAT A BOT KNOWS: only what its own client is sent (classic_mirror.hpp) and what its teammates
// say on `say_team`, read by its own mind. Four bots in one process share no memory: the process
// holds them side by side, and only the team's report (`bot_team_report`) and the picture of the
// team (bots/team_view.hpp) read them all, never writing back.
//
// THE MAP: loaded from the player's own packs (`basedir`, the game folder serverdata names over
// the base game's) by the name the model list carries, its nav built off a local copy of the map
// spawned by the baseline mod (bots/map_nav.hpp: the floor, the movers, the water), so it is the
// map's own shape whatever mod the server runs. The nav's movers are then named by the server's
// entity numbers (the entity whose baseline carries the same brush model). A map change (the
// server's reconnect) loads the new map and sets every bot up again.
//
// THE MATCH MOD'S WAYS: the team and colours are the user info's from the start; any `color`,
// `team` or `setinfo` a mod stuffs is obeyed (classic_client.hpp). With `ready` set, each bot says
// `ready` once it is set up on the map and again after every map change or match end
// (a `break` from the console takes it back).
//
// THE CONSOLE (`bot_team_command`): a line goes to every bot; `bot<N> <line>` (`bot1 skill
// expert`) to one. A bot takes its own settings (`skill <name>`, `temper <name>`, `traits
// <name=v/...>`, `<trait> <1..10>`; a setting sets the bot up again on its next tick, its map
// knowledge started over), `status`, and sends anything else to the server as a console line
// (`ready`, `break`, `say hi`, `team red`).
//
// THE RECORDING: with `out` set, each bot's client recording (every packet read and every command
// sent, classic_record.hpp) is written to `<out>/<name>.demo` at the end, and the team's lines to
// `<out>/team.txt`.
//
// THE DEATHS: each bot's death is a line in the log: where (the place name), its mode and role,
// its goal, how deep it stood in water and how long its head had been under, whether it was on a
// dive or swimming out, so a match's deaths can be read back by cause.
//
// THE CLOCK: the caller's (`bot_team_tick` with microseconds since start; the bots' moves are 13
// ms each, the server's frame), so a window or a headless loop drives it alike.
//
// ge:summary A team of bots on any classic server: N fighters on N UDP clients, one team, a console
// ge:keywords bot-team live-server existing-server mvdsv ktx 4on4 ready udp bots-join
// ge:keywords bot_team_start bot_team_tick bot_team_command bot_team_report per-bot-settings
// ge:keywords match-folder recording reconnect map-change mover-remap
// ge:skill bots net shooter
// ge:test tests/shooters_edge_bot_team_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "shooter/aim.hpp"
#include "shooter/traits.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/team.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_mirror.hpp"
#include "shooters_edge/net/classic_record.hpp"
#include "shooters_edge/net/classic_team_nav.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

struct BotTeamConfig {
  std::string server = "127.0.0.1:27500";
  int bots = 4;
  std::string team = "bots";
  int colour = 4;                     // top and bottom
  std::string name_prefix = "gebot";  // the bots' names: the prefix and a number (1..)
  // Each bot's skill and traits (cycled when fewer than `bots`): "expert", "average", "novice",
  // "perfect"; traits over the skill's preset as shooter/traits.hpp reads them ("aim_rl=9").
  std::vector<std::string> skills = {"expert", "expert", "average", "average"};
  std::vector<std::string> traits;
  std::string basedir;  // the folder holding the game folders (the player's own packs)
  bool ready = true;    // say `ready` once set up on the map
  std::string out;      // the match folder ("": nothing written)
  uint32_t seed = 1;    // bot k's hands seed + 31 k
  uint16_t first_qport = 2100;
  // When set, gives a place name file's text for a map ("" none): `locs/<map>.loc` in the packs is
  // read when this is unset.
  std::function<std::string(const std::string& map)> locs;
  // A test's or a lab's hooks: how bot k's transport opens (unset: a UDP socket on any port), and
  // how a game file is read (unset: from `basedir`'s game folders).
  std::function<bool(gn::Transport& t, int k, std::string& error)> open;
  std::optional<gn::Address> address;  // the server's, in place of `server` (a simulated one)
  std::function<std::vector<uint8_t>(const std::string& gamedir, const std::string& path)>
      read_file;
};

struct BotSeat {
  int index = 0;  // 0..bots-1; its console name is bot<index+1>
  std::string name;
  sh::AimSkill skill = sh::AimSkill::average;
  sh::Traits traits;
  gn::Transport transport;
  ClassicClientEnd c;
  ClientRecorder recording;
  std::unique_ptr<ClientMirror> mirror;
  std::unique_ptr<bt::Fighter> fighter;  // made anew each time it is set up
  bool set_up = false;                   // the fighter is on the map
  bool reset = false;                    // set up again on the next tick (a setting changed)
  bool said_ready = false;
  int spawncount = -1;  // the map the fighter was set up on
  size_t heard = 0;     // its client's prints read so far
  size_t printed = 0;   // its client's prints already in the team's log
  int64_t commands = 0;
  int last_health = 0;  // its health at the last tick (a fall to 0 is a death, logged)
  int64_t deaths = 0;
};

struct BotTeam {
  BotTeamConfig cfg;
  gn::Address server;
  std::vector<std::unique_ptr<BotSeat>> seats;
  bt::Team team;

  // The map the team is on.
  std::string map;  // its file ("maps/e1m2.bsp"); "" before the first is loaded
  std::string map_error;
  std::unique_ptr<client::LoadedMap> loaded;
  std::unique_ptr<server::RulesRunner> local;  // the map spawned locally, for its nav
  std::unique_ptr<TeamNav> nav;
  bool movers_named = false;
  bool plan_said = false;  // the map plan's lines in the log
  int maps = 0;            // maps loaded

  int64_t now_us = 0, ticks = 0;
  std::vector<std::string> log;  // what the process prints: the server's lines once, the console's
  size_t log_read = 0;           // the caller's place in `log`
};

// Opens a socket a bot and starts every client toward `cfg.server`. False (and `error`, with the
// next step) when the address does not read or a socket does not open.
bool bot_team_start(BotTeam& t, const BotTeamConfig& cfg, std::string& error);
// One tick of every bot at the caller's clock (microseconds since the start).
void bot_team_tick(BotTeam& t, int64_t now_us);
// A console line (see the header). Answers go into `log`.
void bot_team_command(BotTeam& t, const std::string& line);
// Every bot disconnected and, with `out`, its recording written. False (and `error`) when a file
// does not write.
bool bot_team_stop(BotTeam& t, std::string& error);
// The team as the bots believe it, a few lines: the score, each bot's place, health, armour,
// weapon, mode, role, goal and last words.
std::string bot_team_report(const BotTeam& t);

// The maps' place names read from `locs/<map>.loc` in the player's game folder `folder` under
// `cfg.basedir` (a client's own, its packs or loose: `ezquake`), into `cfg.locs`. False (nothing
// set) when the folder does not open.
bool bot_team_locs_from(BotTeamConfig& cfg, const std::string& folder);

// "expert", "average", "novice", "perfect" (or e, a, n, p) as a skill; false for anything else.
bool aim_skill_named(std::string_view name, sh::AimSkill& out);

}  // namespace shooters_edge::net
