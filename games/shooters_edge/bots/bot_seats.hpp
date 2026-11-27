// THE BOTS IN A GAME'S SEATS, FOR EVERY CLASSIC GAME: fighters (shooterbots/fighter.hpp: each reads
// only what a client is sent) seated in the rules' free slots, as a listen server's lobby
// (client/lobby_room.hpp) and a dedicated server's `sv_bots` (QuakeWorld's net/qw_game.cpp, Quake
// II's net/q2_game.cpp) both seat them:
//
// - `bot_seats_add` seats one in the lowest slot free on the rules, under their player limit and
//   outside the slots held elsewhere (a dedicated server's clients), named `bot1`, `bot2`...;
// - the NAV is built once a map, the first time a bot is seated: off the world model's clip hulls
//   (shooterbots/nav.hpp; on the built-in arena its door and plat are its movers), or off a map of
//   brushes (shooterbots/map_nav.hpp, from its deathmatch spots), Quake II's move and arsenal on
//   Quake II's rules, Quake's elsewhere. With `background` (the window's) it is built on a thread
//   of its own from a copy of the map and of the entities as they stand, so no frame waits seconds
//   for it: the bots seated meanwhile stand still (an idle command a frame) until it lands, and
//   each frame's `bot_seats_packets` lands it and makes their fighters. Without it (tests, labs, a
//   server's own run) it is built at once, so a run stays the same frame by frame;
// - every map entered remakes each bot's fighter on the new nav (connected again first when the
//   caller asks); a map no bot can walk sends them all away;
// - each frame a bot's packet is made from what its client is sent and runs in the rules' frame in
//   slot order; a view the rules turned (a spawn spot's, a teleport's) is taken as its own: sent
//   back whole where the clients take the server's view, and on Quake II's own model (the view is
//   the command's angles plus the rules' delta) the delta taken off every view it sends.
//
// Deterministic: a bot's hands are seeded off the seats' seed and its slot.
//
// ge:summary The bots in a game's seats: fighters seated, their nav per map, their packets a frame
// ge:keywords bot-seats sv_bots bot_seats_add bot_seats_enter bot_seats_packets fighters seats
// ge:keywords dedicated-bots q2-bots addbot nav-per-map
// ge:skill bots shooter
// ge:test tests/shooters_edge_q2_bots_server_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "shooterbots/aim.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/nav.hpp"
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::bots {

namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

struct SeatBot {
  Fighter fighter;
  bool made = false;  // its fighter made on the map's nav (false while a background nav builds)
  AimSkill skill = AimSkill::average;
  std::string name;     // its player's name on the rules
  sh::MoveCmd cmds[3];  // the three newest, oldest first
  uint32_t sequence = 0;
  int16_t delta_seen[3] = {};  // Quake II's own view model: the turn the rules last gave it
};

struct NavJob;  // a nav building on another thread (bot_seats.cpp)

struct BotSeats {
  std::array<std::unique_ptr<SeatBot>, sh::kMaxClients> bots;  // by slot (null: no bot)
  std::unique_ptr<NavGraph> graph;                             // a map of clip hulls': the nav
  std::unique_ptr<sh::MoveWorld> world;                        // and its solids (the hop's look)
  std::unique_ptr<MapNav> nav;          // a map of brushes': the nav and its solids
  std::string nav_of;                   // the map the nav was built for
  const void* nav_from = nullptr;       // and the loaded map it traces
  std::string entities;                 // the map's entity text the fighters learn items from
  const BotArsenal* arsenal = nullptr;  // the game's weapons the fighters read
  uint32_t seed = 1;
  int named = 0;      // bots ever seated (their names count up)
  uint32_t held = 0;  // a bit a slot no bot may take (a dedicated server's clients)
  // The nav built on a thread of its own (above); the map's copy it traces kept beside it.
  bool background = false;
  std::shared_ptr<NavJob> job;
  std::shared_ptr<const sv::MapSpec> nav_spec;
  // A background nav that found no floor: the slots its bots held (emptied here; the caller drops
  // their players and clears this) and why.
  std::vector<int> gone;
  std::string gone_why;
};

// How many bots are seated.
int bot_seats_count(const BotSeats& b);
// A fighter of `skill` seated on `rules` (playing `map`) in the lowest slot from 1 free on them,
// under their player limit and outside `held` and `taken` (a bit a slot): its slot, or -1 with
// `error` (the server full, or no floor a bot can walk on the map). With `background` its nav may
// still be building: it is seated and stands still until the nav lands.
int bot_seats_add(BotSeats& b, const sv::RulesRunner& rules, const sv::MapSpec& map, AimSkill skill,
                  uint32_t taken, std::string& error);
// The highest bot's slot, or -1 when none is seated.
int bot_seats_last(const BotSeats& b);
// Bot `slot` forgotten (the caller drops its player).
void bot_seats_forget(BotSeats& b, int slot);
// A map entered on `rules`, in three steps a caller may put its own between (a lobby's teams):
// - the nav for `map` (built when the map is another): false when no bot can walk it, the seats
//   emptied, the slots they held in `gone` (the caller drops them), said in `error` (with
//   `background`, a map found to have no floor later empties them into `BotSeats::gone`);
bool bot_seats_nav(BotSeats& b, const sv::RulesRunner& rules, const sv::MapSpec& map,
                   std::vector<int>& gone, std::string& error);
// - every bot connected again (new rules, or a level's start that seats its players again);
void bot_seats_connect(BotSeats& b, const sv::RulesRunner& rules);
// - every seated bot's fighter made again on the nav.
void bot_seats_remake(BotSeats& b, const sv::RulesRunner& rules);
// The three at once, the bots connected again when `reconnect`.
void bot_seats_enter(BotSeats& b, const sv::RulesRunner& rules, const sv::MapSpec& map,
                     bool reconnect, std::vector<int>& gone, std::string& error);
// Whether the nav the bots play on is built (false while a background nav builds).
bool bot_seats_nav_ready(const BotSeats& b);
// A background nav landed when it is done: every seated bot's fighter made on it, or, when it
// found no floor, every bot emptied into `gone`. True when it landed this call.
bool bot_seats_land(BotSeats& b, const sv::RulesRunner& rules);
// The bots' packets for the next frame (each one `msec` long), appended to `out` in slot order;
// a background nav landed first (`bot_seats_land`), and a bot still waiting on it sends a command
// that stands still.
void bot_seats_packets(BotSeats& b, const sv::RulesRunner& rules, int msec,
                       std::vector<sh::Packet>& out);

}  // namespace shooters_edge::bots
