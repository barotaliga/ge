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

// An item's name in the loaded game's own words: the server's local info under the item's class
// name, which the window sets from the game's profile (profiles/items.hpp's `names`); `fallback`
// (Shooter's Edge's own word) when the game names none.
string(string class_name, string fallback) name_of = {
  local string s;
  s = infokey(world, class_name);
  if (s == "") return fallback;
  return s;
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

// --- teams ---

// A teammate's blow that does not harm him does not push him either.
float(entity a, entity b) team_shoves = { return FALSE; };

// Two players on the same named team while `teamplay` is on: a teammate is not harmed.
float(entity a, entity b) same_team = {
  local string t;
  if (!teamplay || a == b) return FALSE;
  if (a.classname != "player" || b.classname != "player") return FALSE;
  t = infokey(a, "team");
  if (t == "") return FALSE;
  return t == infokey(b, "team");
};

// --- a loaded game's own words ---
//
// A classic game's server words (its profile's table, profiles/words.hpp) are in the server's
// local info; `words` (#640) fills one in. Each of these is "" when the game names none, and the
// caller then says the mod's own words, so Shooter's Edge's content keeps its own.

string(string key, string a, string b, string c, string d) game_words = {
  return words(key, a, b, c, d);
};

// One of the variants `stem`_1 .. `stem`_n, drawn at random (one draw, only when there are two or
// more); `stem` itself when it has no numbered ones.
string(string stem, string a, string b, string c, string d) game_variant = {
  local float n, k;
  n = 0;
  while (n < 9 && words(strcat(stem, "_", ftos(n + 1)), "", "", "", "") != "") n = n + 1;
  if (n == 0) return words(stem, a, b, c, d);
  k = 0;
  if (n > 1) {
    k = floor(random() * n);
    if (k >= n) k = n - 1;
  }
  return words(strcat(stem, "_", ftos(k + 1)), a, b, c, d);
};

// Before the match starts (the lobby's warmup and countdown, the host's `ge_phase`) a game that
// asks for it (`quiet_warmup`) tells no death and no leaving, and scores none.
// A blast's pellets on one body: each its own blow here (the matches the bots and their goldens
// record play so); the campaign sums them into one.
float() pellets_as_one_blow = { return FALSE; };

float() game_quiet = {
  local string phase;
  if (words("quiet_warmup", "", "", "", "") == "") return FALSE;
  phase = infokey(world, "ge_phase");
  return phase == "warmup" || phase == "countdown";
};

// A player's pronoun as the game's words say it: "her" for the user info's gender "f", else "his".
string(entity e) pronoun = {
  if (infokey(e, "gender") == "f") return "her";
  return "his";
};

// A death in the loaded game's words: by whom, with what, how badly ("" when it names none).
string(entity targ, entity attacker) death_words = {
  local string how, v, k, hv, hk, key;
  local float gib, quad, c;
  if (words("obit_tele", "", "", "", "") == "") return "";
  how = targ.deathtype;
  v = targ.netname;
  k = attacker.netname;
  hv = pronoun(targ);
  hk = pronoun(attacker);
  gib = targ.health < -40;
  if (attacker == targ) {
    if (how == "gibbed by a grenade") return words("self_gl", v, k, hv, hk);
    if (how == "blown apart") return game_variant("self_rl", v, k, hv, hk);
    if (how == "electrocuted") {
      c = pointcontents(targ.origin);
      if (c == CONTENT_SLIME) return words("self_discharge_slime", v, k, hv, hk);
      if (c == CONTENT_LAVA) return words("self_discharge_lava", v, k, hv, hk);
      return game_variant("self_discharge_water", v, k, hv, hk);
    }
    if (how == "squished") return words("self_squish", v, k, hv, hk);
    if (how == "suicides") return words("self_kill", v, k, hv, hk);
    return words("self_other", v, k, hv, hk);
  }
  if (attacker.classname == "player") {
    if (same_team(targ, attacker)) {
      if (how == "telefragged") return words("tk_tele", v, k, hv, hk);
      if (how == "squished") return words("tk_squish", v, k, hv, hk);
      return game_variant("tk", v, k, hv, hk);
    }
    quad = attacker.super_damage_finished > time;
    if (how == "cut down") return words("obit_axe", v, k, hv, hk);
    if (how == "shot") {
      if (attacker.weapon == W_DOUBLE) {
        if (quad) return words("obit_ssg_quad", v, k, hv, hk);
        return words("obit_ssg", v, k, hv, hk);
      }
      if (gib) return words("obit_sg_gib", v, k, hv, hk);
      return words("obit_sg", v, k, hv, hk);
    }
    if (how == "nailed") {
      if (attacker.weapon == W_HEAVY_SPIKER) {
        key = game_variant("obit_sng", v, k, hv, hk);  // drawn even when the gib's line wins
        if (gib) return words("obit_sng_gib", v, k, hv, hk);
        return key;
      }
      return game_variant("obit_ng", v, k, hv, hk);
    }
    if (how == "gibbed by a grenade") {
      if (gib) return words("obit_gl_gib", v, k, hv, hk);
      return words("obit_gl", v, k, hv, hk);
    }
    if (how == "blown apart") {
      if (gib && quad) return game_variant("obit_rl_quad", v, k, hv, hk);
      if (gib) return words("obit_rl_gib", v, k, hv, hk);
      return words("obit_rl", v, k, hv, hk);
    }
    if (how == "shafted") {
      if (gib) return words("obit_lg_gib", v, k, hv, hk);
      return words("obit_lg", v, k, hv, hk);
    }
    if (how == "electrocuted") return game_variant("obit_discharge", v, k, hv, hk);
    if (how == "telefragged") return words("obit_tele", v, k, hv, hk);
    if (how == "squished") return words("obit_squish", v, k, hv, hk);
    return words("obit_other", v, k, hv, hk);
  }
  if (how == "drowned") return game_variant("world_drown", v, k, hv, hk);
  if (how == "dissolved") return game_variant("world_slime", v, k, hv, hk);
  if (how == "burned") {
    key = game_variant("world_lava", v, k, hv, hk);  // drawn before the flames are looked at
    if (targ.health < -15) return words("world_lava_gib", v, k, hv, hk);
    return key;
  }
  if (how == "squished") return words("world_squish", v, k, hv, hk);
  return words("world_died", v, k, hv, hk);
};

// A dead player leaves a backpack (items.qc) with his ammunition and the weapon he held.
void() backpack_drop;
void() drop_on_death = { backpack_drop(); };

// A taken item comes back after its wait: the arena is always a deathmatch.
float() items_come_back = { return TRUE; };
// The multiplayer form's move jumps for the program; the program only hears it. Before the move,
// with the jump button down: deep in a liquid, the swimmer's splash; on the ground with the button
// let go since the last jump, the jump's sound (the move makes the jump itself, by the same rule).
void() jump_sound;
void() swim_sound;
void() player_jump = {
  if (!self.button2) {
    self.flags = self.flags | FL_JUMPRELEASED;
    return;
  }
  if (self.waterlevel >= 2) {
    swim_sound();
    return;
  }
  if (!(self.flags & FL_ONGROUND) || !(self.flags & FL_JUMPRELEASED)) return;
  self.flags = self.flags - FL_JUMPRELEASED;
  jump_sound();
};

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
