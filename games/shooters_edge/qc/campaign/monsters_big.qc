// THE ROSTER, CONTINUED: the big kinds (porting/shooters-edge/campaign-rules.md § The roster):
// the leaper, the giant and the caster, each in the large hull.

// --- the leaper (monster_demon1): 300 health, claws, and a leap that strikes on contact ---

// Two claws as it closes in, 10-15 each.
void() demon_claw = {
  self.think = demon_claw;
  self.nextthink = time + 0.1;
  anim_tick();
  ai_charge(8);
  if (self.anim_step == 3 || self.anim_step == 6) {
    if (vlen(self.enemy.origin - self.origin) <= 80 && can_see(self.enemy)) {
      sound(self, CHAN_WEAPON, "demon/dhit2.wav", 1, ATTN_NORM);
      self.enemy.deathtype = "eviscerated";
      damage(self.enemy, self, self, 10 + 5 * random());
    }
  }
  if (self.anim_step >= 8) monster_run();
};

void() demon_melee = {
  anim_begin(self.fr_melee);
  demon_claw();
};

// In the air: the first thing that takes damage it meets at speed takes 40-50.
void() demon_leap_touch = {
  if (other.takedamage && vlen(self.velocity) > 400) {
    other.deathtype = "eviscerated";
    damage(other, self, self, 40 + 10 * random());
    self.touch = noop;
  }
};

void() demon_leap_step = {
  self.think = demon_leap_step;
  self.nextthink = time + 0.1;
  anim_tick();
  if ((self.anim_step > 2 && (self.flags & FL_ONGROUND)) || self.anim_step > 30) {
    if (self.flags & FL_ONGROUND) sound(self, CHAN_BODY, "demon/dland2.wav", 1, ATTN_NORM);
    self.touch = noop;
    monster_run();
  }
};

// 600 forward, 250 up.
void() demon_leap = {
  face_enemy();
  makevectors(self.angles);
  self.velocity = v_forward * 600 + '0 0 250';
  self.origin_z = self.origin_z + 1;
  setorigin(self, self.origin);
  self.flags = self.flags - (self.flags & FL_ONGROUND);
  self.touch = demon_leap_touch;
  sound(self, CHAN_VOICE, "demon/djump.wav", 1, ATTN_NORM);
  anim_begin(self.fr_leap);
  demon_leap_step();
};

// The leaper's own decision: the claws in melee range; the leap from 100 to 350 units off, the
// enemy not far above or below, at most every 2 s.
float() demon_check = {
  local vector delta;
  local float d;
  if (range_to(self.enemy) == RANGE_MELEE && can_see(self.enemy)) {
    self.attack_state = AS_MELEE;
    return TRUE;
  }
  delta = self.enemy.origin - self.origin;
  if (fabs(delta_z) > 64) return FALSE;
  d = vlen(delta);
  if (d < 100 || d > 350) return FALSE;
  if (time < self.attack_finished) return FALSE;
  if (!can_see(self.enemy)) return FALSE;
  self.attack_finished = time + 2;
  demon_leap();
  return TRUE;
};

void() demon_flinch = {
  self.think = demon_flinch;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step >= 3) monster_run();
};

void() demon_pain = {
  if (self.touch == demon_leap_touch) return;  // no flinch in the air
  if (self.pain_finished < time + 1) self.pain_finished = time + 1;
  sound(self, CHAN_VOICE, "demon/dpain1.wav", 1, ATTN_NORM);
  anim_begin(self.fr_pain);
  demon_flinch();
};

void() monster_demon1 = {
  if (no_monsters_here()) return;
  precache_model("progs/demon.mdl");
  precache_model("progs/h_demon.mdl");
  precache_sound("demon/ddeath.wav");
  precache_sound("demon/dhit2.wav");
  precache_sound("demon/djump.wav");
  precache_sound("demon/dpain1.wav");
  precache_sound("demon/idle1.wav");
  precache_sound("demon/sight2.wav");
  precache_sound("demon/dland2.wav");
  self.noise = "demon/sight2.wav";
  self.noise1 = "demon/ddeath.wav";
  self.head_model = "progs/h_demon.mdl";
  self.gib_health = -80;
  self.walk_step = 8;
  self.run_step = 20;
  self.th_melee = demon_melee;
  self.th_check = demon_check;
  self.th_pain = demon_pain;
  self.th_die = monster_die;
  self.fr_stand = '0 12 1';
  self.fr_walk = '13 20 1';
  self.fr_run = '21 26 1';
  self.fr_melee = '54 68 1';
  self.fr_leap = '27 38 1';
  self.fr_pain = '39 44 1';
  self.fr_death = '45 53 1';
  walkmonster_start("progs/demon.mdl", 300, '-32 -32 -24', '32 32 64');
};

// --- the giant (monster_shambler): 600 health, claws, lightning; blasts do it half ---

// The blow the claw in progress lands with: 20 a swipe, 40 the smash.
.float claw_factor;

// A swipe (3 randoms x 20) or, one time in three, the two-handed smash (x 40), closing in.
void() shambler_claw = {
  self.think = shambler_claw;
  self.nextthink = time + 0.1;
  anim_tick();
  ai_charge(6);
  if (self.anim_step == 1) {
    if (random() < 0.33) {
      self.claw_factor = 40;  // the smash
      sound(self, CHAN_WEAPON, "shambler/melee2.wav", 1, ATTN_NORM);
    } else {
      self.claw_factor = 20;
      sound(self, CHAN_WEAPON, "shambler/melee1.wav", 1, ATTN_NORM);
    }
  }
  if (self.anim_step == 5) {
    if (vlen(self.enemy.origin - self.origin) <= 100 && can_see(self.enemy))
      sound(self, CHAN_VOICE, "shambler/smack.wav", 1, ATTN_NORM);
    melee_blow_n(3, self.claw_factor);
  }
  if (self.anim_step >= 9) monster_run();
};

void() shambler_melee = {
  anim_begin(self.fr_melee);
  shambler_claw();
};

// The bolt: three strokes along the line to the enemy's eye, 10 each on what they reach.
void() shambler_bolt = {
  local vector from, dir;
  self.think = shambler_bolt;
  self.nextthink = time + 0.1;
  anim_tick();
  face_enemy();
  if (self.anim_step == 1) sound(self, CHAN_WEAPON, "shambler/sattck1.wav", 1, ATTN_NORM);
  if (self.anim_step >= 4 && self.anim_step <= 6) {
    from = self.origin + '0 0 40';
    dir = normalize(self.enemy.origin + self.enemy.view_ofs - from);
    traceline(from, from + dir * 600, TRUE, self);
    effect_beam(from, trace_endpos);
    traceline(from, from + dir * 600, FALSE, self);
    if (trace_ent.takedamage) {
      trace_ent.deathtype = "struck by lightning";
      damage(trace_ent, self, self, 10);
    }
  }
  if (self.anim_step >= 9) monster_run();
};

void() shambler_missile = {
  anim_begin(self.fr_missile);
  shambler_bolt();
};

void() shambler_flinch = {
  self.think = shambler_flinch;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step >= 4) monster_run();
};

// Only a hit that takes a good share of it makes it flinch, and at most every 2 s.
void() shambler_pain = {
  if (self.health > 400 && random() < 0.5) return;
  if (self.pain_finished < time + 2) self.pain_finished = time + 2;
  sound(self, CHAN_VOICE, "shambler/shurt2.wav", 1, ATTN_NORM);
  anim_begin(self.fr_pain);
  shambler_flinch();
};

void() monster_shambler = {
  if (no_monsters_here()) return;
  precache_model("progs/shambler.mdl");
  precache_model("progs/h_shams.mdl");
  precache_sound("shambler/sattck1.wav");
  precache_sound("shambler/sboom.wav");
  precache_sound("shambler/sdeath.wav");
  precache_sound("shambler/shurt2.wav");
  precache_sound("shambler/sidle.wav");
  precache_sound("shambler/ssight.wav");
  precache_sound("shambler/melee1.wav");
  precache_sound("shambler/melee2.wav");
  precache_sound("shambler/smack.wav");
  self.noise = "shambler/ssight.wav";
  self.noise1 = "shambler/sdeath.wav";
  self.head_model = "progs/h_shams.mdl";
  self.gib_health = -60;
  self.walk_step = 8;
  self.run_step = 20;
  self.blast_share = 0.5;
  self.th_melee = shambler_melee;
  self.th_missile = shambler_missile;
  self.th_pain = shambler_pain;
  self.th_die = monster_die;
  self.fr_stand = '0 16 1';
  self.fr_walk = '17 28 1';
  self.fr_run = '29 34 1';
  self.fr_melee = '35 46 1';
  self.fr_missile = '65 76 1';
  self.fr_pain = '77 82 1';
  self.fr_death = '83 93 1';
  walkmonster_start("progs/shambler.mdl", 600, '-32 -32 -24', '32 32 64');
};

// --- the caster (monster_shalrath): 400 health, a homing ball ---

// The ball turns toward its target every 0.2 s; it hits for 110, with a blast of 40 round it.
void() caster_ball_touch = {
  if (other == self.owner) return;
  if (other.takedamage) {
    other.deathtype = "exploded by a homing ball";
    damage(other, self, self.owner, 110);
  }
  radius_damage(self, self.owner, 40, other, "caught by a homing ball's blast");
  sound(self, CHAN_VOICE, "weapons/r_exp3.wav", 1, ATTN_NORM);
  effect_explosion(self.origin);
  remove(self);
};

void() caster_ball_home = {
  if (time > self.attack_finished || self.enemy.health <= 0) {
    remove(self);
    return;
  }
  self.velocity = normalize(self.enemy.origin + '0 0 10' - self.origin) * 400;
  self.angles = vectoangles(self.velocity);
  self.nextthink = time + 0.2;
};

void() caster_cast = {
  local entity m;
  self.think = caster_cast;
  self.nextthink = time + 0.1;
  anim_tick();
  face_enemy();
  if (self.anim_step == 1) sound(self, CHAN_VOICE, "shalrath/attack.wav", 1, ATTN_NORM);
  if (self.anim_step == 6) {
    sound(self, CHAN_WEAPON, "shalrath/attack2.wav", 1, ATTN_NORM);
    m = monster_missile("progs/v_spike.mdl", 400, 110, 0);
    m.enemy = self.enemy;
    m.touch = caster_ball_touch;
    m.think = caster_ball_home;
    m.nextthink = time + 0.1;
    m.attack_finished = time + 20;
  }
  if (self.anim_step >= 9) monster_run();
};

void() caster_missile = {
  anim_begin(self.fr_missile);
  caster_cast();
};

void() caster_flinch = {
  self.think = caster_flinch;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step >= 4) monster_run();
};

void() caster_pain = {
  if (self.pain_finished < time + 3) self.pain_finished = time + 3;
  sound(self, CHAN_VOICE, "shalrath/pain.wav", 1, ATTN_NORM);
  anim_begin(self.fr_pain);
  caster_flinch();
};

void() monster_shalrath = {
  if (no_monsters_here()) return;
  precache_model("progs/shalrath.mdl");
  precache_model("progs/h_shal.mdl");
  precache_model("progs/v_spike.mdl");
  precache_sound("shalrath/attack.wav");
  precache_sound("shalrath/attack2.wav");
  precache_sound("shalrath/death.wav");
  precache_sound("shalrath/idle.wav");
  precache_sound("shalrath/pain.wav");
  precache_sound("shalrath/sight.wav");
  precache_sound("weapons/r_exp3.wav");
  self.noise = "shalrath/sight.wav";
  self.noise1 = "shalrath/death.wav";
  self.head_model = "progs/h_shal.mdl";
  self.gib_health = -90;
  self.walk_step = 6;
  self.run_step = 6;
  self.th_missile = caster_missile;
  self.th_pain = caster_pain;
  self.th_die = monster_die;
  self.fr_stand = '23 34 1';
  self.fr_walk = '23 34 1';
  self.fr_run = '23 34 1';
  self.fr_missile = '0 10 1';
  self.fr_pain = '11 15 1';
  self.fr_death = '16 22 1';
  walkmonster_start("progs/shalrath.mdl", 400, '-32 -32 -24', '32 32 64');
};
