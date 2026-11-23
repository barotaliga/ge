// QUAKE II'S MONSTERS' INNER WORKINGS, shared by the files that run them: q2_monsters.cpp (the
// frame, senses, choosing, death), q2_monster_shots.cpp (aiming and every kind of shot),
// q2_monster_medic.cpp (raising the dead) and each mission pack's monsters
// (q2_reckoning_monsters.cpp), so a pack's kinds reuse the base game's helpers instead of copying
// them. Not for anything outside the monsters: the rest of the rules call q2_monsters.hpp.
//
// ge:summary Quake II's monsters' shared helpers: foes, sight, the step move, aiming, attacks
// ge:keywords q2 monsters monster-ai monster-helpers foe range aim shoot begin-attack
// ge:skill shooter
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "classic/q2_move.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server::monster_ai {

constexpr float kStep = 0.1f;  // a rules frame, s
constexpr float kMelee = 80, kNear = 500, kMid = 1000;
constexpr float kStepUp = 18;     // the highest step a walker climbs
constexpr float kInFront = 0.3f;  // the cosine of in front
constexpr double kPainEvery = 3;  // s
constexpr float kHear = 1000;     // a noise's reach
constexpr int32_t kMonsterMask =
    sh::kSqSolid | sh::kSqWindow | sh::kSqMonster | 0x20000;  // + monster clip
constexpr int32_t kOpaque = sh::kSqSolid | sh::kSqSlime | sh::kSqLava;
constexpr int32_t kShotMask =
    sh::kSqSolid | sh::kSqWindow | sh::kSqSlime | sh::kSqLava | sh::kSqMonster | sh::kSqDeadMonster;
constexpr uint32_t kEveryone = 0xffffffffu;

enum class Range : uint8_t { melee, near, mid, far };

using roster::Attack;
using roster::End;
using roster::Fire;
using roster::Kind;
using roster::kKindCount;
using roster::kKinds;
using roster::Test;

// ---- Rules and words ----
int kind_of(const Q2Server& s, std::string_view classname);
bool deathmatch(const Q2Server& s);
int skill(const Q2Server& s);
double random01(Q2Server& s);
float crandom(Q2Server& s);
sh::Vec3f unpack(const int16_t v[3]);
void event(Q2Server& s, NetEvent ev);
// A sound from monster entity `e` on its voice (`reach` 1 normal, 2 an idle's, 0 everywhere).
void sound(Q2Server& s, int e, const char* sample, float reach = 1);
// Where a monster of kind `k` looks from: 25 above its origin, a swimmer's 10.
sh::Vec3f eye(const Q2Entity& e, const Kind& k);
sh::Vec3f player_eye(const Q2Player& p);
bool hunted(const Q2Server& s, int slot);
bool targetable(const Q2Server& s, int slot);

// ---- Foes: a player's slot, or another monster as -2 - its entity ----
int foe_of_entity(int e);
int foe_entity(int foe);
const Q2Monster* foe_monster(const Q2Server& s, int foe);
bool foe_alive(const Q2Server& s, int foe);
sh::Vec3f foe_at(const Q2Server& s, int foe);
sh::Vec3f foe_velocity(const Q2Server& s, int foe);
sh::Vec3f foe_eye(const Q2Server& s, int foe);
void foe_box(const Q2Server& s, int foe, sh::Vec3f& mins, sh::Vec3f& maxs);
Range range_to(sh::Vec3f a, sh::Vec3f b);
sh::Vec3f forward_of(float yaw);
bool in_front(const Q2Entity& e, sh::Vec3f at);
float yaw_to(sh::Vec3f d);

// ---- The body ----
const Kind& kind(const Q2Monster& m);
bool named(const Q2Monster& m, std::string_view classname);
Q2Entity& body(Q2Server& s, const Q2Monster& m);
void gib_monster(Q2Server& s, const Q2Monster& m, int damage);
bool afloat(const Kind& k);
bool wet(const Q2Server& s, sh::Vec3f p);
sh::Vec3f foot(const Q2Entity& e, sh::Vec3f origin);
ge::classic::Q2Trace sweep(const Q2Server& s, const Q2Monster& m, sh::Vec3f start, sh::Vec3f end);
bool reaches(const Q2Server& s, const Q2Monster& m, int foe, int32_t mask);
bool on_floor(const Q2Server& s, const Q2Monster& m);
bool step(Q2Server& s, Q2Monster& m, sh::Vec3f move);
void fall(Q2Server& s, Q2Monster& m);
void turn(Q2Entity& e, const Kind& k, float ideal);
void go_to(Q2Server& s, Q2Monster& m, sh::Vec3f goal, float speed);

// ---- Noticing ----
void found(Q2Server& s, Q2Monster& m, int foe, bool saw);
bool sees(const Q2Server& s, const Q2Monster& m, int foe);
void notice(Q2Server& s, Q2Monster& m);
bool sees_point(const Q2Server& s, const Q2Monster& m, sh::Vec3f p);

// ---- Attacks (q2_monster_shots.cpp) ----
// The enemy's eye, `lead` s along his motion.
sh::Vec3f mark(const Q2Server& s, const Q2Monster& m, float lead);
sh::Vec3f aim_at(Q2Server& s, const Attack& a, sh::Vec3f from, sh::Vec3f target);
sh::Vec3f aim_point(Q2Server& s, const Q2Monster& m, const Attack& a, sh::Vec3f from);
sh::Vec3f aim(Q2Server& s, Q2Monster& m, const Attack& a, sh::Vec3f from);
// The cause a monster's blow is scored as: its kind's, or the attack's own words.
int cause_of(const Q2Monster& m);
void hurt_foe(Q2Server& s, const Q2Monster& m, int dmg, sh::Vec3f dir);
const Attack& attack_at(const Q2Monster& m, int i);
const Attack& attack_of(const Q2Monster& m);
bool blow(Q2Server& s, Q2Monster& m, const Attack& a);
// One frame's shots of attack `a` (`n` the shot's number in its loop).
void shoot(Q2Server& s, Q2Monster& m, const Attack& a, int n);
int frames(float seconds);
void begin_attack(Q2Server& s, Q2Monster& m, int i);
// The mutant's leap test: the heights overlap and he is 100 or more away (then one time in ten).
bool leap_test(Q2Server& s, const Q2Monster& m);
// The missile an attack throws: its own (a pack's), else `usual`.
SqShot shot_of(const Attack& a, SqShot usual);
// Monster `m`'s beam `i` (0 or 1) shown from `from` to `to` for this frame and the next (blue
// when `heal`), when it has beam rows (q2_monster_rows.cpp).
void beam_show(Q2Server& s, Q2Monster& m, int i, sh::Vec3f from, sh::Vec3f to, bool heal);
// A power shield's part of a blow on `m` along `dir`: two thirds of it (rounded down), two damage
// a point while points last, the shell shown and its sparks; returns what is left of the blow.
int shield_take(Q2Server& s, Q2Monster& m, int damage, sh::Vec3f dir);

// ---- Beams (q2_monster_rows.cpp) ----
// Rows of their own for the beams of every monster whose kind fires lasers or heals by a beam:
// two map entities each, after the map's own (at spawn).
void beams_spawn(Q2Server& s);
// Whether kind `k` draws beams.
bool beams_of(const Kind& k);

// ---- The medic (q2_monster_medic.cpp) ----
bool sees_body(const Q2Server& s, const Q2Monster& medic, const Q2Monster& c);
int find_patient(const Q2Server& s, const Q2Monster& medic);
// Corpse `c` comes back as its kind; `enemy` the one it turns on (-1 none).
void revive(Q2Server& s, Q2Monster& c, int enemy);
// A medic's frame with a corpse to raise; true when it was busy with one.
bool tend(Q2Server& s, Q2Monster& m);

// ---- A mission pack's kinds (q2_reckoning_monsters.cpp) ----
// Set up at spawn: a chant (spawn flag 8), the repair bot's first work.
void reckoning_monster_spawn(Q2Server& s, Q2Monster& m);
// Its own part of a think before the usual (the amphibian's water, the repair bot's work); true
// when that was the whole think.
bool reckoning_monster_think(Q2Server& s, Q2Monster& m);
// The gekk's own attack choice (`melee` in melee range, `dist` to its enemy).
void reckoning_choose(Q2Server& s, Q2Monster& m, bool melee, float dist);
// A search or idle call that heals: 10 to 20, to its maximum.
void reckoning_heal(Q2Server& s, Q2Monster& m);

}  // namespace shooters_edge::server::monster_ai
