// THE ROSTER, CONTINUED: the kinds that fight up close (porting/shooters-edge/campaign-rules.md
// § The roster): the swordsman, the brute (and its marksman, the same) and the ghoul. Each is
// numbers and attacks over the minds in ai.qc, its models and sounds the pack's names mapped to
// our own by content/classic.assetmap.

// --- the swordsman (monster_knight): 75 health, a sword swung while charging ---

// Six steps: closing in, the blade landing on the third and the fifth.
void() knight_swing = {
  self.think = knight_swing;
  self.nextthink = time + 0.1;
  anim_tick();
  ai_charge(10);
  if (self.anim_step == 1) {
    if (random() < 0.5) sound(self, CHAN_WEAPON, "knight/sword1.wav", 1, ATTN_NORM);
    else sound(self, CHAN_WEAPON, "knight/sword2.wav", 1, ATTN_NORM);
  }
  if (self.anim_step == 3 || self.anim_step == 5) melee_blow(3);
  if (self.anim_step >= 6) monster_run();
};

void() knight_melee = {
  anim_begin(self.fr_melee);
  knight_swing();
};

// The swordsman's own decision: the swing once the enemy is within 80 units and seen.
float() knight_check = {
  if (vlen(self.enemy.origin - self.origin) > 80) return FALSE;
  if (!can_see(self.enemy)) return FALSE;
  self.attack_state = AS_MELEE;
  return TRUE;
};

void() knight_flinch = {
  self.think = knight_flinch;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step >= 3) monster_run();
};

void() knight_pain = {
  if (self.pain_finished < time + 1) self.pain_finished = time + 1;
  sound(self, CHAN_VOICE, "knight/khurt.wav", 1, ATTN_NORM);
  anim_begin(self.fr_pain);
  knight_flinch();
};

void() monster_knight = {
  if (no_monsters_here()) return;
  precache_model("progs/knight.mdl");
  precache_model("progs/h_knight.mdl");
  precache_sound("knight/kdeath.wav");
  precache_sound("knight/khurt.wav");
  precache_sound("knight/ksight.wav");
  precache_sound("knight/sword1.wav");
  precache_sound("knight/sword2.wav");
  precache_sound("knight/idle.wav");
  self.noise = "knight/ksight.wav";
  self.noise1 = "knight/kdeath.wav";
  self.head_model = "progs/h_knight.mdl";
  self.gib_health = -40;
  self.walk_step = 3;
  self.run_step = 14;
  self.th_melee = knight_melee;
  self.th_check = knight_check;
  self.th_pain = knight_pain;
  self.th_die = monster_die;
  self.fr_stand = '0 8 1';
  self.fr_walk = '53 66 1';
  self.fr_run = '9 16 1';
  self.fr_melee = '17 27 1';
  self.fr_pain = '28 30 1';
  self.fr_death = '76 85 1';
  self.fr_death2 = '86 96 1';
  self.death_solid = 2;
  walkmonster_start("progs/knight.mdl", 75, '-16 -16 -24', '16 16 40');
};

// --- the brute (monster_ogre): 200 health, a chainsaw and a lobbed grenade ---

// The grenade's blast: 40 less half the distance to everything near, the brute's own kind too.
void() ogre_grenade_blast = {
  radius_damage(self, self.owner, 40, world, "blown up by a grenade");
  sound(self, CHAN_VOICE, "weapons/r_exp3.wav", 1, ATTN_NORM);
  effect_explosion(self.origin);
  remove(self);
};

void() ogre_grenade_touch = {
  if (other == self.owner) return;
  if (other.takedamage == DAMAGE_AIM) {
    ogre_grenade_blast();
    return;
  }
  sound(self, CHAN_VOICE, "weapons/bounce.wav", 1, ATTN_NORM);
};

// Five steps: aiming, the grenade (600 at the enemy, 200 up, bouncing, a 2.5 s fuse), recovering.
void() ogre_throw = {
  local entity g;
  self.think = ogre_throw;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step <= 2) face_enemy();
  if (self.anim_step == 3) {
    sound(self, CHAN_WEAPON, "weapons/grenade.wav", 1, ATTN_NORM);
    g = monster_missile("progs/grenade.mdl", 600, 0, 200);
    g.movetype = MOVETYPE_BOUNCE;
    g.avelocity = '300 300 300';
    g.touch = ogre_grenade_touch;
    g.think = ogre_grenade_blast;
    g.nextthink = time + 2.5;
  }
  if (self.anim_step >= 5) monster_run();
};

void() ogre_missile = {
  anim_begin(self.fr_missile);
  ogre_throw();
};

// Eight steps of the saw, closing in, biting on four of them.
void() ogre_saw = {
  self.think = ogre_saw;
  self.nextthink = time + 0.1;
  anim_tick();
  ai_charge(6);
  if (self.anim_step == 1) sound(self, CHAN_WEAPON, "ogre/ogsawatk.wav", 1, ATTN_NORM);
  if (self.anim_step >= 3 && self.anim_step <= 6) melee_blow(4);
  if (self.anim_step >= 8) monster_run();
};

void() ogre_melee = {
  anim_begin(self.fr_melee);
  ogre_saw();
};

void() ogre_flinch = {
  self.think = ogre_flinch;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step >= 4) monster_run();
};

void() ogre_pain = {
  if (self.pain_finished < time + 1.5) self.pain_finished = time + 1.5;
  sound(self, CHAN_VOICE, "ogre/ogpain1.wav", 1, ATTN_NORM);
  anim_begin(self.fr_pain);
  ogre_flinch();
};

void() monster_ogre = {
  if (no_monsters_here()) return;
  precache_model("progs/ogre.mdl");
  precache_model("progs/h_ogre.mdl");
  precache_model("progs/grenade.mdl");
  precache_sound("ogre/ogdrag.wav");
  precache_sound("ogre/ogdth.wav");
  precache_sound("ogre/ogidle.wav");
  precache_sound("ogre/ogpain1.wav");
  precache_sound("ogre/ogsawatk.wav");
  precache_sound("ogre/ogwake.wav");
  precache_sound("weapons/grenade.wav");
  precache_sound("weapons/bounce.wav");
  self.noise = "ogre/ogwake.wav";
  self.noise1 = "ogre/ogdth.wav";
  self.head_model = "progs/h_ogre.mdl";
  self.gib_health = -80;
  self.walk_step = 2;
  self.run_step = 14;
  self.th_melee = ogre_melee;
  self.th_missile = ogre_missile;
  self.th_pain = ogre_pain;
  self.th_die = monster_die;
  self.fr_stand = '0 8 1';
  self.fr_walk = '9 24 1';
  self.fr_run = '25 32 1';
  self.fr_melee = '33 46 1';
  self.fr_missile = '61 66 1';
  self.fr_pain = '67 71 1';
  self.fr_death = '112 125 1';
  self.fr_death2 = '126 135 1';
  self.death_solid = 2;
  self.drop_ammo = DROP_ROCKETS;
  self.gib_model = "progs/gib3.mdl";
  walkmonster_start("progs/ogre.mdl", 200, '-32 -32 -24', '32 32 64');
};

void() monster_ogre_marksman = { monster_ogre(); };

// --- the ghoul (monster_zombie): 60 health that every hit short of a big one gives back ---

float GHOUL_HEALTH = 60;
float GHOUL_KNOCKDOWN = 25;  // a single hit this big knocks it down; the next stand is whole

// Thrown flesh: 600 at the enemy, 200 up, 10 on what it hits.
void() ghoul_throw = {
  local entity m;
  self.think = ghoul_throw;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step <= 3) face_enemy();
  if (self.anim_step == 4) {
    sound(self, CHAN_WEAPON, "zombie/z_shot1.wav", 1, ATTN_NORM);
    m = monster_missile("progs/zom_gib.mdl", 600, 10, 200);
    m.deathtype = "hit by flesh";
  }
  if (self.anim_step >= 6) monster_run();
};

void() ghoul_missile = {
  anim_begin(self.fr_missile);
  ghoul_throw();
};

// Down: not solid to shots for five seconds, then up again with full health.
void() ghoul_down = {
  self.think = ghoul_down;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step == 50) {
    self.solid = SOLID_SLIDEBOX;
    self.takedamage = DAMAGE_AIM;
    self.angles_z = 0;
    setorigin(self, self.origin + '0 0 16');
    self.health = GHOUL_HEALTH;
    sound(self, CHAN_VOICE, "zombie/z_idle.wav", 1, ATTN_IDLE);
  }
  if (self.anim_step >= 55) monster_run();
};

// A hit it lives through is shrugged off (its health back), unless big enough to knock it down;
// it still turns on whoever hit it.
void() ghoul_hurt = {
  local float taken;
  taken = GHOUL_HEALTH - self.health;
  self.health = GHOUL_HEALTH;
  monster_hurt();
  if (taken < GHOUL_KNOCKDOWN) return;
  sound(self, CHAN_VOICE, "zombie/z_fall.wav", 1, ATTN_NORM);
  self.solid = SOLID_NOT;
  self.takedamage = DAMAGE_NO;
  self.angles_z = 90;
  setorigin(self, self.origin - '0 0 16');
  anim_begin(self.fr_pain);
  ghoul_down();
};

void() ghoul_pain = {
  if (self.pain_finished < time + 1) self.pain_finished = time + 1;
  sound(self, CHAN_VOICE, "zombie/z_pain.wav", 1, ATTN_NORM);
};

// Only a single blow of its whole health (an explosion, say) ends it, in pieces.
void() ghoul_die = {
  self.health = -1000;  // whatever killed it: it always bursts
  monster_die();
};

void() monster_zombie = {
  if (no_monsters_here()) return;
  precache_model("progs/zombie.mdl");
  precache_model("progs/h_zombie.mdl");
  precache_model("progs/zom_gib.mdl");
  precache_sound("zombie/z_idle.wav");
  precache_sound("zombie/z_idle1.wav");
  precache_sound("zombie/z_shot1.wav");
  precache_sound("zombie/z_gib.wav");
  precache_sound("zombie/z_pain.wav");
  precache_sound("zombie/z_pain1.wav");
  precache_sound("zombie/z_fall.wav");
  precache_sound("zombie/z_miss.wav");
  precache_sound("zombie/z_hit.wav");
  precache_sound("zombie/idle_w2.wav");
  self.noise = "zombie/z_idle.wav";
  self.noise1 = "zombie/z_gib.wav";
  self.head_model = "progs/h_zombie.mdl";
  self.gib_noise = "zombie/z_gib.wav";
  self.gib_health = 0;
  self.walk_step = 1;
  self.run_step = 6;
  self.th_missile = ghoul_missile;
  self.th_pain = ghoul_pain;
  self.th_die = ghoul_die;
  self.fr_stand = '0 14 1';
  self.fr_walk = '15 33 1';
  self.fr_run = '34 51 1';
  self.fr_missile = '52 64 1';
  self.fr_pain = '91 102 1';
  walkmonster_start("progs/zombie.mdl", GHOUL_HEALTH, '-16 -16 -24', '16 16 40');
  self.th_hurt = ghoul_hurt;
};
