// THE ITEMS: health, armour, ammunition and weapons lying on the map. Each is a trigger a living
// player picks up by touching; it is gone for its respawn time, then comes back with a sound.
//
//   item_health      25 (spawnflags 1: 15; 2: 100 that may go over the maximum, to 250, and
//                    decays back a point a second)                                  20 s
//   item_armor1      100 at 30 %     item_armor2  150 at 60 %     item_armorInv  200 at 80 %
//                    (taken only when it protects more than what is worn)          20 s
//   item_shells 20   item_spikes 25   item_rockets 5   item_cells 6 (spawnflags 1: twice)  20 s
//   weapon_*         the weapon and some of its ammunition                          30 s
//   item_artifact_super_damage    the QUAD: four times the damage dealt, 30 s       60 s
//   item_artifact_invulnerability the SHIELD: no damage taken, 30 s                  60 s
//                    (a power held glows round its player: blue, red, or both)

float MAX_SHELLS = 100;
float MAX_NAILS = 200;
float MAX_ROCKETS = 100;
float MAX_CELLS = 100;

// Shows the item again after its respawn time.
void() item_return = {
  self.solid = SOLID_TRIGGER;
  setmodel(self, self.model);
  setorigin(self, self.origin);
  sound(self, CHAN_VOICE, "items/itembk2.wav", 1, ATTN_NORM);
};

// Takes the item away for its respawn time.
void() item_taken = {
  sound(other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
  stuffcmd(other, "bf\n");
  self.solid = SOLID_NOT;
  self.modelindex = 0;
  self.nextthink = time + self.respawn_time;
  self.think = item_return;
  activator = other;
  use_targets();
};

// Common setup: a trigger box on the floor, its model and pick-up sound.
void(string mdl, string snd, vector lo, vector hi, float respawn) item_start = {
  precache_model(mdl);
  precache_sound(snd);
  setmodel(self, mdl);
  self.noise = snd;
  self.flags = FL_ITEM;
  self.solid = SOLID_TRIGGER;
  self.movetype = MOVETYPE_TOSS;
  self.respawn_time = respawn;
  setsize(self, lo, hi);
  self.origin = self.origin + '0 0 6';
  if (!droptofloor()) {
    dprint("an item fell out of the map at ", vtos(self.origin), "\n");
    remove(self);
  }
};

// True when `other` is a living player who may pick things up.
float() can_pick_up = {
  return other.classname == "player" && other.health > 0 && !match_over;
};

// --- health ---

void() health_decay = {
  local entity p;
  p = self.owner;
  if (p.health > p.max_health) {
    p.health = p.health - 1;
    self.nextthink = time + 1;
    return;
  }
  // Back at the maximum: the big box comes back after its own wait.
  self.owner = world;
  self.nextthink = time + self.respawn_time;
  self.think = item_return;
};

void() health_touch = {
  local float limit;
  if (!can_pick_up()) return;
  limit = other.max_health;
  if (self.spawnflags & 2) limit = 250;
  if (other.health >= limit) return;
  other.health = other.health + self.health;
  if (other.health > limit) other.health = limit;
  say_to(other, PRINT_LOW, "You receive ", ftos(self.health), " health\n", "");
  item_taken();
  if (self.spawnflags & 2) {
    self.owner = other;
    self.nextthink = time + 5;
    self.think = health_decay;
  }
};

void() item_health = {
  self.touch = health_touch;
  if (self.spawnflags & 1) {
    self.health = 15;
    item_start("maps/b_bh10.bsp", "items/r_item1.wav", '0 0 0', '32 32 56', 20);
  } else if (self.spawnflags & 2) {
    self.health = 100;
    item_start("maps/b_bh100.bsp", "items/r_item2.wav", '0 0 0', '32 32 56', 20);
  } else {
    self.health = 25;
    item_start("maps/b_bh25.bsp", "items/health1.wav", '0 0 0', '32 32 56', 20);
  }
};

// --- armour ---

void() armour_touch = {
  if (!can_pick_up()) return;
  if (other.armortype * other.armorvalue >= self.armortype * self.armorvalue) return;
  other.armortype = self.armortype;
  other.armorvalue = self.armorvalue;
  other.items = other.items - (other.items & IT_ARMOR_ALL) + self.items;
  say_to(other, PRINT_LOW, "You got armor\n", "", "", "");
  item_taken();
};

void(float share, float value, float bit, float skin) armour_start = {
  self.armortype = share;
  self.armorvalue = value;
  self.items = bit;
  self.touch = armour_touch;
  self.skin = skin;
  item_start("progs/armor.mdl", "items/armor1.wav", '-16 -16 0', '16 16 56', 20);
};

void() item_armor1 = { armour_start(0.3, 100, IT_ARMOR1, 0); };
void() item_armor2 = { armour_start(0.6, 150, IT_ARMOR2, 1); };
void() item_armorInv = { armour_start(0.8, 200, IT_ARMOR3, 2); };

// --- ammunition ---

// Adds to `other`'s ammunition up to the maxima; true when anything was taken.
float(float shells, float nails, float rockets, float cells) ammo_give = {
  local float before;
  before = other.ammo_shells + other.ammo_nails + other.ammo_rockets + other.ammo_cells;
  other.ammo_shells = min_of(other.ammo_shells + shells, MAX_SHELLS);
  other.ammo_nails = min_of(other.ammo_nails + nails, MAX_NAILS);
  other.ammo_rockets = min_of(other.ammo_rockets + rockets, MAX_ROCKETS);
  other.ammo_cells = min_of(other.ammo_cells + cells, MAX_CELLS);
  return other.ammo_shells + other.ammo_nails + other.ammo_rockets + other.ammo_cells > before;
};

void() ammo_touch = {
  local entity was_self;
  if (!can_pick_up()) return;
  if (!ammo_give(self.ammo_shells, self.ammo_nails, self.ammo_rockets, self.ammo_cells)) return;
  say_to(other, PRINT_LOW, "You got ammunition\n", "", "", "");
  item_taken();
  was_self = self;
  self = other;
  weapon_show();
  self = was_self;
};

void(string mdl) ammo_start = {
  local float n;
  n = 1;
  if (self.spawnflags & 1) n = 2;
  self.ammo_shells = self.ammo_shells * n;
  self.ammo_nails = self.ammo_nails * n;
  self.ammo_rockets = self.ammo_rockets * n;
  self.ammo_cells = self.ammo_cells * n;
  self.touch = ammo_touch;
  item_start(mdl, "weapons/lock4.wav", '0 0 0', '32 32 56', 20);
};

void() item_shells = { self.ammo_shells = 20; ammo_start("maps/b_shell0.bsp"); };
void() item_spikes = { self.ammo_nails = 25; ammo_start("maps/b_nail0.bsp"); };
void() item_rockets = { self.ammo_rockets = 5; ammo_start("maps/b_rock0.bsp"); };
void() item_cells = { self.ammo_cells = 6; ammo_start("maps/b_batt0.bsp"); };

// --- weapons ---

void() weapon_touch = {
  local entity was_self;
  local float had;
  if (!can_pick_up()) return;
  had = other.items & self.weapon;
  ammo_give(self.ammo_shells, self.ammo_nails, self.ammo_rockets, self.ammo_cells);
  other.items = other.items | self.weapon;
  say_to(other, PRINT_LOW, "You got the ", self.netname, "\n", "");
  item_taken();
  was_self = self;
  self = other;
  // A new weapon better than the one held is taken in hand.
  if (!had && weapon_rank(self.weapon) < weapon_rank(was_self.weapon)) {
    weapon_set(was_self.weapon);
  } else {
    weapon_show();
  }
  self = was_self;
};

void(float w, string name, string mdl) weapon_start = {
  self.weapon = w;
  self.netname = name;
  self.touch = weapon_touch;
  item_start(mdl, "weapons/pkup.wav", '-16 -16 0', '16 16 56', 30);
};

void() weapon_supershotgun = {
  self.ammo_shells = 5;
  weapon_start(W_DOUBLE, "Double Scattergun", "progs/g_shot.mdl");
};
void() weapon_nailgun = {
  self.ammo_nails = 30;
  weapon_start(W_SPIKER, "Spiker", "progs/g_nail.mdl");
};
void() weapon_supernailgun = {
  self.ammo_nails = 30;
  weapon_start(W_HEAVY_SPIKER, "Heavy Spiker", "progs/g_nail2.mdl");
};
void() weapon_grenadelauncher = {
  self.ammo_rockets = 5;
  weapon_start(W_GRENADES, "Grenade Launcher", "progs/g_rock.mdl");
};
void() weapon_rocketlauncher = {
  self.ammo_rockets = 5;
  weapon_start(W_ROCKETS, "Rocket Launcher", "progs/g_rock2.mdl");
};
void() weapon_lightning = {
  self.ammo_cells = 15;
  weapon_start(W_ARC, "Arc Gun", "progs/g_light.mdl");
};

// --- backpacks ---

// A backpack: what a dead player carried, there for two minutes or until a living player takes it
// (its ammunition, and its weapon if he has none such).
void() backpack_touch = {
  local entity was_self;
  local float had;
  if (!can_pick_up()) return;
  had = other.items & self.weapon;
  ammo_give(self.ammo_shells, self.ammo_nails, self.ammo_rockets, self.ammo_cells);
  other.items = other.items | self.weapon;
  if (self.weapon) say_to(other, PRINT_LOW, "You got a backpack with the ", self.netname, "\n", "");
  else say_to(other, PRINT_LOW, "You got a backpack\n", "", "", "");
  sound(other, CHAN_ITEM, "weapons/lock4.wav", 1, ATTN_NORM);
  stuffcmd(other, "bf\n");
  was_self = self;
  self = other;
  if (!had && was_self.weapon && weapon_rank(was_self.weapon) < weapon_rank(self.weapon)) {
    weapon_set(was_self.weapon);
  } else {
    weapon_show();
  }
  self = was_self;
  remove(self);
};

void() backpack_gone = { remove(self); };

string(float w) pack_weapon_name = {
  if (w == W_DOUBLE) return "Double Scattergun";
  if (w == W_SPIKER) return "Spiker";
  if (w == W_HEAVY_SPIKER) return "Heavy Spiker";
  if (w == W_GRENADES) return "Grenade Launcher";
  if (w == W_ROCKETS) return "Rocket Launcher";
  return "Arc Gun";
};

// The dead player `self` drops his pack where he fell: nothing when he carried no ammunition.
void() backpack_drop = {
  local entity pack;
  if (self.ammo_shells + self.ammo_nails + self.ammo_rockets + self.ammo_cells <= 0) return;
  pack = spawn();
  pack.classname = "backpack";
  pack.origin = self.origin - '0 0 24';
  pack.ammo_shells = self.ammo_shells;
  pack.ammo_nails = self.ammo_nails;
  pack.ammo_rockets = self.ammo_rockets;
  pack.ammo_cells = self.ammo_cells;
  // The weapon in hand, unless it is one every player starts with.
  if (self.weapon != W_BLADE && self.weapon != W_SCATTER) {
    pack.weapon = self.weapon;
    pack.netname = pack_weapon_name(self.weapon);
  }
  pack.velocity_z = 300;  // tossed straight up (no draw on the server's generator)
  pack.flags = FL_ITEM;
  pack.solid = SOLID_TRIGGER;
  pack.movetype = MOVETYPE_TOSS;
  setmodel(pack, "progs/backpack.mdl");
  setsize(pack, '-16 -16 0', '16 16 56');
  setorigin(pack, pack.origin);
  pack.touch = backpack_touch;
  pack.nextthink = time + 120;
  pack.think = backpack_gone;
};

// --- the powers ---

float POWER_TIME = 30;

void() power_touch = {
  if (!can_pick_up()) return;
  if (self.items == IT_QUAD) {
    other.super_damage_finished = time + POWER_TIME;
    say_to(other, PRINT_LOW, "You got the Quad Damage\n", "", "", "");
  } else {
    other.invincible_finished = time + POWER_TIME;
    say_to(other, PRINT_LOW, "You got the Shield\n", "", "", "");
  }
  other.items = other.items | self.items;
  item_taken();
};

void(string mdl, string snd, float bit) power_start = {
  self.items = bit;
  self.touch = power_touch;
  if (bit == IT_QUAD) self.effects = EF_DIMLIGHT | EF_BLUE;
  else self.effects = EF_DIMLIGHT | EF_RED;
  item_start(mdl, snd, '-16 -16 -24', '16 16 32', 60);
};

void() item_artifact_super_damage = {
  power_start("progs/quaddama.mdl", "items/damage.wav", IT_QUAD);
};
void() item_artifact_invulnerability = {
  power_start("progs/invulner.mdl", "items/protect.wav", IT_INVULNERABILITY);
};

// Each frame for a living player: the powers that ran out taken away, the glow of the ones held.
void() power_frame = {
  if (self.super_damage_finished && self.super_damage_finished < time) {
    self.super_damage_finished = 0;
    self.items = self.items - (self.items & IT_QUAD);
  }
  if (self.invincible_finished && self.invincible_finished < time) {
    self.invincible_finished = 0;
    self.items = self.items - (self.items & IT_INVULNERABILITY);
  }
  self.effects = self.effects - (self.effects & (EF_DIMLIGHT | EF_BLUE | EF_RED));
  if (self.super_damage_finished) self.effects = self.effects | EF_DIMLIGHT | EF_BLUE;
  if (self.invincible_finished) self.effects = self.effects | EF_DIMLIGHT | EF_RED;
};

void() item_precache = {
  precache_sound("items/itembk2.wav");
};
