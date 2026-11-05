// THE MAP'S THINGS every variant answers: teleport destinations, lights and flames, walls, doors,
// plats, buttons and triggers, teleporters, pushes and hurts.

void() info_teleport_destination = {
  self.origin = self.origin + '0 0 27';
  if (self.targetname == "") objerror("a teleport destination needs a targetname");
};

// Lights are the map compiler's business; nothing is left of them at run time but the torches
// and the flames, which the clients draw where they stand (static: the server forgets them).
void() light = { remove(self); };
void() light_fluoro = { remove(self); };

void(string mdl, float fr) flame_start = {
  precache_model(mdl);
  setmodel(self, mdl);
  self.frame = fr;
  makestatic(self);
};
void() light_torch_small_walltorch = { flame_start("progs/flame.mdl", 0); };
void() light_flame_large_yellow = { flame_start("progs/flame2.mdl", 1); };
void() light_flame_small_yellow = { flame_start("progs/flame2.mdl", 0); };
void() light_flame_small_white = { flame_start("progs/flame2.mdl", 0); };

// --- brush entities ---

// A brush entity: its own model, standing still, blocking.
void() brush_start = {
  self.solid = SOLID_BSP;
  self.movetype = MOVETYPE_PUSH;
  setorigin(self, self.origin);
  setmodel(self, self.model);
};

void() func_wall = {
  self.angles = '0 0 0';
  brush_start();
};

void() func_illusionary = {
  self.angles = '0 0 0';
  self.solid = SOLID_NOT;
  self.movetype = MOVETYPE_NONE;
  setmodel(self, self.model);
  makestatic(self);
};

// A trigger field the size of `self`'s brush (or of `lo`..`hi` around it when given), invisible.
void() trigger_start = {
  if (self.angles != '0 0 0') set_movedir();
  self.solid = SOLID_TRIGGER;
  self.movetype = MOVETYPE_NONE;
  setmodel(self, self.model);
  self.modelindex = 0;
  self.model = "";
};

// --- doors ---
//
// Doors whose boxes touch are one GROUP and open and shut together (spawnflag 4 keeps a door out
// of any group). The group's first door is its master, every door's .owner; .enemy chains the
// group from the master. The master answers for the group: a field round the whole group opens
// it when a player comes near, unless a door in it is named (a trigger opens it) or shot open (it
// has health); a door that needs a key (spawnflag 16 the silver, 8 the gold) always has its field,
// and opens only for a player who holds the key (he keeps it until the map ends). A named group's
// words (.message) are shown to a player who touches it, as .netname, every 2 s at most.

float DOOR_APART = 4;
float DOOR_GOLD = 8;
float DOOR_SILVER = 16;

void() door_go_down;
void() door_go_up;

void() door_hit_bottom = {
  sound(self, CHAN_VOICE, "doors/drclos4.wav", 1, ATTN_NORM);
  self.state = STATE_BOTTOM;
};

void() door_hit_top = {
  sound(self, CHAN_VOICE, "doors/drclos4.wav", 1, ATTN_NORM);
  self.state = STATE_TOP;
  if (self.wait < 0) return;  // stays open
  self.think = door_go_down;
  self.nextthink = self.ltime + self.wait;
};

void() door_go_down = {
  sound(self, CHAN_VOICE, "doors/doormv1.wav", 1, ATTN_NORM);
  self.state = STATE_DOWN;
  calc_move(self.pos1, self.speed, door_hit_bottom);
};

void() door_go_up = {
  if (self.state == STATE_UP) return;
  if (self.state == STATE_TOP) {  // touched again while open: wait longer
    self.nextthink = self.ltime + self.wait;
    return;
  }
  sound(self, CHAN_VOICE, "doors/doormv1.wav", 1, ATTN_NORM);
  self.state = STATE_UP;
  calc_move(self.pos2, self.speed, door_hit_top);
  use_targets();
};

// Every door of `self`'s group opens (or, below, shuts).
void() door_group_up = {
  local entity d, was_self;
  was_self = self;
  d = self.owner;
  while (d) {
    self = d;
    door_go_up();
    d = d.enemy;
  }
  self = was_self;
};

void() door_group_down = {
  local entity d, was_self;
  was_self = self;
  d = self.owner;
  while (d) {
    self = d;
    if (self.state == STATE_UP || self.state == STATE_TOP) door_go_down();
    d = d.enemy;
  }
  self = was_self;
};

// Something stands in the way: it takes the door's damage and the group goes back.
void() door_blocked = {
  other.deathtype = "squished";
  damage(other, self, self, self.dmg);
  if (self.wait >= 0) {
    if (self.state == STATE_DOWN) door_group_up();
    else door_group_down();
  }
};

// Used by a trigger (the activator is whoever set the trigger off).
void() door_use = { door_group_up(); };

// A blow that takes a shot door's health opens its group; the door can be shot again.
void() door_killed = {
  self.health = self.max_health;
  activator = damage_attacker;
  door_group_up();
};

// `words` for `who`, from the door `self`, every 2 s at most (the master keeps the time).
void(entity who, string words) door_say = {
  if (time < self.owner.attack_finished) return;
  self.owner.attack_finished = time + 2;
  centerprint(who, words);
};

// A named group's door touched by a player: its words, if it has any.
void() door_touch = {
  if (other.classname != "player" || other.health <= 0) return;
  if (self.owner.netname == "") return;
  door_say(other, self.owner.netname);
};

void() door_trigger_touch = {
  local entity field;
  local float key;
  if (other.health <= 0 || other.classname != "player") return;
  field = self;
  self = self.owner;
  key = 0;
  if (self.spawnflags & DOOR_SILVER) key = IT_KEY1;
  else if (self.spawnflags & DOOR_GOLD) key = IT_KEY2;
  if (key && !(other.items & key)) {
    if (key == IT_KEY1) door_say(other, "This door needs the silver key.");
    else door_say(other, "This door needs the gold key.");
  } else {
    activator = other;
    door_group_up();
  }
  self = field;
};

// The field round a group (`lo`..`hi`, its doors' linked bounds, each a unit wider than the door's
// box) that opens it when a player comes near.
void(vector lo, vector hi) door_spawn_field = {
  local entity t;
  t = spawn();
  t.classname = "door_trigger";
  t.movetype = MOVETYPE_NONE;
  t.solid = SOLID_TRIGGER;
  t.owner = self;
  t.touch = door_trigger_touch;
  setsize(t, lo - '59 59 7', hi + '59 59 7');
};

// Whether two things' boxes touch or overlap.
float(entity a, entity b) boxes_touch = {
  if (a.absmin_x > b.absmax_x || b.absmin_x > a.absmax_x) return FALSE;
  if (a.absmin_y > b.absmax_y || b.absmin_y > a.absmax_y) return FALSE;
  if (a.absmin_z > b.absmax_z || b.absmin_z > a.absmax_z) return FALSE;
  return TRUE;
};

// Once every brush has its size, the first door of a group to think gathers every door touching
// it (and those touching them) into its group, then gives the group its field or its words.
void() door_link = {
  local entity d, m, tail;
  local float grew, named, shot;
  local vector lo, hi;
  if (self.owner != self) return;  // already in another door's group
  tail = self;
  grew = !(self.spawnflags & DOOR_APART);
  while (grew) {
    grew = FALSE;
    d = find(world, classname, "func_door");
    while (d) {
      if (d.owner == d && d != self && !d.enemy && !(d.spawnflags & DOOR_APART)) {
        m = self;
        while (m) {
          if (boxes_touch(m, d)) {
            tail.enemy = d;
            tail = d;
            d.owner = self;
            grew = TRUE;
            m = world;
          } else {
            m = m.enemy;
          }
        }
      }
      d = find(d, classname, "func_door");
    }
  }
  named = FALSE;
  shot = FALSE;
  lo = self.absmin;
  hi = self.absmax;
  d = self;
  while (d) {
    self.spawnflags = self.spawnflags | (d.spawnflags & (DOOR_GOLD | DOOR_SILVER));
    if (d.targetname != "") named = TRUE;
    if (d.max_health) shot = TRUE;
    if (d.absmin_x < lo_x) lo_x = d.absmin_x;
    if (d.absmin_y < lo_y) lo_y = d.absmin_y;
    if (d.absmin_z < lo_z) lo_z = d.absmin_z;
    if (d.absmax_x > hi_x) hi_x = d.absmax_x;
    if (d.absmax_y > hi_y) hi_y = d.absmax_y;
    if (d.absmax_z > hi_z) hi_z = d.absmax_z;
    d = d.enemy;
  }
  if ((self.spawnflags & (DOOR_GOLD | DOOR_SILVER)) || (!named && !shot)) {
    door_spawn_field(lo, hi);
    return;
  }
  // A named group shows its words on a touch, never as it opens.
  d = self;
  while (d) {
    if (self.netname == "") self.netname = d.message;
    d.message = "";
    d.touch = door_touch;
    d = d.enemy;
  }
};

void() func_door = {
  set_movedir();
  brush_start();
  self.blocked = door_blocked;
  self.use = door_use;
  if (!self.speed) self.speed = 100;
  if (!self.wait) self.wait = 3;
  if (!self.lip) self.lip = 8;
  if (!self.dmg) self.dmg = 2;
  self.pos1 = self.origin;
  self.pos2 = self.pos1 + self.movedir * (fabs(self.movedir * self.size) - self.lip);
  self.state = STATE_BOTTOM;
  self.owner = self;  // alone until door_link gathers it
  if (self.health) {
    self.max_health = self.health;
    self.takedamage = DAMAGE_YES;
    self.th_hurt = noop;
    self.th_die = door_killed;
  }
  self.think = door_link;
  self.nextthink = self.ltime + 0.1;  // after every brush has its size
};

// --- plats ---

void() plat_go_down;

void() plat_hit_top = {
  sound(self, CHAN_VOICE, "plats/plat2.wav", 1, ATTN_NORM);
  self.state = STATE_TOP;
  self.think = plat_go_down;
  self.nextthink = self.ltime + 3;
};

void() plat_hit_bottom = {
  sound(self, CHAN_VOICE, "plats/plat2.wav", 1, ATTN_NORM);
  self.state = STATE_BOTTOM;
};

void() plat_go_down = {
  sound(self, CHAN_VOICE, "plats/plat1.wav", 1, ATTN_NORM);
  self.state = STATE_DOWN;
  calc_move(self.pos2, self.speed, plat_hit_bottom);
};

void() plat_go_up = {
  sound(self, CHAN_VOICE, "plats/plat1.wav", 1, ATTN_NORM);
  self.state = STATE_UP;
  calc_move(self.pos1, self.speed, plat_hit_top);
};

void() plat_blocked = {
  other.deathtype = "squished";
  damage(other, self, self, 1);
  if (self.state == STATE_UP) plat_go_down();
  else if (self.state == STATE_DOWN) plat_go_up();
};

// The field on top of a lowered plat: a player standing on it rides it up.
void() plat_field_touch = {
  local entity field;
  if (other.classname != "player" || other.health <= 0) return;
  field = self;
  self = self.owner;
  if (self.state == STATE_BOTTOM) plat_go_up();
  else if (self.state == STATE_TOP) self.nextthink = self.ltime + 1;
  self = field;
};

void() plat_spawn_field = {
  local entity t;
  t = spawn();
  t.classname = "plat_trigger";
  t.movetype = MOVETYPE_NONE;
  t.solid = SOLID_TRIGGER;
  t.owner = self;
  t.touch = plat_field_touch;
  setsize(t, self.pos2 + self.mins + '25 25 0',
          self.pos2 + self.maxs + '-25 -25 8');
};

// A named plat waits at its top until it is used; then it comes down and is an ordinary plat.
void() plat_use = {
  if (self.state != STATE_WAITING) return;
  plat_go_down();
};

void() func_plat = {
  brush_start();
  self.blocked = plat_blocked;
  if (!self.speed) self.speed = 150;
  self.pos1 = self.origin;  // the top: where the map put it
  if (self.height) self.pos2 = self.pos1 - '0 0 1' * self.height;
  else self.pos2 = self.pos1 - '0 0 1' * (self.size_z - 8);
  if (self.targetname != "") {
    self.state = STATE_WAITING;
    self.use = plat_use;
  } else {
    setorigin(self, self.pos2);
    self.state = STATE_BOTTOM;
  }
  plat_spawn_field();
};

// --- buttons ---

void() button_return = {
  self.state = STATE_DOWN;
  calc_move(self.pos1, self.speed, noop);
  self.frame = 0;
};

void() button_pressed = {
  self.state = STATE_TOP;
  self.frame = 1;
  self.think = button_return;
  self.nextthink = self.ltime + self.wait;
  use_targets();
};

void() button_fire = {
  if (self.state == STATE_UP || self.state == STATE_TOP) return;
  sound(self, CHAN_VOICE, "buttons/switch21.wav", 1, ATTN_NORM);
  self.state = STATE_UP;
  calc_move(self.pos2, self.speed, button_pressed);
};

void() button_touch = {
  if (other.classname != "player") return;
  activator = other;
  button_fire();
};

void() button_use = {
  activator = other;
  button_fire();
};

void() func_button = {
  set_movedir();
  brush_start();
  self.touch = button_touch;
  self.use = button_use;
  if (!self.speed) self.speed = 40;
  if (!self.wait) self.wait = 1;
  if (!self.lip) self.lip = 4;
  self.pos1 = self.origin;
  self.pos2 = self.pos1 + self.movedir * (fabs(self.movedir * self.size) - self.lip);
  self.state = STATE_BOTTOM;
};

// --- triggers ---

void() trigger_ready = {
  self.solid = SOLID_TRIGGER;
};

void() trigger_fire = {
  if (self.nextthink > time) return;  // still waiting since the last time
  use_targets();
  if (self.wait > 0) {
    self.think = trigger_ready;
    self.nextthink = time + self.wait;
  } else {
    // A trigger_once: done for good.
    self.touch = noop;
    self.nextthink = time + 0.1;
    self.think = remove_self;
  }
};

void() trigger_touch = {
  if (other.classname != "player" || other.health <= 0) return;
  activator = other;
  trigger_fire();
};

void() trigger_use = {
  activator = other;
  trigger_fire();
};

void() trigger_multiple = {
  if (!self.wait) self.wait = 0.2;
  trigger_start();
  self.touch = trigger_touch;
  self.use = trigger_use;
};

void() trigger_once = {
  self.wait = -1;
  trigger_start();
  self.touch = trigger_touch;
  self.use = trigger_use;
};

// Sends what the variant lets it (`teleport_takes`) to the destination named by .target, which
// may be any named thing, even a moving one; the variant says what arriving does.
void() teleport_touch = {
  local entity dest;
  if (!teleport_takes(other)) return;
  dest = find(world, targetname, self.target);
  if (!dest) {
    objerror("a teleporter cannot find its destination ", self.target);
    return;
  }
  makevectors(dest.angles);
  setorigin(other, dest.origin);
  other.angles = dest.angles;
  other.fixangle = TRUE;
  other.velocity = v_forward * 300;
  other.teleport_time = time + 0.7;
  other.flags = other.flags - (other.flags & FL_ONGROUND);
  sound(other, CHAN_VOICE, "misc/r_tele1.wav", 1, ATTN_NORM);
  teleport_arrived(other);
};

void() trigger_teleport = {
  trigger_start();
  self.touch = teleport_touch;
  if (self.target == "") objerror("a teleporter needs a target");
};

// Pushes whatever is in it along its angle at .speed (1000 when not set).
void() push_touch = {
  if (other.health <= 0 || other.movetype == MOVETYPE_NONE) return;
  other.velocity = self.movedir * self.speed * 10;
};

void() trigger_push = {
  trigger_start();
  self.touch = push_touch;
  if (!self.speed) self.speed = 1000;
};

// Hurts whatever is in it by .dmg (5 when not set) once a second.
void() hurt_ready = {
  self.solid = SOLID_TRIGGER;
};

void() hurt_touch = {
  if (!other.takedamage) return;
  self.solid = SOLID_NOT;
  other.deathtype = "hurt";
  damage(other, self, self, self.dmg);
  self.think = hurt_ready;
  self.nextthink = time + 1;
};

void() trigger_hurt = {
  trigger_start();
  self.touch = hurt_touch;
  if (!self.dmg) self.dmg = 5;
};
