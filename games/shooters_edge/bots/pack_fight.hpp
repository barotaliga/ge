// A DEATHMATCH ON A GAME'S OWN MAP AND PROGRAM: the campaign's server (server/campaign.hpp) loads a
// map from the player's packs under the packs' own program (a mission pack's progs, laid over the
// base game's) with `deathmatch` and `teamplay` set as asked, and each client slot is a fighter
// (fighter.hpp) on the map's nav (shooterbots/map_nav.hpp, built from the deathmatch starts),
// sending one packet a server frame like any client. It ends when the program's time or frag limit
// starts the intermission or the frame budget runs out.
//
// WHAT IT IS FOR: a pack's deathmatch played by bots on its own rules, to show that the map and the
// program run a match (frags, deaths, every item taken, no fault). It is the fight lab's runner
// (fight_run.hpp) on a real map; the fight lab keeps the test arena and the baseline mod.
//
// THE REPORT: run_fight's (fight_run.hpp), and every item taken by its class name, the program's
// last fault, and whether the intermission came.
//
// ge:summary A deathmatch of fighters on a pack's own map and program (a mission pack's dm map)
// ge:keywords pack-fight run_pack_fight deathmatch teamplay fighters mission-pack hipdm1 ctf1
// ge:keywords pack-program frags items-taken intermission
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#pragma once

#include <map>
#include <string>
#include <vector>

#include "shooters_edge/bots/fight_run.hpp"
#include "shooters_edge/server/campaign.hpp"

namespace shooters_edge::bots {

struct PackFightOptions {
  server::MapLoader load = nullptr;  // the maps by name (the packs')
  void* load_ctx = nullptr;
  const ge::progs::Progs* progs = nullptr;  // the packs' program
  const sb::BotArsenal* arsenal = nullptr;  // what the fighters know (none: Quake's own)
  std::string map;
  bool registered = false;   // the content is the full game
  bool wide_models = false;  // a game's own content: its movers sized as the window does
  std::vector<sb::AimSkill> skills = {sb::AimSkill::expert, sb::AimSkill::average,
                                      sb::AimSkill::average, sb::AimSkill::novice};
  int deathmatch = 1;
  int teamplay = 0;   // with teamplay, fighter k plays for team k % 2 (its colour)
  int timelimit = 0;  // minutes (0: none)
  int fraglimit = 0;
  int frames = 72 * 120;  // host frames of 1/72 s (two minutes)
  uint32_t seed = 1;
};

struct PackFightReport {
  FightReport fight;
  std::map<std::string, int> taken;  // every item taken, by its class name
  bool intermission = false;         // the program's limit ended the match
  std::string last_fault;
  std::vector<sh::Vec3f> nodes;  // the nav's floor (a picture's)
};

PackFightReport run_pack_fight(const PackFightOptions& opt);

}  // namespace shooters_edge::bots
