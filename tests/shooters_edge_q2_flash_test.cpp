// Tests: Quake II's muzzle flashes (games/shooters_edge/server/q2_flash.hpp, the window's
// client/q2_flash_fx.hpp). A player's flash values: the shotgun's shot then its pump 0.1 s
// later on the auto channel, the chain gun's three shots in a frame, the machine gun's five
// sounds picked among, the silenced shot at 0.2, a value with no case adding nothing. A monster's:
// each kind's gun picks its value and the value its sound and its gun's place. The window's own
// player hears his flash but draws none (his hand does); another player's is drawn at his gun.
//
// ge:summary Tests: Quake II's muzzle flash values, sounds and the flash drawn at the gun
// ge:keywords q2 muzzle-flash flash-test silenced
// ge:skill shooter
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/q2_flash_fx.hpp"
#include "shooters_edge/server/q2_flash.hpp"

namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

TEST(a_players_flash_plays_the_shot_and_its_tail) {
  std::vector<cl::DemoSound> sounds;
  std::vector<cl::DemoEffect> effects;
  CHECK(cl::q2_flash_fx(2, false, 2, {0, 0, 0}, {0, 0, 0}, false, 0, sounds, effects));
  CHECK(sounds.size() == 2);
  CHECK(std::string(sounds[0].name) == "weapons/shotgf1b.wav" && sounds[0].delay == 0);
  CHECK(sounds[0].channel == 1 && sounds[0].volume == 1);
  CHECK(std::string(sounds[1].name) == "weapons/shotgr1b.wav" && sounds[1].channel == 0);
  CHECK(sounds[1].delay > 0.099f && sounds[1].delay < 0.101f);
  // Drawn at his gun: 18 ahead and 16 to his right (yaw 0 faces +x, right is −y).
  CHECK(effects.size() == 1 && effects[0].name == "te_monster_flash.gun");
  CHECK(effects[0].at.x > 17.9f && effects[0].at.x < 18.1f);
  CHECK(effects[0].at.y > -16.1f && effects[0].at.y < -15.9f);
}

TEST(the_chain_guns_three_shots_and_the_machine_guns_five_sounds) {
  std::vector<cl::DemoSound> sounds;
  std::vector<cl::DemoEffect> effects;
  CHECK(cl::q2_flash_fx(5, false, 2, {}, {}, false, 0, sounds, effects));
  CHECK(sounds.size() == 3 && sounds[2].delay > 0.06f);
  std::vector<std::string> seen;
  for (uint32_t pick = 0; pick < 5; pick++) {
    std::vector<cl::DemoSound> one;
    cl::q2_flash_fx(1, false, 2, {}, {}, false, pick, one, effects);
    seen.push_back(one.at(0).name);
  }
  CHECK(seen[0] == "weapons/machgf1b.wav" && seen[4] == "weapons/machgf5b.wav");
}

TEST(a_silenced_shot_is_quiet_and_the_own_flash_is_heard_not_drawn) {
  std::vector<cl::DemoSound> sounds;
  std::vector<cl::DemoEffect> effects;
  CHECK(cl::q2_flash_fx(7 | sv::kQ2FlashSilenced, false, 1, {}, {}, true, 0, sounds, effects));
  CHECK(sounds.size() == 2 && sounds[0].volume == 0.2f && sounds[1].volume == 0.2f);
  CHECK(effects.empty());
  CHECK(!cl::q2_flash_fx(15, false, 1, {}, {}, false, 0, sounds, effects));  // no case
  CHECK(sounds.size() == 2);
}

TEST(a_monsters_gun_picks_its_value_and_the_value_its_sound) {
  CHECK(sv::q2_monster_flash_value("monster_soldier_light", "blaster") == 39);
  CHECK(sv::q2_monster_flash_value("monster_soldier", "gun") == 41);
  CHECK(sv::q2_monster_flash_value("monster_soldier_ss", "gun") == 43);
  CHECK(sv::q2_monster_flash_value("monster_gunner", "rocket") == 53);
  CHECK(sv::q2_monster_flash_value("monster_jorg", "bfg") == 132);
  CHECK(sv::q2_monster_flash_value("monster_berserk", "gun") == 0);
  const sv::Q2Flash* f = sv::q2_monster_flash(26);
  CHECK(f != nullptr && std::string(f->sounds[0].name) == "infantry/infatck1.wav" && f->puff);
  CHECK(sv::q2_monster_flash(61) != nullptr && sv::q2_monster_flash(61)->sounds[0].name == nullptr);
  CHECK(sv::q2_monster_flash(140) == nullptr && sv::q2_monster_flash(0) == nullptr);
  std::vector<cl::DemoSound> sounds;
  std::vector<cl::DemoEffect> effects;
  // The supertank's guns high on its right: the flash there, its machine gun heard.
  CHECK(cl::q2_flash_fx(64, true, 90, {100, 0, 0}, {0, 0, 0}, false, 0, sounds, effects));
  CHECK(sounds.size() == 1 && std::string(sounds[0].name) == "infantry/infatck1.wav");
  CHECK(effects.size() == 1 && effects[0].at.z == 88 && effects[0].at.x == 132);
}

// The mission packs' flashes, as a pack's game library on a server sends them: The Reckoning's
// ion ripper, blue hyperblaster and phalanx, Ground Zero's flechettes, prox launcher and
// disintegrator; their monsters' guns (the carrier's, the stalker's disintegrator) by value.
TEST(the_mission_packs_flashes_play_their_own_shots) {
  const auto heard = [](int value, bool monster) {
    std::vector<cl::DemoSound> sounds;
    std::vector<cl::DemoEffect> effects;
    CHECK(cl::q2_flash_fx(value, monster, 2, {}, {}, false, 0, sounds, effects));
    return sounds.empty() || sounds[0].name == nullptr ? std::string() : sounds[0].name;
  };
  CHECK(heard(16, false) == "weapons/rippfire.wav");  // the ion ripper
  CHECK(heard(17, false) == "weapons/hyprbf1a.wav");  // the blue hyperblaster
  CHECK(heard(18, false) == "weapons/plasshot.wav");  // the phalanx
  CHECK(heard(30, false) == "weapons/nail1.wav");     // the flechettes
  CHECK(heard(32, false) == "weapons/shotg2.wav");    // the pack's shotgun
  CHECK(heard(34, false) == "weapons/blastf1a.wav");  // the pack's blaster
  CHECK(heard(35, false) == "weapons/disint2.wav");   // the disintegrator
  std::vector<cl::DemoSound> prox;
  std::vector<cl::DemoEffect> effects;
  CHECK(cl::q2_flash_fx(31, false, 2, {}, {}, false, 0, prox, effects));
  CHECK(prox.size() == 2 && std::string(prox[1].name) == "weapons/proxlr1a.wav");
  CHECK(heard(138, true) == "infantry/infatck1.wav");  // the carrier's left gun
  CHECK(heard(148, true) == "weapons/disint2.wav");    // a monster's disintegrator
  CHECK(sv::q2_monster_flash(144) != nullptr && sv::q2_monster_flash(164) != nullptr);
}
