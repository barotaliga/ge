// Midair in the arena (`k_midair 1` in deathmatch): only a rocket on a player in the air counts,
// and it counts for more the higher the target flew above its shooter. The mode's rules and numbers
// are every game's (plugins/shooter/midair.hpp); this file is the arena's glue:
//
//   THE SPAWN   250 health, 200 red armour, the blade and the rocket launcher in hand, rockets
//               never used up (`ammo_spend`), no protection; nothing lies on the map
//               (`item_start`) and a dead player drops no pack.
//   A BLOW      on a player (`midair_blow`, from `damage`): pushed as the mode says (harder the
//               higher the target is, lifted on the floor). Another's rocket at the least height or
//               more
//               (`k_midair_minheight`) kills the target through any armour, its frags by the rise
//               kept for `score_death`; drowning, lava, slime, a telefrag and the kill command take
//               what they take; anything else wears the armour and takes no health.
//   THE SCORE   a counted hit's band's frags to its shooter, told to all ("Gold midair by ...");
//               any other kill of a player gives its killer nothing; a suicide or the world's kill
//               costs as usual.
//   A DEATH     back by himself after 2 s (`player_dead_frame`).
//
// The setting is read once a frame into a global (`StartFrame`, `worldspawn`), so a match that
// does not play midair calls nothing more than before.
//
// ge:summary The arena's midair glue: the kit, the counted rocket, its score, the respawn
// ge:keywords arena native-arena midair k_midair k_midair_minheight midair_blow counted-hit rise
// ge:keywords midair_give fired-z
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_midair_test.cpp
#include <cstdint>

#include "shooter/midair.hpp"
#include "shooters_edge/rules/variant.hpp"

namespace shooters_edge::server::rules::SE_RULES_NS {

namespace mid = ge::shooter;

namespace {

// The least height's level (`k_midair_minheight`, 0..4; the default when unset or another).
int midair_level_now(A& q) {
  return mid::midair_level(q.text(q.cvar_string(q.s(s_k_midair_minheight))));
}

// `p`'s height over the floor under that player: a line straight down that only the world stops.
// The trace's globals are put back as they were (a weapon's trace may still be read after its
// blow).
float midair_height_of(A& q, int32_t p) {
  const auto& g = q.L.g;
  const float allsolid = q.gf(g.trace_allsolid), startsolid = q.gf(g.trace_startsolid);
  const float fraction = q.gf(g.trace_fraction), dist = q.gf(g.trace_plane_dist);
  const float inopen = q.gf(g.trace_inopen), inwater = q.gf(g.trace_inwater);
  const Vec3 endpos = q.gv(g.trace_endpos), normal = q.gv(g.trace_plane_normal);
  const int32_t ent = q.gi(g.trace_ent);
  const Vec3 o = q.v(p, q.L.f.origin);
  q.traceline(o, o - Vec3{0, 0, mid::kMidairProbe}, 1, p);
  const float floor = q.gv(g.trace_endpos).z;
  q.set_gf(g.trace_allsolid, allsolid);
  q.set_gf(g.trace_startsolid, startsolid);
  q.set_gf(g.trace_fraction, fraction);
  q.set_gf(g.trace_plane_dist, dist);
  q.set_gf(g.trace_inopen, inopen);
  q.set_gf(g.trace_inwater, inwater);
  q.set_gv(g.trace_endpos, endpos);
  q.set_gv(g.trace_plane_normal, normal);
  q.set_gi(g.trace_ent, ent);
  return mid::midair_height(o.z, q.v(p, q.L.f.mins).z, floor);
}

// A harm the mode keeps: drowning, lava, slime, a telefrag, the kill command.
bool midair_harm(A& q, int32_t targ) {
  const int32_t how = q.i(targ, f_deathtype);
  const bool tele = q.g(g_telefragging) != 0;
  return tele | q.same(how, q.s(s_drowned)) | q.same(how, q.s(s_burned)) |
         q.same(how, q.s(s_dissolved)) | q.same(how, q.s(s_telefragged)) |
         q.same(how, q.s(s_suicides));
}

// The band's words that open a counted hit's line.
int32_t midair_words(A& q, mid::MidairBand b) {
  switch (b) {
    case mid::MidairBand::platinum:
      return q.s(s_platinum_midair_by);
    case mid::MidairBand::gold:
      return q.s(s_gold_midair_by);
    case mid::MidairBand::silver:
      return q.s(s_silver_midair_by);
    default:
      return q.s(s_bronze_midair_by);
  }
}

}  // namespace

bool midair_on(A& q) { return q.g(g_midair) != 0; }

// The setting read (each frame, and before the map's items spawn): on only in deathmatch.
void midair_read(A& q) {
  q.set_g(g_midair, (q.deathmatch() != 0) & (q.cvar(q.s(s_k_midair)) != 0) ? 1.0f : 0.0f);
}

// A spawn's kit.
void midair_give(A& q) {
  const int32_t me = q.self();
  q.set_f(me, q.L.f.items, kWBlade | kWRockets | kItArmor3);
  q.set_f(me, q.L.f.health, mid::kMidairHealth);
  q.set_f(me, q.L.f.armortype, 0.8f);
  q.set_f(me, q.L.f.armorvalue, mid::kMidairArmour);
  q.set_f(me, q.L.f.ammo_shells, 0);
  q.set_f(me, q.L.f.ammo_nails, 0);
  q.set_f(me, q.L.f.ammo_rockets, 255);
  q.set_f(me, q.L.f.ammo_cells, 0);
  weapon_set(q, kWRockets);
}

// A rocket fired by `self`: where the shooter stood, for the rise of what it hits.
void midair_fired(A& q, int32_t rocket) {
  q.set_f(rocket, f_fired_z, q.v(q.self(), q.L.f.origin).z);
}

float midair_blow(A& q, int32_t targ, int32_t inflictor, int32_t attacker, float amount) {
  q.set_g(g_midair_frags, 0);
  if (midair_harm(q, targ)) return amount;
  const float height = midair_height_of(q, targ);
  const bool own = attacker == targ;
  // The push, worked out from the blow before the mode takes its harm away.
  if ((inflictor != q.world()) & (q.f(targ, q.L.f.movetype) == kMovetypeWalk)) {
    Vec3 dir = q.v(targ, q.L.f.origin) -
               (q.v(inflictor, q.L.f.absmin) + q.v(inflictor, q.L.f.absmax)) * 0.5f;
    dir = q.normalize(dir);
    const mid::MidairPush m = mid::midair_push(height, amount, own);
    const float push = amount * 8 * m.scale;
    Vec3 v = q.v(targ, q.L.f.velocity) + dir * push;
    v.z = v.z + dir.z * push * m.lift;
    q.set_v(targ, q.L.f.velocity, v);
  }
  // Another player's rocket high enough: through any armour, and scored by the rise.
  const bool rocket = q.is(q.i(inflictor, q.L.f.classname), "rocket");
  const bool player = q.is(q.i(attacker, q.L.f.classname), "player");
  if (rocket & player & !own) {
    const float rise = q.v(targ, q.L.f.origin).z - q.f(inflictor, f_fired_z);
    const int frags = mid::midair_score(height, rise, midair_level_now(q));
    if (frags > 0) {
      q.set_g(g_midair_frags, static_cast<float>(frags));
      q.set_g(g_midair_rise, rise);
      q.set_f(targ, q.L.f.armorvalue, 0);
      q.set_f(targ, q.L.f.armortype, 0);
      return mid::kMidairKill;
    }
  }
  // The rest wears the armour as usual and spares the health.
  float saved = q.ceil(q.f(targ, q.L.f.armortype) * amount);
  if (saved >= q.f(targ, q.L.f.armorvalue)) {
    saved = q.f(targ, q.L.f.armorvalue);
    q.set_f(targ, q.L.f.armortype, 0);
    const float items = q.f(targ, q.L.f.items);
    q.set_f(targ, q.L.f.items, items - band(items, kItArmorAll));
  }
  q.set_f(targ, q.L.f.armorvalue, q.f(targ, q.L.f.armorvalue) - saved);
  q.set_f(targ, q.L.f.dmg_save, q.f(targ, q.L.f.dmg_save) + saved);
  q.set_i(targ, q.L.f.dmg_inflictor, inflictor);
  return -1;
}

void midair_kill(A& q, int32_t targ, int32_t attacker, int32_t told, int32_t how) {
  const float frags = q.g(g_midair_frags);
  q.set_g(g_midair_frags, 0);
  if (frags > 0) {
    q.set_f(attacker, q.L.f.frags, q.f(attacker, q.L.f.frags) + frags);
    note_frag(q, attacker, targ);
    const float rise = q.g(g_midair_rise);
    say_all(q, kPrintMedium,
            {midair_words(q, mid::midair_band(rise)), q.i(attacker, q.L.f.netname), q.s(s_on),
             q.i(targ, q.L.f.netname),
             q.strcat({q.s(s_open_bracket), q.ftos(q.rint(rise)), q.s(s_up_nl)})});
    return;
  }
  // Any other kill of a player (a telefrag): told, and worth nothing.
  if (!q.is(told, "")) {
    say_all(q, kPrintMedium, {told});
    return;
  }
  if (q.is(how, "")) how = q.s(s_fragged);
  say_all(q, kPrintMedium,
          {q.i(targ, q.L.f.netname), q.s(s_was), how, q.s(s_by), q.i(attacker, q.L.f.netname),
           q.s(s_nl)});
}

}  // namespace shooters_edge::server::rules::SE_RULES_NS
