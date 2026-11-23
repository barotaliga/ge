// Quake's episodes and levels (episodes.hpp).
//
// ge:summary Quake's episode and level lists with their long names, and which apply to the packs
// ge:keywords episode level episodes_for level_of
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/episodes.hpp"

#include <algorithm>

namespace shooters_edge::menu {

namespace {

constexpr Level kStart[] = {{"start", "Entrance"}};
constexpr Level kE1[] = {{"e1m1", "Slipgate Complex"},    {"e1m2", "Castle of the Damned"},
                         {"e1m3", "The Necropolis"},      {"e1m4", "The Grisly Grotto"},
                         {"e1m5", "Gloom Keep"},          {"e1m6", "The Door To Chthon"},
                         {"e1m7", "The House of Chthon"}, {"e1m8", "Ziggurat Vertigo"}};
constexpr Level kE2[] = {{"e2m1", "The Installation"},   {"e2m2", "Ogre Citadel"},
                         {"e2m3", "Crypt of Decay"},     {"e2m4", "The Ebon Fortress"},
                         {"e2m5", "The Wizard's Manse"}, {"e2m6", "The Dismal Oubliette"},
                         {"e2m7", "Underearth"}};
constexpr Level kE3[] = {{"e3m1", "Termination Central"}, {"e3m2", "The Vaults of Zin"},
                         {"e3m3", "The Tomb of Terror"},  {"e3m4", "Satan's Dark Delight"},
                         {"e3m5", "Wind Tunnels"},        {"e3m6", "Chambers of Torment"},
                         {"e3m7", "The Haunted Halls"}};
constexpr Level kE4[] = {{"e4m1", "The Sewage System"},    {"e4m2", "The Tower of Despair"},
                         {"e4m3", "The Elder God Shrine"}, {"e4m4", "The Palace of Hate"},
                         {"e4m5", "Hell's Atrium"},        {"e4m6", "The Pain Maze"},
                         {"e4m7", "Azure Agony"},          {"e4m8", "The Nameless City"}};
constexpr Level kEnd[] = {{"end", "Shub-Niggurath's Pit"}};
constexpr Level kDm[] = {{"dm1", "Place of Two Deaths"}, {"dm2", "Claustrophobopolis"},
                         {"dm3", "The Abandoned Base"},  {"dm4", "The Bad Place"},
                         {"dm5", "The Cistern"},         {"dm6", "The Dark Zone"}};
constexpr Episode kBase[] = {{"Welcome to Quake", kStart},  {"Doomed Dimension", kE1},
                             {"Realm of Black Magic", kE2}, {"Netherworld", kE3},
                             {"The Elder World", kE4},      {"Final Level", kEnd},
                             {"Deathmatch Arena", kDm}};

constexpr Level kHipStart[] = {{"start", "Command HQ"}};
constexpr Level kHip1[] = {{"hip1m1", "The Pumping Station"},
                           {"hip1m2", "Storage Facility"},
                           {"hip1m3", "The Lost Mine"},
                           {"hip1m4", "Research Facility"},
                           {"hip1m5", "Military Complex"}};
constexpr Level kHip2[] = {{"hip2m1", "Ancient Realms"}, {"hip2m2", "The Black Cathedral"},
                           {"hip2m3", "The Catacombs"},  {"hip2m4", "The Crypt"},
                           {"hip2m5", "Mortum's Keep"},  {"hip2m6", "The Gremlin's Domain"}};
constexpr Level kHip3[] = {{"hip3m1", "Tur Torment"},
                           {"hip3m2", "Pandemonium"},
                           {"hip3m3", "Limbo"},
                           {"hip3m4", "The Gauntlet"}};
constexpr Level kHipEnd[] = {{"hipend", "Armagon's Lair"}};
constexpr Level kHipDm[] = {{"hipdm1", "The Edge of Oblivion"}};
constexpr Episode kHipnotic[] = {{"Scourge of Armagon", kHipStart}, {"Fortress of the Dead", kHip1},
                                 {"Dominion of Darkness", kHip2},   {"The Rift", kHip3},
                                 {"Final Level", kHipEnd},          {"Deathmatch Arena", kHipDm}};

constexpr Level kRogueStart[] = {{"start", "Split Decision"}};
constexpr Level kR1[] = {{"r1m1", "Deviant's Domain"},    {"r1m2", "Dread Portal"},
                         {"r1m3", "Judgement Call"},      {"r1m4", "Cave of Death"},
                         {"r1m5", "Towers of Wrath"},     {"r1m6", "Temple of Pain"},
                         {"r1m7", "Tomb of the Overlord"}};
constexpr Level kR2[] = {{"r2m1", "Tempus Fugit"},      {"r2m2", "Elemental Fury I"},
                         {"r2m3", "Elemental Fury II"}, {"r2m4", "Curse of Osiris"},
                         {"r2m5", "Wizard's Keep"},     {"r2m6", "Blood Sacrifice"},
                         {"r2m7", "Last Bastion"},      {"r2m8", "Source of Evil"}};
constexpr Level kRogueDm[] = {{"ctf1", "Division of Change"}};
constexpr Episode kRogue[] = {{"Introduction", kRogueStart},
                              {"Hell's Fortress", kR1},
                              {"Corridors of Time", kR2},
                              {"Deathmatch Arena", kRogueDm}};

bool holds(std::span<const std::string> maps, std::string_view map) {
  return std::any_of(maps.begin(), maps.end(), [&](const std::string& m) { return m == map; });
}

}  // namespace

std::vector<Episode> episodes_for(std::span<const std::string> maps) {
  std::span<const Episode> list = kBase;
  if (holds(maps, "hip1m1"))
    list = kHipnotic;
  else if (holds(maps, "r1m1"))
    list = kRogue;
  std::vector<Episode> out;
  for (const Episode& e : list)
    if (holds(maps, e.levels.front().map)) out.push_back(e);
  return out;
}

bool team_modes(std::span<const std::string> maps) {
  return !holds(maps, "hip1m1") && holds(maps, "r1m1");
}

bool level_of(std::span<const Episode> episodes, std::string_view map, int* episode, int* level) {
  for (size_t e = 0; e < episodes.size(); e++)
    for (size_t l = 0; l < episodes[e].levels.size(); l++)
      if (episodes[e].levels[l].map == map) {
        *episode = static_cast<int>(e);
        *level = static_cast<int>(l);
        return true;
      }
  return false;
}

}  // namespace shooters_edge::menu
