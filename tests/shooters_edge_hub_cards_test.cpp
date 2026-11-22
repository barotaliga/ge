// games/shooters_edge/hub/cards.hpp: the game half of the hub's match cards. A record names its
// classic game by its `*game` key (Quake's when it names none or one not known); a map's still is
// taken from its first intermission spot by its `mangle` (Quake) or `angles` (Quake II), else the
// first deathmatch spawn at the eye's height looking its `angle`, else the player's start, else
// the world's middle.
//
// ge:summary Test: the hub cards' game half (the record's game, the still's camera)
// ge:keywords test shooters-edge hub match-card still intermission mangle angles spawn game-of
// ge:skill net testing
#include "shooters_edge/hub/cards.hpp"

#include "check.hpp"

namespace sh = shooters_edge::hub;

namespace {

ge::brush::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  ge::brush::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

}  // namespace

TEST(game_of_reads_the_records_key) {
  ge::net::Status s;
  CHECK(sh::hub_game_of(s) == "q1");
  s.keys = {{"fraglimit", "20"}, {"*game", "q2"}};
  CHECK(sh::hub_game_of(s) == "q2");
  s.keys = {{"*game", "../q9"}};
  CHECK(sh::hub_game_of(s) == "q1");
}

TEST(still_view_prefers_the_intermission_spot) {
  ge::brush::Map m;
  m.entities.push_back(entity({{"classname", "worldspawn"}}));
  m.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "10 20 30"}, {"angle", "90"}}));
  sh::StillView v = sh::still_view(m);
  CHECK(v.from == "info_player_deathmatch" && v.origin[2] == 52 && v.yaw == 90 && v.pitch == 0);
  m.entities.push_back(
      entity({{"classname", "info_intermission"}, {"origin", "1 2 3"}, {"mangle", "15 180 0"}}));
  m.entities.push_back(
      entity({{"classname", "info_intermission"}, {"origin", "7 7 7"}, {"mangle", "0 0 0"}}));
  v = sh::still_view(m);
  CHECK(v.from == "info_intermission" && v.origin[0] == 1 && v.origin[2] == 3);
  CHECK(v.pitch == 15 && v.yaw == 180);
  ge::brush::Map q2;
  q2.entities.push_back(entity(
      {{"classname", "info_player_intermission"}, {"origin", "5 6 7"}, {"angles", "20 270 0"}}));
  v = sh::still_view(q2);
  CHECK(v.from == "info_player_intermission" && v.pitch == 20 && v.yaw == 270 && v.origin[2] == 7);
  CHECK(sh::still_view(ge::brush::Map{}).from.empty());
}
