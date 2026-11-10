// THE BOSSES AND THEIR SET PIECES (porting/shooters-edge/campaign-rules.md § The roster, § The map
// vocabulary): the lava boss and the lightning that alone can hurt it, the final boss and the
// moving teleport destination that alone can kill it, and the campaign's teleporting under both
// (monsters go through too; arriving inside something kills it).

// --- teleporting, the campaign's way ---

// A teleporter sends a living player or monster; one with spawnflags 1 sends players only.
float(entity who) teleport_takes = {
  if (who.health <= 0) return FALSE;
  if (who.classname == "player") return TRUE;
  if (self.spawnflags & 1) return FALSE;
  return (who.flags & FL_MONSTER) != 0;
};

// Arriving inside anything that takes damage kills it, whatever its health.
void(entity who) teleport_arrived = {
  local entity e;
  e = findradius(who.origin, 400);
  while (e) {
    if (e != who && e.takedamage && e.health > 0 && e.absmin_x < who.absmax_x &&
        e.absmax_x > who.absmin_x && e.absmin_y < who.absmax_y && e.absmax_y > who.absmin_y &&
        e.absmin_z < who.absmax_z && e.absmax_z > who.absmin_z) {
      e.deathtype = "telefragged";
      damage(e, who, who, 50000);
    }
    e = e.chain;
  }
};

// A teleport destination that moves: once used it rides its path of corners round at .speed
// (100 when not set), a teleporter naming it sending a player to wherever it is.
void() teleporttrain_next = {
  local entity corner;
  local vector delta;
  corner = find(world, targetname, self.target);
  if (!corner) {
    self.velocity = '0 0 0';
    return;
  }
  self.target = corner.target;
  delta = corner.origin - self.origin;
  if (vlen(delta) < 1) {
    self.think = teleporttrain_next;
    self.nextthink = time + 0.1;
    return;
  }
  self.velocity = normalize(delta) * self.speed;
  self.think = teleporttrain_next;
  self.nextthink = time + vlen(delta) / self.speed;
};

void() teleporttrain_use = {
  if (self.velocity != '0 0 0') return;  // already riding
  self.use = noop;
  teleporttrain_next();
};

void() misc_teleporttrain = {
  if (!self.speed) self.speed = 100;
  if (self.target == "") objerror("a teleport train needs a path");
  precache_model("progs/teleport.mdl");
  self.solid = SOLID_NOT;
  self.movetype = MOVETYPE_NOCLIP;
  self.avelocity = '100 200 300';
  setmodel(self, "progs/teleport.mdl");
  setsize(self, '0 0 0', '0 0 0');
  setorigin(self, self.origin);
  self.use = teleporttrain_use;
  if (self.targetname == "") {  // unnamed: it rides from the start, once its path has spawned
    self.think = teleporttrain_use;
    self.nextthink = time + 0.1;
  }
};

// --- the lava boss (monster_boss): out of the lava when used; three strikes of lightning ---

entity lava_boss;  // the one on the map, for the lightning

// A ball of fire lobbed at its enemy at 300, high enough to land on it; 20 on a hit, a blast of
// 40 round it.
void() boss_ball_touch = {
  if (other == self.owner) return;
  if (other.takedamage) {
    other.deathtype = "burned by the lava boss";
    damage(other, self, self.owner, 20);
  }
  radius_damage(self, self.owner, 40, other, "caught by the lava boss's fire");
  effect_explosion(self.origin);
  remove(self);
};

// The nearest living player it can see, kept while he lives; world when there is none.
float() boss_pick = {
  local entity p;
  if (self.enemy.health <= 0 || self.enemy.classname != "player") {
    self.enemy = world;
    p = find(world, classname, "player");
    while (p) {
      if (p.health > 0 && (!self.enemy || vlen(p.origin - self.origin) <
                                             vlen(self.enemy.origin - self.origin)))
        self.enemy = p;
      p = find(p, classname, "player");
    }
  }
  return self.enemy && can_see(self.enemy);
};

// The throw itself, lobbed to land on the enemy: as much upward as gravity takes back over the
// flight.
void() boss_throw = {
  local entity m;
  if (!boss_pick()) return;
  face_enemy();
  sound(self, CHAN_WEAPON, "boss1/throw.wav", 1, ATTN_NORM);
  m = monster_missile("progs/lavaball.mdl", 300, 20, 400 * vlen(self.enemy.origin - self.origin) / 300);
  m.avelocity = '200 100 300';
  m.touch = boss_ball_touch;
};

// Up, a frame every 0.1 s: the rise, a strike's shudder or a throw played through (the ball leaves
// on the throw's twelfth frame), else its idle sway; a throw begins every 2 s while a player is
// in sight.
void() boss_think = {
  self.think = boss_think;
  self.nextthink = time + 0.1;
  if (self.fr_now_z) {
    anim_tick();
    if (self.fr_now == self.fr_missile && self.anim_step == 12) boss_throw();
    if (self.frame >= self.fr_now_y) self.fr_now = '0 0 0';
    return;
  }
  frame_loop(self.fr_walk);
  if (time < self.attack_finished) return;
  self.attack_finished = time + 2;
  if (boss_pick()) anim_begin(self.fr_missile);
};

// Used (the rune taken): it rises out of the lava, angry at whoever took it.
void() boss_use = {
  self.use = noop;
  self.solid = SOLID_SLIDEBOX;
  self.takedamage = DAMAGE_NO;  // only the lightning hurts it
  setmodel(self, "progs/boss.mdl");
  setsize(self, '-128 -128 -24', '128 128 256');
  self.view_ofs = '0 0 96';  // it throws from high on its body
  setorigin(self, self.origin);
  self.enemy = activator;
  sound(self, CHAN_VOICE, "boss1/sight1.wav", 1, ATTN_NORM);
  effect_explosion(self.origin + '0 0 64');
  anim_begin(self.fr_stand);  // the rise
  self.attack_finished = time + 1.5;
  boss_think();
};

// Its death frames played, it is gone.
void() boss_sink = {
  anim_tick();
  if (self.frame < self.fr_now_y) {
    self.nextthink = time + 0.1;
    return;
  }
  self.modelindex = 0;
  self.think = noop;
  self.nextthink = -1;
};

// One strike: it shudders; three and it sinks back for good, its targets fired.
void() boss_struck = {
  local entity was_self;
  was_self = self;
  self = lava_boss;
  self.health = self.health - 1;
  if (self.health > 0) {
    sound(self, CHAN_VOICE, "boss1/pain.wav", 1, ATTN_NORM);
    anim_begin(self.fr_pain);
    self.attack_finished = time + 2;  // a strike stops its throwing a while
  } else {
    sound(self, CHAN_VOICE, "boss1/death.wav", 1, ATTN_NORM);
    killed_monsters = killed_monsters + 1;
    WriteByte(MSG_ALL, SVC_KILLEDMONSTER);
    activator = self.enemy;
    use_targets();
    effect_explosion(self.origin + '0 0 64');
    self.solid = SOLID_NOT;
    anim_begin(self.fr_death);
    self.think = boss_sink;
    self.nextthink = time + 0.1;
  }
  self = was_self;
};

void() monster_boss = {
  if (no_monsters_here()) return;
  precache_model("progs/boss.mdl");
  precache_model("progs/lavaball.mdl");
  precache_sound("boss1/death.wav");
  precache_sound("boss1/out1.wav");
  precache_sound("boss1/pain.wav");
  precache_sound("boss1/sight1.wav");
  precache_sound("boss1/throw.wav");
  precache_sound("weapons/r_exp3.wav");
  total_monsters = total_monsters + 1;
  self.flags = self.flags | FL_MONSTER;
  self.fr_stand = '0 16 1';     // the rise
  self.fr_walk = '17 47 1';     // its sway while up
  self.fr_missile = '57 79 1';  // a throw
  self.fr_pain = '80 89 1';     // a strike's shudder
  self.fr_death = '48 56 1';
  self.health = 3;
  self.solid = SOLID_NOT;  // under the lava until used
  self.movetype = MOVETYPE_STEP;
  self.use = boss_use;
  lava_boss = self;
};

// The lightning: used (its button), it strikes between the two doors that name it as their
// target, only while both stand fully open, and then sends them back down. Each strike that
// finds the boss up is one of its three.
void() lightning_use = {
  local entity a, b, e, was_self;
  if (time < self.attack_finished) return;
  a = world;
  b = world;
  e = find(world, target, "lightning");
  while (e) {
    if (e.classname == "func_door") {
      if (!a) a = e;
      else if (!b) b = e;
    }
    e = find(e, target, "lightning");
  }
  if (!a || !b) return;
  if (a.state != STATE_TOP || b.state != STATE_TOP) return;
  if (!lava_boss || lava_boss.health <= 0 || lava_boss.solid == SOLID_NOT) return;
  self.attack_finished = time + 1;
  effect_beam((a.absmin + a.absmax) * 0.5, (b.absmin + b.absmax) * 0.5);
  sound(self, CHAN_VOICE, "misc/power.wav", 1, ATTN_NONE);
  boss_struck();
  was_self = self;
  self = a;
  door_go_down();
  self = b;
  door_go_down();
  self = was_self;
};

void() event_lightning = {
  precache_sound("misc/power.wav");
  self.use = lightning_use;
};

// --- the final boss (monster_oldone): nothing hurts it but arriving inside it ---

void() oldone_end = {
  intermission_start();
  next_map = "start";
  WriteByte(MSG_ALL, SVC_FINALE);
  WriteString(MSG_ALL,
              "The thing at the bottom of the world is gone, torn apart from the inside.\n\nThe "
              "four runes go quiet. The way home is open.");
};

// Fifty pieces from all through its body (a grid 32 apart, five by five across, at two
// heights), each the first piece three times in ten, the second three, the third four, torn
// away as fast as a piece can fly.
void() oldone_gibs = {
  local float x, y, z, r;
  local vector org;
  local string mdl;
  for (z = 16; z <= 112; z += 96) {
    for (x = -64; x <= 64; x += 32) {
      for (y = -64; y <= 64; y += 32) {
        org = self.origin + x * '1 0 0' + y * '0 1 0' + z * '0 0 1';
        r = random();
        if (r < 0.3) mdl = "progs/gib1.mdl";
        else if (r < 0.6) mdl = "progs/gib2.mdl";
        else mdl = "progs/gib3.mdl";
        throw_gib_at(mdl, org, -999);
      }
    }
  }
};

// Torn apart: it shakes through its last frames, then bursts, and the game ends.
void() oldone_burst = {
  anim_tick();
  if (self.frame < self.fr_now_y) {
    self.nextthink = time + 0.1;
    return;
  }
  sound(self, CHAN_VOICE, "boss2/pop2.wav", 1, ATTN_NONE);
  oldone_gibs();
  self.modelindex = 0;
  self.think = oldone_end;
  self.nextthink = time + 2;
};

void() oldone_die = {
  self.solid = SOLID_NOT;
  self.takedamage = DAMAGE_NO;
  killed_monsters = killed_monsters + 1;
  WriteByte(MSG_ALL, SVC_KILLEDMONSTER);
  sound(self, CHAN_VOICE, "boss2/death.wav", 1, ATTN_NONE);
  activator = damage_attacker;
  use_targets();
  anim_begin(self.fr_death);
  self.think = oldone_burst;
  self.nextthink = time + 0.1;
};

// Alive, it sways: a frame every 0.1 s.
void() oldone_sway = {
  frame_loop(self.fr_stand);
  self.nextthink = time + 0.1;
};

void() monster_oldone = {
  if (no_monsters_here()) return;
  precache_model("progs/oldone.mdl");
  precache_sound("boss2/death.wav");
  precache_sound("boss2/idle.wav");
  precache_sound("boss2/sight.wav");
  precache_sound("boss2/pop2.wav");
  total_monsters = total_monsters + 1;
  self.flags = self.flags | FL_MONSTER;
  self.solid = SOLID_SLIDEBOX;
  self.movetype = MOVETYPE_STEP;
  setmodel(self, "progs/oldone.mdl");
  setsize(self, '-160 -128 -24', '160 128 256');
  setorigin(self, self.origin);
  self.health = 40000;
  self.takedamage = DAMAGE_YES;
  self.th_hurt = noop;  // it does not flinch
  self.th_die = oldone_die;
  self.fr_stand = '0 45 1';   // its sway
  self.fr_death = '46 66 1';  // its shaking apart
  self.think = oldone_sway;
  self.nextthink = time + 0.1 + random() * 0.5;
};
