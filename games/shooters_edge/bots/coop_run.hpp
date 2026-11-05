// A CO-OP RUN: our campaign program (games/shooters_edge/qc/campaign) on the campaign host
// (server/campaign.hpp) with `coop 1` and `teamplay 1`, on the COOP HALL: the test arena with a
// button, coop starts, items and monsters, its door opened only by the button. Slot 0 is the
// LEADER, a scripted player who walks a tour of legs and does nothing else (never shoots, never
// presses a button): the person a companion plays beside. Every other slot is a COMPANION
// (companion.hpp) of a skill (or traits) of its own.
//
// THE TOUR (`kCoopTour`): he waits at the start (the companion grows bored), walks to the shut
// door and waits by it (the button is beside him, out of his way: the companion presses it), walks
// into the east room when it opens and stands there (its monsters wake and come for him: the
// companion defends him), walks back to the west room and waits again.
//
// THE REPORT: per frame where everyone stands, the leader's health, each companion's mode, idle act
// and distance from him; when the button was pressed and the door opened; the monsters killed and
// by whom (a companion's kills are those in its sight when they fell); a digest a frame (the loop's
// state, every player's origin, view and health), so two runs agreeing on every frame replay to the
// bit.
//
// ge:summary A co-op run: our campaign on the coop hall, a scripted leader and companions beside
// ge:keywords coop-run run_coop co-op campaign companion leader tour coop-hall button door
// ge:keywords monsters digest kcooptour coop_hall_entities
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

// A leg of the leader's tour: walk to `to` (none when `wait_s` alone), then stand `wait_s`.
struct TourLeg {
  const char* name;
  double wait_s;
  sh::Vec3f to;
  bool walk;
};
inline constexpr TourLeg kCoopTour[] = {
    {"waits at the start", 9, {-416, 0, 24}, false},
    {"walks to the shut door", 0, {72, 0, 24}, true},
    {"waits by the door", 6, {72, 0, 24}, false},
    {"walks into the east room", 0, {300, 120, 24}, true},
    {"stands among the monsters", 14, {300, 120, 24}, false},
    {"walks back west", 0, {-224, 224, 24}, true},
    {"waits in the west room", 12, {-224, 224, 24}, false},
};

// The coop hall's entities (the arena's models plus the button as "*3").
extern const char* const kCoopHallEntities;
// The hall with a way out: the same, and an exit (`trigger_changelevel` "*4", back to the hall) in
// the east room's north-east corner, behind the door the button alone opens. The loader gives it
// under any name (a campaign's maps for coop_episode.hpp).
extern const char* const kCoopHallExitEntities;
bool coop_hall_exit_map(void* ctx, const std::string& name, server::MapSpec& out,
                        std::string& error);

struct CoopOptions {
  std::vector<sh::AimSkill> skills = {sh::AimSkill::expert};  // one companion each
  std::vector<std::string> traits;  // companion k's traits over its skill's preset (traits.hpp)
  double seconds = 60;              // the run's length (the tour ends sooner: he stands on)
  uint32_t seed = 1;
  int skill = 1;               // the campaign's `skill`
  std::string root = ".";      // the repository's root (the campaign's sources)
  ge::ai::Why* why = nullptr;  // decisions recorded for the first companion
  bool exit = false;           // the hall with its exit (kCoopHallExitEntities): left to him
};

struct CoopMate {
  std::string name;
  sh::AimSkill skill = sh::AimSkill::expert;
  sh::Traits traits;
  std::vector<sh::Vec3f> path;  // a frame a point
  std::vector<float> dist;      // a frame its distance from the leader, units
  std::vector<uint8_t> mode;    // a frame its EscortMode (255 while dead)
  std::vector<uint8_t> act;     // a frame its IdleAct
  std::vector<float> yaw;       // a frame its view's yaw, degrees
  EscortStats escort;
  FighterStats fighter;
  sh::IdleState idle;
  int deaths = 0, kills = 0;
};

struct CoopKill {
  double t = 0;
  int ent = 0;
  sh::Vec3f at;
  int by = -1;  // the companion's slot whose sight it fell in (-1: none saw it)
};

struct CoopReport {
  bool ok = false;
  std::string error;
  std::vector<float> t;  // a frame its time from the start, s
  std::vector<sh::Vec3f> leader;
  std::vector<float> leader_health;
  std::vector<uint8_t> leg;  // a frame the tour's leg
  std::vector<CoopMate> mates;
  std::vector<CoopKill> kills;
  bool exited = false;  // the map changed (only the leader may: he never walks the exit here)
  std::vector<sh::Vec3f> monsters;  // where each monster stood at the start
  sh::Vec3f button_at, lo, hi;
  double button_pressed_at = -1, door_open_at = -1;
  int monsters_total = 0, leader_deaths = 0;
  int64_t faults = 0;
  std::vector<uint64_t> digests;
  uint64_t final_digest = 0;
};

CoopReport run_coop(const CoopOptions& opt);

}  // namespace shooters_edge::bots
