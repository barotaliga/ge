// THE WORLD: worldspawn, the frame's start (the settings, the time and frag limits, the end of the
// match), spawn points, and the map's moving and touching things: doors, plats, buttons,
// walls, triggers, teleporters, pushes and hurts.

// --- worldspawn and the match ---

void() worldspawn = {
  last_spawn = world;
  match_over = 0;
  intermission_time = 0;
  player_precache();
  weapon_precache();
  item_precache();
  precache_sound("misc/r_tele1.wav");
  precache_sound("doors/drclos4.wav");
  precache_sound("doors/doormv1.wav");
  precache_sound("plats/plat1.wav");
  precache_sound("plats/plat2.wav");
  precache_sound("buttons/switch21.wav");
  precache_sound("weapons/lock4.wav");
  precache_sound("weapons/pkup.wav");
  precache_sound("items/armor1.wav");
  // Light style 0 is the steady light; 1.. flicker and pulse.
  lightstyle(0, "m");
  lightstyle(1, "mmnmmommommnonmmonqnmmo");
  lightstyle(2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");
};

// Ends the match: everyone is held where they stand, the scores are printed, and five seconds
// later the map starts again.
void(string why) end_match = {
  local entity p;
  match_over = 1;
  intermission_time = time;
  bprint(PRINT_HIGH, why, "\n");
  p = find(world, classname, "player");
  while (p) {
    p.takedamage = DAMAGE_NO;
    p.movetype = MOVETYPE_NONE;
    p.velocity = '0 0 0';
    p.button0 = 0;
    bprint(PRINT_HIGH, p.netname, ": ", ftos(p.frags), " frags\n");
    p = find(p, classname, "player");
  }
};

void() StartFrame = {
  local entity p;
  deathmatch = cvar("deathmatch");
  fraglimit = cvar("fraglimit");
  timelimit = cvar("timelimit");
  if (match_over) {
    if (time > intermission_time + 5) {
      changelevel(mapname);
      intermission_time = time + 1000;  // asked once
    }
    return;
  }
  if (timelimit && time >= timelimit * 60) {
    end_match("Timelimit hit.");
    return;
  }
  if (fraglimit) {
    p = find(world, classname, "player");
    while (p) {
      if (p.frags >= fraglimit) {
        end_match("Fraglimit hit.");
        return;
      }
      p = find(p, classname, "player");
    }
  }
};

void() main = {};

void() info_player_deathmatch = {};
void() info_player_start = {};
void() info_intermission = {};
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

// Something stands in the way: it takes the door's damage and the door goes back.
void() door_blocked = {
  other.deathtype = "squished";
  damage(other, self, self, self.dmg);
  if (self.wait >= 0) {
    if (self.state == STATE_DOWN) door_go_up();
    else door_go_down();
  }
};

void() door_use = {
  local entity was_self;
  was_self = self;
  if (self.owner) self = self.owner;  // the trigger field opens its door
  activator = other;
  door_go_up();
  self = was_self;
};

void() door_trigger_touch = {
  local entity field;
  if (other.health <= 0 || other.classname != "player") return;
  activator = other;
  field = self;
  self = self.owner;
  door_go_up();
  self = field;
};

// The field around a door that opens it when a player comes near.
void() door_spawn_field = {
  local entity t;
  t = spawn();
  t.classname = "door_trigger";
  t.movetype = MOVETYPE_NONE;
  t.solid = SOLID_TRIGGER;
  t.owner = self;
  t.touch = door_trigger_touch;
  setsize(t, self.mins - '60 60 8', self.maxs + '60 60 8');
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
  if (self.targetname == "") {
    self.think = door_spawn_field;
    self.nextthink = self.ltime + 0.1;  // after every brush has its size
  }
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

void() func_plat = {
  brush_start();
  self.blocked = plat_blocked;
  if (!self.speed) self.speed = 150;
  self.pos1 = self.origin;  // the top: where the map put it
  if (self.height) self.pos2 = self.pos1 - '0 0 1' * self.height;
  else self.pos2 = self.pos1 - '0 0 1' * (self.size_z - 8);
  setorigin(self, self.pos2);
  self.state = STATE_BOTTOM;
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

// Sends a player (or anything moving) to the destination named by .target.
void() teleport_touch = {
  local entity dest;
  if (other.classname != "player" || other.health <= 0) return;
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
