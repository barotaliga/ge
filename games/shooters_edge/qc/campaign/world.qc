// THE CAMPAIGN'S WORLD: worldspawn, the frame's start (the intermission and the way to the next
// map), the spawn points, and the campaign's own map things (porting/shooters-edge/
// campaign-rules.md § The map vocabulary, § The game's flow): the exits and the intermission,
// secrets, counters and relays, the skill triggers, the registered-only triggers, the episode and
// boss gates, the runes and the keys, and the words at each episode's end. The monsters are
// ai.qc's and monsters*.qc's, the bosses and their set pieces bosses.qc's, the other set pieces
// set_pieces.qc's.

.string map;       // a changelevel trigger's next map
.float count;      // a counter's uses still owed
.vector mangle;    // an intermission spot's view angles

float intermission_running;  // the level is over: everyone is held at an intermission spot
float intermission_exit;     // a button press after this leaves for the next map
string next_map;
string finale_words;         // an episode's closing words, shown before the next map ("" none)

float SVC_FINALE = 31;

float SVC_INTERMISSION = 30;
float SVC_FOUNDSECRET = 28;

// --- worldspawn and the frame ---

void() worldspawn = {
  last_spawn = world;
  match_over = 0;
  intermission_running = 0;
  next_map = "";
  finale_words = "";
  player_precache();
  weapon_precache();
  item_precache();
  ai_precache();
  precache_sound("misc/r_tele1.wav");
  precache_sound("doors/drclos4.wav");
  precache_sound("doors/doormv1.wav");
  precache_sound("plats/plat1.wav");
  precache_sound("plats/plat2.wav");
  precache_sound("buttons/switch21.wav");
  precache_sound("weapons/lock4.wav");
  precache_sound("weapons/pkup.wav");
  precache_sound("items/armor1.wav");
  precache_sound("misc/secret.wav");
  precache_sound("misc/talk.wav");
  lightstyle(0, "m");
  lightstyle(1, "mmnmmommommnonmmonqnmmo");
  lightstyle(2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");
};

// During the intermission: once its wait is over, any player's button leaves for the next map,
// or first shows the episode's closing words when it has them (then a second button leaves).
void() intermission_frame = {
  local entity p;
  if (time < intermission_exit) return;
  p = find(world, classname, "player");
  while (p) {
    if (p.button0 || p.button1 || p.button2) {
      if (finale_words != "") {
        WriteByte(MSG_ALL, SVC_FINALE);
        WriteString(MSG_ALL, finale_words);
        finale_words = "";
        intermission_exit = time + 2;
        return;
      }
      intermission_exit = time + 1000;  // asked once
      changelevel(next_map);
      return;
    }
    p = find(p, classname, "player");
  }
};

void() StartFrame = {
  if (intermission_running) intermission_frame();
};

void() main = {};

// --- spawn points ---

void() info_player_start = {};
void() info_player_start2 = {};
void() info_player_coop = {};
void() info_player_deathmatch = {};
void() info_intermission = {};
void() info_null = { remove(self); };
void() info_notnull = {};

// --- the end of a level ---

// Where the players watch the intermission from: a random intermission spot, else the start.
entity() intermission_spot = {
  local entity spot, pick;
  local float n;
  spot = find(world, classname, "info_intermission");
  n = 0;
  pick = world;
  while (spot) {
    n = n + 1;
    if (random() * n < 1) pick = spot;
    spot = find(spot, classname, "info_intermission");
  }
  if (!pick) pick = find(world, classname, "info_player_start");
  return pick;
};

// The level is over: every player held at the intermission spot, unseen and unhurt, and told.
void() intermission_start = {
  local entity spot, p;
  intermission_running = 1;
  match_over = 1;
  if (deathmatch) intermission_exit = time + 5;
  else intermission_exit = time + 2;
  spot = intermission_spot();
  p = find(world, classname, "player");
  while (p) {
    p.view_ofs = '0 0 0';
    p.angles = spot.mangle;
    p.v_angle = spot.mangle;
    p.fixangle = TRUE;
    p.takedamage = DAMAGE_NO;
    p.solid = SOLID_NOT;
    p.movetype = MOVETYPE_NONE;
    p.velocity = '0 0 0';
    p.modelindex = 0;
    p.weaponmodel = "";
    setorigin(p, spot.origin);
    p = find(p, classname, "player");
  }
  WriteByte(MSG_ALL, SVC_INTERMISSION);
};

// The episode (its bit, as a rune's) a start map's way in leads to; 0 for any other map.
float(string m) episode_of = {
  if (m == "e1m1") return 1;
  if (m == "e2m1") return 2;
  if (m == "e3m1") return 4;
  if (m == "e4m1") return 8;
  return 0;
};

// In coop the episodes open one at a time: the first always, each next once the one before is
// done (`sv_episodes_done`, a rune's bit for each, kept by the host from game to game).
float(string m) episode_shut = {
  local float bit;
  if (!coop || mapname != "start") return FALSE;
  bit = episode_of(m);
  if (bit <= 1) return FALSE;
  return !(cvar("sv_episodes_done") & (bit / 2));
};

void() changelevel_touch = {
  if (other.classname != "player" || intermission_running) return;
  if (episode_shut(self.map)) {
    if (time >= self.attack_finished) {
      centerprint(other, "This episode opens once the one before it is done.");
      self.attack_finished = time + 2;
    }
    return;
  }
  next_map = self.map;
  activator = other;
  use_targets();
  if ((self.spawnflags & 1) && !deathmatch) {
    changelevel(next_map);  // no intermission: straight on
    return;
  }
  self.touch = noop;
  self.think = intermission_start;
  self.nextthink = time + 0.1;
};

void() trigger_changelevel = {
  if (self.map == "") objerror("a changelevel trigger without a map");
  trigger_start();
  self.touch = changelevel_touch;
};

// --- secrets, counters, relays ---

void() secret_touch = {
  if (other.classname != "player" || other.health <= 0) return;
  found_secrets = found_secrets + 1;
  WriteByte(MSG_ALL, SVC_FOUNDSECRET);
  if (self.message == "") self.message = "A secret!";
  sound(other, CHAN_ITEM, "misc/secret.wav", 1, ATTN_NORM);
  activator = other;
  self.wait = -1;
  trigger_fire();
};

void() trigger_secret = {
  total_secrets = total_secrets + 1;
  trigger_start();
  self.touch = secret_touch;
};

// Fires its targets on its `count`th use (2 when not set); each use before it counts down out
// loud unless spawn flag 1 keeps it quiet.
void() counter_use = {
  self.count = self.count - 1;
  if (self.count < 0) return;
  if (self.count > 0) {
    if (activator.classname == "player" && !(self.spawnflags & 1)) {
      centerprint(activator, ftos(self.count), " more to go");
    }
    return;
  }
  if (activator.classname == "player" && !(self.spawnflags & 1)) centerprint(activator, "Done.");
  use_targets();
};

void() trigger_counter = {
  if (!self.count) self.count = 2;
  self.use = counter_use;
};

void() trigger_relay = { self.use = use_targets; };

// --- skill and the full game ---

void() setskill_touch = {
  if (other.classname != "player") return;
  cvar_set("skill", self.message);
};

void() trigger_setskill = {
  trigger_start();
  self.touch = setskill_touch;
};

// Acts as a trigger_once on the full game's packs; on the shareware ones it only shows its message.
void() onlyregistered_touch = {
  if (other.classname != "player" || other.health <= 0) return;
  if (self.attack_finished > time) return;
  self.attack_finished = time + 2;
  if (cvar("registered")) {
    self.message = "";
    activator = other;
    self.wait = -1;
    trigger_fire();
    return;
  }
  if (self.message != "") {
    centerprint(other, self.message);
    sound(other, CHAN_BODY, "misc/talk.wav", 1, ATTN_NORM);
  }
};

void() trigger_onlyregistered = {
  trigger_start();
  self.touch = onlyregistered_touch;
};

void() trigger_monsterjump = { trigger_start(); };

// --- the episodes ---

// Stands (blocking the hall) once its episode is done.
void() func_episodegate = {
  if (!(serverflags & self.spawnflags)) {
    remove(self);
    return;
  }
  func_wall();
};

// Gone once all four episodes are done.
void() func_bossgate = {
  if ((serverflags & 15) == 15) {
    remove(self);
    return;
  }
  func_wall();
};

// A pick-up the size of a player, its model off the map's entity.
void(string mdl) pickup_start = {
  self.solid = SOLID_TRIGGER;
  self.movetype = MOVETYPE_TOSS;
  setmodel(self, mdl);
  setsize(self, '-16 -16 -24', '16 16 32');
  setorigin(self, self.origin + '0 0 1');
};

// Each episode's closing words, shown when its last map is left (our own words).
string(float bit) episode_words = {
  if (bit & 1)
    return "The first rune hums in your hand, and the halls behind you fall silent.\n\nThree more "
           "lie in deeper places. The way back to the hall of gates is open.";
  if (bit & 2)
    return "The second rune is yours. The dead in the old keeps lie still at last.\n\nTwo more "
           "remain, further down.";
  if (bit & 4)
    return "Three runes now. The pits below have stopped their noise.\n\nOne more waits in the "
           "oldest place of all.";
  return "The last rune is yours, and something at the bottom of the world has felt it.\n\nThe "
         "four gates stand open. Go down and finish it.";
};

// The rune: its episode's bit in serverflags, and the episode's closing words for the way out.
void() sigil_touch = {
  if (other.classname != "player" || other.health <= 0) return;
  serverflags = serverflags | (self.spawnflags & 15);
  // In coop the next episode opens for good (the host keeps it from game to game).
  if (coop) cvar_set("sv_episodes_done", ftos(cvar("sv_episodes_done") | (self.spawnflags & 15)));
  if (!deathmatch) finale_words = episode_words(self.spawnflags & 15);
  centerprint(other, "The episode's rune is yours.");
  sound(other, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);
  activator = other;
  use_targets();
  remove(self);
};

// Its model by its episode's bit (the first episode's when the flags name none).
void() item_sigil = {
  local string mdl;
  mdl = "progs/end1.mdl";
  if (self.spawnflags & 2) mdl = "progs/end2.mdl";
  if (self.spawnflags & 4) mdl = "progs/end3.mdl";
  if (self.spawnflags & 8) mdl = "progs/end4.mdl";
  precache_model(mdl);
  pickup_start(mdl);
  self.touch = sigil_touch;
};

// A key: taken by the first player in single player; in coop every player may take it.
void() key_touch = {
  if (other.classname != "player" || other.health <= 0) return;
  if (other.items & self.items) return;
  other.items = other.items | self.items;
  centerprint(other, "You have a key.");
  sound(other, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);
  activator = other;
  use_targets();
  if (!coop) remove(self);
};

void() item_key1 = {
  precache_model("progs/w_s_key.mdl");
  self.items = IT_KEY1;
  pickup_start("progs/w_s_key.mdl");
  self.touch = key_touch;
};

void() item_key2 = {
  precache_model("progs/w_g_key.mdl");
  self.items = IT_KEY2;
  pickup_start("progs/w_g_key.mdl");
  self.touch = key_touch;
};

