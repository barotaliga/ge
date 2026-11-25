// THE LOBBY ON THE LISTEN SERVER: the lobby's rules (server/lobby.hpp) played in the player's own
// process, with BOTS in the seats the player fills. The listen (listen.hpp) owns one when
// `ListenOptions::lobby` is on, and every map then opens in WARMUP:
//
// - the player (slot 0, the leader) and every bot are on the map at once, free to roam and fight;
//   the server's frag and time limits are held at 0 so nothing ends the warmup but the lobby;
// - `addbot [novice|average|expert]` seats a fighter (shooterbots/fighter.hpp: it reads only what a
//   client is sent) in the lowest free slot, ready as it joins, on the smaller team; `removebot`
//   frees the highest bot seat. A bot's packets are made each frame like the player's and run in
//   the same server frame after his, in slot order;
// - when the countdown runs out the listen restarts the map with every seat on it, the limits as
//   the settings set them: the match. Whatever warmup scored is gone with the old server.
//
// The bots themselves are bots/bot_seats.hpp's, every game's: their nav is built once a map the
// first time a bot is seated, and kept for the restart.
//
// Deterministic: the bots' hands are seeded off the listen's seed and their seat.
//
// ge:summary The lobby on the listen server: bots seated, warmup's limits held, the match restart
// ge:keywords lobby room listen-server addbot removebot bots fighter warmup match-restart
// ge:keywords nav-graph seats room_enter room_packets
// ge:skill shooter bots
// ge:test tests/shooters_edge_lobby_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "shooterbots/aim.hpp"
#include "shooters_edge/bots/bot_seats.hpp"
#include "shooters_edge/hud/lobby_panel.hpp"
#include "shooters_edge/server/announce.hpp"
#include "shooters_edge/server/lobby.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;
namespace sv = shooters_edge::server;

struct Room {
  sv::Lobby lobby;
  sv::Announcer announcer;  // the lobby's words as the server's events (server/announce.hpp)
  bots::BotSeats seats;  // the bots (bots/bot_seats.hpp): `seats.held` a dedicated server's clients
};

// A fighter of `skill` seated in the lowest free slot of `s` (playing `map`) and the lobby, under
// the server's player limit and outside `held`; its slot, or -1 with `error` (the server full, or
// no way to walk on the map).
int room_add_bot(Room& r, const sv::RulesRunner& s, const sv::MapSpec& map, sb::AimSkill skill,
                 std::string& error);
// The highest bot seat freed; false when there is none.
bool room_remove_bot(Room& r, const sv::RulesRunner& s);
// The seats put on a new server `s` for `map` (slot 0, the player, already connected): every bot
// connected and its fighter made again, every seat's team applied, and in warmup the limits held.
void room_enter(Room& r, const sv::RulesRunner& s, const sv::MapSpec& map);
// The bots' packets for the next frame (each one `msec` long), appended to `out` in slot order.
void room_packets(Room& r, const sv::RulesRunner& s, int msec, std::vector<sh::Packet>& out);
// What the lobby's banner and screen show of the room at server time `now`, slot `me` the
// player's (the frags left 0: listen.hpp's `listen_lobby_view` reads them off the server).
hud::LobbyView lobby_view(const Room& r, double now, int me);
// A skill named `novice`, `average` or `expert`; false for anything else.
bool skill_named(const std::string& word, sb::AimSkill& out);
// A co-op companion's words after `addbot`, in any order: a skill (`skill_named`) and a role,
// `escort` (keeps at the player's shoulder) or `helper` (works the level near the people). The
// first word neither is, in `bad`; false then.
bool companion_words(const std::string& words, sb::AimSkill& skill, bool& helper, std::string& bad);

}  // namespace shooters_edge::client
