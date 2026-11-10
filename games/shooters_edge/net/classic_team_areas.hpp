// THE MAP'S AREAS AS A BOT TEAM BELIEVES THEM: safe, lost, contested or unknown, read off the
// team's bots (classic_bot_team.hpp) and the team's own lines, for the team's view to colour the
// map by. Nothing is written back: the bots plan from their own beliefs, never from this.
//
// THE AREAS are the map's place names (its `.loc` points): every nav node belongs to the place
// nearest it, so the floor is cut into the areas players call by name. With no place names, the
// key items' callouts name them (as the bots do).
//
// WHAT MAKES AN AREA, each update (the team's clock: the clients' own):
// - SAFE: one of the team's bots alive in it with no enemy in sight, or a `safe [area]` line;
// - LOST: a `lost [area]` line, an enemy the team sees in it with none of the team there, a death
//   of the team's there (the bots' danger spots: their own deaths, a mate's, a call for help), an
//   `enemy at [area]` line;
// - CONTESTED: a fight in it (a bot of the team in a fight there, the enemy it fights there, a
//   `help [area]` line, a death there), for `kContested_s` after;
// - UNKNOWN: no news of it for `kNews_s`.
// The newer of safe and lost wins; a fight lately overrides both.
//
// THE FLOOR'S LIQUIDS: each node's contents at its feet (water, slime, lava), off the map's hull,
// so the view draws them apart from the areas' colours.
//
// THE FEED: every team line the first bot's client was sent (the team's bots' and its people's),
// with who said it and when, for the view to show.
//
// ge:summary The map's areas as a bot team believes them: safe, lost, contested, unknown; the feed
// ge:keywords team-areas area-state safe lost contested unknown locs place-names team-feed
// ge:keywords liquids water slime lava team_areas_update team_area_state
// ge:skill bots net
// ge:test tests/shooters_edge_bot_team_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooter/locs.hpp"
#include "shooters_edge/net/classic_bot_team.hpp"

namespace shooters_edge::net {

inline constexpr double kContested_s = 5;  // a fight in an area keeps it contested this long
inline constexpr double kNews_s = 20;      // news of an area older than this is no news

enum class AreaState : uint8_t { unknown, safe, lost, contested };
const char* area_state_name(AreaState s);

struct TeamLineSeen {
  double t = 0;
  std::string who, text;
};

struct TeamAreas {
  std::string map;                  // the map the areas were cut for
  std::vector<sh::Loc> points;      // the areas: a name and a spot each
  std::vector<int32_t> node_area;   // by nav node: its area (-1: none)
  std::vector<int8_t> node_liquid;  // by nav node: 0 dry, else the liquid's contents number
  std::vector<double> safe_t, lost_t, fight_t;  // by area: the newest news of each kind
  std::vector<TeamLineSeen> feed;               // the team's lines, oldest first
  size_t prints_read = 0;                       // the first bot's prints taken so far
  int spawncount = -1;
};

// The areas brought up to the team's now. Cut anew when the team's map changes.
void team_areas_update(TeamAreas& a, const BotTeam& t);
// What area `i` is believed at `now`.
AreaState team_area_state(const TeamAreas& a, int32_t i, double now);
// The area a place's name names (-1: none).
int32_t team_area_named(const TeamAreas& a, const std::string& name);

}  // namespace shooters_edge::net
