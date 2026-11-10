// THE DEDICATED COOP SERVER: `shooters_edge_server` with `sv_coop 1` runs the campaign in coop
// (server/campaign.hpp) in place of a match, with nobody's window behind it, and serves it over
// the classic protocol through the coop door (classic_coop.hpp) as the window's coop game is
// served. Clients who connect enter the campaign on a free coop start and are carried from map
// to map; `sv_coopbots` companions (client/coop_party.hpp: 0 to 7) play beside them from the
// start, as the endpoint's local seats every client sees as players; a coop game takes eight
// players, the companions counted. When nobody has joined yet the companions play on alone (they
// never take an exit), so the first player arrives in a running game.
//
// THE GAME: the game folder's single-player program (the packs' own, through the game's adapter:
// client/game_folder.hpp's `load_programs`) on its maps; `sv_map` is the first map (the default,
// `arena`, means the profile's start map); `skill` (0..3) is the monsters' for everyone;
// `sv_friendlyfire`, `sv_friendlypush` and `sv_episodes_done` as in the window's coop mode
// (client/coop_mode.hpp). A folder with no single-player program is refused, naming it.
//
// LISTED AS COOP: the server info a browser's status query reads says `coop 1`, `deathmatch 0`,
// the `skill` and the `map` playing (classic_server.cpp), and the status line says
// `coop=1 skill=… bots=…`.
//
// Offline (no socket: a test, a lab) each frame is the campaign's own frame with the companions'
// moves; on the network it is the endpoint's frame at the network's time.
//
// ge:summary The dedicated coop server: the campaign in coop served on the classic protocol
// ge:keywords dedicated-server coop co-op sv_coop sv_coopbots companions campaign classic_coop_game
// ge:keywords dedicated-coop coop-server listed-coop server-browser status
// ge:skill net shooter
// ge:test tests/shooters_edge_coop_server_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "net/transport.hpp"
#include "server/server.hpp"
#include "shooters_edge/client/coop_party.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::net {

// A map by name for a game with no folder (a test's or a lab's built-in maps); null when none.
using CoopMapSupply = std::unique_ptr<client::LoadedMap> (*)(const std::string& name);

struct CoopGame {
  // What the campaign plays: the folder's content, or a program and maps handed in.
  client::GameContent content;
  const ge::progs::Progs* program = nullptr;
  client::SearchPath files;
  CoopMapSupply supply = nullptr;
  std::string first_map;

  std::unique_ptr<client::LoadedMap> map;  // the map playing: the spec's models point into it
  server::Campaign campaign;
  client::Party party;
  int bots = 0;          // sv_coopbots
  void* door = nullptr;  // the endpoint (classic_coop()), once served
  size_t entered = 0;    // the campaign's history the companions have entered
  std::string hostname;
  uint32_t seed = 1;
  std::string failed;  // why the last map change did not happen ("": it did)
  ~CoopGame();
};

// True when the settings ask for a coop campaign (`sv_coop` 1).
bool coop_asked(const ge::server::ServerConfig& c);
// The coop campaign on its first map with its companions: from the folder under `c.root` (its
// single-player program and maps), or, when `program` is given, that program on the maps
// `supply` hands out. Null with `error` (what, and the next action).
std::unique_ptr<CoopGame> coop_open(const ge::server::ServerConfig& c, std::string& error,
                                    const ge::progs::Progs* program = nullptr,
                                    CoopMapSupply supply = nullptr);
// Served on `port`: the endpoint opened on the campaign, the companions its local seats.
bool coop_serve(CoopGame& g, ge::net::Transport& port, std::string& error);
void coop_frame(CoopGame& g);                      // offline
void coop_net_frame(CoopGame& g, int64_t now_us);  // on the network
int coop_seated(const CoopGame& g);                // companions and joined players
uint64_t coop_hash(const CoopGame& g);
std::string coop_status(const CoopGame& g);

}  // namespace shooters_edge::net
