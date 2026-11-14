// THE CAMPAIGN'S DECLARATION: the single-player game's own globals, fields and functions after the
// shared ones (common_decl.hpp), the enums the rules index by, the campaign's view of the server's
// calls (`A`) over the single-player form, its constants, and the rules one campaign file calls in
// another. Internal to games/shooters_edge/rules.
//
// ge:summary The campaign's declaration: its own lists, enums, the rules' view and helpers
// ge:keywords campaign native-campaign declaration x-list globals fields functions monsters
// ge:keywords single-player-form
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#pragma once

#include <cstdint>

#include "shooters_edge/rules/common_decl.hpp"

namespace shooters_edge::server::rules::campaign {

// THE CAMPAIGN'S OWN GLOBALS (the server, the bots and the labs find `intermission_running` and
// `finale_words` by name).
#define SE_CAMPAIGN_GLOBALS(X)        \
  X(sight_entity, kTypeEntity)        \
  X(sight_entity_time, kTypeFloat)    \
  X(intermission_running, kTypeFloat) \
  X(intermission_exit, kTypeFloat)    \
  X(next_map, kTypeString)            \
  X(finale_words, kTypeString)        \
  X(lava_boss, kTypeEntity)

// THE CAMPAIGN'S OWN FIELDS: a monster's mind and frames, then the map's keys (`map`, `count`,
// `mangle`, `t_length`, `t_width`).
#define SE_CAMPAIGN_FIELDS(X)  \
  X(th_stand, kTypeFunction)   \
  X(th_walk, kTypeFunction)    \
  X(th_run, kTypeFunction)     \
  X(th_melee, kTypeFunction)   \
  X(th_missile, kTypeFunction) \
  X(th_pain, kTypeFunction)    \
  X(th_check, kTypeFunction)   \
  X(movetarget, kTypeEntity)   \
  X(oldenemy, kTypeEntity)     \
  X(pausetime, kTypeFloat)     \
  X(search_time, kTypeFloat)   \
  X(attack_state, kTypeFloat)  \
  X(walk_step, kTypeFloat)     \
  X(run_step, kTypeFloat)      \
  X(anim_step, kTypeFloat)     \
  X(fr_stand, kTypeVector)     \
  X(fr_walk, kTypeVector)      \
  X(fr_run, kTypeVector)       \
  X(fr_melee, kTypeVector)     \
  X(fr_missile, kTypeVector)   \
  X(fr_leap, kTypeVector)      \
  X(fr_pain, kTypeVector)      \
  X(fr_death, kTypeVector)     \
  X(fr_now, kTypeVector)       \
  X(fr_death2, kTypeVector)    \
  X(death_solid, kTypeFloat)   \
  X(drop_ammo, kTypeFloat)     \
  X(gib_health, kTypeFloat)    \
  X(head_model, kTypeString)   \
  X(gib_model, kTypeString)    \
  X(gib_first, kTypeFloat)     \
  X(gib_noise, kTypeString)    \
  X(claw_factor, kTypeFloat)   \
  X(map, kTypeString)          \
  X(count, kTypeFloat)         \
  X(mangle, kTypeVector)       \
  X(t_length, kTypeFloat)      \
  X(t_width, kTypeFloat)       \
  X(dest1, kTypeVector)        \
  X(dest2, kTypeVector)

// THE CAMPAIGN'S OWN FUNCTIONS by name: the hooks, the map's things, every think, touch, use and
// `th_` a rule stores (the C++ `main_hook` is the program's `main`). `use_targets` is shared but
// stored here as a relay's use.
#define SE_CAMPAIGN_FUNCTIONS(X)                      \
  X(main_hook, "main")                                \
  X(StartFrame, "StartFrame")                         \
  X(SetNewParms, "SetNewParms")                       \
  X(SetChangeParms, "SetChangeParms")                 \
  X(worldspawn, "worldspawn")                         \
  X(use_targets, "use_targets")                       \
  X(info_player_start, "info_player_start")           \
  X(info_player_start2, "info_player_start2")         \
  X(info_player_coop, "info_player_coop")             \
  X(info_player_deathmatch, "info_player_deathmatch") \
  X(info_intermission, "info_intermission")           \
  X(info_null, "info_null")                           \
  X(info_notnull, "info_notnull")                     \
  X(intermission_start, "intermission_start")         \
  X(changelevel_touch, "changelevel_touch")           \
  X(trigger_changelevel, "trigger_changelevel")       \
  X(secret_touch, "secret_touch")                     \
  X(trigger_secret, "trigger_secret")                 \
  X(counter_use, "counter_use")                       \
  X(trigger_counter, "trigger_counter")               \
  X(trigger_relay, "trigger_relay")                   \
  X(setskill_touch, "setskill_touch")                 \
  X(trigger_setskill, "trigger_setskill")             \
  X(onlyregistered_touch, "onlyregistered_touch")     \
  X(trigger_onlyregistered, "trigger_onlyregistered") \
  X(trigger_monsterjump, "trigger_monsterjump")       \
  X(func_episodegate, "func_episodegate")             \
  X(func_bossgate, "func_bossgate")                   \
  X(sigil_touch, "sigil_touch")                       \
  X(item_sigil, "item_sigil")                         \
  X(key_touch, "key_touch")                           \
  X(item_key1, "item_key1")                           \
  X(item_key2, "item_key2")                           \
  X(monster_stand, "monster_stand")                   \
  X(monster_walk, "monster_walk")                     \
  X(monster_run, "monster_run")                       \
  X(monster_hurt, "monster_hurt")                     \
  X(monster_use, "monster_use")                       \
  X(corpse_fall, "corpse_fall")                       \
  X(monster_die, "monster_die")                       \
  X(monster_begin, "monster_begin")                   \
  X(monster_missile_touch, "monster_missile_touch")   \
  X(corner_touch, "corner_touch")                     \
  X(path_corner, "path_corner")                       \
  X(army_attack, "army_attack")                       \
  X(army_missile, "army_missile")                     \
  X(army_flinch, "army_flinch")                       \
  X(army_pain, "army_pain")                           \
  X(monster_army, "monster_army")                     \
  X(dog_bite_step, "dog_bite_step")                   \
  X(dog_melee, "dog_melee")                           \
  X(dog_leap_touch, "dog_leap_touch")                 \
  X(dog_leap_step, "dog_leap_step")                   \
  X(dog_check, "dog_check")                           \
  X(dog_flinch, "dog_flinch")                         \
  X(dog_pain, "dog_pain")                             \
  X(monster_dog, "monster_dog")                       \
  X(knight_swing, "knight_swing")                     \
  X(knight_melee, "knight_melee")                     \
  X(knight_check, "knight_check")                     \
  X(knight_flinch, "knight_flinch")                   \
  X(knight_pain, "knight_pain")                       \
  X(monster_knight, "monster_knight")                 \
  X(ogre_grenade_blast, "ogre_grenade_blast")         \
  X(ogre_grenade_touch, "ogre_grenade_touch")         \
  X(ogre_throw, "ogre_throw")                         \
  X(ogre_missile, "ogre_missile")                     \
  X(ogre_saw, "ogre_saw")                             \
  X(ogre_melee, "ogre_melee")                         \
  X(ogre_flinch, "ogre_flinch")                       \
  X(ogre_pain, "ogre_pain")                           \
  X(monster_ogre, "monster_ogre")                     \
  X(monster_ogre_marksman, "monster_ogre_marksman")   \
  X(ghoul_throw, "ghoul_throw")                       \
  X(ghoul_missile, "ghoul_missile")                   \
  X(ghoul_down, "ghoul_down")                         \
  X(ghoul_hurt, "ghoul_hurt")                         \
  X(ghoul_pain, "ghoul_pain")                         \
  X(ghoul_die, "ghoul_die")                           \
  X(monster_zombie, "monster_zombie")                 \
  X(wizard_spit, "wizard_spit")                       \
  X(wizard_missile, "wizard_missile")                 \
  X(wizard_flinch, "wizard_flinch")                   \
  X(wizard_pain, "wizard_pain")                       \
  X(wizard_die, "wizard_die")                         \
  X(monster_wizard, "monster_wizard")                 \
  X(fish_bite_step, "fish_bite_step")                 \
  X(fish_melee, "fish_melee")                         \
  X(fish_pain, "fish_pain")                           \
  X(monster_fish, "monster_fish")                     \
  X(demon_claw, "demon_claw")                         \
  X(demon_melee, "demon_melee")                       \
  X(demon_leap_touch, "demon_leap_touch")             \
  X(demon_leap_step, "demon_leap_step")               \
  X(demon_check, "demon_check")                       \
  X(demon_flinch, "demon_flinch")                     \
  X(demon_pain, "demon_pain")                         \
  X(monster_demon1, "monster_demon1")                 \
  X(shambler_claw, "shambler_claw")                   \
  X(shambler_melee, "shambler_melee")                 \
  X(shambler_bolt, "shambler_bolt")                   \
  X(shambler_missile, "shambler_missile")             \
  X(shambler_flinch, "shambler_flinch")               \
  X(shambler_pain, "shambler_pain")                   \
  X(monster_shambler, "monster_shambler")             \
  X(caster_ball_touch, "caster_ball_touch")           \
  X(caster_ball_home, "caster_ball_home")             \
  X(caster_cast, "caster_cast")                       \
  X(caster_missile, "caster_missile")                 \
  X(caster_flinch, "caster_flinch")                   \
  X(caster_pain, "caster_pain")                       \
  X(monster_shalrath, "monster_shalrath")             \
  X(enforcer_fire, "enforcer_fire")                   \
  X(enforcer_missile, "enforcer_missile")             \
  X(enforcer_flinch, "enforcer_flinch")               \
  X(enforcer_pain, "enforcer_pain")                   \
  X(monster_enforcer, "monster_enforcer")             \
  X(tarbaby_touch, "tarbaby_touch")                   \
  X(tarbaby_bounce, "tarbaby_bounce")                 \
  X(tarbaby_check, "tarbaby_check")                   \
  X(tarbaby_die, "tarbaby_die")                       \
  X(monster_tarbaby, "monster_tarbaby")               \
  X(hknight_swing, "hknight_swing")                   \
  X(hknight_melee, "hknight_melee")                   \
  X(hknight_charge, "hknight_charge")                 \
  X(hknight_cast, "hknight_cast")                     \
  X(hknight_missile, "hknight_missile")               \
  X(hknight_check, "hknight_check")                   \
  X(hknight_flinch, "hknight_flinch")                 \
  X(hknight_pain, "hknight_pain")                     \
  X(monster_hell_knight, "monster_hell_knight")       \
  X(teleporttrain_next, "teleporttrain_next")         \
  X(teleporttrain_use, "teleporttrain_use")           \
  X(misc_teleporttrain, "misc_teleporttrain")         \
  X(boss_ball_touch, "boss_ball_touch")               \
  X(boss_think, "boss_think")                         \
  X(boss_use, "boss_use")                             \
  X(boss_sink, "boss_sink")                           \
  X(boss_struck, "boss_struck")                       \
  X(monster_boss, "monster_boss")                     \
  X(lightning_use, "lightning_use")                   \
  X(event_lightning, "event_lightning")               \
  X(oldone_end, "oldone_end")                         \
  X(oldone_burst, "oldone_burst")                     \
  X(oldone_die, "oldone_die")                         \
  X(oldone_sway, "oldone_sway")                       \
  X(monster_oldone, "monster_oldone")                 \
  X(train_blocked, "train_blocked")                   \
  X(train_arrived, "train_arrived")                   \
  X(train_next, "train_next")                         \
  X(train_place, "train_place")                       \
  X(func_train, "func_train")                         \
  X(secret_closed, "secret_closed")                   \
  X(secret_back, "secret_back")                       \
  X(secret_shut_rest, "secret_shut_rest")             \
  X(secret_shut, "secret_shut")                       \
  X(secret_open_full, "secret_open_full")             \
  X(secret_aside, "secret_aside")                     \
  X(secret_open_rest, "secret_open_rest")             \
  X(secret_use, "secret_use")                         \
  X(secret_shot, "secret_shot")                       \
  X(secret_blocked, "secret_blocked")                 \
  X(secret_door_touch, "secret_door_touch")           \
  X(func_door_secret, "func_door_secret")             \
  X(explobox_die, "explobox_die")                     \
  X(misc_explobox, "misc_explobox")                   \
  X(misc_explobox2, "misc_explobox2")                 \
  X(fireball_touch, "fireball_touch")                 \
  X(fireball_fly, "fireball_fly")                     \
  X(misc_fireball, "misc_fireball")                   \
  X(trap_fire, "trap_fire")                           \
  X(trap_spikeshooter, "trap_spikeshooter")           \
  X(trap_shooter_think, "trap_shooter_think")         \
  X(trap_shooter, "trap_shooter")                     \
  X(bubble_rise, "bubble_rise")                       \
  X(bubbles_make, "bubbles_make")                     \
  X(air_bubbles, "air_bubbles")                       \
  X(ambient_comp_hum, "ambient_comp_hum")             \
  X(ambient_drip, "ambient_drip")                     \
  X(ambient_drone, "ambient_drone")                   \
  X(ambient_flouro_buzz, "ambient_flouro_buzz")       \
  X(ambient_light_buzz, "ambient_light_buzz")         \
  X(ambient_suck_wind, "ambient_suck_wind")           \
  X(ambient_swamp1, "ambient_swamp1")                 \
  X(ambient_swamp2, "ambient_swamp2")                 \
  X(ambient_thunder, "ambient_thunder")

// THE RULES called by name WITH ARGUMENTS (the tests' and the tools' blows), after the list above:
// each reads its parameters as the bytecode passes them.
#define SE_CAMPAIGN_CALLS(X)             \
  X(damage_call, "damage")               \
  X(radius_damage_call, "radius_damage") \
  X(teleport_arrived_call, "teleport_arrived")

#define SE_X(name, ...) g_##name,
enum Gl : uint8_t { SE_COMMON_GLOBALS(SE_X) SE_CAMPAIGN_GLOBALS(SE_X) kGlobalCount };
#undef SE_X
#define SE_X(name, ...) f_##name,
enum Fd : uint8_t { SE_COMMON_FIELDS(SE_X) SE_CAMPAIGN_FIELDS(SE_X) kFieldCount };
#undef SE_X
#define SE_X(name, ...) s_##name,
enum St : uint16_t { SE_RULES_STRINGS(SE_X) kStringCount };
#undef SE_X
#define SE_X(name, ...) fn_##name,
enum Fn : uint16_t {
  SE_COMMON_FUNCTIONS(SE_X) SE_CAMPAIGN_FUNCTIONS(SE_X) SE_CAMPAIGN_CALLS(SE_X) kFunctionCount
};
#undef SE_X

// Where an effect's bytes go: the broadcast, already in it when written.
constexpr float kMsgEffect = 0;

// THE RULES' VIEW of the server's calls: Q's over the single-player form, plus the campaign's own
// globals, fields, strings and functions by their enums.
struct A : Q {
  explicit A(pg::Vm& v) : Q(v) {}
  using Q::f;
  using Q::i;
  using Q::set_f;
  using Q::set_i;
  using Q::set_v;
  using Q::v;
  float f(int32_t e, Fd k) { return Q::f(e, own_field(k)); }
  int32_t i(int32_t e, Fd k) { return Q::i(e, own_field(k)); }
  Vec3 v(int32_t e, Fd k) { return Q::v(e, own_field(k)); }
  void set_f(int32_t e, Fd k, float x) { Q::set_f(e, own_field(k), x); }
  void set_i(int32_t e, Fd k, int32_t x) { Q::set_i(e, own_field(k), x); }
  void set_v(int32_t e, Fd k, Vec3 x) { Q::set_v(e, own_field(k), x); }
  float g(Gl k) const { return gf(own_global(k)); }
  int32_t ge(Gl k) const { return gi(own_global(k)); }
  void set_g(Gl k, float x) { set_gf(own_global(k), x); }
  void set_ge(Gl k, int32_t x) { set_gi(own_global(k), x); }
  int32_t s(St k) const { return str(k); }
  int32_t fn(Fn k) const { return Q::fn(k); }
  // The single-player form's own.
  float deathmatch() const { return gf(L.g.deathmatch); }
  float coop() const { return gf(L.g.coop); }
  float serverflags() const { return gf(L.g.serverflags); }
  // The form's counters, kept for the status bar.
  void add_to(int g, float n) { set_gf(g, gf(g) + n); }
};

#include "shooters_edge/rules/common_rules.inc"

// THE CAMPAIGN'S CONSTANTS.
constexpr float kMovetypeStep = 4, kMovetypeFly = 5, kMovetypeNoclip = 8;
constexpr int kFlFly = 1, kFlSwim = 2, kFlMonster = 32, kFlNotarget = 128, kFlWaterjump = 2048;
constexpr float kContentWater = -3;
constexpr float kAttnIdle = 2, kAttnStatic = 3;
constexpr float kMsgBroadcast = 0, kMsgAll = 2;
constexpr float kSvcKilledmonster = 27, kSvcFoundsecret = 28, kSvcIntermission = 30,
                kSvcFinale = 31;
constexpr float kDropShells = 1, kDropCells = 2, kDropRockets = 3;
constexpr float kAsStraight = 1, kAsMelee = 3, kAsMissile = 4;
constexpr float kRangeMelee = 0, kRangeNear = 1, kRangeMid = 2, kRangeFar = 3;
constexpr int kSpawnAmbush = 1;
constexpr float kForever = 100000000;

// Vector words as the bytecode makes them: a float times a vector (MUL_FV) and the dot product
// (MUL_V), each in its own order.
inline Vec3 fv(float k, Vec3 v) { return {k * v.x, k * v.y, k * v.z}; }
inline float dot(Vec3 a, Vec3 b) { return (a.x * b.x + a.y * b.y) + a.z * b.z; }

// A rule stored in a field, run as the bytecode's call of it (a float one's answer read after).
inline void run_field(A& q, int32_t e, Fd k) { q.call(q.i(e, k)); }

// A step of an attack, a leap or a pain: the think again in 0.1 s.
inline void think_again(A& q, Fn fn) {
  q.set_i(q.self(), q.L.f.think, q.fn(fn));
  q.set_f(q.self(), q.L.f.nextthink, q.time() + 0.1f);
}
inline float anim_step(A& q) { return q.f(q.self(), f_anim_step); }
// The next pain no sooner than `secs` from now.
inline void hold_pain(A& q, float secs) {
  if (q.f(q.self(), f_pain_finished) < q.time() + secs)
    q.set_f(q.self(), f_pain_finished, q.time() + secs);
}
// A kind's frames for a state: `first` to `last`, the model has them.
inline void set_frames(A& q, Fd k, float first, float last) {
  q.set_v(q.self(), k, {first, last, 1});
}
inline Vec3 enemy_origin(A& q) { return q.v(q.i(q.self(), q.L.f.enemy), q.L.f.origin); }
inline float enemy_distance(A& q) { return q.vlen(enemy_origin(q) - q.v(q.self(), q.L.f.origin)); }

// THE RULES one campaign file calls in another.
// campaign_world
void intermission_start(A& q);
// campaign_ai
float range_to(A& q, int32_t targ);
bool can_see(A& q, int32_t targ);
bool in_front(A& q, int32_t targ);
bool face_enemy(A& q);
void found_target(A& q);
bool look_for_target(A& q);
bool check_attack(A& q);
void melee_blow_n(A& q, float n, float factor);
void melee_blow(A& q, float factor);
void ai_charge(A& q, float dist);
void frame_loop(A& q, Vec3 r);
void anim_begin(A& q, Vec3 r);
void anim_tick(A& q);
void throw_gib_at(A& q, int32_t mdl, Vec3 org, float dm);
void monster_start(A& q, int32_t mdl, float hp, Vec3 lo, Vec3 hi);
void walkmonster_start(A& q, int32_t mdl, float hp, Vec3 lo, Vec3 hi);
void flymonster_start(A& q, int32_t mdl, float hp, Vec3 lo, Vec3 hi);
void swimmonster_start(A& q, int32_t mdl, float hp, Vec3 lo, Vec3 hi);
bool no_monsters_here(A& q);
void aim_along(A& q, Vec3 dir);
int32_t monster_missile(A& q, int32_t mdl, float spd, float points, float lob);
void flinch(A& q, Fn again, float steps);
void monster_kind(A& q, St sight, St death, St head, float gib_health, float walk, float run);
void lands_no_flag(A& q, int32_t e);
// campaign_monsters
void leap_off(A& q, float fwd, float up, Fn touch);
void ai_precache(A& q);

#define SE_X(name, ...) void name(A& q);
SE_CAMPAIGN_FUNCTIONS(SE_X)
SE_CAMPAIGN_CALLS(SE_X)
#undef SE_X

}  // namespace shooters_edge::server::rules::campaign
