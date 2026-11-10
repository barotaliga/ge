// THE EPISODES IN ORDER: a campaign's episodes open one at a time, each once the one before it is
// done, and a done episode stays shut; what the player has done is kept from game to game.
//
// - **Done** is a bit an episode (1, 2, 4, 8: its rune's bit in the program's `serverflags`). The
//   server's `sv_episodes_done` holds it; at every map's start the host adds the runes the game
//   carried in (an episode's last map left with its rune), so a finished episode is recorded the
//   moment its way out is taken. The window archives the setting (client/coop_mode.hpp), so it
//   outlives the game, the session and a new game.
// - **On the start map** the program's `serverflags` gains every done episode's bit before the
//   world spawns, so the program's own gates stand as the runes say: a done episode's gate shuts
//   its hall, and with all four done the way to the last map opens. That holds for a new game
//   too, which in the classic rules starts with no runes.
// - **The bars**: the start map keeps episodes 2 to 4 behind bars (doors a full-game-only trigger
//   opens when a player walks up to them, porting/shooters-edge/campaign.md § Episodes). The host
//   rewrites the start map's entity text before it spawns: the full-game trigger in front of an
//   episode not yet open becomes a plain message trigger with no targets, so its bars stay shut
//   and a player walking up is told which episode opens them. Episodes 2 and 3 are each barred
//   by two doors two units apart, one named by the trigger and one not; the classic engine reads
//   brush models a unit larger on every side, so there the two touch and open together, but
//   here the unnamed one would open on a player's approach. So an unnamed door within 4 units of
//   a shut bar takes its name and waits to be fired too. The trigger's episode is the one
//   whose way in (a level change to the episode's first map) is nearest to it. Entities are only
//   changed, never added or taken away, so every entity keeps its number (saves rely on it). With
//   the shareware packs nothing is rewritten: those triggers keep their own message.
//
// Any program gets the order, the packs' own as much as ours, because it lives in the map's
// entities and the program's `serverflags`, never in the program's code.
//
// ge:summary Episodes in order on the start map: done kept, bars shut until the one before is done
// ge:keywords episodes episode-order episode-unlock sv_episodes_done start-map bars runes
// ge:keywords serverflags trigger_onlyregistered func_episodegate func_bossgate campaign
// ge:skill shooter progs
// ge:test tests/shooters_edge_campaign_episodes_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

// A game's episodes: its start map and each episode's first map, in order (episode k's bit is
// 1 << k). An empty start map keeps no order.
struct EpisodeOrder {
  std::string start_map;
  std::vector<std::string> first_maps;
};

constexpr uint32_t kEpisodeBits = 15;  // the four runes' bits in `serverflags`

// The episodes a player may start now: each not done whose episode before is done (the first
// needs none). A bit an episode.
uint32_t episodes_open(uint32_t done, int episodes);

// What the start map's rewrite did, for the labs and the tests.
struct EpisodeBars {
  std::vector<int> shut;       // entity indices (0 the world) of the triggers made bars
  std::vector<int> episode;    // each one's episode (0-based)
  std::vector<int> left_open;  // entity indices of full-game triggers left as they were
  std::vector<int> named;      // entity indices of unnamed bars given the shut ones' name
};

// `map.entities` with the bars in front of every episode not open kept shut (above). False with
// `error` when the entity text does not parse (the map is then left as it was). `registered`
// false leaves the text as it is.
bool shut_episode_bars(MapSpec& map, const EpisodeOrder& order, uint32_t done, bool registered,
                       EpisodeBars& out, std::string& error);

// The message a shut episode's bars give: which episode must be done first.
std::string episode_bars_message(int episode);

}  // namespace shooters_edge::server
