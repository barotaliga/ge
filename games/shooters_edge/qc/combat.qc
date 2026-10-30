// DAMAGE AND DEATH: what a hit does (armour first, then health, a push away from the blow),
// what a blast does to everything near it, and how a death is scored and told.

void() player_die;
void() player_pain;

// The frags a death is worth: a kill +1, a death by one's own hand or the world's -1.
void(entity targ, entity attacker) score_death = {
  local string how;
  if (targ.classname != "player") return;
  how = targ.deathtype;
  if (attacker == targ) {
    targ.frags = targ.frags - 1;
    bprint(PRINT_MEDIUM, targ.netname, " fragged themselves\n");
    return;
  }
  if (attacker.classname == "player") {
    attacker.frags = attacker.frags + 1;
    logfrag(attacker, targ);
    if (how == "") how = "fragged";
    bprint(PRINT_MEDIUM, targ.netname, " was ", how, " by ", attacker.netname, "\n");
    return;
  }
  targ.frags = targ.frags - 1;
  if (how == "") how = "died";
  bprint(PRINT_MEDIUM, targ.netname, " ", how, "\n");
};

// `damage` points from `attacker` through `inflictor` (the missile, or the attacker itself).
void(entity targ, entity inflictor, entity attacker, float damage) damage = {
  local float saved, taken;
  local vector dir;
  local entity was_self;
  if (!targ.takedamage || targ.health <= 0) return;
  if (match_over) return;
  // Armour takes its share, as far as it lasts.
  saved = ceil(targ.armortype * damage);
  if (saved >= targ.armorvalue) {
    saved = targ.armorvalue;
    targ.armortype = 0;
    targ.items = targ.items - (targ.items & IT_ARMOR_ALL);
  }
  targ.armorvalue = targ.armorvalue - saved;
  taken = ceil(damage - saved);
  targ.dmg_take = targ.dmg_take + taken;
  targ.dmg_save = targ.dmg_save + saved;
  targ.dmg_inflictor = inflictor;
  // The push: away from the blow's centre, more for a lighter blow's target than a heavier.
  if (inflictor != world && targ.movetype == MOVETYPE_WALK) {
    dir = targ.origin - (inflictor.absmin + inflictor.absmax) * 0.5;
    dir = normalize(dir);
    targ.velocity = targ.velocity + dir * damage * 8;
  }
  targ.health = targ.health - taken;
  if (targ.health <= 0) {
    score_death(targ, attacker);
    was_self = self;
    self = targ;
    player_die();
    self = was_self;
    return;
  }
  was_self = self;
  self = targ;
  player_pain();
  self = was_self;
};

// A blast at `inflictor`: everything within reach that takes damage gets `points` less half its
// distance from the centre (the attacker himself half of that). `ignore` takes nothing.
void(entity inflictor, entity attacker, float points, entity ignore, string how) radius_damage = {
  local entity e;
  local vector centre;
  local float d;
  e = findradius(inflictor.origin, points + 40);
  while (e) {
    if (e != ignore && e.takedamage) {
      centre = e.origin + (e.mins + e.maxs) * 0.5;
      d = points - 0.5 * vlen(inflictor.origin - centre);
      if (e == attacker) d = d * 0.5;
      if (d > 0) {
        e.deathtype = how;
        damage(e, inflictor, attacker, d);
      }
    }
    e = e.chain;
  }
};
