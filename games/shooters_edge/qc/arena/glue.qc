// THE ARENA VARIANT'S GLUE: what the multiplayer form does where the two forms differ. Printing
// takes a level, effects go to the multicast buffer and are sent to those who can hear or see
// them, a frag is logged, a missile made this frame runs once at once (`newmis`); every spawn is
// fresh, on the next free deathmatch spawn point, and a dead player comes back at once.

void(float level, string a, string b, string c, string d, string e, string f) say_all = {
  bprint(level, a, b, c, d, e, f);
};

void(entity to, float level, string a, string b, string c, string d) say_to = {
  sprint(to, level, a, b, c, d);
};

void(vector org, float set) effect_send = { multicast(org, set); };

// A puff where a shot hits: blood on something that bleeds, a chip of the wall elsewhere.
void(vector org, float count, float bleeds) effect_impact = {
  WriteByte(MSG_EFFECT, SVC_TEMPENTITY);
  if (bleeds) WriteByte(MSG_EFFECT, TE_BLOOD);
  else WriteByte(MSG_EFFECT, TE_GUNSHOT);
  WriteByte(MSG_EFFECT, count);
  WriteCoord(MSG_EFFECT, org.x);
  WriteCoord(MSG_EFFECT, org.y);
  WriteCoord(MSG_EFFECT, org.z);
  multicast(org, MULTICAST_PVS);
};

void(entity killer, entity killed) note_frag = { logfrag(killer, killed); };

void(entity m) missile_now = { newmis = m; };

// --- spawn parms (kept by the server across a map change; deathmatch starts everyone fresh) ---

void() SetNewParms = {
  parm1 = 0;
};

void() SetChangeParms = {
  parm1 = 0;
};

// --- spawn points ---

// The next deathmatch spawn point after the last one used whose spot is clear of players, round
// the map's list; any one when all are taken; the start point on a map without any.
entity() spawn_point = {
  local entity spot, p;
  local float tries, clear;
  spot = last_spawn;
  for (tries = 0; tries < 64; tries += 1) {
    spot = find(spot, classname, "info_player_deathmatch");
    if (!spot) spot = find(world, classname, "info_player_deathmatch");
    if (!spot) break;
    clear = TRUE;
    p = findradius(spot.origin, 84);
    while (p) {
      if (p.classname == "player" && p.health > 0) clear = FALSE;
      p = p.chain;
    }
    if (clear) {
      last_spawn = spot;
      return spot;
    }
  }
  if (spot) return spot;
  spot = find(world, classname, "info_player_start");
  if (!spot) error("the map has no spawn point for players");
  return spot;
};

// --- a player's start and his respawn ---

// Every spawn the same: the blade, the scattergun and 25 shells, no armour.
void() give_inventory = {
  self.items = W_BLADE | W_SCATTER;
  self.ammo_shells = 25;
  self.ammo_nails = 0;
  self.ammo_rockets = 0;
  self.ammo_cells = 0;
  self.armortype = 0;
  self.armorvalue = 0;
  weapon_set(W_SCATTER);
};

void() player_respawn = {
  self.deadflag = DEAD_RESPAWNABLE;
  PutClientInServer();
};
