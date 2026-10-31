// THE PLAYERS: coming in, spawning, the frame around each command (dying, drowning, burning,
// respawning, firing), pain and death, the kill command and leaving.

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

// --- coming in ---

void() ClientConnect = {
  bprint(PRINT_HIGH, self.netname, " entered the game\n");
};

void() PutClientInServer = {
  local entity spot;
  self.classname = "player";
  self.health = 100;
  self.max_health = 100;
  self.takedamage = DAMAGE_AIM;
  self.solid = SOLID_SLIDEBOX;
  self.movetype = MOVETYPE_WALK;
  self.flags = FL_CLIENT;
  self.deadflag = DEAD_NO;
  self.effects = 0;
  self.super_damage_finished = 0;
  self.invincible_finished = 0;
  self.items = W_BLADE | W_SCATTER;
  self.ammo_shells = 25;
  self.ammo_nails = 0;
  self.ammo_rockets = 0;
  self.ammo_cells = 0;
  self.armortype = 0;
  self.armorvalue = 0;
  self.attack_finished = time;
  self.pain_finished = 0;
  self.air_finished = time + 12;
  self.dmg_take = 0;
  self.dmg_save = 0;
  self.deathtype = "";
  self.maxspeed = 320;
  self.gravity = 1;
  self.spawn_time = time;
  weapon_set(W_SCATTER);
  spot = spawn_point();
  self.origin = spot.origin + '0 0 1';
  self.angles = spot.angles;
  self.fixangle = TRUE;
  self.velocity = '0 0 0';
  self.view_ofs = '0 0 22';
  setmodel(self, "progs/player.mdl");
  setsize(self, '-16 -16 -24', '16 16 32');
  setorigin(self, self.origin);
  if (match_over) {
    self.takedamage = DAMAGE_NO;
    self.movetype = MOVETYPE_NONE;
  }
};

// --- pain and death ---

void() player_pain = {
  if (time < self.pain_finished) return;
  self.pain_finished = time + 0.5;
  if (self.deathtype == "drowned") sound(self, CHAN_VOICE, "player/drown1.wav", 1, ATTN_NORM);
  else sound(self, CHAN_VOICE, "player/pain1.wav", 1, ATTN_NORM);
};

void() player_die = {
  self.deadflag = DEAD_DEAD;
  self.takedamage = DAMAGE_NO;
  self.solid = SOLID_NOT;
  self.movetype = MOVETYPE_TOSS;
  self.flags = self.flags - (self.flags & FL_ONGROUND);
  self.view_ofs = '0 0 -8';
  self.velocity_z = self.velocity_z + 160;
  self.weaponmodel = "";
  self.effects = 0;
  self.super_damage_finished = 0;
  self.invincible_finished = 0;
  self.items = self.items - (self.items & (IT_QUAD | IT_INVULNERABILITY));
  self.respawn_ready = time + 1;
  if (self.health < -40) sound(self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NONE);
  else sound(self, CHAN_VOICE, "player/death1.wav", 1, ATTN_NORM);
};

// A dead player comes back when he presses attack, jump or use after a second, or by himself
// after five.
void() player_dead_frame = {
  if (time < self.respawn_ready) return;
  if (self.button0 || self.button1 || self.button2 || time > self.respawn_ready + 4) {
    player_respawn();
  }
};

void() player_respawn = {
  self.deadflag = DEAD_RESPAWNABLE;
  PutClientInServer();
};

// --- water and slime and lava ---

void() player_water = {
  if (self.waterlevel == 3) {
    if (self.air_finished < time && self.pain_finished < time) {
      self.deathtype = "drowned";
      damage(self, world, world, 4);
      self.pain_finished = time + 1;
    }
  } else {
    self.air_finished = time + 12;
  }
  if (self.waterlevel && self.watertype == CONTENT_LAVA && self.hazard_finished < time) {
    self.hazard_finished = time + 0.2;
    self.deathtype = "burned";
    damage(self, world, world, 10 * self.waterlevel);
  } else if (self.waterlevel && self.watertype == CONTENT_SLIME && self.hazard_finished < time) {
    self.hazard_finished = time + 1;
    self.deathtype = "dissolved";
    damage(self, world, world, 4 * self.waterlevel);
  }
};

// --- the frame around a command ---

void() PlayerPreThink = {
  self.effects = self.effects - (self.effects & EF_MUZZLEFLASH);
  if (match_over) return;
  if (self.deadflag != DEAD_NO) {
    player_dead_frame();
    return;
  }
  player_water();
};

void() PlayerPostThink = {
  if (self.deadflag != DEAD_NO || match_over) return;
  if (self.impulse) weapon_impulse();
  weapon_frame();
  power_frame();
  self.dmg_take = 0;
};

// --- the kill command and leaving ---

void() ClientKill = {
  if (self.deadflag != DEAD_NO || match_over) return;
  if (time < self.spawn_time + 1) return;
  self.deathtype = "suicides";
  self.armorvalue = 0;
  damage(self, self, self, 1000);
};

void() ClientDisconnect = {
  bprint(PRINT_HIGH, self.netname, " left the game with ", ftos(self.frags), " frags\n");
  sound(self, CHAN_BODY, "player/tornoff2.wav", 1, ATTN_NONE);
  self.classname = "disconnected";
  self.takedamage = DAMAGE_NO;
  self.solid = SOLID_NOT;
};

void() player_precache = {
  precache_model("progs/player.mdl");
  precache_sound("player/pain1.wav");
  precache_sound("player/drown1.wav");
  precache_sound("player/death1.wav");
  precache_sound("player/udeath.wav");
  precache_sound("player/tornoff2.wav");
  precache_sound("items/r_item1.wav");
};
