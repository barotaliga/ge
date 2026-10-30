// HELPERS every other file uses: removing an entity later, a mover's straight move to a point at
// a speed, the direction an editor's angle means, and firing an entity's targets.

void() noop = {};

void() remove_self = { remove(self); };

// The direction a map's "angle" key means: -1 is straight up, -2 straight down, else a yaw in
// degrees. Clears the angles so a brush entity is not drawn turned.
void() set_movedir = {
  if (self.angles == '0 -1 0') {
    self.movedir = '0 0 1';
  } else if (self.angles == '0 -2 0') {
    self.movedir = '0 0 -1';
  } else {
    makevectors(self.angles);
    self.movedir = v_forward;
  }
  self.angles = '0 0 0';
};

// --- a mover's straight move ---

void() calc_move_arrive = {
  setorigin(self, self.finaldest);
  self.velocity = '0 0 0';
  self.nextthink = -1;
  if (self.move_done) self.move_done();
};

// Moves `self` (a pusher: its clock is .ltime) to `dest` at `spd` units a second, then calls
// `done`. The move ends exactly on `dest` at a think on the mover's own clock.
void(vector dest, float spd, void() done) calc_move = {
  local vector delta;
  local float travel;
  if (!spd) objerror("calc_move: no speed for ", self.classname);
  self.move_done = done;
  self.finaldest = dest;
  self.think = calc_move_arrive;
  delta = dest - self.origin;
  if (delta == '0 0 0') {
    self.nextthink = self.ltime + 0.1;
    return;
  }
  travel = vlen(delta) / spd;
  if (travel < 0.1) {
    self.velocity = '0 0 0';
    self.nextthink = self.ltime + 0.1;
    return;
  }
  self.nextthink = self.ltime + travel;
  self.velocity = delta * (1 / travel);
};

// --- firing targets ---

void() delayed_use = {
  activator = self.enemy;
  use_targets();
  remove(self);
};

// Fires `self`'s targets for `activator`: after .delay seconds when it has one (a helper entity
// waits), shows .message to a player who set it off, removes every entity named by .killtarget,
// and calls `use` on every entity named by .target.
void() use_targets = {
  local entity t, was_self, was_other;
  if (self.delay) {
    t = spawn();
    t.classname = "delayed_use";
    t.nextthink = time + self.delay;
    t.think = delayed_use;
    t.enemy = activator;
    t.message = self.message;
    t.killtarget = self.killtarget;
    t.target = self.target;
    return;
  }
  if (activator.classname == "player" && self.message != "") {
    centerprint(activator, self.message);
  }
  if (self.killtarget != "") {
    t = find(world, targetname, self.killtarget);
    while (t) {
      remove(t);
      t = find(t, targetname, self.killtarget);
    }
  }
  if (self.target != "") {
    was_self = self;
    was_other = other;
    t = find(world, targetname, self.target);
    while (t) {
      self = t;
      other = was_self;
      if (self.use) self.use();
      self = was_self;
      other = was_other;
      t = find(t, targetname, self.target);
    }
  }
};

// --- effects ---

void(vector org) effect_explosion = {
  WriteByte(MSG_MULTICAST, SVC_TEMPENTITY);
  WriteByte(MSG_MULTICAST, TE_EXPLOSION);
  WriteCoord(MSG_MULTICAST, org.x);
  WriteCoord(MSG_MULTICAST, org.y);
  WriteCoord(MSG_MULTICAST, org.z);
  multicast(org, MULTICAST_PHS);
};

// A puff where a shot hits: blood on something that bleeds, a chip of the wall elsewhere.
void(vector org, float count, float bleeds) effect_impact = {
  WriteByte(MSG_MULTICAST, SVC_TEMPENTITY);
  if (bleeds) WriteByte(MSG_MULTICAST, TE_BLOOD);
  else WriteByte(MSG_MULTICAST, TE_GUNSHOT);
  WriteByte(MSG_MULTICAST, count);
  WriteCoord(MSG_MULTICAST, org.x);
  WriteCoord(MSG_MULTICAST, org.y);
  WriteCoord(MSG_MULTICAST, org.z);
  multicast(org, MULTICAST_PVS);
};

// The arc gun's beam from `from` to `to`, drawn as `self`'s.
void(vector from, vector to) effect_beam = {
  WriteByte(MSG_MULTICAST, SVC_TEMPENTITY);
  WriteByte(MSG_MULTICAST, TE_LIGHTNING2);
  WriteEntity(MSG_MULTICAST, self);
  WriteCoord(MSG_MULTICAST, from.x);
  WriteCoord(MSG_MULTICAST, from.y);
  WriteCoord(MSG_MULTICAST, from.z);
  WriteCoord(MSG_MULTICAST, to.x);
  WriteCoord(MSG_MULTICAST, to.y);
  WriteCoord(MSG_MULTICAST, to.z);
  multicast(from, MULTICAST_PHS);
};

// A random float in -1..1.
float() crandom = { return 2 * random() - 1; };

float(float a, float b) min_of = { if (a < b) return a; return b; };
