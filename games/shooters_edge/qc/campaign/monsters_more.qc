// THE ROSTER, CONTINUED: the trooper, the spawn and the heavy swordsman
// (porting/shooters-edge/campaign-rules.md § The roster).

// --- the trooper (monster_enforcer): 80 health, laser bolts ---

// Aiming, two bolts (600, 15 each) a step apart, recovering.
void() enforcer_fire = {
  local entity m;
  self.think = enforcer_fire;
  self.nextthink = time + 0.1;
  self.effects = self.effects - (self.effects & EF_MUZZLEFLASH);
  anim_tick();
  face_enemy();
  if (self.anim_step == 4 || self.anim_step == 6) {
    sound(self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);
    self.effects = self.effects | EF_MUZZLEFLASH;
    m = monster_missile("progs/laser.mdl", 600, 15, 0);
    m.deathtype = "lasered";
  }
  if (self.anim_step >= 9) monster_run();
};

void() enforcer_missile = {
  anim_begin(self.fr_missile);
  enforcer_fire();
};

void() enforcer_flinch = {
  self.think = enforcer_flinch;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step >= 3) monster_run();
};

void() enforcer_pain = {
  if (self.pain_finished < time + 1) self.pain_finished = time + 1;
  if (random() < 0.5) sound(self, CHAN_VOICE, "enforcer/pain1.wav", 1, ATTN_NORM);
  else sound(self, CHAN_VOICE, "enforcer/pain2.wav", 1, ATTN_NORM);
  anim_begin(self.fr_pain);
  enforcer_flinch();
};

void() monster_enforcer = {
  local float r;
  if (no_monsters_here()) return;
  precache_model("progs/enforcer.mdl");
  precache_model("progs/h_mega.mdl");
  precache_model("progs/laser.mdl");
  precache_sound("enforcer/death1.wav");
  precache_sound("enforcer/enfire.wav");
  precache_sound("enforcer/enfstop.wav");
  precache_sound("enforcer/idle1.wav");
  precache_sound("enforcer/pain1.wav");
  precache_sound("enforcer/pain2.wav");
  precache_sound("enforcer/sight1.wav");
  precache_sound("enforcer/sight2.wav");
  precache_sound("enforcer/sight3.wav");
  precache_sound("enforcer/sight4.wav");
  r = random();  // one of four sight calls
  if (r < 0.25) self.noise = "enforcer/sight1.wav";
  else if (r < 0.5) self.noise = "enforcer/sight2.wav";
  else if (r < 0.75) self.noise = "enforcer/sight3.wav";
  else self.noise = "enforcer/sight4.wav";
  self.noise1 = "enforcer/death1.wav";
  self.head_model = "progs/h_mega.mdl";
  self.gib_health = -35;
  self.walk_step = 2;
  self.run_step = 14;
  self.th_missile = enforcer_missile;
  self.th_pain = enforcer_pain;
  self.th_die = monster_die;
  self.fr_stand = '0 6 1';
  self.fr_walk = '7 22 1';
  self.fr_run = '23 30 1';
  self.fr_missile = '31 40 1';
  self.fr_pain = '66 69 1';
  self.fr_death = '41 54 1';
  self.fr_death2 = '55 65 1';
  self.death_solid = 2;
  self.drop_ammo = DROP_CELLS;
  walkmonster_start("progs/enforcer.mdl", 80, '-16 -16 -24', '16 16 40');
};

// --- the spawn (monster_tarbaby): 80 health, bouncing leaps; it bursts when it dies ---

// Each landing near its enemy springs it again: 600 at the enemy, 200 up; what it meets at
// speed takes 10-20.
void() tarbaby_jump;

void() tarbaby_touch = {
  if (other.takedamage && other != self.owner && vlen(self.velocity) > 400) {
    other.deathtype = "slimed";
    damage(other, self, self, 10 + 10 * random());
    sound(self, CHAN_WEAPON, "blob/hit1.wav", 1, ATTN_NORM);
  } else if (other.solid == SOLID_BSP) {
    sound(self, CHAN_WEAPON, "blob/land1.wav", 1, ATTN_NORM);
  }
};

void() tarbaby_bounce = {
  self.think = tarbaby_bounce;
  self.nextthink = time + 0.1;
  anim_tick();
  if ((self.anim_step > 2 && (self.flags & FL_ONGROUND)) || self.anim_step > 30) {
    if (self.enemy.health > 0 && can_see(self.enemy) && range_to(self.enemy) <= RANGE_NEAR) {
      tarbaby_jump();
      return;
    }
    self.touch = noop;
    monster_run();
  }
};

void() tarbaby_jump = {
  face_enemy();
  makevectors(self.angles);
  self.velocity = v_forward * 600 + '0 0 200';
  self.origin_z = self.origin_z + 1;
  setorigin(self, self.origin);
  self.flags = self.flags - (self.flags & FL_ONGROUND);
  self.touch = tarbaby_touch;
  anim_begin(self.fr_leap);
  tarbaby_bounce();
};

// The spawn's own decision: a jump whenever its enemy is near and seen.
float() tarbaby_check = {
  if (range_to(self.enemy) > RANGE_NEAR) return FALSE;
  if (!can_see(self.enemy)) return FALSE;
  tarbaby_jump();
  return TRUE;
};

// It bursts: 120 less half the distance to everything near.
void() tarbaby_die = {
  self.health = 0;  // never gibbed: it bursts instead
  monster_die();
  self.touch = noop;
  sound(self, CHAN_VOICE, "blob/death1.wav", 1, ATTN_NORM);
  radius_damage(self, self, 120, world, "caught by a spawn's burst");
  effect_explosion(self.origin);
  remove(self);
};

void() monster_tarbaby = {
  if (no_monsters_here()) return;
  precache_model("progs/tarbaby.mdl");
  precache_sound("blob/death1.wav");
  precache_sound("blob/hit1.wav");
  precache_sound("blob/land1.wav");
  precache_sound("blob/sight1.wav");
  self.noise = "blob/sight1.wav";
  self.noise1 = "";
  self.head_model = "progs/gib1.mdl";
  self.gib_health = -1000;
  self.walk_step = 2;
  self.run_step = 2;
  self.th_check = tarbaby_check;
  self.th_die = tarbaby_die;
  self.fr_stand = '0 24 1';
  self.fr_walk = '0 24 1';
  self.fr_run = '25 49 1';
  self.fr_leap = '50 59 1';
  walkmonster_start("progs/tarbaby.mdl", 80, '-16 -16 -24', '16 16 40');
};

// --- the heavy swordsman (monster_hell_knight): 250 health, a sword, a charge, magic bolts ---

// Six steps of the sword, closing in, landing on the third and the fifth.
void() hknight_swing = {
  self.think = hknight_swing;
  self.nextthink = time + 0.1;
  anim_tick();
  ai_charge(10);
  if (self.anim_step == 1) sound(self, CHAN_WEAPON, "hknight/slash1.wav", 1, ATTN_NORM);
  if (self.anim_step == 3 || self.anim_step == 5) melee_blow(3);
  if (self.anim_step >= 6) monster_run();
};

void() hknight_melee = {
  anim_begin(self.fr_melee);
  hknight_swing();
};

// The charge: a dozen steps at speed, the blade landing whenever it reaches.
void() hknight_charge = {
  self.think = hknight_charge;
  self.nextthink = time + 0.1;
  anim_tick();
  ai_charge(20);
  if (self.anim_step >= 4 && self.anim_step <= 10) melee_blow(2);
  if (self.anim_step >= 12) monster_run();
};

// A fan of six slow bolts (300, 9 each) across its enemy.
void() hknight_cast = {
  local entity m;
  local float i;
  self.think = hknight_cast;
  self.nextthink = time + 0.1;
  anim_tick();
  face_enemy();
  if (self.anim_step == 1) sound(self, CHAN_VOICE, "hknight/attack1.wav", 1, ATTN_NORM);
  if (self.anim_step == 4) {
    for (i = -2.5; i <= 2.5; i += 1) {
      m = monster_missile("progs/k_spike.mdl", 300, 9, 0);
      m.velocity = normalize(m.velocity + v_right * i * 0.1 * 300) * 300;
      m.angles = vectoangles(m.velocity);
      m.deathtype = "burned by magic";
    }
  }
  if (self.anim_step >= 8) monster_run();
};

void() hknight_missile = {
  anim_begin(self.fr_missile);
  hknight_cast();
};

// Its own decision: the sword in reach, the charge from 80 to 250 units, else the bolts.
float() hknight_check = {
  local float d;
  if (!can_see(self.enemy)) return FALSE;
  d = vlen(self.enemy.origin - self.origin);
  if (d <= 80) {
    self.attack_state = AS_MELEE;
    return TRUE;
  }
  if (time < self.attack_finished) return FALSE;
  if (d <= 250 && fabs(self.enemy.origin_z - self.origin_z) <= 20) {
    self.attack_finished = time + 2;
    face_enemy();
    anim_begin(self.fr_leap);
    hknight_charge();
    return TRUE;
  }
  return check_attack();
};

void() hknight_flinch = {
  self.think = hknight_flinch;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step >= 3) monster_run();
};

void() hknight_pain = {
  if (self.pain_finished < time + 1) self.pain_finished = time + 1;
  sound(self, CHAN_VOICE, "hknight/pain1.wav", 1, ATTN_NORM);
  anim_begin(self.fr_pain);
  hknight_flinch();
};

void() monster_hell_knight = {
  if (no_monsters_here()) return;
  precache_model("progs/hknight.mdl");
  precache_model("progs/h_hellkn.mdl");
  precache_model("progs/k_spike.mdl");
  precache_sound("hknight/attack1.wav");
  precache_sound("hknight/death1.wav");
  precache_sound("hknight/pain1.wav");
  precache_sound("hknight/sight1.wav");
  precache_sound("hknight/hit.wav");
  precache_sound("hknight/slash1.wav");
  precache_sound("hknight/idle.wav");
  precache_sound("hknight/grunt.wav");
  self.noise = "hknight/sight1.wav";
  self.noise1 = "hknight/death1.wav";
  self.head_model = "progs/h_hellkn.mdl";
  self.gib_health = -40;
  self.walk_step = 2;
  self.run_step = 16;
  self.th_melee = hknight_melee;
  self.th_missile = hknight_missile;
  self.th_check = hknight_check;
  self.th_pain = hknight_pain;
  self.th_die = monster_die;
  self.fr_stand = '0 8 1';
  self.fr_walk = '9 28 1';
  self.fr_run = '29 36 1';
  self.fr_melee = '112 121 1';
  self.fr_missile = '155 165 1';
  self.fr_leap = '63 78 1';
  self.fr_pain = '37 41 1';
  self.fr_death = '42 53 1';
  self.fr_death2 = '54 62 1';
  self.death_solid = 2;
  walkmonster_start("progs/hknight.mdl", 250, '-16 -16 -24', '16 16 40');
};
