// WHAT THE SCORES KEY SHOWS, read off the game playing (menu/scoreboard.hpp,
// menu/level_screens.hpp): a match's players off the listen server's clients (each name, team and
// colours its user info's `name`, `team`, `topcolor` and `bottomcolor`, `*spectator` a
// spectator; the frags its entity's `frags` field; the ping the server's; the minutes since it
// connected; the local player slot 0), `teamplay` the server's cvar, the map's clock since the
// player entered; and a campaign's statistics (the kills, the secrets, the time on the map).
//
// WHICH SCREEN (`scores_screen`): in a campaign, the level's own ending first, else the solo
// scorebar while the key is held; in a match, the players' board while the key is held, and the
// teams' board when the player is dead (the players' board with teamplay off).
//
// ge:summary The scoreboard's rows and the level's statistics read off the listen server
// ge:keywords scores_of level_stats_of scores_screen scoreboard frags ping teamplay spectator
// ge:keywords topcolor bottomcolor tab showscores dead campaign match
// ge:skill ui shooter
// ge:test tests/shooters_edge_scores_test.cpp
#pragma once

#include <cstdint>

#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/menu/level_screens.hpp"
#include "shooters_edge/menu/scoreboard.hpp"

namespace shooters_edge::client {

// A match's board: every client playing or watching, by slot.
menu::Scoreboard scores_of(const Listen& l);
// A campaign's statistics now: the time on the map frozen at the level's end.
menu::LevelStats level_stats_of(const Listen& l);

enum class ScoresScreen : uint8_t { none, players, teams, solo };
// What the scores key (`held`) and the game playing show.
ScoresScreen scores_screen(const Listen& l, bool held);

}  // namespace shooters_edge::client
