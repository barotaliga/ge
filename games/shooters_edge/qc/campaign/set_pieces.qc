// THE SET PIECES (porting/shooters-edge/campaign-rules.md § The map vocabulary): trains on their
// paths, secret doors, exploding boxes, fireball spouts, spike and laser traps, bubbles and the
// ambient sounds. The numbers are this variant's own.

// --- the train (func_train): a brush that rides its path of corners ---

// It waits at each corner for that corner's .wait (none when unset; -1 stops it until used
// again), crushes what blocks it for .dmg (2) and rides at .speed (100). Its lowest corner is
// the one that rides the path. Sounds 1 gives it a voice.
void() train_next;

void() train_blocked = {
  if (time < self.attack_finished) return;
  self.attack_finished = time + 0.5;
  other.deathtype = "crushed";
  damage(other, self, self, self.dmg);
};

void() train_arrived = {
  if (self.noise1 != "") sound(self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
  if (self.wait < 0) {  // stops here until used
    self.use = train_next;
    return;
  }
  if (self.wait > 0) {
    self.think = train_next;
    self.nextthink = self.ltime + self.wait;
    return;
  }
  train_next();
};

void() train_next = {
  local entity corner;
  corner = find(world, targetname, self.target);
  if (!corner) {
    objerror("a train's path ends at nothing named ", self.target);
    return;
  }
  self.use = noop;
  self.target = corner.target;
  self.wait = corner.wait;
  if (self.noise != "") sound(self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
  calc_move(corner.origin - self.mins, self.speed, train_arrived);
};

// Once every corner has spawned: stood on its first, and away unless something names it.
void() train_place = {
  local entity corner;
  corner = find(world, targetname, self.target);
  if (!corner) {
    objerror("a train cannot find its first corner ", self.target);
    return;
  }
  self.target = corner.target;
  setorigin(self, corner.origin - self.mins);
  if (self.targetname == "") {
    self.think = train_next;
    self.nextthink = self.ltime + 0.1;
  } else {
    self.use = train_next;
  }
};

void() func_train = {
  if (!self.speed) self.speed = 100;
  if (!self.dmg) self.dmg = 2;
  if (self.target == "") objerror("a train needs a path");
  if (self.sounds == 1) {
    self.noise = "plats/train2.wav";
    self.noise1 = "plats/train1.wav";
    precache_sound(self.noise);
    precache_sound(self.noise1);
  }
  self.angles = '0 0 0';
  brush_start();
  self.blocked = train_blocked;
  self.use = noop;
  self.think = train_place;
  self.nextthink = self.ltime + 0.1;
};

// --- the secret door (func_door_secret): back into the wall, then aside ---

// Its angle is the way it first moves, by its depth (.t_length when set); then it slides to its
// right by its width (.t_width when set), to its left with spawn flag 2, or down by its height
// with 4. It stands open .wait seconds (5) and closes the way it came, or stays open with flag 1
// or a wait of -1. Something names it: used; else shot (and flag 16 makes it shot as well; 8 never).
.float t_length;
.float t_width;
.vector dest1;  // a secret door's first stop, back in the wall
.vector dest2;  // its second, aside

void() secret_shut;

void() secret_closed = {
  if (self.noise3 != "") sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
  self.state = STATE_BOTTOM;
  if (self.max_health) {
    self.health = self.max_health;
    self.takedamage = DAMAGE_YES;
  }
};

void() secret_back = {
  if (self.noise2 != "") sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
  calc_move(self.pos1, self.speed, secret_closed);
};

void() secret_shut = {
  self.state = STATE_DOWN;
  if (self.noise2 != "") sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
  calc_move(self.dest1, self.speed, secret_back);
};

void() secret_open_full = {
  if (self.noise3 != "") sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
  self.state = STATE_TOP;
  if (self.wait < 0 || (self.spawnflags & 1)) return;  // open for good
  self.think = secret_shut;
  self.nextthink = self.ltime + self.wait;
};

void() secret_aside = {
  if (self.noise2 != "") sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
  calc_move(self.dest2, self.speed, secret_open_full);
};

void() secret_use = {
  if (self.state != STATE_BOTTOM) return;
  self.state = STATE_UP;
  self.takedamage = DAMAGE_NO;
  use_targets();
  if (self.noise1 != "") sound(self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
  calc_move(self.dest1, self.speed, secret_aside);
};

void() secret_shot = {
  self.health = self.max_health;
  activator = damage_attacker;
  secret_use();
};

void() secret_blocked = {
  if (time < self.attack_finished) return;
  self.attack_finished = time + 0.5;
  other.deathtype = "squished";
  damage(other, self, self, self.dmg);
};

// A message on it is told to whoever walks into it, at most every 2 s.
void() secret_door_touch = {
  if (other.classname != "player" || self.message == "") return;
  if (time < self.attack_finished) return;
  self.attack_finished = time + 2;
  centerprint(other, self.message);
  sound(other, CHAN_BODY, "misc/talk.wav", 1, ATTN_NORM);
};

void() func_door_secret = {
  local vector fwd, side;
  local float depth, width;
  if (!self.sounds) self.sounds = 3;
  if (self.sounds == 1) {
    self.noise1 = "doors/latch2.wav";
    self.noise2 = "doors/winch2.wav";
    self.noise3 = "doors/drclos4.wav";
  } else if (self.sounds == 2) {
    self.noise1 = "doors/airdoor2.wav";
    self.noise2 = "doors/airdoor1.wav";
    self.noise3 = "doors/airdoor2.wav";
  } else {
    self.noise1 = "doors/basesec2.wav";
    self.noise2 = "doors/basesec1.wav";
    self.noise3 = "doors/basesec2.wav";
  }
  precache_sound(self.noise1);
  precache_sound(self.noise2);
  precache_sound(self.noise3);
  makevectors(self.angles);
  side = v_right;
  set_movedir();
  fwd = self.movedir;
  brush_start();
  if (!self.speed) self.speed = 50;
  if (!self.wait) self.wait = 5;
  if (!self.dmg) self.dmg = 2;
  depth = self.t_length;
  if (!depth) depth = fabs(fwd * self.size);
  self.pos1 = self.origin;
  self.dest1 = self.origin + fwd * depth;
  if (self.spawnflags & 4) {
    width = self.t_width;
    if (!width) width = self.size_z;
    self.dest2 = self.dest1 - '0 0 1' * width;
  } else {
    if (self.spawnflags & 2) side = '0 0 0' - side;
    width = self.t_width;
    if (!width) width = fabs(side * self.size);
    self.dest2 = self.dest1 + side * width;
  }
  self.state = STATE_BOTTOM;
  self.use = secret_use;
  self.blocked = secret_blocked;
  self.touch = secret_door_touch;
  if (!(self.spawnflags & 8) && (self.targetname == "" || (self.spawnflags & 16))) {
    self.max_health = 10000;
    self.health = 10000;
    self.takedamage = DAMAGE_YES;
    self.th_hurt = secret_shot;
    self.th_die = secret_shot;
  }
};

// --- the exploding boxes (misc_explobox, misc_explobox2): 20 health, then a blast of 160 ---

void() explobox_blow = {
  self.origin = self.origin + '0 0 32';
  radius_damage(self, self, 160, world, "blew up");
  sound(self, CHAN_VOICE, "weapons/r_exp3.wav", 1, ATTN_NORM);
  effect_explosion(self.origin);
  remove(self);
};

// Dying: it blows a moment later, so a row of boxes goes off one after the other.
void() explobox_die = {
  self.takedamage = DAMAGE_NO;
  self.think = explobox_blow;
  self.nextthink = time + 0.2;
};

void(string mdl) explobox_start = {
  if (deathmatch) {
    remove(self);
    return;
  }
  precache_model(mdl);
  precache_sound("weapons/r_exp3.wav");
  self.solid = SOLID_BBOX;
  self.movetype = MOVETYPE_NONE;
  setmodel(self, mdl);
  self.origin_z = self.origin_z + 2;
  setorigin(self, self.origin);
  if (!droptofloor()) setorigin(self, self.origin);  // left where the map put it
  self.health = 20;
  self.takedamage = DAMAGE_AIM;
  self.th_hurt = noop;
  self.th_die = explobox_die;
};

void() misc_explobox = { explobox_start("maps/b_explob.bsp"); };
void() misc_explobox2 = { explobox_start("maps/b_exbox2.bsp"); };

// --- the fireball spout (misc_fireball): a ball of lava up at .speed (1000) every 0-5 s ---

void() fireball_touch = {
  if (other.takedamage) {
    other.deathtype = "burned";
    damage(other, self, self, 20);
  }
  remove(self);
};

void() fireball_fly = {
  local entity b;
  b = spawn();
  b.classname = "fireball";
  b.solid = SOLID_TRIGGER;
  b.movetype = MOVETYPE_TOSS;
  b.velocity = '0 0 1' * self.speed + crandom() * '50 0 0' + crandom() * '0 50 0';
  b.avelocity = '200 100 300';
  b.touch = fireball_touch;
  b.think = remove_self;
  b.nextthink = time + 5;
  setmodel(b, "progs/lavaball.mdl");
  setsize(b, '0 0 0', '0 0 0');
  setorigin(b, self.origin);
  self.nextthink = time + random() * 5;
};

void() misc_fireball = {
  precache_model("progs/lavaball.mdl");
  if (!self.speed) self.speed = 1000;
  self.think = fireball_fly;
  self.nextthink = time + random() * 5;
};

// --- the traps (trap_spikeshooter, trap_shooter): a spike along their angle when used ---

// A nail at 500 for 9; spawn flag 1 a heavy one for 15, flag 2 a laser at 600 for 15. The
// shooter also fires on its own every .wait seconds (1).
void() trap_fire = {
  local entity m;
  v_forward = self.movedir;  // what launch aims along
  if (self.spawnflags & 2) {
    sound(self, CHAN_VOICE, "enforcer/enfire.wav", 1, ATTN_NORM);
    m = launch("progs/laser.mdl", MOVETYPE_FLYMISSILE, 600, 5);
    m.dmg = 15;
    m.touch = monster_missile_touch;
    m.deathtype = "lasered";
  } else {
    sound(self, CHAN_VOICE, "weapons/spike2.wav", 1, ATTN_NORM);
    if (self.spawnflags & 1) m = launch("progs/s_spike.mdl", MOVETYPE_FLYMISSILE, 500, 6);
    else m = launch("progs/spike.mdl", MOVETYPE_FLYMISSILE, 500, 6);
    m.classname = "spike";
    m.dmg = 9;
    if (self.spawnflags & 1) m.dmg = 15;
    m.touch = spike_touch;
  }
  setsize(m, '0 0 0', '0 0 0');
  setorigin(m, self.origin);
};

void() trap_start = {
  set_movedir();
  precache_model("progs/spike.mdl");
  precache_model("progs/s_spike.mdl");
  precache_model("progs/laser.mdl");
  precache_sound("weapons/spike2.wav");
  precache_sound("enforcer/enfire.wav");
  self.use = trap_fire;
};

void() trap_spikeshooter = { trap_start(); };

void() trap_shooter_think = {
  trap_fire();
  self.nextthink = time + self.wait;
};

void() trap_shooter = {
  trap_start();
  if (!self.wait) self.wait = 1;
  self.think = trap_shooter_think;
  self.nextthink = time + self.wait;
};

// --- bubbles (air_bubbles): one a second rising out of the water's floor ---

void() bubble_rise = {
  if (pointcontents(self.origin) != CONTENT_WATER || time > self.attack_finished) {
    remove(self);
    return;
  }
  self.velocity = '0 0 15' + crandom() * '4 0 0' + crandom() * '0 4 0';
  self.nextthink = time + 0.5;
};

void() bubbles_make = {
  local entity b;
  b = spawn();
  b.classname = "bubble";
  b.solid = SOLID_NOT;
  b.movetype = MOVETYPE_NOCLIP;
  b.velocity = '0 0 15';
  b.attack_finished = time + 10;
  b.think = bubble_rise;
  b.nextthink = time + 0.5;
  setmodel(b, "progs/s_bubble.spr");
  setsize(b, '0 0 0', '0 0 0');
  setorigin(b, self.origin);
  self.nextthink = time + 0.5 + random();
};

void() air_bubbles = {
  if (deathmatch) {
    remove(self);
    return;
  }
  precache_model("progs/s_bubble.spr");
  self.think = bubbles_make;
  self.nextthink = time + 1;
};

// --- the ambient sounds: each a loop the clients play where it stands ---

void(string snd, float vol) ambient_start = {
  precache_sound(snd);
  ambientsound(self.origin, snd, vol, ATTN_STATIC);
  remove(self);
};

void() ambient_comp_hum = { ambient_start("ambience/comp1.wav", 1); };
void() ambient_drip = { ambient_start("ambience/drip1.wav", 0.5); };
void() ambient_drone = { ambient_start("ambience/drone6.wav", 0.5); };
void() ambient_flouro_buzz = { ambient_start("ambience/buzz1.wav", 1); };
void() ambient_light_buzz = { ambient_start("ambience/fl_hum1.wav", 0.5); };
void() ambient_suck_wind = { ambient_start("ambience/suck1.wav", 1); };
void() ambient_swamp1 = { ambient_start("ambience/swamp1.wav", 0.5); };
void() ambient_swamp2 = { ambient_start("ambience/swamp2.wav", 0.5); };
void() ambient_thunder = { ambient_start("ambience/thunder1.wav", 0.5); };
