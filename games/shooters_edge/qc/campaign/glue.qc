// THE CAMPAIGN VARIANT'S GLUE: what the single-player form does where the two forms differ.
// Printing has no level, effects are already in the broadcast when they are written, there is no
// frag log and no `newmis`. The player's inventory is carried from map to map in the spawn parms;
// he starts on the map's start (a coop start in coop); and a death restarts the map with what he
// entered it with, or in coop puts him back on a coop start with a fresh inventory.

void(float level, string a, string b, string c, string d, string e, string f) say_all = {
  bprint(a, b, c, d, e, f);
};

void(entity to, float level, string a, string b, string c, string d) say_to = {
  sprint(to, a, b, c, d);
};

// An item's name: the form has no local info to read a game's own words from, so always ours.
string(string class_name, string fallback) name_of = { return fallback; };

// A loaded game's own words: the form has no local info, so none (the mod's own words always).
string(string key, string a, string b, string c, string d) game_words = { return ""; };
float() game_quiet = { return FALSE; };
string(entity targ, entity attacker) death_words = { return ""; };

void(vector org, float set) effect_send = {};

// A puff where a shot hits: a burst of red particles on something that bleeds (the form has no
// blood message), a chip of the wall elsewhere (its gunshot carries no count).
void(vector org, float count, float bleeds) effect_impact = {
  if (bleeds) {
    particle(org, '0 0 0', 73, count * 20);
    return;
  }
  WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
  WriteByte(MSG_BROADCAST, TE_GUNSHOT);
  WriteCoord(MSG_BROADCAST, org.x);
  WriteCoord(MSG_BROADCAST, org.y);
  WriteCoord(MSG_BROADCAST, org.z);
};

void(entity killer, entity killed) note_frag = {};

void(entity m) missile_now = {};

// In coop the players are one side unless `sv_friendlyfire` is on: one player's shots never harm
// another (his own blast still harms himself). Monsters are on no side. No backpacks are left.
float(entity a, entity b) same_team = {
  if (!coop || a == b || cvar("sv_friendlyfire")) return FALSE;
  return (a.flags & FL_CLIENT) && (b.flags & FL_CLIENT);
};

// And a teammate's blow that does not harm him pushes him only with `sv_friendlypush` on (off: no
// rocket boost off a friend).
float(entity a, entity b) team_shoves = { return cvar("sv_friendlypush"); };
void() drop_on_death = {};

// --- the jump: the single-player form's program jumps for itself (the move never does) ---

float FL_WATERJUMP = 2048;     // out of the water onto a ledge: the move carries him by movedir
float FL_JUMPRELEASED = 4096;  // the jump let go since the last one (no pogo)

// A ledge ahead at the water's edge: the jump out (checked at water level 2, every frame).
void() water_jump_check = {
  local vector start, end;
  makevectors(self.angles);
  start = self.origin;
  start_z = start_z + 8;
  v_forward_z = 0;
  end = start + v_forward * 24;  // not renormalised: a little short when the body is pitched
  traceline(start, end, TRUE, self);
  if (trace_fraction == 1) return;
  start_z = start_z + self.maxs_z - 8;
  end = start + v_forward * 24;
  self.movedir = trace_plane_normal * -50;
  traceline(start, end, TRUE, self);
  if (trace_fraction < 1) return;
  self.flags = self.flags | FL_WATERJUMP;
  self.velocity_z = 225;
  self.flags = self.flags - (self.flags & FL_JUMPRELEASED);
  self.teleport_time = time + 2;
};

// Before the move: the water jump's check, then the jump button: swimming up in deep water, a jump
// of 270 off the ground once the button was let go since the last.
void() player_jump = {
  if (self.waterlevel == 2) water_jump_check();
  if (!self.button2) {
    self.flags = self.flags | FL_JUMPRELEASED;
    return;
  }
  if (self.flags & FL_WATERJUMP) return;
  if (self.waterlevel >= 2) {
    if (self.watertype == CONTENT_WATER) self.velocity_z = 100;
    else if (self.watertype == CONTENT_SLIME) self.velocity_z = 80;
    else self.velocity_z = 50;
    return;
  }
  if (!(self.flags & FL_ONGROUND) || !(self.flags & FL_JUMPRELEASED)) return;
  self.flags = self.flags - (self.flags & FL_JUMPRELEASED);
  self.flags = self.flags - (self.flags & FL_ONGROUND);
  self.velocity_z = self.velocity_z + 270;
};

// --- spawn parms: the inventory carried from map to map ---
// parm1 the items, parm2 health, parm3 armour, parm4..7 shells, nails, rockets and cells, parm8 the
// weapon held, parm9 the armour's share x 100.

void() SetNewParms = {
  parm1 = W_BLADE | W_SCATTER;
  parm2 = 100;
  parm3 = 0;
  parm4 = 25;
  parm5 = 0;
  parm6 = 0;
  parm7 = 0;
  parm8 = W_SCATTER;
  parm9 = 0;
};

// Leaving a map keeps the weapons, the ammunition and the armour; the keys and the powers stay
// behind, health comes back to 50..100 and the shells to at least 25. A dead player starts anew.
void() SetChangeParms = {
  if (self.health <= 0) {
    SetNewParms();
    return;
  }
  self.items = self.items - (self.items & (IT_KEY1 | IT_KEY2 | IT_QUAD | IT_INVULNERABILITY));
  if (self.health > 100) self.health = 100;
  if (self.health < 50) self.health = 50;
  parm1 = self.items;
  parm2 = self.health;
  parm3 = self.armorvalue;
  if (self.ammo_shells < 25) parm4 = 25;
  else parm4 = self.ammo_shells;
  parm5 = self.ammo_nails;
  parm6 = self.ammo_rockets;
  parm7 = self.ammo_cells;
  parm8 = self.weapon;
  parm9 = self.armortype * 100;
};

// --- spawn points ---

// The next coop start after the last one used, round the map's list.
entity(string kind) next_spot = {
  local entity spot;
  spot = find(last_spawn, classname, kind);
  if (!spot) spot = find(world, classname, kind);
  if (spot) last_spawn = spot;
  return spot;
};

// A spot no living player stands on (within a player's width), self aside.
float(entity spot) spot_free = {
  local entity e;
  e = findradius(spot.origin, 40);
  while (e) {
    if (e != self && (e.flags & FL_CLIENT) && e.health > 0) return FALSE;
    e = e.chain;
  }
  return TRUE;
};

// In coop the next coop start no one stands on (round the map's list; the map's start counts as
// one), the next in turn when every one is taken.
entity() coop_spot = {
  local entity first, spot;
  local float n;
  first = next_spot("info_player_coop");
  spot = first;
  n = 0;
  while (spot && n < 16) {
    if (spot_free(spot)) return spot;
    spot = next_spot("info_player_coop");
    if (spot == first) spot = world;
    n = n + 1;
  }
  spot = find(world, classname, "info_player_start");
  if (spot && spot_free(spot)) return spot;
  return first;
};

// A coop start in coop, a deathmatch spawn point in deathmatch, else the map's start; the start
// when the map has none of the kind asked for.
entity() spawn_point = {
  local entity spot;
  spot = world;
  if (coop) spot = coop_spot();
  else if (deathmatch) spot = next_spot("info_player_deathmatch");
  if (!spot) spot = find(world, classname, "info_player_start");
  if (!spot) error("the map has no info_player_start");
  return spot;
};

// --- a player's start and his respawn ---

// The inventory the parms carry; afresh on the start map once an episode is done (a rune held).
void() give_inventory = {
  if (mapname == "start" && serverflags) SetNewParms();
  self.items = parm1;
  self.health = parm2;
  self.armorvalue = parm3;
  self.ammo_shells = parm4;
  self.ammo_nails = parm5;
  self.ammo_rockets = parm6;
  self.ammo_cells = parm7;
  self.armortype = parm9 / 100;
  weapon_set(parm8);
};

void() player_respawn = {
  if (coop || deathmatch) {
    self.deadflag = DEAD_RESPAWNABLE;
    SetNewParms();
    PutClientInServer();
    return;
  }
  localcmd("restart\n");
};
