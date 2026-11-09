// THE WEAPONS: eight slots, each a bit in .items when carried and in .weapon when held, filled by
// the ARSENAL: one row of data a slot (games/shooters_edge/profiles/arsenal.hpp), which says how
// the slot's weapon fires and with what numbers, models and sounds. The game puts its rows in the
// server's local info (`weapon1` .. `weapon8`); a slot with none keeps the row below, our own.
// Impulses 1..8 pick one, 10 the next one carried; a weapon out of ammunition gives way to the
// best one that has some. A row's fields, a space between ("-" is none):
//
//   0 fire      melee, pellets, spike, grenade, rocket or beam: the code below that fires it
//   1 ammo      none, shells, nails, rockets or cells          2 cost   a shot's ammunition
//   3 refire    s between shots     4 count   pellets a shot     5 damage   each pellet's, a hit's
//   6 spread    the pellets' cone   7 reach   a cut's, a beam's or a pellet's reach; a missile's speed
//   8 life      s a missile flies   9 splash  a blast's radius damage; a beam's discharge a cell
//  10 view      its view model     11 fire sound   12 missile model   13 land sound  14 blast sound
//  15 pickup    the model it lies on a map as          16 pickup ammo   the ammunition with it

// --- the arsenal ---

// Our own rows: what a slot holds when the game gives none.
string(float slot) weapon_own_row = {
  if (slot == 1) return "melee none 0 0.5 1 20 0 64 0 0 progs/v_axe.mdl weapons/ax1.wav - player/axhit2.wav - - 0";
  if (slot == 2) return "pellets shells 1 0.5 6 4 0.04 2048 0 0 progs/v_shot.mdl weapons/guncock.wav - - - - 0";
  if (slot == 3) return "pellets shells 2 0.7 14 4 0.07 2048 0 0 progs/v_shot2.mdl weapons/shotgn2.wav - - - progs/g_shot.mdl 5";
  if (slot == 4) return "spike nails 1 0.1 1 9 0 1000 6 0 progs/v_nail.mdl weapons/rocket1i.wav progs/spike.mdl - - progs/g_nail.mdl 30";
  if (slot == 5) return "spike nails 2 0.1 1 18 0 1000 6 0 progs/v_nail2.mdl weapons/spike2.wav progs/s_spike.mdl - - progs/g_nail2.mdl 30";
  if (slot == 6) return "grenade rockets 1 0.6 1 0 0 600 2.5 120 progs/v_rock.mdl weapons/grenade.wav progs/grenade.mdl weapons/bounce.wav weapons/r_exp3.wav progs/g_rock.mdl 5";
  if (slot == 7) return "rocket rockets 1 0.8 1 100 0 1000 5 120 progs/v_rock2.mdl weapons/sgun1.wav progs/missile.mdl - weapons/r_exp3.wav progs/g_rock2.mdl 5";
  return "beam cells 1 0.1 1 30 0 600 0 35 progs/v_light.mdl weapons/lhit.wav - - - progs/g_light.mdl 15";
};

// The rows in force, and the two models of each kept for as long as the map runs (a model's name
// is held by what wears it, so it must outlive the frame).
string arsenal_row1, arsenal_row2, arsenal_row3, arsenal_row4;
string arsenal_row5, arsenal_row6, arsenal_row7, arsenal_row8;
string arsenal_view1, arsenal_view2, arsenal_view3, arsenal_view4;
string arsenal_view5, arsenal_view6, arsenal_view7, arsenal_view8;
string arsenal_missile1, arsenal_missile2, arsenal_missile3, arsenal_missile4;
string arsenal_missile5, arsenal_missile6, arsenal_missile7, arsenal_missile8;

// The setting that holds weapon slot `slot`'s row.
string(float slot) arsenal_key = {
  if (slot == 1) return "arsenal_weapon1";
  if (slot == 2) return "arsenal_weapon2";
  if (slot == 3) return "arsenal_weapon3";
  if (slot == 4) return "arsenal_weapon4";
  if (slot == 5) return "arsenal_weapon5";
  if (slot == 6) return "arsenal_weapon6";
  if (slot == 7) return "arsenal_weapon7";
  return "arsenal_weapon8";
};

// Slot `slot`'s row taken from the game's setting, else our own, and kept.
void(float slot) arsenal_keep = {
  local string row, view, missile;
  row = cvar_string(arsenal_key(slot));
  if (row == "") row = weapon_own_row(slot);
  row = strzone(row);
  tokenize(row);
  view = strzone(argv(10));
  missile = strzone(argv(12));
  if (slot == 1) { arsenal_row1 = row; arsenal_view1 = view; arsenal_missile1 = missile; }
  if (slot == 2) { arsenal_row2 = row; arsenal_view2 = view; arsenal_missile2 = missile; }
  if (slot == 3) { arsenal_row3 = row; arsenal_view3 = view; arsenal_missile3 = missile; }
  if (slot == 4) { arsenal_row4 = row; arsenal_view4 = view; arsenal_missile4 = missile; }
  if (slot == 5) { arsenal_row5 = row; arsenal_view5 = view; arsenal_missile5 = missile; }
  if (slot == 6) { arsenal_row6 = row; arsenal_view6 = view; arsenal_missile6 = missile; }
  if (slot == 7) { arsenal_row7 = row; arsenal_view7 = view; arsenal_missile7 = missile; }
  if (slot == 8) { arsenal_row8 = row; arsenal_view8 = view; arsenal_missile8 = missile; }
};

// The number key of weapon bit `w` (1..8), and the order the best-weapon choice prefers.
float(float w) weapon_rank = {
  if (w == W_BLADE) return 1;
  if (w == W_SCATTER) return 2;
  if (w == W_DOUBLE) return 3;
  if (w == W_SPIKER) return 4;
  if (w == W_HEAVY_SPIKER) return 5;
  if (w == W_GRENADES) return 6;
  if (w == W_ROCKETS) return 7;
  if (w == W_ARC) return 8;
  return 0;
};

float(float rank) weapon_of_rank = {
  if (rank == 1) return W_BLADE;
  if (rank == 2) return W_SCATTER;
  if (rank == 3) return W_DOUBLE;
  if (rank == 4) return W_SPIKER;
  if (rank == 5) return W_HEAVY_SPIKER;
  if (rank == 6) return W_GRENADES;
  if (rank == 7) return W_ROCKETS;
  if (rank == 8) return W_ARC;
  return 0;
};

string(float w) weapon_row = {
  local float r;
  r = weapon_rank(w);
  if (r == 1) return arsenal_row1;
  if (r == 2) return arsenal_row2;
  if (r == 3) return arsenal_row3;
  if (r == 4) return arsenal_row4;
  if (r == 5) return arsenal_row5;
  if (r == 6) return arsenal_row6;
  if (r == 7) return arsenal_row7;
  return arsenal_row8;
};

// Field `i` of weapon `w`'s row: as a word (good until the next row is read) and as a number.
string(float w, float i) weapon_word = {
  tokenize(weapon_row(w));
  return argv(i);
};
float(float w, float i) weapon_number = { return number_of(weapon_word(w, i)); };

float(float w) weapon_cost = { return weapon_number(w, 2); };
float(float w) weapon_delay = { return weapon_number(w, 3); };

string(float w) weapon_model = {
  local float r;
  r = weapon_rank(w);
  if (r == 1) return arsenal_view1;
  if (r == 2) return arsenal_view2;
  if (r == 3) return arsenal_view3;
  if (r == 4) return arsenal_view4;
  if (r == 5) return arsenal_view5;
  if (r == 6) return arsenal_view6;
  if (r == 7) return arsenal_view7;
  return arsenal_view8;
};

string(float w) weapon_missile = {
  local float r;
  r = weapon_rank(w);
  if (r == 1) return arsenal_missile1;
  if (r == 2) return arsenal_missile2;
  if (r == 3) return arsenal_missile3;
  if (r == 4) return arsenal_missile4;
  if (r == 5) return arsenal_missile5;
  if (r == 6) return arsenal_missile6;
  if (r == 7) return arsenal_missile7;
  return arsenal_missile8;
};

// A sound of weapon `w` (field `i`) played on `e`, when its row has one.
void(entity e, float w, float i) weapon_sound = {
  local string snd;
  snd = weapon_word(w, i);
  if (snd != "-") sound(e, CHAN_WEAPON, snd, 1, ATTN_NORM);
};

// The ammunition `self` has for weapon `w`, and its status-bar bit.
float(float w) ammo_for = {
  local string kind;
  kind = weapon_word(w, 1);
  if (kind == "shells") return self.ammo_shells;
  if (kind == "nails") return self.ammo_nails;
  if (kind == "rockets") return self.ammo_rockets;
  if (kind == "cells") return self.ammo_cells;
  return 1;
};

float(float w) ammo_bit = {
  local string kind;
  kind = weapon_word(w, 1);
  if (kind == "shells") return IT_SHELLS;
  if (kind == "nails") return IT_NAILS;
  if (kind == "rockets") return IT_ROCKETS;
  if (kind == "cells") return IT_CELLS;
  return 0;
};

void(float w, float n) ammo_spend = {
  local string kind;
  kind = weapon_word(w, 1);
  if (kind == "shells") self.ammo_shells = self.ammo_shells - n;
  else if (kind == "nails") self.ammo_nails = self.ammo_nails - n;
  else if (kind == "rockets") self.ammo_rockets = self.ammo_rockets - n;
  else if (kind == "cells") self.ammo_cells = self.ammo_cells - n;
};

// --- holding a weapon ---

// The status bar's ammunition and the view model for the held weapon.
void() weapon_show = {
  self.currentammo = ammo_for(self.weapon);
  self.items = self.items - (self.items & (IT_SHELLS | IT_NAILS | IT_ROCKETS | IT_CELLS));
  self.items = self.items | ammo_bit(self.weapon);
  self.weaponmodel = weapon_model(self.weapon);
  self.weaponframe = 0;
};

void(float w) weapon_set = {
  self.weapon = w;
  weapon_show();
};

// The best carried weapon with ammunition: the highest rank, the arc only out of water.
void() weapon_select_best = {
  local float rank, w;
  rank = 8;
  while (rank > 1) {
    w = weapon_of_rank(rank);
    if ((self.items & w) && ammo_for(w) >= weapon_cost(w)) {
      if (weapon_word(w, 0) != "beam" || self.waterlevel <= 1) {
        weapon_set(w);
        return;
      }
    }
    rank = rank - 1;
  }
  weapon_set(W_BLADE);
};

// Impulses: 1..8 pick that weapon when carried and loaded; 10 the next carried one.
void() weapon_impulse = {
  local float w, rank, tries;
  if (self.impulse >= 1 && self.impulse <= 8) {
    w = weapon_of_rank(self.impulse);
    if (!(self.items & w)) {
      say_to(self, PRINT_HIGH, "no weapon\n", "", "", "");
    } else if (ammo_for(w) < weapon_cost(w)) {
      say_to(self, PRINT_HIGH, "not enough ammo\n", "", "", "");
    } else {
      weapon_set(w);
    }
  } else if (self.impulse == 10) {
    rank = weapon_rank(self.weapon);
    for (tries = 0; tries < 8; tries += 1) {
      rank = rank + 1;
      if (rank > 8) rank = 1;
      w = weapon_of_rank(rank);
      if ((self.items & w) && ammo_for(w) >= weapon_cost(w)) {
        weapon_set(w);
        break;
      }
    }
  }
  self.impulse = 0;
};

// --- firing ---

// A hitscan line from the eye along v_forward with a spread, `reach` long: what it hits takes
// `points`. Where the game says so (`pellets_as_one_blow`: the campaign), the pellets that strike
// one body in a row land as ONE blow, their points summed (a pellet on another body lands the sum
// so far first), so a blast that kills does it with all of its weight: the double barrel at close
// range bursts a body the single pellets would only fell. Each pellet still bleeds where it
// struck.
void(float pellets, float points, float spread, float reach) fire_pellets_far = {
  local vector from, dir, to;
  local float i, summed;
  local entity struck;
  from = self.origin + self.view_ofs;
  struck = world;
  summed = 0;
  for (i = 0; i < pellets; i += 1) {
    dir = v_forward + crandom() * spread * v_right + crandom() * spread * v_up;
    to = from + dir * reach;
    traceline(from, to, FALSE, self);
    if (trace_fraction < 1) {
      if (trace_ent.takedamage) {
        effect_impact(trace_endpos, 1, TRUE);
        if (!pellets_as_one_blow()) {
          trace_ent.deathtype = "shot";
          damage(trace_ent, self, self, points);
        } else {
          if (trace_ent != struck) {
            if (summed > 0) damage(struck, self, self, summed);
            struck = trace_ent;
            struck.deathtype = "shot";
            summed = 0;
          }
          summed = summed + points;
        }
      } else {
        effect_impact(trace_endpos, 1, FALSE);
      }
    }
  }
  if (summed > 0) damage(struck, self, self, summed);
};

void(float pellets, float points, float spread) fire_pellets = {
  fire_pellets_far(pellets, points, spread, 2048);
};

// A cut `reach` in front: `points` to what bleeds, the weapon's land sound on a wall.
void(float w) fire_blade = {
  local vector from;
  from = self.origin + self.view_ofs;
  traceline(from, from + v_forward * weapon_number(w, 7), FALSE, self);
  if (trace_fraction == 1) return;
  if (trace_ent.takedamage) {
    trace_ent.deathtype = "cut down";
    damage(trace_ent, self, self, weapon_number(w, 5));
    effect_impact(trace_endpos, 3, TRUE);
  } else {
    weapon_sound(self, w, 13);
    effect_impact(trace_endpos, 3, FALSE);
  }
};

// A missile: spawned at the eye, flying along v_forward at `spd`, removed after `life` seconds.
entity(string mdl, float mtype, float spd, float life) launch = {
  local entity m;
  m = spawn();
  m.owner = self;
  m.movetype = mtype;
  m.solid = SOLID_BBOX;
  m.velocity = v_forward * spd;
  m.angles = vectoangles(m.velocity);
  m.nextthink = time + life;
  m.think = remove_self;
  setmodel(m, mdl);
  setsize(m, '0 0 0', '0 0 0');
  setorigin(m, self.origin + self.view_ofs + v_forward * 8 - '0 0 8');
  missile_now(m);
  return m;
};

// Weapon `w`'s missile, at its row's speed and life; it carries `w` in .weapon so its touch and
// its blast read the same row.
entity(float w, float mtype) launch_weapon = {
  local entity m;
  m = launch(weapon_missile(w), mtype, weapon_number(w, 7), weapon_number(w, 8));
  m.weapon = w;
  return m;
};

void() spike_touch = {
  if (other == self.owner) return;
  if (pointcontents(self.origin) == CONTENT_SKY) {
    remove(self);
    return;
  }
  if (other.takedamage) {
    effect_impact(self.origin, 1, TRUE);
    other.deathtype = "nailed";
    damage(other, self, self.owner, self.dmg);
  } else {
    WriteByte(MSG_EFFECT, SVC_TEMPENTITY);
    WriteByte(MSG_EFFECT, TE_SPIKE);
    WriteCoord(MSG_EFFECT, self.origin.x);
    WriteCoord(MSG_EFFECT, self.origin.y);
    WriteCoord(MSG_EFFECT, self.origin.z);
    effect_send(self.origin, MULTICAST_PHS);
  }
  remove(self);
};

void(float w) fire_spike = {
  local entity m;
  m = launch_weapon(w, MOVETYPE_FLYMISSILE);
  m.classname = "spike";
  m.dmg = weapon_number(w, 5);
  m.touch = spike_touch;
};

// A missile's blast where it is: its row's splash to everything near but `spare`, its sound.
void(entity spare) blast_at = {
  local string snd;
  radius_damage(self, self.owner, weapon_number(self.weapon, 9), spare, self.deathtype);
  snd = weapon_word(self.weapon, 14);
  if (snd != "-") sound(self, CHAN_VOICE, snd, 1, ATTN_NORM);
  effect_explosion(self.origin);
  remove(self);
};

void() blast = { blast_at(world); };

void() rocket_touch = {
  if (other == self.owner) return;
  if (pointcontents(self.origin) == CONTENT_SKY) {
    remove(self);
    return;
  }
  if (other.takedamage) {
    other.deathtype = "blown apart";
    damage(other, self, self.owner, weapon_number(self.weapon, 5));
  }
  // Back the blast off the wall so its centre is in the open.
  self.origin = self.origin - normalize(self.velocity) * 8;
  self.deathtype = "blown apart";
  blast_at(other);
};

void() grenade_touch = {
  local string snd;
  if (other == self.owner) return;
  if (other.takedamage == DAMAGE_AIM) {
    blast();
    return;
  }
  snd = weapon_word(self.weapon, 13);
  if (snd != "-") sound(self, CHAN_VOICE, snd, 1, ATTN_NORM);
  if (self.velocity == '0 0 0') self.avelocity = '0 0 0';
};

void(float w) fire_grenade = {
  local entity m;
  m = launch_weapon(w, MOVETYPE_BOUNCE);
  m.classname = "grenade";
  m.velocity = m.velocity + '0 0 200';
  m.avelocity = '300 300 300';
  m.angles = vectoangles(m.velocity);
  m.touch = grenade_touch;
  m.think = blast;
  m.deathtype = "gibbed by a grenade";
};

void(float w) fire_rocket = {
  local entity m;
  m = launch_weapon(w, MOVETYPE_FLYMISSILE);
  m.classname = "rocket";
  m.touch = rocket_touch;
};

// A beam its reach long; in water it discharges: its splash a cell to everything near.
void(float w) fire_arc = {
  local vector from;
  local float reach;
  reach = weapon_number(w, 7);
  from = self.origin + self.view_ofs;
  traceline(from, from + v_forward * reach, TRUE, self);
  effect_beam(from, trace_endpos);
  if (self.waterlevel > 1) {
    // Discharged into water: everything near takes it, the firer too.
    self.deathtype = "discharged";
    radius_damage(self, self, weapon_number(w, 9) * ammo_for(w), world, "electrocuted");
    ammo_spend(w, ammo_for(w));
    weapon_select_best();
    return;
  }
  traceline(from, from + v_forward * reach, FALSE, self);
  if (trace_ent.takedamage) {
    effect_impact(trace_endpos, 3, TRUE);
    trace_ent.deathtype = "shafted";
    damage(trace_ent, self, self, weapon_number(w, 5));
  }
};

// Fires the held weapon when attack is down and it is ready: its row says how.
void() weapon_frame = {
  local float w;
  local string fire;
  if (time < self.attack_finished) return;
  if (self.deadflag != DEAD_NO) return;
  if (!self.button0) return;
  w = self.weapon;
  if (ammo_for(w) < weapon_cost(w)) {
    weapon_select_best();
    return;
  }
  makevectors(self.v_angle);
  self.attack_finished = time + weapon_delay(w);
  self.show_hostile = time + 1;  // the noise wakes monsters that hear it
  ammo_spend(w, weapon_cost(w));
  self.effects = self.effects | EF_MUZZLEFLASH;
  weapon_sound(self, w, 11);
  fire = weapon_word(w, 0);
  if (fire == "melee") fire_blade(w);
  else if (fire == "pellets")
    fire_pellets_far(weapon_number(w, 4), weapon_number(w, 5), weapon_number(w, 6),
                     weapon_number(w, 7));
  else if (fire == "spike") fire_spike(w);
  else if (fire == "grenade") fire_grenade(w);
  else if (fire == "rocket") fire_rocket(w);
  else if (fire == "beam") fire_arc(w);
  weapon_show();
  if (ammo_for(w) < weapon_cost(w)) weapon_select_best();
};

// Reads the arsenal and loads what its rows name: every view model, then every missile, then
// every sound.
void() weapon_precache = {
  local float slot, i;
  local string name;
  for (slot = 1; slot <= 8; slot += 1) arsenal_keep(slot);
  for (slot = 1; slot <= 8; slot += 1) precache_model(weapon_model(weapon_of_rank(slot)));
  for (slot = 1; slot <= 8; slot += 1) {
    name = weapon_missile(weapon_of_rank(slot));
    if (name != "-") precache_model(name);
  }
  for (slot = 1; slot <= 8; slot += 1) {
    for (i = 11; i <= 14; i += 1) {
      if (i != 12) {
        name = weapon_word(weapon_of_rank(slot), i);
        if (name != "-") precache_sound(name);
      }
    }
  }
};
