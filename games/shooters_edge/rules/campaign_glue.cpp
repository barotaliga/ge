// The campaign's glue: what the single-player form does where the two forms differ. Printing has
// no level, effects are already in the broadcast when they are written, there is no frag log and
// no `newmis`. The player's inventory is carried from map to map in the spawn parms; he starts on
// the map's start (a coop start in coop); and a death restarts the map with what he entered it
// with, or in coop puts him back on a coop start with a fresh inventory. The form's program jumps
// for itself (the move never does), the water jump with it.
//
// ge:summary The campaign's single-player glue in C++: prints, effects, sides, parms, spawns, jumps
// ge:keywords campaign native-campaign say_all same_team spawn_point coop_spot give_inventory
// ge:keywords player_jump water_jump_check setnewparms setchangeparms spawn-parms
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_rules_test.cpp
#include <cstdint>

#include "shooters_edge/rules/campaign_decl.hpp"

namespace shooters_edge::server::rules::campaign {

void say_all(A& q, float, std::initializer_list<int32_t> text) {
  int32_t s[6] = {};
  int k = 0;
  for (int32_t t : text) s[k++] = t;
  q.bprint({s[0], s[1], s[2], s[3], s[4], s[5]});
}

void say_to(A& q, int32_t to, float, std::initializer_list<int32_t> text) {
  int32_t s[4] = {};
  int k = 0;
  for (int32_t t : text) s[k++] = t;
  q.sprint(to, {s[0], s[1], s[2], s[3]});
}

// An item's name: the form has no local info to read a game's own words from, so always ours.
int32_t name_of(A&, int32_t, int32_t fallback) { return fallback; }

// A loaded game's own words: none on this form (the mod's own words always).
int32_t game_words(A& q, int32_t, int32_t, int32_t, int32_t, int32_t) { return q.s(s_empty); }
bool game_quiet(A&) { return false; }
// A blast's pellets on one body land as one blow, as the classic single-player game has them.
bool pellets_as_one_blow() { return true; }
int32_t death_words(A& q, int32_t, int32_t) { return q.s(s_empty); }

void effect_send(A&, Vec3, float) {}

// A puff where a shot hits: a burst of red particles on something that bleeds (the form has no
// blood message), eight drops a pellet thrown back off the wound (along the shot, twice as hard
// out of the body's side it struck, jittered across), a chip of the wall elsewhere (its gunshot
// carries no count).
void effect_impact(A& q, Vec3 org, float count, float bleeds) {
  if (on(bleeds)) {
    Vec3 way = fv(4, q.v_forward()) + fv(8, q.gv(q.L.g.trace_plane_normal));
    const float r1 = 8 * q.random() - 4;
    way = way + fv(r1, q.v_right());
    const float r2 = 8 * q.random() - 4;
    way = way + fv(r2, q.v_up());
    q.particle(org, way, 73, count * 8);
    return;
  }
  q.WriteByte(kMsgBroadcast, kSvcTempentity);
  q.WriteByte(kMsgBroadcast, kTeGunshot);
  q.WriteCoord(kMsgBroadcast, org.x);
  q.WriteCoord(kMsgBroadcast, org.y);
  q.WriteCoord(kMsgBroadcast, org.z);
}

void note_frag(A&, int32_t, int32_t) {}

void missile_now(A&, int32_t) {}

// In coop the players are one side unless `sv_friendlyfire` is on: one player's shots never harm
// another (his own blast still harms himself). Monsters are on no side.
bool same_team(A& q, int32_t a, int32_t b) {
  const bool friendly = q.cvar(q.s(s_sv_friendlyfire)) != 0;  // asked either way
  if ((q.coop() == 0) | (a == b) | friendly) return false;
  return (band(q.f(a, q.L.f.flags), kFlClient) != 0) & (band(q.f(b, q.L.f.flags), kFlClient) != 0);
}

// A teammate's blow that does not harm him pushes him only with `sv_friendlypush` on.
bool team_shoves(A& q, int32_t, int32_t) { return on(q.cvar(q.s(s_sv_friendlypush))); }
void drop_on_death(A&) {}

// A taken item is gone for good in single player and co-op; only a deathmatch brings it back.
bool items_come_back(A& q) { return q.deathmatch() != 0; }

namespace {

// A ledge ahead at the water's edge: the jump out (checked at water level 2, every frame).
void water_jump_check(A& q) {
  const int32_t me = q.self();
  q.makevectors(q.v(me, q.L.f.angles));
  Vec3 start = q.v(me, q.L.f.origin);
  start.z = start.z + 8;
  Vec3 fwd = q.v_forward();
  fwd.z = 0;
  q.set_gv(q.L.g.v_forward, fwd);
  Vec3 end = start + fwd * 24;  // not renormalised: a little short when the body is pitched
  q.traceline(start, end, 1, me);
  if (q.gf(q.L.g.trace_fraction) == 1) return;
  start.z = start.z + q.v(me, q.L.f.maxs).z - 8;
  end = start + q.v_forward() * 24;
  q.set_v(me, q.L.f.movedir, q.gv(q.L.g.trace_plane_normal) * -50);
  q.traceline(start, end, 1, me);
  if (q.gf(q.L.g.trace_fraction) < 1) return;
  q.set_f(me, q.L.f.flags, bor(q.f(me, q.L.f.flags), kFlWaterjump));
  Vec3 vel = q.v(me, q.L.f.velocity);
  vel.z = 225;
  q.set_v(me, q.L.f.velocity, vel);
  const float flags = q.f(me, q.L.f.flags);
  q.set_f(me, q.L.f.flags, flags - band(flags, kFlJumpreleased));
  q.set_f(me, q.L.f.teleport_time, q.time() + 2);
}

void set_velocity_z(A& q, int32_t e, float z) {
  Vec3 vel = q.v(e, q.L.f.velocity);
  vel.z = z;
  q.set_v(e, q.L.f.velocity, vel);
}

// The next coop start after the last one used, round the map's list.
int32_t next_spot(A& q, int32_t kind) {
  int32_t spot = q.find(q.ge(g_last_spawn), q.L.f.classname, kind);
  if (spot == 0) spot = q.find(q.world(), q.L.f.classname, kind);
  if (spot != 0) q.set_ge(g_last_spawn, spot);
  return spot;
}

// A spot no living player stands on (within a player's width), self aside.
bool spot_free(A& q, int32_t spot) {
  for (int32_t e = q.findradius(q.v(spot, q.L.f.origin), 40); e != 0; e = q.i(e, q.L.f.chain)) {
    if ((e != q.self()) & (band(q.f(e, q.L.f.flags), kFlClient) != 0) &
        (q.f(e, q.L.f.health) > 0))
      return false;
  }
  return true;
}

// In coop the next coop start no one stands on (round the map's list; the map's start counts as
// one), the next in turn when every one is taken.
int32_t coop_spot(A& q) {
  const int32_t first = next_spot(q, q.s(s_info_player_coop));
  int32_t spot = first;
  float n = 0;
  while ((spot != 0) & (n < 16)) {
    if (spot_free(q, spot)) return spot;
    spot = next_spot(q, q.s(s_info_player_coop));
    if (spot == first) spot = q.world();
    n = n + 1;
  }
  spot = q.find(q.world(), q.L.f.classname, q.s(s_info_player_start));
  const bool free = spot_free(q, spot);  // asked either way
  if ((spot != 0) & free) return spot;
  return first;
}

}  // namespace

// Before the move: the water jump's check, then the jump button: swimming up in deep water, a jump
// of 270 off the ground once the button was let go since the last.
void player_jump(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.waterlevel) == 2) water_jump_check(q);
  if (!on(q.f(me, q.L.f.button2))) {
    q.set_f(me, q.L.f.flags, bor(q.f(me, q.L.f.flags), kFlJumpreleased));
    return;
  }
  if (band(q.f(me, q.L.f.flags), kFlWaterjump) != 0) return;
  if (q.f(me, q.L.f.waterlevel) >= 2) {
    const float wt = q.f(me, q.L.f.watertype);
    if (wt == kContentWater) set_velocity_z(q, me, 100);
    else if (wt == kContentSlime) set_velocity_z(q, me, 80);
    else set_velocity_z(q, me, 50);
    swim_sound(q);
    return;
  }
  const float flags = q.f(me, q.L.f.flags);
  if ((band(flags, kFlOnground) == 0) | (band(flags, kFlJumpreleased) == 0)) return;
  float f = flags - band(flags, kFlJumpreleased);
  q.set_f(me, q.L.f.flags, f);
  f = q.f(me, q.L.f.flags);
  q.set_f(me, q.L.f.flags, f - band(f, kFlOnground));
  set_velocity_z(q, me, q.v(me, q.L.f.velocity).z + 270);
  jump_sound(q);
}

// THE SPAWN PARMS: the inventory carried from map to map. parm1 the items, parm2 health, parm3
// armour, parm4..7 shells, nails, rockets and cells, parm8 the weapon held, parm9 the armour's
// share x 100.
void SetNewParms(A& q) {
  q.set_gf(q.parm(1), static_cast<float>(kWBlade | kWScatter));
  q.set_gf(q.parm(2), 100);
  q.set_gf(q.parm(3), 0);
  q.set_gf(q.parm(4), 25);
  q.set_gf(q.parm(5), 0);
  q.set_gf(q.parm(6), 0);
  q.set_gf(q.parm(7), 0);
  q.set_gf(q.parm(8), kWScatter);
  q.set_gf(q.parm(9), 0);
}

// Leaving a map keeps the weapons, the ammunition and the armour; the keys and the powers stay
// behind, health comes back to 50..100 and the shells to at least 25. A dead player starts anew.
void SetChangeParms(A& q) {
  const int32_t me = q.self();
  if (q.f(me, q.L.f.health) <= 0) {
    SetNewParms(q);
    return;
  }
  const float items = q.f(me, q.L.f.items);
  q.set_f(me, q.L.f.items,
          items - band(items, static_cast<float>(kItKey1 | kItKey2 | kItQuad | kItInvulnerability)));
  if (q.f(me, q.L.f.health) > 100) q.set_f(me, q.L.f.health, 100);
  if (q.f(me, q.L.f.health) < 50) q.set_f(me, q.L.f.health, 50);
  q.set_gf(q.parm(1), q.f(me, q.L.f.items));
  q.set_gf(q.parm(2), q.f(me, q.L.f.health));
  q.set_gf(q.parm(3), q.f(me, q.L.f.armorvalue));
  if (q.f(me, q.L.f.ammo_shells) < 25) q.set_gf(q.parm(4), 25);
  else q.set_gf(q.parm(4), q.f(me, q.L.f.ammo_shells));
  q.set_gf(q.parm(5), q.f(me, q.L.f.ammo_nails));
  q.set_gf(q.parm(6), q.f(me, q.L.f.ammo_rockets));
  q.set_gf(q.parm(7), q.f(me, q.L.f.ammo_cells));
  q.set_gf(q.parm(8), q.f(me, q.L.f.weapon));
  q.set_gf(q.parm(9), q.f(me, q.L.f.armortype) * 100);
}

// A coop start in coop, a deathmatch spawn point in deathmatch, else the map's start; the start
// when the map has none of the kind asked for.
int32_t spawn_point(A& q) {
  int32_t spot = q.world();
  if (on(q.coop())) spot = coop_spot(q);
  else if (on(q.deathmatch())) spot = next_spot(q, q.s(s_info_player_deathmatch));
  if (spot == 0) spot = q.find(q.world(), q.L.f.classname, q.s(s_info_player_start));
  if (spot == 0) q.error({q.s(s_the_map_has_no)});
  return spot;
}

// The inventory the parms carry; afresh on the start map once an episode is done (a rune held).
void give_inventory(A& q) {
  if (q.is(q.mapname(), "start") & (q.serverflags() != 0)) SetNewParms(q);
  const int32_t me = q.self();
  q.set_f(me, q.L.f.items, q.gf(q.parm(1)));
  q.set_f(me, q.L.f.health, q.gf(q.parm(2)));
  q.set_f(me, q.L.f.armorvalue, q.gf(q.parm(3)));
  q.set_f(me, q.L.f.ammo_shells, q.gf(q.parm(4)));
  q.set_f(me, q.L.f.ammo_nails, q.gf(q.parm(5)));
  q.set_f(me, q.L.f.ammo_rockets, q.gf(q.parm(6)));
  q.set_f(me, q.L.f.ammo_cells, q.gf(q.parm(7)));
  q.set_f(me, q.L.f.armortype, q.gf(q.parm(9)) / 100);
  weapon_set(q, q.gf(q.parm(8)));
}

void player_respawn(A& q) {
  if ((q.coop() != 0) | (q.deathmatch() != 0)) {
    q.set_f(q.self(), q.L.f.deadflag, kDeadRespawnable);
    SetNewParms(q);
    PutClientInServer(q);
    return;
  }
  q.localcmd(q.s(s_restart_n));
}

}  // namespace shooters_edge::server::rules::campaign
