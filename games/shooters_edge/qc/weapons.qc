// THE WEAPONS: eight, each a bit in .items when carried and in .weapon when held. A table in
// functions: what each costs, how often it fires, what it does. Impulses 1..8 pick one, 10 the
// next one carried; a weapon out of ammunition gives way to the best one that has some.
//
//   1 blade          no ammunition   a cut in front, 20
//   2 scattergun     1 shell         6 pellets of 4 in a 4 % cone
//   3 double         2 shells        14 pellets of 4 in a 7 % cone
//   4 spiker         1 nail          a spike at 1000 u/s, 9
//   5 heavy spiker   2 nails         a heavy spike at 1000 u/s, 18
//   6 grenades       1 rocket        a bouncing grenade, 2.5 s fuse, blast 120
//   7 rockets        1 rocket        a rocket at 1000 u/s, 100 to what it hits, blast 120
//   8 arc            1 cell a tenth  a beam 600 units long, 30 a tenth of a second

// --- the table ---

float(float w) weapon_cost = {
  if (w == W_DOUBLE || w == W_HEAVY_SPIKER) return 2;
  if (w == W_BLADE) return 0;
  return 1;
};

float(float w) weapon_delay = {
  if (w == W_BLADE) return 0.5;
  if (w == W_SCATTER) return 0.5;
  if (w == W_DOUBLE) return 0.7;
  if (w == W_SPIKER || w == W_HEAVY_SPIKER) return 0.1;
  if (w == W_GRENADES) return 0.6;
  if (w == W_ROCKETS) return 0.8;
  return 0.1;
};

// The weapon's number (impulse 1..8), and the order the best-weapon choice prefers.
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

// The ammunition `self` has for weapon `w`.
float(float w) ammo_for = {
  if (w == W_SCATTER || w == W_DOUBLE) return self.ammo_shells;
  if (w == W_SPIKER || w == W_HEAVY_SPIKER) return self.ammo_nails;
  if (w == W_GRENADES || w == W_ROCKETS) return self.ammo_rockets;
  if (w == W_ARC) return self.ammo_cells;
  return 1;
};

void(float w, float n) ammo_spend = {
  if (w == W_SCATTER || w == W_DOUBLE) self.ammo_shells = self.ammo_shells - n;
  else if (w == W_SPIKER || w == W_HEAVY_SPIKER) self.ammo_nails = self.ammo_nails - n;
  else if (w == W_GRENADES || w == W_ROCKETS) self.ammo_rockets = self.ammo_rockets - n;
  else if (w == W_ARC) self.ammo_cells = self.ammo_cells - n;
};

string(float w) weapon_model = {
  if (w == W_BLADE) return "progs/v_axe.mdl";
  if (w == W_SCATTER) return "progs/v_shot.mdl";
  if (w == W_DOUBLE) return "progs/v_shot2.mdl";
  if (w == W_SPIKER) return "progs/v_nail.mdl";
  if (w == W_HEAVY_SPIKER) return "progs/v_nail2.mdl";
  if (w == W_GRENADES) return "progs/v_rock.mdl";
  if (w == W_ROCKETS) return "progs/v_rock2.mdl";
  return "progs/v_light.mdl";
};

// --- holding a weapon ---

// The status bar's ammunition and the view model for the held weapon.
void() weapon_show = {
  self.currentammo = ammo_for(self.weapon);
  self.items = self.items - (self.items & (IT_SHELLS | IT_NAILS | IT_ROCKETS | IT_CELLS));
  if (self.weapon == W_SCATTER || self.weapon == W_DOUBLE) self.items = self.items | IT_SHELLS;
  if (self.weapon == W_SPIKER || self.weapon == W_HEAVY_SPIKER) self.items = self.items | IT_NAILS;
  if (self.weapon == W_GRENADES || self.weapon == W_ROCKETS) self.items = self.items | IT_ROCKETS;
  if (self.weapon == W_ARC) self.items = self.items | IT_CELLS;
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
      if (w != W_ARC || self.waterlevel <= 1) {
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
      sprint(self, PRINT_HIGH, "no weapon\n");
    } else if (ammo_for(w) < weapon_cost(w)) {
      sprint(self, PRINT_HIGH, "not enough ammo\n");
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

// A hitscan line from the eye along v_forward with a spread: what it hits takes `points`.
void(float pellets, float points, float spread) fire_pellets = {
  local vector from, dir, to;
  local float i;
  from = self.origin + self.view_ofs;
  for (i = 0; i < pellets; i += 1) {
    dir = v_forward + crandom() * spread * v_right + crandom() * spread * v_up;
    to = from + dir * 2048;
    traceline(from, to, FALSE, self);
    if (trace_fraction < 1) {
      if (trace_ent.takedamage) {
        effect_impact(trace_endpos, 1, TRUE);
        trace_ent.deathtype = "shot";
        damage(trace_ent, self, self, points);
      } else {
        effect_impact(trace_endpos, 1, FALSE);
      }
    }
  }
};

void() fire_blade = {
  local vector from;
  from = self.origin + self.view_ofs;
  traceline(from, from + v_forward * 64, FALSE, self);
  if (trace_fraction == 1) return;
  if (trace_ent.takedamage) {
    trace_ent.deathtype = "cut down";
    damage(trace_ent, self, self, 20);
    effect_impact(trace_endpos, 3, TRUE);
  } else {
    sound(self, CHAN_WEAPON, "player/axhit2.wav", 1, ATTN_NORM);
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
  newmis = m;
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
    WriteByte(MSG_MULTICAST, SVC_TEMPENTITY);
    WriteByte(MSG_MULTICAST, TE_SPIKE);
    WriteCoord(MSG_MULTICAST, self.origin.x);
    WriteCoord(MSG_MULTICAST, self.origin.y);
    WriteCoord(MSG_MULTICAST, self.origin.z);
    multicast(self.origin, MULTICAST_PHS);
  }
  remove(self);
};

void(float points) fire_spike = {
  local entity m;
  m = launch("progs/spike.mdl", MOVETYPE_FLYMISSILE, 1000, 6);
  m.classname = "spike";
  m.dmg = points;
  m.touch = spike_touch;
  if (points > 10) setmodel(m, "progs/s_spike.mdl");
  setsize(m, '0 0 0', '0 0 0');
};

void() blast = {
  radius_damage(self, self.owner, 120, world, self.deathtype);
  sound(self, CHAN_VOICE, "weapons/r_exp3.wav", 1, ATTN_NORM);
  effect_explosion(self.origin);
  remove(self);
};

void() rocket_touch = {
  if (other == self.owner) return;
  if (pointcontents(self.origin) == CONTENT_SKY) {
    remove(self);
    return;
  }
  if (other.takedamage) {
    other.deathtype = "blown apart";
    damage(other, self, self.owner, 100);
  }
  // Back the blast off the wall so its centre is in the open.
  self.origin = self.origin - normalize(self.velocity) * 8;
  self.deathtype = "blown apart";
  radius_damage(self, self.owner, 120, other, self.deathtype);
  sound(self, CHAN_VOICE, "weapons/r_exp3.wav", 1, ATTN_NORM);
  effect_explosion(self.origin);
  remove(self);
};

void() grenade_touch = {
  if (other == self.owner) return;
  if (other.takedamage == DAMAGE_AIM) {
    blast();
    return;
  }
  sound(self, CHAN_VOICE, "weapons/bounce.wav", 1, ATTN_NORM);
  if (self.velocity == '0 0 0') self.avelocity = '0 0 0';
};

void() fire_grenade = {
  local entity m;
  m = launch("progs/grenade.mdl", MOVETYPE_BOUNCE, 600, 2.5);
  m.classname = "grenade";
  m.velocity = m.velocity + '0 0 200';
  m.avelocity = '300 300 300';
  m.angles = vectoangles(m.velocity);
  m.touch = grenade_touch;
  m.think = blast;
  m.deathtype = "gibbed by a grenade";
};

void() fire_rocket = {
  local entity m;
  m = launch("progs/missile.mdl", MOVETYPE_FLYMISSILE, 1000, 5);
  m.classname = "rocket";
  m.touch = rocket_touch;
};

void() fire_arc = {
  local vector from;
  from = self.origin + self.view_ofs;
  traceline(from, from + v_forward * 600, TRUE, self);
  effect_beam(from, trace_endpos);
  if (self.waterlevel > 1) {
    // Discharged into water: everything near takes it, the firer too.
    self.deathtype = "discharged";
    radius_damage(self, self, 35 * self.ammo_cells, world, "electrocuted");
    self.ammo_cells = 0;
    weapon_select_best();
    return;
  }
  traceline(from, from + v_forward * 600, FALSE, self);
  if (trace_ent.takedamage) {
    effect_impact(trace_endpos, 3, TRUE);
    trace_ent.deathtype = "shafted";
    damage(trace_ent, self, self, 30);
  }
};

// Fires the held weapon when attack is down and it is ready.
void() weapon_frame = {
  local float w;
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
  ammo_spend(w, weapon_cost(w));
  self.effects = self.effects | EF_MUZZLEFLASH;
  if (w == W_BLADE) {
    sound(self, CHAN_WEAPON, "weapons/ax1.wav", 1, ATTN_NORM);
    fire_blade();
  } else if (w == W_SCATTER) {
    sound(self, CHAN_WEAPON, "weapons/guncock.wav", 1, ATTN_NORM);
    fire_pellets(6, 4, 0.04);
  } else if (w == W_DOUBLE) {
    sound(self, CHAN_WEAPON, "weapons/shotgn2.wav", 1, ATTN_NORM);
    fire_pellets(14, 4, 0.07);
  } else if (w == W_SPIKER) {
    sound(self, CHAN_WEAPON, "weapons/rocket1i.wav", 1, ATTN_NORM);
    fire_spike(9);
  } else if (w == W_HEAVY_SPIKER) {
    sound(self, CHAN_WEAPON, "weapons/spike2.wav", 1, ATTN_NORM);
    fire_spike(18);
  } else if (w == W_GRENADES) {
    sound(self, CHAN_WEAPON, "weapons/grenade.wav", 1, ATTN_NORM);
    fire_grenade();
  } else if (w == W_ROCKETS) {
    sound(self, CHAN_WEAPON, "weapons/sgun1.wav", 1, ATTN_NORM);
    fire_rocket();
  } else if (w == W_ARC) {
    sound(self, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
    fire_arc();
  }
  weapon_show();
  if (ammo_for(w) < weapon_cost(w)) weapon_select_best();
};

void() weapon_precache = {
  precache_model("progs/v_axe.mdl");
  precache_model("progs/v_shot.mdl");
  precache_model("progs/v_shot2.mdl");
  precache_model("progs/v_nail.mdl");
  precache_model("progs/v_nail2.mdl");
  precache_model("progs/v_rock.mdl");
  precache_model("progs/v_rock2.mdl");
  precache_model("progs/v_light.mdl");
  precache_model("progs/spike.mdl");
  precache_model("progs/s_spike.mdl");
  precache_model("progs/grenade.mdl");
  precache_model("progs/missile.mdl");
  precache_sound("weapons/ax1.wav");
  precache_sound("player/axhit2.wav");
  precache_sound("weapons/guncock.wav");
  precache_sound("weapons/shotgn2.wav");
  precache_sound("weapons/rocket1i.wav");
  precache_sound("weapons/spike2.wav");
  precache_sound("weapons/grenade.wav");
  precache_sound("weapons/bounce.wav");
  precache_sound("weapons/sgun1.wav");
  precache_sound("weapons/r_exp3.wav");
  precache_sound("weapons/lhit.wav");
};
