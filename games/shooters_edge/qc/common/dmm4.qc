// DEATHMATCH 4 ("deathmatch 4"): a duel or a free-for-all where nobody gathers anything. Every
// spawn is armed with every weapon and full of everything, so the match is the aim and the move:
//
//   THE SPAWN      health 250 (the maximum stays 100), 200 red armour at 80 %, 255 of every
//                  ammunition, every weapon but those `k_disallow_weapons` names (the weapons'
//                  own bits: 16 the grenade launcher, as duel servers set it), the rocket
//                  launcher in hand (else the arc, the heavy spiker, the double, the spiker, the
//                  scattergun, the blade).
//   THE PROTECTION `dmm4_invinc_time` seconds after a spawn (unset or 0: 2 s; negative: none; at
//                  most 30): no harm to health from anything but the kill command; the armour
//                  still wears and a blow still pushes. The shield's icon, no glow; told almost
//                  burned out 3 s before it ends (at once at 2 s), a flash a second after; a hit
//                  it soaks heard, every 2 s at most. Health, armour and packs are not taken while
//                  any shield holds. Two protected players on one spot both die; an unprotected
//                  one landing on a protected one dies; the one who dies loses a frag, nobody
//                  gains one.
//   THE MAP        no weapon, ammunition or mega health lies on it; health, armour and the powers
//                  stay. Ammunition is never used up.
//   THE BLOWS      the quad is the OctaPower: eight times the damage; taking it strips the armour
//                  and the cells, and as it wears off health is cut to 100 at most, the armour set
//                  to 1 point of red and the cells to 255. The blade cuts for 75. The arc fired
//                  under water kills its firer one time in two, else it discharges.
//   THE PACKS      a dead player's pack is 10 more health (no ammunition); at 300 or more, 30 s of
//                  the shield and the OctaPower, the cells emptied, told to all.
//
// The variant's glue calls `dmm4_give` for a spawn's inventory; the common code asks `deathmatch
// == 4` where the rules differ and calls the rest of these.

float DMM4_HEALTH = 250;
float DMM4_ARMOUR = 200;
float DMM4_AMMO = 255;
float DMM4_PACK = 10;      // a pack's health
float DMM4_BONUS = 300;    // health that wins the bonus powers
float DMM4_POWERS = 30;    // their seconds

// The protection's seconds after a spawn (0: none).
float() dmm4_shield_time = {
  local float t;
  t = cvar("dmm4_invinc_time");
  if (t < 0) return 0;
  if (t == 0) return 2;
  if (t > 30) return 30;
  return t;
};

// The weapons a spawn holds.
float() dmm4_weapons = {
  local float all;
  all = W_BLADE | W_SCATTER | W_DOUBLE | W_SPIKER | W_HEAVY_SPIKER | W_GRENADES | W_ROCKETS | W_ARC;
  return all - (all & cvar("k_disallow_weapons"));
};

// The weapon in hand at a spawn: the rocket launcher, else the best of the rest that is not a
// launcher (the arc only out of water).
void() dmm4_hand = {
  if (self.items & W_ROCKETS) weapon_set(W_ROCKETS);
  else if ((self.items & W_ARC) && self.waterlevel <= 1) weapon_set(W_ARC);
  else if (self.items & W_HEAVY_SPIKER) weapon_set(W_HEAVY_SPIKER);
  else if (self.items & W_DOUBLE) weapon_set(W_DOUBLE);
  else if (self.items & W_SPIKER) weapon_set(W_SPIKER);
  else if (self.items & W_SCATTER) weapon_set(W_SCATTER);
  else weapon_set(W_BLADE);
};

// A spawn's inventory and protection.
void() dmm4_give = {
  local float t;
  self.items = dmm4_weapons() | IT_ARMOR3;
  self.health = DMM4_HEALTH;
  self.armortype = 0.8;
  self.armorvalue = DMM4_ARMOUR;
  self.ammo_shells = DMM4_AMMO;
  self.ammo_nails = DMM4_AMMO;
  self.ammo_rockets = DMM4_AMMO;
  self.ammo_cells = DMM4_AMMO;
  self.shield_finished = 0;
  self.shield_said = 0;
  t = dmm4_shield_time();
  if (t > 0) {
    self.shield_finished = time + t;
    self.items = self.items | IT_INVULNERABILITY;
  }
  dmm4_hand();
};

// True while any shield holds `p` (the spawn's or the power's).
float(entity p) dmm4_shielded = {
  return p.shield_finished > time || p.invincible_finished > time;
};

// Each frame for a living player: the protection told running out, a flash a second, and gone.
void() dmm4_frame = {
  if (!self.shield_finished) return;
  if (self.shield_finished <= time) {
    self.shield_finished = 0;
    if (self.invincible_finished <= time)
      self.items = self.items - (self.items & IT_INVULNERABILITY);
    return;
  }
  if (!self.shield_said && self.shield_finished - 3 <= time) {
    self.shield_said = 1;
    self.shield_flash = time + 1;
    say_to(self, PRINT_HIGH, "Protection is almost burned out\n", "", "", "");
    sound(self, CHAN_ITEM, "items/protect2.wav", 1, ATTN_NORM);
    stuffcmd(self, "bf\n");
  } else if (self.shield_said && time >= self.shield_flash) {
    self.shield_flash = self.shield_flash + 1;
    stuffcmd(self, "bf\n");
  }
};

// A hit the protection soaked: heard, every 2 s at most.
void(entity targ) dmm4_soaked = {
  if (time < targ.shield_heard) return;
  targ.shield_heard = time + 2;
  sound(targ, CHAN_ITEM, "items/protect3.wav", 1, ATTN_NORM);
};

// The OctaPower taken by `other`: the armour and the cells go.
void() dmm4_octa_taken = {
  other.armortype = 0;
  other.armorvalue = 0;
  other.items = other.items - (other.items & IT_ARMOR_ALL);
  other.ammo_cells = 0;
};

// The OctaPower worn off `self`: health 100 at most, a point of red armour, the cells back.
void() dmm4_octa_off = {
  if (self.health > 100) self.health = 100;
  self.armortype = 0.8;
  self.armorvalue = 1;
  self.items = self.items - (self.items & IT_ARMOR_ALL) + IT_ARMOR3;
  self.ammo_cells = DMM4_AMMO;
  weapon_show();
};

// A pack taken by `other` (the pack is `self`): 10 health, the bonus powers at 300.
void() dmm4_pack_take = {
  other.health = other.health + DMM4_PACK;
  say_to(other, PRINT_LOW, "You get 10 additional health\n", "", "", "");
  stuffcmd(other, "bf\n");
  if (other.health > DMM4_HEALTH && other.health < DMM4_BONUS)
    sound(other, CHAN_ITEM, "items/protect3.wav", 1, ATTN_NORM);
  else
    sound(other, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);
  if (other.health >= DMM4_BONUS) {
    other.invincible_finished = time + DMM4_POWERS;
    other.super_damage_finished = time + DMM4_POWERS;
    other.power_said = other.power_said - (other.power_said & (IT_QUAD | IT_INVULNERABILITY));
    other.items = other.items | IT_QUAD | IT_INVULNERABILITY;
    other.ammo_cells = 0;
    sound(other, CHAN_VOICE, "items/protect.wav", 1, ATTN_NONE);
    say_all(PRINT_HIGH, other.netname, " gained bonus powers!!!\n", "", "", "", "");
  }
};

void() dmm4_precache = {
  precache_sound("items/protect.wav");
  precache_sound("items/protect2.wav");
  precache_sound("items/protect3.wav");
  precache_sound("weapons/pkup.wav");
};
