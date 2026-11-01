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

// The campaign has no teams and leaves no backpacks.
float(entity a, entity b) same_team = { return FALSE; };
void() drop_on_death = {};

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

// A coop start in coop, a deathmatch spawn point in deathmatch, else the map's start; the start
// when the map has none of the kind asked for.
entity() spawn_point = {
  local entity spot;
  spot = world;
  if (coop) spot = next_spot("info_player_coop");
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
