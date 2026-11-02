// THE LOBBY ON THE LISTEN SERVER: the lobby's rules (server/lobby.hpp) played in the player's own
// process, with BOTS in the seats the player fills. The listen (listen.hpp) owns one when
// `ListenOptions::lobby` is on, and every map then opens in WARMUP:
//
// - the player (slot 0, the leader) and every bot are on the map at once, free to roam and fight;
//   the server's frag and time limits are held at 0 so nothing ends the warmup but the lobby;
// - `addbot [novice|average|expert]` seats a fighter (bots/fighter.hpp: it reads only what a
//   client is sent) in the lowest free slot, ready as it joins, on the smaller team; `removebot`
//   frees the highest bot seat. A bot's packets are made each frame like the player's and run in
//   the same server frame after his, in slot order;
// - when the countdown runs out the listen restarts the map with every seat on it, the limits as
//   the settings set them: the match. Whatever warmup scored is gone with the old server.
//
// The bots' NAV GRAPH is built once a map (shooter/nav.hpp, off the map's world model; on the
// built-in arena its door and plat are its movers, as the `se-bots` lab's) the first time a bot
// is seated, and kept for the restart.
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

#include "shooter/aim.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/hud/lobby_panel.hpp"
#include "shooters_edge/server/lobby.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

struct RoomBot {
  bots::Fighter fighter;
  sh::AimSkill skill = sh::AimSkill::average;
  std::string name;     // its player's name on the server
  sh::MoveCmd cmds[3];  // the three newest, oldest first
  uint32_t sequence = 0;
};

struct Room {
  sv::Lobby lobby;
  std::array<std::unique_ptr<RoomBot>, sv::kLobbySeats> bots;  // by slot (null: no bot)
  std::unique_ptr<sh::NavGraph> graph;                         // the bots' map's
  std::unique_ptr<sh::MoveWorld> world;                        // its solids (the hop's look)
  std::string graph_of;                                        // the map it was built for
  uint32_t seed = 1;
  int bots_named = 0;               // bots ever seated (their names count up)
  std::vector<sh::Packet> packets;  // a frame's, the player's first (reused)
};

// A fighter of `skill` seated in the lowest free slot of `s` (playing `map`) and the lobby; its
// slot, or -1 with `error` (the server full, or no way to walk on the map).
int room_add_bot(Room& r, sv::QcServer& s, const sv::MapSpec& map, sh::AimSkill skill,
                 std::string& error);
// The highest bot seat freed; false when there is none.
bool room_remove_bot(Room& r, sv::QcServer& s);
// The seats put on a new server `s` for `map` (slot 0, the player, already connected): every bot
// connected and its fighter made again, every seat's team applied, and in warmup the limits held.
void room_enter(Room& r, sv::QcServer& s, const sv::MapSpec& map);
// The bots' packets for the next frame (each one `msec` long), appended to `out` in slot order.
void room_packets(Room& r, const sv::QcServer& s, int msec, std::vector<sh::Packet>& out);
// What the lobby panel shows of the room at server time `now`, slot `me` the player's.
hud::LobbyView lobby_view(const Room& r, double now, int me, bool roster);
// A skill named `novice`, `average` or `expert`; false for anything else.
bool skill_named(const std::string& word, sh::AimSkill& out);

}  // namespace shooters_edge::client
