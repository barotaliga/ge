// THE GUNS' SPENT SHELLS (ce_shells, the modern look): each classic game's guns that throw out a
// case as they fire, and when and how, as the game's own data; the cases themselves (their kinds,
// model and sound) are classic enhance's (classicenhance/shells.hpp) and their flight the engine's
// thrown debris (detail/debris.hpp).
//
// - WHO THROWS. The player's own gun (the hand's shot: `held_cases`, by the held weapon's model or,
//   for Shooter's Edge's own set, its weapon); another player in Quake (his gun's flash on a
//   shotgun's firing frame, `shotatt*`) and a monster with a shotgun (its flash; Quake's soldier);
//   in Quake II every gun's flash value, a player's or a monster's (its shotguns throw hulls, its
//   machine guns brass, the big guns of its tanks and bosses heavy brass).
// - WHEN. A pump or a break throws its hulls a moment after the shot (`Ejection::delay_s`); a
//   machine gun at once. The chain gun's flash value says how many rounds left it.
// - WHERE. From the gun's port, a little back from its muzzle and to its right, out to the right
//   and up with the shooter's own speed; each case on its own seed, so a burst sprays. They fall
//   under the game's gravity, bounce off the map's faces (the marks' receivers), clatter by what
//   they land on (the face's texture's class), lie for kCaseLife and fade.
//
// ge:summary The guns' spent shells: each game's ejecting guns, thrown, drawn and heard
// ge:keywords shells spent-case casing eject ejection ce_shells shotgun machinegun chaingun
// ge:keywords held_cases step_cases case-sound case-model angles_of_turn
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_shells_test.cpp
#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "classicenhance/shells.hpp"
#include "core/quat.hpp"
#include "detail/debris.hpp"
#include "shooter/vec.hpp"

namespace shooters_edge::client {

using ge::classicenhance::Casing;

// How a gun throws its spent cases.
struct Ejection {
  Casing casing = Casing::hull;
  int count = 1;        // cases a shot
  float delay_s = 0;    // after the shot (a pump's stroke, a break's opening)
  float gap_s = 0.03f;  // between one shot's cases
  // Its port from the muzzle (the held gun) or the gun's point (another's), map units: back along
  // the gun, to its right, below it.
  float back_u = 8, right_u = 2, down_u = 1;
  // The way a case leaves, m/s: to the gun's right, up, back.
  float right_mps = 1.6f, up_mps = 1.6f, back_mps = 0.3f;
};

// A held weapon's model's ejection (Quake's and Quake II's own held weapons by their files); null
// for a gun that throws none.
const Ejection* held_ejection(std::string_view model);
// Shooter's Edge's own set's, by its weapon's index (view::Weapon); null for none.
const Ejection* own_set_ejection(int weapon);
// Quake: another player's gun by his figure's frame's name as it flashes (a shotgun's firing
// frames); a monster's by its model's file.
const Ejection* q1_player_ejection(std::string_view frame);
const Ejection* q1_monster_ejection(std::string_view model);
// Quake II: a flash value's, a player's (the silenced bit stripped) or a monster's; null for none.
const Ejection* q2_flash_ejection(int value, bool monster);

// The pool's size, the gravity the cases fall under (the classic games' 800 units a second
// squared), how long a case lies before it fades, and the landings heard in one frame at most.
inline constexpr uint32_t kCases = 96;
inline constexpr double kCaseGravity = 20;
inline constexpr int kCaseSoundsAFrame = 6;
// A drawn case's entity: this plus its slot (its own last pose for the models' draw).
inline constexpr int kCaseEntity = 1 << 23;

// A shot's cases waiting for their moment (a pump's) where the gun was.
struct DueCase {
  double due = 0;
  const Ejection* e = nullptr;
  double port[3] = {0, 0, 0};  // engine m
  double fwd[3] = {1, 0, 0}, up[3] = {0, 1, 0};
  bool held = false;  // the player's own: thrown from the hand's gun as it is then
};

// Everything the shells keep between frames.
struct Cases {
  ge::detail::DebrisPool pool;
  std::vector<DueCase> due;
  std::unordered_map<int, double> fired;  // each shooter's last throw (a flash held for frames)
  std::size_t effects_seen = 0;           // of FxScene::effects (Quake II's flashes)
  const void* classed = nullptr;          // the map the texture classes are for
  std::vector<uint8_t> texture_class;     // each texture's surface class (detail/surfaces.hpp)
  double carry[3] = {0, 0, 0};            // the player's own speed, engine m/s
  uint32_t seed = 0x5be11u;
};

// A case thrown now from `port` (engine m) of a gun pointing `fwd` with its top `up` (unit), at
// `carry` m/s more (the shooter's own speed).
void throw_case(Cases& c, const Ejection& e, const double port[3], const double fwd[3],
                const double up[3], const double carry[3], double now_s);

// A shot's cases owed: each at its moment (`e.delay_s`, then `e.gap_s` apart).
void owe_cases(Cases& c, const Ejection& e, const double port[3], const double fwd[3],
               const double up[3], bool held, double now_s);

// Where a gun at `at` (engine m) pointing `fwd` with its top `up` throws its cases from (its
// ejection port).
void case_port(const Ejection& e, const double at[3], const double fwd[3], const double up[3],
               double out[3]);

// The model angles (degrees: pitch, yaw, roll, as a vertex-animated model's) that turn its body
// axes as `turn` does in the engine's frame (models::rotation_of's inverse).
ge::shooter::Vec3f angles_of_turn(const ge::core::Quat& turn);

// The name the sounds are asked for one by (classicenhance's `casing_sound_name`), kept for the
// program's life as DemoSound::name asks.
const char* case_sound_name(Casing c, ge::detail::SurfaceClass on, int variant);

}  // namespace shooters_edge::client
