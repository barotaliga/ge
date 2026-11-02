// A DEATHMATCH OF FIGHTERS on the baseline mod: the mod compiled from its QC sources
// (server/baseline_mod.hpp) runs on the mod server over the test arena with its door, its plat and
// every item, "deathmatch 1", and each client slot is a fighter (fighter.hpp) with its own aim
// skill, sending one packet a server frame (its three newest commands) like any client. The nav
// graph is built once off the arena's world with the door and the plat as its movers.
//
// THE REPORT: each player's path, frags, deaths, shots, pickups, weapon switches and the commands
// spent in each mode and with each weapon; where each death happened; the frag log; and a digest a
// frame (the mod server's loop state folded with every player's origin), so a run that agrees on
// every frame replays to the bit.
//
// ge:summary A deathmatch of fighters on the baseline mod over the arena, and what it scored
// ge:keywords fight-run run_fight deathmatch fighters frags deaths baseline-mod arena report
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

struct FightOptions {
  std::vector<sh::AimSkill> skills = {sh::AimSkill::expert, sh::AimSkill::average,
                                      sh::AimSkill::average, sh::AimSkill::novice};
  std::vector<sh::Temper> tempers;  // fighter k's temperament (missing: balanced)
  int frames = 9000;                // server frames of 13 ms (about two minutes)
  uint32_t seed = 1;                // the mod's generator; fighter k's hands seed + 31 k
  std::string root = ".";           // the repository's root (the mod's sources)
  ge::ai::Why* why = nullptr;       // decisions recorded for one fighter (its slot is the agent)
};

struct FightPlayer {
  std::string name;
  sh::AimSkill skill = sh::AimSkill::average;
  sh::Temper temper = sh::Temper::balanced;
  std::vector<sh::Vec3f> path;  // a frame a point
  std::vector<uint8_t> mode;    // a frame its FightMode (255 while dead)
  std::vector<float> frags_at;  // a frame its frags
  float frags = 0;
  int deaths = 0;
  FighterStats stats;
};

struct FightDeath {
  int frame = 0;
  int victim = 0;  // the slot
  sh::Vec3f at;
};

// An item as the fighters know it (the map's text matched to the server's entity), for a picture.
struct FightItem {
  std::string name;
  sh::Vec3f at;
};

struct FightReport {
  bool ok = false;
  std::string error;  // why it did not run, with the next step
  int frames = 0;
  std::vector<FightPlayer> players;
  std::vector<FightDeath> deaths;
  std::vector<std::string> frag_log;
  std::vector<uint64_t> digests;
  uint64_t final_digest = 0;
  int64_t faults = 0;
  std::vector<FightItem> items;
  sh::Vec3f lo, hi;  // the nav grid's bounds (for a picture)
};

FightReport run_fight(const FightOptions& opt);

}  // namespace shooters_edge::bots
