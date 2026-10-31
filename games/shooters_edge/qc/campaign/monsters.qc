// THE ROSTER: each kind's numbers and attacks over the minds in ai.qc (porting/shooters-edge/
// campaign-rules.md § The roster). The classnames are the maps'; the models and sounds are the
// pack's names, mapped to our own by content/classic.assetmap. Kinds not here yet are not spawned.

// --- the rifleman (monster_army): 30 health, a four-pellet blast after a short wind-up ---

void() army_fire = {
  local vector dir;
  dir = self.enemy.origin + self.enemy.view_ofs - (self.origin + self.view_ofs);
  makevectors(vectoangles(dir));
  sound(self, CHAN_WEAPON, "soldier/sattck1.wav", 1, ATTN_NORM);
  self.effects = self.effects | EF_MUZZLEFLASH;
  fire_pellets(4, 4, 0.1);
};

// Four steps aiming, the shot, three steps to recover.
void() army_attack = {
  self.think = army_attack;
  self.nextthink = time + 0.1;
  self.effects = self.effects - (self.effects & EF_MUZZLEFLASH);
  self.anim_step = self.anim_step + 1;
  if (self.anim_step <= 4) face_enemy();
  if (self.anim_step == 5) army_fire();
  if (self.anim_step >= 8) monster_run();
};

void() army_missile = {
  self.anim_step = 0;
  army_attack();
};

// Flinches for 0.3 s.
void() army_flinch = {
  self.think = army_flinch;
  self.nextthink = time + 0.1;
  self.anim_step = self.anim_step + 1;
  if (self.anim_step >= 3) monster_run();
};

void() army_pain = {
  if (self.pain_finished < time + 1) self.pain_finished = time + 1;
  if (random() < 0.5) sound(self, CHAN_VOICE, "soldier/pain1.wav", 1, ATTN_NORM);
  else sound(self, CHAN_VOICE, "soldier/pain2.wav", 1, ATTN_NORM);
  self.anim_step = 0;
  army_flinch();
};

void() monster_army = {
  if (deathmatch) {
    remove(self);
    return;
  }
  precache_model("progs/soldier.mdl");
  precache_model("progs/h_guard.mdl");
  precache_sound("soldier/death1.wav");
  precache_sound("soldier/idle.wav");
  precache_sound("soldier/pain1.wav");
  precache_sound("soldier/pain2.wav");
  precache_sound("soldier/sattck1.wav");
  precache_sound("soldier/sight1.wav");
  self.noise = "soldier/sight1.wav";
  self.noise1 = "soldier/death1.wav";
  self.head_model = "progs/h_guard.mdl";
  self.gib_health = -35;
  self.walk_step = 1;
  self.run_step = 12;
  self.th_missile = army_missile;
  self.th_pain = army_pain;
  self.th_die = monster_die;
  walkmonster_start("progs/soldier.mdl", 30, '-16 -16 -24', '16 16 40');
};

// --- the hound (monster_dog): 25 health, a bite, and a leap that bites on contact ---

void() dog_bite_step = {
  self.think = dog_bite_step;
  self.nextthink = time + 0.1;
  self.anim_step = self.anim_step + 1;
  ai_charge(12);
  if (self.anim_step == 3) {
    sound(self, CHAN_VOICE, "dog/dattack1.wav", 1, ATTN_NORM);
    melee_blow(8);
  }
  if (self.anim_step >= 5) monster_run();
};

void() dog_melee = {
  self.anim_step = 0;
  dog_bite_step();
};

// In the air: the first thing that takes damage it meets at speed is bitten; on the ground again
// it runs.
void() dog_leap_touch = {
  if (other.takedamage && vlen(self.velocity) > 300) {
    other.deathtype = "mauled";
    damage(other, self, self, 10 + 10 * random());
    self.touch = noop;
  }
};

void() dog_leap_step = {
  self.think = dog_leap_step;
  self.nextthink = time + 0.1;
  self.anim_step = self.anim_step + 1;
  if (self.anim_step > 2 && (self.flags & FL_ONGROUND)) {
    self.touch = noop;
    monster_run();
  }
  if (self.anim_step > 30) {  // stuck in the air (on a monster, a ledge's lip): run anyway
    self.touch = noop;
    monster_run();
  }
};

void() dog_leap = {
  face_enemy();
  makevectors(self.angles);
  self.velocity = v_forward * 300 + '0 0 200';
  self.origin_z = self.origin_z + 1;
  setorigin(self, self.origin);
  self.flags = self.flags - (self.flags & FL_ONGROUND);
  self.touch = dog_leap_touch;
  sound(self, CHAN_VOICE, "dog/dattack1.wav", 1, ATTN_NORM);
  self.anim_step = 0;
  dog_leap_step();
};

// The hound's own decision: the bite in melee range; the leap from 80 to 150 units off, the enemy
// not far above or below.
float() dog_check = {
  local vector delta;
  local float d;
  if (range_to(self.enemy) == RANGE_MELEE) {
    traceline(self.origin + self.view_ofs, self.enemy.origin + self.enemy.view_ofs, FALSE, self);
    if (trace_ent == self.enemy) {
      self.attack_state = AS_MELEE;
      return TRUE;
    }
  }
  delta = self.enemy.origin - self.origin;
  if (fabs(delta_z) > 32) return FALSE;
  d = vlen(delta);
  if (d < 80 || d > 150) return FALSE;
  if (time < self.attack_finished) return FALSE;
  if (!can_see(self.enemy)) return FALSE;
  self.attack_finished = time + 1;
  dog_leap();
  return TRUE;
};

void() dog_flinch = {
  self.think = dog_flinch;
  self.nextthink = time + 0.1;
  self.anim_step = self.anim_step + 1;
  if (self.anim_step >= 3) monster_run();
};

void() dog_pain = {
  if (self.pain_finished < time + 1) self.pain_finished = time + 1;
  sound(self, CHAN_VOICE, "dog/dpain1.wav", 1, ATTN_NORM);
  self.touch = noop;
  self.anim_step = 0;
  dog_flinch();
};

void() monster_dog = {
  if (deathmatch) {
    remove(self);
    return;
  }
  precache_model("progs/dog.mdl");
  precache_model("progs/h_dog.mdl");
  precache_sound("dog/dattack1.wav");
  precache_sound("dog/ddeath.wav");
  precache_sound("dog/dpain1.wav");
  precache_sound("dog/dsight.wav");
  precache_sound("dog/idle.wav");
  self.noise = "dog/dsight.wav";
  self.noise1 = "dog/ddeath.wav";
  self.head_model = "progs/h_dog.mdl";
  self.gib_health = -35;
  self.walk_step = 8;
  self.run_step = 18;
  self.th_melee = dog_melee;
  self.th_check = dog_check;
  self.th_pain = dog_pain;
  self.th_die = monster_die;
  walkmonster_start("progs/dog.mdl", 25, '-32 -32 -24', '32 32 40');
};
