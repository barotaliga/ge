// THE MONSTERS' MINDS: a monster's life (spawning, being used, dying), its senses (the four
// ranges, seeing, in front, noticing a player) and its states (stand, walk a path, run at an
// enemy, attack, pain), as porting/shooters-edge/campaign-rules.md § A monster's life, § Senses
// and § The states give them. Each kind (monsters.qc) fills in its own numbers and its attacks:
// the `th_` hooks below, its step distances, its pain and its death.
//
// A state is a think that runs every 0.1 s and takes one decision with the kind's step distance.
// A kind starts with `walkmonster_start`, `flymonster_start` or `swimmonster_start`.
// The animation frames are not chosen yet: every monster holds its model's first frame, and a
// dead one is laid on its side.

// --- the fields a monster carries ---

.void() th_stand;       // the kind's states, each entered by calling it
.void() th_walk;
.void() th_run;
.void() th_melee;       // unset: the kind has no melee attack
.void() th_missile;     // unset: no ranged attack
.void() th_pain;        // the kind's pain reaction (after the anger below)
.float() th_check;      // the kind's own attack decision (unset: `check_attack`)
.entity movetarget;     // the path corner walked to
.entity oldenemy;       // the player turned from to fight another monster, returned to after
.float pausetime;       // a standing monster walks again after this
.float search_time;     // the enemy is remembered until this without being seen
.float attack_state;    // AS_ below
.float walk_step;       // the kind's units a 0.1 s step when walking
.float run_step;        // ... and when running
.float anim_step;       // how far into an attack or a pain the monster is, in 0.1 s steps
// The kind's model frames for each state: _x the first, _y the last, _z 1 when the model has them
// (unset: the frame stays). A loop for standing, walking and running; played through once, a
// frame a 0.1 s step and holding the last, for an attack, a leap, a pain and a death.
.vector fr_stand, fr_walk, fr_run, fr_melee, fr_missile, fr_leap, fr_pain, fr_death;
.vector fr_now;         // the frames of the attack, leap or pain in progress
.float gib_health;      // a death below this health gibs the body
.string head_model;     // what is left of a gibbed body

float AS_STRAIGHT = 1;  // chasing the enemy
float AS_MELEE = 3;     // turning to strike
float AS_MISSILE = 4;   // turning to fire

float RANGE_MELEE = 0;
float RANGE_NEAR = 1;
float RANGE_MID = 2;
float RANGE_FAR = 3;

float FL_NOTARGET = 128;     // a player monsters do not look for
float SPAWN_AMBUSH = 1;      // wakes only by seeing a player itself or being hurt
float SVC_KILLEDMONSTER = 27;
float FOREVER = 100000000;

entity sight_entity;         // the room's latest sighting: a monster that just spotted a player
float sight_entity_time;

// --- the senses ---

float(entity targ) range_to = {
  local float r;
  r = vlen(self.origin + self.view_ofs - (targ.origin + targ.view_ofs));
  if (r < 120) return RANGE_MELEE;
  if (r < 500) return RANGE_NEAR;
  if (r < 1000) return RANGE_MID;
  return RANGE_FAR;
};

// A line from eye to eye that ignores monsters, and does not cross into or out of water.
float(entity targ) can_see = {
  traceline(self.origin + self.view_ofs, targ.origin + targ.view_ofs, TRUE, self);
  if (trace_inopen && trace_inwater) return FALSE;
  return trace_fraction == 1;
};

// Within about 73 degrees either side of where the monster faces.
float(entity targ) in_front = {
  makevectors(self.angles);
  return normalize(targ.origin - self.origin) * v_forward > 0.3;
};

// The yaw toward the enemy, and whether the monster now faces it within 45 degrees.
float() face_enemy = {
  local float delta;
  self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
  changeyaw();
  delta = self.angles_y - self.ideal_yaw;
  while (delta < 0) delta = delta + 360;
  while (delta >= 360) delta = delta - 360;
  return delta <= 45 || delta >= 315;
};

// The monster has an enemy: it calls out, tells the room, heads for him and holds its fire a second.
void() found_target = {
  if (self.enemy.classname == "player") {
    sight_entity = self;
    sight_entity_time = time;
  }
  self.show_hostile = time + 1;
  self.goalentity = self.enemy;
  self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
  self.search_time = time + 5;
  self.attack_finished = time + 1;
  self.attack_state = AS_STRAIGHT;
  if (self.noise != "") sound(self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
  self.th_run();
};

// Once a frame: one candidate, the client the server offers or a monster that has just spotted
// one, judged by its range: far never, mid in front and seen, near seen and in front unless he
// made noise, melee seen.
float() look_for_target = {
  local entity client;
  local float r;
  if (sight_entity_time >= time - 0.1 && !(self.spawnflags & SPAWN_AMBUSH)) {
    client = sight_entity;
    if (client.enemy == self.enemy) return FALSE;
  } else {
    client = checkclient();
    if (!client) return FALSE;
  }
  if (client == self.enemy) return FALSE;
  if (client.flags & FL_NOTARGET) return FALSE;
  r = range_to(client);
  if (r == RANGE_FAR) return FALSE;
  if (!can_see(client)) return FALSE;
  if (r == RANGE_NEAR) {
    if (client.show_hostile < time && !in_front(client)) return FALSE;
  } else if (r == RANGE_MID) {
    if (!in_front(client)) return FALSE;
  }
  self.enemy = client;
  if (self.enemy.classname != "player") {
    self.enemy = self.enemy.enemy;  // a monster passes on its own enemy
    if (self.enemy.classname != "player") {
      self.enemy = world;
      return FALSE;
    }
  }
  found_target();
  return TRUE;
};

// --- deciding to attack ---

// The default: a clear line to the enemy, then the melee blow in melee range, else a ranged attack
// by chance when its cooldown has passed and the enemy is not far.
float() check_attack = {
  local float r, chance;
  traceline(self.origin + self.view_ofs, self.enemy.origin + self.enemy.view_ofs, FALSE, self);
  if (trace_inopen && trace_inwater) return FALSE;
  if (trace_ent != self.enemy) return FALSE;
  r = range_to(self.enemy);
  if (r == RANGE_MELEE && self.th_melee) {
    self.attack_state = AS_MELEE;
    return TRUE;
  }
  if (!self.th_missile) return FALSE;
  if (time < self.attack_finished) return FALSE;
  if (r == RANGE_FAR) return FALSE;
  if (r == RANGE_MELEE) chance = 0.9;
  else if (r == RANGE_NEAR) {
    if (self.th_melee) chance = 0.2;
    else chance = 0.4;
  } else {
    if (self.th_melee) chance = 0.05;
    else chance = 0.1;
  }
  if (random() < chance) {
    self.attack_state = AS_MISSILE;
    self.attack_finished = time + 2 * random();
    return TRUE;
  }
  return FALSE;
};

// A melee blow: within 60 units of the enemy and seen, `n` random numbers summed times `factor`.
void(float n, float factor) melee_blow_n = {
  local float sum;
  if (vlen(self.enemy.origin - self.origin) > 60) return;
  if (!can_see(self.enemy)) return;
  sum = 0;
  while (n > 0) {
    sum = sum + random();
    n = n - 1;
  }
  self.enemy.deathtype = "mauled";
  damage(self.enemy, self, self, sum * factor);
};

// The usual blow: three random numbers.
void(float factor) melee_blow = { melee_blow_n(3, factor); };

// A step of a melee attack: facing the enemy, closing in by `dist` until within the blow's reach.
void(float dist) ai_charge = {
  face_enemy();
  if (vlen(self.enemy.origin - self.origin) > 60) walkmove(self.ideal_yaw, dist);
};

// --- the frames ---

// The next frame of a loop, from its first when the monster was showing another.
void(vector r) frame_loop = {
  if (!r_z) return;
  if (self.frame < r_x || self.frame >= r_y) self.frame = r_x;
  else self.frame = self.frame + 1;
};

// An attack, a leap or a pain begins: its frames from the first.
void(vector r) anim_begin = {
  self.anim_step = 0;
  self.fr_now = r;
};

// One 0.1 s step further into it, and the frame that step shows (the last held).
void() anim_tick = {
  local float f;
  self.anim_step = self.anim_step + 1;
  if (!self.fr_now_z) return;
  f = self.fr_now_x + self.anim_step - 1;
  if (f > self.fr_now_y) f = self.fr_now_y;
  self.frame = f;
};

// --- the states ---

void(float dist) ai_stand = {
  if (look_for_target()) return;
  if (time > self.pausetime) self.th_walk();
};

void(float dist) ai_walk = {
  if (look_for_target()) return;
  movetogoal(dist);
};

// The enemy dead: back to the one it turned from, else to its path, else it stands.
void() lost_enemy = {
  self.enemy = world;
  if (self.oldenemy.health > 0 && self.oldenemy.takedamage) {
    self.enemy = self.oldenemy;
    self.oldenemy = world;
    found_target();
    return;
  }
  if (self.movetarget) {
    self.goalentity = self.movetarget;
    self.th_walk();
  } else {
    self.pausetime = time + FOREVER;
    self.th_stand();
  }
};

void(float dist) ai_run = {
  local float decided;
  if (self.enemy.health <= 0 || !self.enemy.takedamage) {
    lost_enemy();
    return;
  }
  if (can_see(self.enemy)) self.search_time = time + 5;
  if (coop && self.search_time < time) {
    if (look_for_target()) return;
  }
  if (self.attack_state == AS_MISSILE) {
    if (face_enemy()) {
      self.attack_state = AS_STRAIGHT;
      self.th_missile();
    }
    return;
  }
  if (self.attack_state == AS_MELEE) {
    if (face_enemy()) {
      self.attack_state = AS_STRAIGHT;
      self.th_melee();
    }
    return;
  }
  if (self.th_check) decided = self.th_check();
  else decided = check_attack();
  if (decided) return;
  movetogoal(dist);
};

// A stand, a walk and a run as thinks: one step every 0.1 s at the kind's distances.
void() monster_stand = {
  self.think = monster_stand;
  self.nextthink = time + 0.1;
  frame_loop(self.fr_stand);
  ai_stand(0);
};

void() monster_walk = {
  self.think = monster_walk;
  self.nextthink = time + 0.1;
  frame_loop(self.fr_walk);
  ai_walk(self.walk_step);
};

void() monster_run = {
  self.think = monster_run;
  self.nextthink = time + 0.1;
  frame_loop(self.fr_run);
  ai_run(self.run_step);
};

// --- being hurt, being used, dying ---

// Hurt by anything but itself or its enemy, it turns on the attacker, unless he is of its own
// kind (riflemen excepted); a player turned from is remembered. Then the kind's pain, at the
// hardest skill no more than every 5 s.
void() monster_hurt = {
  local entity a;
  a = damage_attacker;
  if (a != self && a != self.enemy && a.takedamage &&
      (a.flags & (FL_CLIENT | FL_MONSTER))) {
    if (a.classname != self.classname || self.classname == "monster_army") {
      if (self.enemy.classname == "player") self.oldenemy = self.enemy;
      self.enemy = a;
      found_target();
    }
  }
  if (time < self.pain_finished) return;
  if (cvar("skill") >= 3) self.pain_finished = time + 5;
  if (self.th_pain) self.th_pain();
};

// Fired by a trigger: angry at the player who set it off.
void() monster_use = {
  if (self.enemy) return;
  if (self.health <= 0) return;
  if (activator.classname != "player") return;
  if (activator.flags & FL_NOTARGET) return;
  self.enemy = activator;
  found_target();
};

void(string mdl, float speed) throw_gib = {
  local entity g;
  g = spawn();
  g.classname = "gib";
  g.movetype = MOVETYPE_BOUNCE;
  g.solid = SOLID_NOT;
  g.velocity = '0 0 200' + crandom() * speed * '1 0 0' + crandom() * speed * '0 1 0';
  g.velocity_z = g.velocity_z + random() * speed;
  g.avelocity = random() * '0 600 0' + random() * '600 0 0';
  setmodel(g, mdl);
  setsize(g, '0 0 0', '0 0 0');
  setorigin(g, self.origin);
  g.think = remove_self;
  g.nextthink = time + 10 + random() * 10;
};

// The body falls through the kind's death frames and lies on the last.
void() corpse_fall = {
  anim_tick();
  if (self.frame >= self.fr_now_y) return;
  self.think = corpse_fall;
  self.nextthink = time + 0.1;
};

// Counted and told, its targets fired for its enemy, then the body: gibbed when the blow was big
// enough (the head left), else laid down.
void() monster_die = {
  killed_monsters = killed_monsters + 1;
  WriteByte(MSG_ALL, SVC_KILLEDMONSTER);
  activator = self.enemy;
  use_targets();
  self.deadflag = DEAD_DEAD;
  self.takedamage = DAMAGE_NO;
  self.solid = SOLID_NOT;
  self.flags = self.flags - (self.flags & FL_MONSTER);
  self.think = noop;
  self.nextthink = -1;
  if (self.health < self.gib_health) {
    sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
    throw_gib("progs/gib1.mdl", 300);
    throw_gib("progs/gib2.mdl", 300);
    throw_gib("progs/gib3.mdl", 300);
    setmodel(self, self.head_model);
    setsize(self, '-16 -16 0', '16 16 56');
    self.movetype = MOVETYPE_BOUNCE;
    self.velocity = '0 0 200';
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    return;
  }
  if (self.noise1 != "") sound(self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
  if (!self.fr_death_z) {  // a kind with no death frames is laid on its side
    self.angles_z = 90;
    setorigin(self, self.origin - '0 0 16');
    return;
  }
  anim_begin(self.fr_death);
  corpse_fall();
};

// --- spawning ---

// Thinking begins: on a path when its target names a corner, else standing.
void() monster_begin = {
  local entity corner;
  self.pausetime = time + FOREVER;
  if (self.target != "") {
    corner = find(world, targetname, self.target);
    if (corner && corner.classname == "path_corner") {
      self.movetarget = corner;
      self.goalentity = corner;
      self.ideal_yaw = vectoyaw(corner.origin - self.origin);
      self.pausetime = 0;
      self.th_walk();
      return;
    }
  }
  self.th_stand();
};

// What every kind's start shares: counted at once, solid, its model, box and health, the mind's
// hooks, thinking a random 0-0.5 s later.
void(string mdl, float hp, vector lo, vector hi) monster_start = {
  total_monsters = total_monsters + 1;
  self.solid = SOLID_SLIDEBOX;
  self.movetype = MOVETYPE_STEP;
  setmodel(self, mdl);
  setsize(self, lo, hi);
  self.health = hp;
  self.max_health = hp;
  self.takedamage = DAMAGE_AIM;
  self.flags = self.flags | FL_MONSTER;
  self.view_ofs = '0 0 25';
  self.yaw_speed = 20;
  self.ideal_yaw = self.angles_y;
  self.use = monster_use;
  self.th_hurt = monster_hurt;
  self.th_stand = monster_stand;
  self.th_walk = monster_walk;
  self.th_run = monster_run;
  self.think = monster_begin;
  self.nextthink = time + 0.1 + random() * 0.5;
};

// A walker: raised a unit and dropped to the floor.
void(string mdl, float hp, vector lo, vector hi) walkmonster_start = {
  monster_start(mdl, hp, lo, hi);
  setorigin(self, self.origin + '0 0 1');
  if (!droptofloor()) dprint(self.classname, " in a wall at ", vtos(self.origin), "\n");
};

// A flyer: stays where it is placed, turns at half a walker's rate.
void(string mdl, float hp, vector lo, vector hi) flymonster_start = {
  monster_start(mdl, hp, lo, hi);
  self.flags = self.flags | FL_FLY;
  self.yaw_speed = 10;
  setorigin(self, self.origin);
};

// A swimmer: stays where it is placed, in its water.
void(string mdl, float hp, vector lo, vector hi) swimmonster_start = {
  monster_start(mdl, hp, lo, hi);
  self.flags = self.flags | FL_SWIM;
  self.yaw_speed = 10;
  setorigin(self, self.origin);
};

// In deathmatch the map's monsters are not there: true when this one was removed.
float() no_monsters_here = {
  if (!deathmatch) return FALSE;
  remove(self);
  return TRUE;
};

// --- missiles ---

void() monster_missile_touch = {
  if (other == self.owner) return;
  if (other.takedamage) {
    other.deathtype = self.deathtype;
    damage(other, self, self.owner, self.dmg);
  }
  remove(self);
};

// The view vectors along `dir`: a direction's angles pitch up where a view's pitch down, so the
// pitch is turned over before the vectors are made.
void(vector dir) aim_along = {
  local vector a;
  a = vectoangles(dir);
  a_x = 0 - a_x;
  makevectors(a);
};

// A missile at the enemy's eye from the monster's: `mdl` at `spd` units a second, `points` on what
// it hits; `lob` units a second added upward (it then falls: a toss), else it flies straight.
entity(string mdl, float spd, float points, float lob) monster_missile = {
  local entity m;
  local vector dir;
  dir = self.enemy.origin + self.enemy.view_ofs - (self.origin + self.view_ofs);
  aim_along(dir);
  if (lob) m = launch(mdl, MOVETYPE_TOSS, spd, 5);
  else m = launch(mdl, MOVETYPE_FLYMISSILE, spd, 5);
  m.velocity = m.velocity + lob * '0 0 1';
  m.classname = "monster_missile";
  m.dmg = points;
  m.deathtype = "hit";
  m.touch = monster_missile_touch;
  return m;
};

// A path corner: a monster walking to it goes on to the next; the last one stands it.
void() corner_touch = {
  local entity next, was_self;
  if (other.movetarget != self) return;
  if (other.enemy) return;
  next = find(world, targetname, self.target);
  if (self.target == "" || !next) {
    other.movetarget = world;
    other.goalentity = world;
    other.pausetime = time + FOREVER;
    was_self = self;
    self = other;
    self.th_stand();
    self = was_self;
    return;
  }
  other.movetarget = next;
  other.goalentity = next;
  other.ideal_yaw = vectoyaw(next.origin - other.origin);
};

void() path_corner = {
  self.solid = SOLID_TRIGGER;
  self.touch = corner_touch;
  setsize(self, '-8 -8 -8', '8 8 8');
};

void() ai_precache = {
  precache_model("progs/gib1.mdl");
  precache_model("progs/gib2.mdl");
  precache_model("progs/gib3.mdl");
  precache_sound("player/udeath.wav");
};
