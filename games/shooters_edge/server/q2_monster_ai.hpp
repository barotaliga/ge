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
#include "shooters_edge/server/q2_monster_mind.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server::monster_ai {

constexpr float kStep = 0.1f;  // a rules frame, s
using q2_mind::kMelee;
using q2_mind::kMid;
using q2_mind::kNear;
constexpr float kStepUp = 18;  // the highest step a walker climbs
using q2_mind::kHear;          // a noise's reach
using q2_mind::kInFront;
using q2_mind::kPainEvery;
constexpr int32_t kMonsterMask =
    sh::kSqSolid | sh::kSqWindow | sh::kSqMonster | 0x20000;  // + monster clip
constexpr int32_t kOpaque = sh::kSqSolid | sh::kSqSlime | sh::kSqLava;
constexpr int32_t kShotMask =
    sh::kSqSolid | sh::kSqWindow | sh::kSqSlime | sh::kSqLava | sh::kSqMonster | sh::kSqDeadMonster;
constexpr uint32_t kEveryone = 0xffffffffu;

using q2_mind::Range;  // the decisions are q2_monster_mind.hpp's, shared with other rules

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
// The world's draw as the shared mind takes it (q2_monster_mind.hpp).
inline auto draw(Q2Server& s) {
  return [&s] { return random01(s); };
}
float crandom(Q2Server& s);
sh::Vec3f unpack(const int32_t v[3]);
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
// Ground Zero's tesla as a foe: missile `i` as a foe, and a foe's missile (-1 none).
constexpr int kFoeMissile = -1000000;
constexpr int foe_of_missile(int i) { return kFoeMissile - i; }
constexpr int foe_missile(int foe) { return foe <= kFoeMissile ? kFoeMissile - foe : -1; }
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
// ---- Ground Zero's kinds (q2_gz_monsters.cpp) ----
// The classes a medic commander's call makes, by strength, and the grow each shows first.
inline constexpr const char* kCommanderGroup[7] = {
    "monster_soldier_light", "monster_soldier", "monster_soldier_ss", "monster_infantry",
    "monster_gunner",        "monster_medic",   "monster_gladiator"};
inline constexpr const char* kSpawnGrow = "models/items/spawngro2/tris.md2";
// A body for a spawner's brood (a commander's guards, a boss's fliers or stalkers): one of the
// spares made with the map for `c` still waiting, or one of its own torn apart; null when none.
Q2Monster* free_spare(Q2Server& s, const Q2Monster& c);
// Spare `m` made afresh as class `k`, hidden, waiting at `at`; `from` its spawner (a monster row).
void become(Q2Server& s, Q2Monster& m, int k, int from, sh::Vec3f at);
// Eight spare bodies, waiting unseen, for each medic commander on the map (with the map's load).
void gz_spares(Q2Server& s);
// A commander's call begun.
void gz_begin_call(Q2Server& s, Q2Monster& m);
// A Ground Zero kind about to fire (not in melee range): a commander's call instead, 80% of the
// time with more than two slots left (true: it called); a stalker's jump first, a third of the
// time on the ground (it still fires).
bool gz_call_instead(Q2Server& s, Q2Monster& m, bool melee);
// A commander about to cast its cable: a call instead, 20% of the time with more than two slots.
bool gz_cable_instead(Q2Server& s, Q2Monster& m);
// A Ground Zero kind's own think this frame (a commander's call); true when it took the frame.
bool gz_think(Q2Server& s, Q2Monster& m);
// A Ground Zero kind set up as the map spawns (a turret's mounting, a wall unit in its wall).
void gz_spawned(Q2Server& s, Q2Monster& m);
// A use of a Ground Zero kind that is its own (a wall turret coming out); true when taken.
bool gz_use(Q2Server& s, Q2Monster& m);
// Whether a blow passes it by (a wall turret not yet out, a boss dying or invulnerable).
bool gz_untouchable(const Q2Server& s, const Q2Monster& m);
// A Ground Zero kind's own pain from a blow of `damage` (a turret's none, a stalker's jump or
// false death); true when it took the blow's pain.
bool gz_pain(Q2Server& s, Q2Monster& m, int damage);
// A Ground Zero kind's death: a turret's debris, a stalker falling from its ceiling; a
// reinforcement gives its commander a slot back.
void gz_died(Q2Server& s, Q2Monster& m);
// Whether a monster's death counts among the level's kills (a reinforcement's does not).
bool gz_counted(const Q2Monster& m);
// A Ground Zero kind's world row: a reinforcement growing shows the grow.
void gz_row(const Q2Server& s, const Q2Monster& m, ge::shooter::WorldEntity& r);

// ---- Ground Zero's changes to the base monsters (q2_gz_base_monsters.cpp) ----
// Each does nothing outside Ground Zero.
// A shot of attack `a` from `from` taken the pack's way (the maiden's and the tanks' rockets, the
// hornet's volley; a soldier's shot held when another stands in the line); true when taken.
bool gz_base_shoot(Q2Server& s, Q2Monster& m, const Attack& a, sh::Vec3f from);
// The share of a soldier's own spread kept (half on hard and nightmare; else all).
float gz_base_spread(const Q2Server& s, const Q2Monster& m);
// The chance added to another loop of the attack (the maiden's 0.05 a skill).
double gz_base_again(const Q2Server& s, const Q2Monster& m);
// An attack begun: a flier's strafing burst, by chance (none on easy, 1 − 0.5 ÷ skill; the
// daedalus 0.1 more), to a side it switches half the time.
void gz_base_begin(Q2Server& s, Q2Monster& m, const Attack& a);
// The frame's own think (the kamikaze's charge; true when it took the frame) and a strafing
// burst's slide of 10 a frame.
bool gz_base_think(Q2Server& s, Q2Monster& m);
// No pain (the kamikaze's never, the infantry's and gunner's in the air); true when none.
bool gz_base_pain(Q2Server& s, Q2Monster& m);
// The pain rule's name: the hover's own under the pack ("gz/monster_hover"), else `usual`.
const char* gz_base_pain_class(const Q2Server& s, const Q2Monster& m, const char* usual);
// A soldier's running fire at first sight under the pack (above easy, not within melee range,
// a time in four, never the machinegun soldier): 1 it runs and fires, 0 not, −1 the base rule.
int gz_base_sight_run(Q2Server& s, Q2Monster& m, float dist);
// Ground Zero's running fire frame by frame (true when it took the frame): the run's own steps,
// the shot on the third frame when he is within about 25 degrees of its facing, and round again
// from there at the last at near range or farther (always on nightmare, else 0.25 a skill).
bool gz_base_run_and_shoot(Q2Server& s, Q2Monster& m);
// The ranged attack `i` chosen, checked under the pack: a blaster or shotgun soldier runs and
// fires instead (0.25 a skill); the gunner's grenades only with a clear lob, else its chaingun;
// the parasite's drain only within its reach. −1 holds the shot (or the running fire took it).
int gz_base_pick(Q2Server& s, Q2Monster& m, int i, Range r);
// A move blocked: the parasite drains its player instead, 0.75 − 0.05 a skill of the time,
// when he is within its reach and in sight (true when it did).
bool gz_base_blocked(Q2Server& s, Q2Monster& m);
// For the shared monster AI the pack changes (q2_gz_monsters.cpp): each kind's ledge jump (the
// drop and the climb it takes, the jump up's and down's speeds forward and up), its dodge (ducks,
// side-steps, holds still for its attacks above easy) and its blind fire (the delay growing each
// try, lowest and highest, and the wait after, s). Null for a kind without one.
struct GzLedge {
  const char* classname;
  float drop, climb;
  float up_forward, up_up, down_forward, down_up;
};
struct GzDodge {
  const char* classname;
  bool ducks, side_steps, attacks_still;
};
struct GzBlind {
  const char* classname;
  float delay_lo, delay_hi, wait_lo, wait_hi;
};
// Ground Zero's tesla (q2_gz_tesla.cpp): tesla `i` as a foe still there, and its origin raised
// `up`; a monster fighting one minds no other blow (true); whether a step of `move` would take
// it into a tesla's bad area, or deeper in toward the tesla.
bool gz_tesla_alive(const Q2Server& s, int i);
sh::Vec3f gz_tesla_at(const Q2Server& s, int i, float up);
bool gz_tesla_holds(Q2Server& s, Q2Monster& m);
bool gz_bad_area_refuses(const Q2Server& s, const Q2Monster& m, sh::Vec3f move);
// Ground Zero's place for a body grown or set down at `p` (q2_gz_monsters.cpp): box `mins`..`maxs`
// fits there (or dropped onto it from 32 above) over solid, dry, nearly flat ground within `depth`
// below, its four corners within 18 of the middle; where it stands in `out`. The medic
// commander's guards (256) and the Doppleganger (64).
bool gz_spawn_spot(const Q2Server& s, int skip, sh::Vec3f mins, sh::Vec3f maxs, sh::Vec3f p,
                   float depth, sh::Vec3f& out);
const GzLedge* gz_ledge(std::string_view classname);
const GzDodge* gz_dodge_of(std::string_view classname);
const GzBlind* gz_blind_of(std::string_view classname);

// ---- Ground Zero's shared monster AI (q2_gz_ai.cpp; gz-monsters.md § 1) ----
// Each does nothing outside Ground Zero (false, or the base game's rule).
bool gz_ai(const Q2Server& s);
// Whether `m` dodges the pack's way (a kind of its dodge table).
bool gz_dodges(const Q2Server& s, const Q2Monster& m);
// A player's shot from `shooter` warned `m` it strikes `hit` in `eta` s: what it does.
enum class GzDodgeAct : uint8_t { none, duck, handled };
GzDodgeAct gz_dodge(Q2Server& s, Q2Monster& m, int shooter, sh::Vec3f hit, double eta);
// A side-step's frame while it lasts; true when it took the frame.
bool gz_dodge_step(Q2Server& s, Q2Monster& m);
// The chance it slides sideways after not firing at range `r` (-1: the base game's rule).
double gz_slide_chance(Q2Server& s, const Q2Monster& m, Range r);
// The sideways step a slide takes a frame: a flier's run, a walker's at most 8.
float gz_slide_step(const Q2Server& s, const Q2Monster& m, float run_step);
// Its enemy seen this frame: blind fire's mark and its delay reset.
void gz_seen(Q2Server& s, Q2Monster& m);
// Its enemy not reached: whether it fires blind at the mark now (its delay grown, its wait set).
bool gz_blind_fire(Q2Server& s, Q2Monster& m);
// Blocked on its way to its enemy: a jump down or up a ledge; true when it jumped.
bool gz_ledge_jump(Q2Server& s, Q2Monster& m);
// In the air on a ledge jump: true while it waits to land (3 s at most).
bool gz_jumping(Q2Server& s, Q2Monster& m);
// Whether shots between it and another start no fight (the pack's flag: the tanks, the medics,
// the turret, a revived monster).
bool gz_ignores_shots(const Q2Server& s, const Q2Monster& m);
// Its enemy out of sight 5 s or more: a hint path's chain to near him taken, and followed; true
// when the path took the frame.
bool gz_hints(Q2Server& s, Q2Monster& m);
// A medic's search: whether corpse `c` may be its patient, and its reach (1024, 400 holding).
bool gz_patient_ok(const Q2Server& s, const Q2Monster& medic, const Q2Monster& c);
float gz_patient_reach(const Q2Server& s, const Q2Monster& medic);
// A medic with a patient this frame: false when it gives the patient up (10 s, too near).
bool gz_patient_keep(Q2Server& s, Q2Monster& medic, Q2Monster& c);

// ---- Ground Zero's bosses (q2_gz_bosses.cpp): the carrier and the black widow's two stages ----
// Whether `m` (or class `classname`) is one of them.
bool gz_boss(const Q2Monster& m);
bool gz_boss(std::string_view classname);
// Their spare bodies, made with the map: fliers for a carrier, stalkers for a widow.
void gz_boss_spares(Q2Server& s);
// One set up as the map spawns: its health and its power shield by skill and co-op.
void gz_boss_spawned(Q2Server& s, Q2Monster& m);
// Its frame: the move it plays, or the choice of one; false when it leaves the frame to the run.
bool gz_boss_think(Q2Server& s, Q2Monster& m);
// Its pain from a blow of `damage` (always its own).
void gz_boss_pain(Q2Server& s, Q2Monster& m, int damage);
// Its death begun: a death sound everywhere, the timers cleared, the second stage's stalkers
// killed.
void gz_boss_died(Q2Server& s, Q2Monster& m);
// A dead boss's frame (a widow's beam-out, her legs, the second stage's blasts); false for any
// other monster, which lies by the base game's rules.
bool gz_boss_lie(Q2Server& s, Q2Monster& m);
// Whether blows pass it by now: dying, or invulnerable by its answer to a power-up.
bool gz_boss_untouchable(const Q2Server& s, const Q2Monster& m);
// Its world row: the frame of its legs or its dead search, its shell.
void gz_boss_row(const Q2Server& s, const Q2Monster& m, ge::shooter::WorldEntity& r);
// The kinds, models and sounds a boss of `classname` brings (its brood, its grows, its gibs).
struct BossContent {
  const char* kinds[2] = {};
  const char* models[10] = {};
  const char* sounds[16] = {};
};
BossContent gz_boss_content(std::string_view classname);

}  // namespace shooters_edge::server::monster_ai
