// SHOOTER'S EDGE'S NATIVE GAME PROGRAM: the rules of a match written in C++ behind the loop's
// program seam (plugins/shooter/program.hpp), until the bytecode VM (M18c, M18p) runs a mod's own.
// Small on purpose: what the loop's test arena needs and no more.
//
// - PLAYERS: put in the server at the arena's spawn (walk, slidebox, 100 health). The post-think
//   fires the weapon held while attack is down and the weapon is ready: impulse 7 picks the
//   ROCKET (flymissile, 1000 units/s along the view, explodes on touch, gone after 5 s), impulse 6
//   the GRENADE (bounce, 600 along the view and 200 up, spinning, explodes after 2.5 s or on a
//   player). Each missile is the program's `newmis`, so the loop runs it 0.05 s ahead.
// - EXPLOSIONS hurt each living player within reach (120 less half the distance, half to the one
//   who fired) and throw him 8 units/s a point; health never drops below 1 (no deaths in the test).
// - THE DOOR goes up when a living player touches its trigger, waits 3 s (touching again restarts
//   the wait) and comes down; a player under it while it comes down blocks it, and it goes back up.
//   Its moves are exact on its own clock: speed, travel time, the think at the end.
// - THE PLAT goes up and down for ever, waiting at each end.
// - THE HEALTH box gives 25 (to 100) to a hurt player who touches it, and is gone.
// Every rule is logged as an event (`Rules::events`) for the lab; nothing reads the wall clock.
//
// ge:summary Shooter's Edge's native game program: players, rockets, grenades, a door, a plat
// ge:keywords game-program rules native deathmatch rocket grenade explosion radius-damage
// ge:keywords knockback door plat trigger health calc-move move-done blocked post-think weapon
// ge:keywords rules_program create_rules spawn_arena_entities events
// ge:skill shooter
// ge:test tests/shooters_edge_match_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/server.hpp"
#include "shooters_edge/sim/arena.hpp"

namespace shooters_edge::sim {

// The program's function numbers (0 is none).
enum Fn : sh::Func {
  kFnNone = 0,
  kFnStartFrame,
  kFnPreThink,
  kFnPostThink,
  kFnClientConnect,
  kFnPutClientInServer,
  kFnRemove,
  kFnRocketTouch,
  kFnGrenadeTouch,
  kFnGrenadeExplode,
  kFnDoorTrigger,
  kFnMoveDone,
  kFnDoorHitTop,
  kFnDoorGoDown,
  kFnDoorHitBottom,
  kFnDoorBlocked,
  kFnPlatGoUp,
  kFnPlatHitTop,
  kFnPlatGoDown,
  kFnPlatHitBottom,
  kFnHealthTouch,
  kFnCount,
};

constexpr uint8_t kImpulseGrenade = 6;
constexpr uint8_t kImpulseRocket = 7;
constexpr float kRocketSpeed = 1000;    // units/s
constexpr float kRocketLife = 5;        // s
constexpr float kRocketReload = 0.8f;   // s
constexpr float kGrenadeSpeed = 600;    // units/s along the view
constexpr float kGrenadeLift = 200;     // units/s up
constexpr float kGrenadeFuse = 2.5f;    // s
constexpr float kGrenadeReload = 0.6f;  // s
constexpr float kBlastReach = 120;      // points at the centre; less half the distance
constexpr float kKnockback = 8;         // units/s a point
constexpr float kHealthGain = 25;

enum class EventKind : uint8_t {
  rocket_fired,
  grenade_thrown,
  grenade_bounce,
  explosion,
  hurt,
  door_opening,
  door_open,
  door_closing,
  door_closed,
  door_blocked,
  plat_up,
  plat_top,
  plat_down,
  plat_bottom,
  health_taken,
};
const char* event_name(EventKind k);

struct GameEvent {
  double time = 0;  // the server's time, s
  EventKind kind = EventKind::rocket_fired;
  int ent = 0, other = 0;
  sh::Vec3f where;
  float amount = 0;
};

constexpr int kMaxEvents = 4096;

// The program's own state: its fields by entity number, and the arena's entities.
struct Rules {
  std::vector<float> attack_finished;
  std::vector<int> weapon;  // kImpulseRocket or kImpulseGrenade
  std::vector<sh::Vec3f> pos1, pos2, dest;
  std::vector<sh::Func> move_done;
  std::vector<int> state;    // a door's or a plat's: 0 at the bottom, 1 rising, 2 top, 3 falling
  std::vector<int> door_of;  // a trigger's door
  std::vector<GameEvent> events;  // reserved to kMaxEvents; more are counted, dropped
  int64_t events_dropped = 0;
  int door = 0, trigger = 0, plat = 0, health = 0;
  const Arena* arena = nullptr;
};

// The program's state for a pool of `max_edicts`, over this arena.
void create_rules(Rules& r, const Arena& arena, int max_edicts);
// The seam the server calls (`Program::call` and the hooks).
sh::Program rules_program(Rules& r);
// The map's own entities: the door, its trigger, the plat, the health box.
void spawn_arena_entities(sh::Server& sv, Rules& r);

}  // namespace shooters_edge::sim
