// CO-OP AS A MODE OF THE WINDOW: the settings a player sets once (in a cfg, on the command line or
// in the console) that make the campaign a cooperative game, and keep it so from game to game:
//
// - `sv_coop 1` starts a coop game on the start map (at the game's start when a cfg sets it, at
//   once when the console does) with `sv_coopbots` companions beside the player (0 to 7: eight
//   players at most); `skill` is the monsters' for everyone;
// - `sv_friendlyfire` (0: teammates are spared) and `sv_friendlypush` (0: a spared teammate's blow
//   does not push either, so no rocket boost off a friend) are the window's, kept in its settings
//   and written to the campaign's server whenever they differ there;
// - `sv_episodes_done` (a bit an episode, as its rune's) is what the player has finished, alone or
//   in coop: the start map opens each next episode once the one before is done and shuts a done
//   one (server/campaign_episodes.hpp), the host sets the bit when an episode is left with its
//   rune, and the window keeps it from game to game (and new game to new game). A new single-
//   player game starts with it too.
//
// ge:summary Co-op as a window mode: sv_coop, the bots, friendly fire and push, episodes unlocked
// ge:keywords coop co-op sv_coop sv_coopbots sv_friendlyfire sv_friendlypush sv_episodes_done
// ge:keywords friendly-fire rocket-boost episode-unlock coop_frame
// ge:skill shooter bots
// ge:test tests/shooters_edge_coop_mode_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "console/console.hpp"
#include "shooters_edge/client/coop_party.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::client {

inline constexpr int kCoopPlayers = kPartyMost + 1;  // a coop game's players at most

struct CoopMode {
  ge::console::Cvar* coop = nullptr;
  ge::console::Cvar* bots = nullptr;
  ge::console::Cvar* friendly_fire = nullptr;
  ge::console::Cvar* friendly_push = nullptr;
  ge::console::Cvar* episodes = nullptr;
  ge::console::Cvars* window = nullptr;  // the window's registry
  uint32_t coop_seen = 0;                // `sv_coop`'s revision last acted on
};

// The window's coop settings, archived with the player's.
void add_coop_cvars(ge::console::Console& con, CoopMode& m);

// The console lines a new coop game starts with (deathmatch 0, coop 1, the window's settings).
void coop_game_lines(const CoopMode& m, ListenOptions& opt);

// Once a frame: `sv_coop` turned on starts a coop game on `start_map` (`next_map` set) when the
// game has a campaign; the window's friendly-fire settings written to the campaign's server where
// they differ; the episodes done merged both ways in any campaign.
void coop_frame(CoopMode& m, Listen& listen, bool has_campaign, const std::string& start_map,
                std::string& next_map);

}  // namespace shooters_edge::client
