// The fighter (fighter.hpp): its stats, the map's items timed off what is sent, the mode, the goal,
// the weapon and the trigger.
//
// ge:summary The fighter: stats, item timing, mode, goal, weapon and trigger on the baseline mod
// ge:keywords fighter fighter_init fighter_command read_stats item-spot weapon-table tolerance
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#include "shooters_edge/bots/fighter.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "ai/choose.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "progs/entity_text.hpp"
#include "progs/layout.hpp"
#include "shooter/usercmd.hpp"

namespace shooters_edge::bots {

namespace {

namespace pg = ge::progs;
namespace ai = ge::ai;

// The baseline mod's weapon bits (games/shooters_edge/qc/common/defs.qc) and ammunition maxima
// (items.qc).
constexpr int32_t kWScatter = 1, kWDouble = 2, kWSpiker = 4, kWHeavy = 8, kWGrenades = 16;
constexpr int32_t kWRockets = 32, kWArc = 64, kWBlade = 4096;
constexpr float kAmmoMax = 100;

constexpr float kRetreatBelow = 50;  // health plus armour's protection: weak under this
constexpr float kStrongAgain = 80;   // and strong again at this
constexpr double kChase_s = 3;       // a contact lost this long ago is still chased
constexpr double kImpulseGap_s = 0.5;
constexpr double kWait_s = 20;     // an item back within this is worth going to wait for
constexpr double kHealWait_s = 3;  // but a retreat goes for health that is there
constexpr double kKeepGoal = 1.5;  // the goal's score is held up by this against the others
constexpr float kRun = 320;        // units/s, the run to an item

enum class Ammo : uint8_t { none, shells, nails, rockets, cells };

struct WeaponRow {
  int32_t bit;
  int number;  // its impulse
  const char* name;
  Ammo ammo;
  float cost;
  float shot_speed;  // 0 a hitscan
  float aim_dz;      // aimed over the middle
  float width;       // the target's half width the trigger allows at range, units
  float range;       // fired within this, units
};

const WeaponRow kWeapons[8] = {
    {kWBlade, 1, "blade", Ammo::none, 0, 0, 0, 24, 72},
    {kWScatter, 2, "scattergun", Ammo::shells, 1, 0, 0, 24, 1200},
    {kWDouble, 3, "double", Ammo::shells, 2, 0, 0, 32, 900},
    {kWSpiker, 4, "spiker", Ammo::nails, 1, 1000, 0, 16, 1500},
    {kWHeavy, 5, "heavy spiker", Ammo::nails, 2, 1000, 0, 20, 1500},
    {kWGrenades, 6, "grenades", Ammo::rockets, 1, 600, -20, 40, 500},
    {kWRockets, 7, "rockets", Ammo::rockets, 1, 1000, -20, 48, 2000},
    {kWArc, 8, "arc", Ammo::cells, 1, 0, 0, 20, 600},
};

float ammo_of(const Stats& st, Ammo a) {
  switch (a) {
    case Ammo::shells:
      return st.shells;
    case Ammo::nails:
      return st.nails;
    case Ammo::rockets:
      return st.rockets;
    case Ammo::cells:
      return st.cells;
    case Ammo::none:
      break;
  }
  return 1;
}

bool loaded(const Stats& st, const WeaponRow& w) {
  return (st.items & w.bit) != 0 && ammo_of(st, w.ammo) >= w.cost;
}

const WeaponRow* row_of(int32_t bit) {
  for (const WeaponRow& w : kWeapons)
    if (w.bit == bit) return &w;
  return nullptr;
}

// How good weapon `w` is at range `d` (higher better).
double weapon_score(const WeaponRow& w, float d) {
  switch (w.number) {
    case 1:
      return d < 72 ? 30 : 0.1;
    case 2:
      return 20;
    case 3:
      return d < 300 ? 60 : 25;
    case 4:
      return 35;
    case 5:
      return d < 600 ? 65 : 40;
    case 6:
      return d > 120 && d < 500 ? 45 : 5;
    case 7:
      return d > 130 ? 90 : 15;
    default:
      return d < 560 ? 85 : 5;
  }
}

float field(const server::QcServer& s, int ent, int f) {
  if (f < 0) return 0;
  return pg::as_float(
      s.q.vm.entities[static_cast<size_t>(ent) * static_cast<size_t>(s.q.vm.entityfields) +
                      static_cast<size_t>(f)]);
}

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

bool parse_vec(const char* text, sh::Vec3f& out) {
  if (!text) return false;
  char* end = nullptr;
  out.x = std::strtof(text, &end);
  out.y = std::strtof(end, &end);
  out.z = std::strtof(end, &end);
  return true;
}

// The map's item from its classname, or false.
bool item_of(const char* cn, int spawnflags, ItemSpot& it) {
  const bool twice = (spawnflags & 1) != 0;
  struct Ammo4 {
    const char* cn;
    ItemKind kind;
    float amount;
  };
  static const Ammo4 kAmmo[4] = {{"item_shells", ItemKind::shells, 20},
                                 {"item_spikes", ItemKind::nails, 25},
                                 {"item_rockets", ItemKind::rockets, 5},
                                 {"item_cells", ItemKind::cells, 6}};
  for (const Ammo4& a : kAmmo)
    if (std::strcmp(cn, a.cn) == 0) {
      it.kind = a.kind;
      it.amount = a.amount * (twice ? 2.f : 1.f);
      it.name = a.cn;
      return true;
    }
  if (std::strcmp(cn, "item_health") == 0) {
    it.kind = (spawnflags & 2) ? ItemKind::mega : ItemKind::health;
    it.amount = (spawnflags & 2) ? 100.f : (spawnflags & 1) ? 15.f : 25.f;
    it.name = (spawnflags & 2) ? "mega health" : "health";
    return true;
  }
  if (std::strcmp(cn, "item_armor1") == 0)
    it.kind = ItemKind::armor1;
  else if (std::strcmp(cn, "item_armor2") == 0)
    it.kind = ItemKind::armor2;
  else if (std::strcmp(cn, "item_armorInv") == 0)
    it.kind = ItemKind::armor3;
  if (std::strncmp(cn, "item_armor", 10) == 0) {
    it.name = cn;
    return true;
  }
  struct Gun {
    const char* cn;
    int32_t bit;
  };
  static const Gun kGuns[6] = {
      {"weapon_supershotgun", kWDouble},    {"weapon_nailgun", kWSpiker},
      {"weapon_supernailgun", kWHeavy},     {"weapon_grenadelauncher", kWGrenades},
      {"weapon_rocketlauncher", kWRockets}, {"weapon_lightning", kWArc}};
  for (const Gun& g : kGuns)
    if (std::strcmp(cn, g.cn) == 0) {
      it.kind = ItemKind::weapon;
      it.weapon = g.bit;
      it.respawn_s = 30;
      it.name = cn;
      return true;
    }
  return false;
}

float protection(float type, float value) { return type * value; }

// What item `it` is worth to a player with `st` now (0: nothing).
double item_value(const ItemSpot& it, const Stats& st) {
  const float worn = protection(st.armortype, st.armor);
  switch (it.kind) {
    case ItemKind::health:
      return st.health >= 100 ? 0 : std::fmin(it.amount, 100 - st.health);
    case ItemKind::mega:
      return st.health >= 250 ? 0 : 20 + 0.8 * (250 - st.health);
    case ItemKind::armor1:
      return std::fmax(0.f, protection(0.3f, 100) - worn);
    case ItemKind::armor2:
      return std::fmax(0.f, protection(0.6f, 150) - worn);
    case ItemKind::armor3:
      return std::fmax(0.f, protection(0.8f, 200) - worn) * 1.2;
    case ItemKind::weapon: {
      if (st.items & it.weapon) return 4;
      switch (it.weapon) {
        case kWRockets:
          return 120;
        case kWArc:
          return 90;
        case kWHeavy:
          return 60;
        case kWDouble:
          return 45;
        case kWGrenades:
          return 40;
        default:
          return 30;
      }
    }
    default:
      break;
  }
  // Ammunition: worth what it fills of a weapon carried that uses it.
  Ammo a = Ammo::shells;
  if (it.kind == ItemKind::nails) a = Ammo::nails;
  if (it.kind == ItemKind::rockets) a = Ammo::rockets;
  if (it.kind == ItemKind::cells) a = Ammo::cells;
  bool uses = false;
  for (const WeaponRow& w : kWeapons) uses |= w.ammo == a && (st.items & w.bit) != 0;
  const float have = ammo_of(st, a);
  if (!uses || have >= kAmmoMax) return 1;
  return 30.0 * (kAmmoMax - have) / kAmmoMax;
}

bool heals(const ItemSpot& it) {
  return it.kind == ItemKind::health || it.kind == ItemKind::mega || it.kind == ItemKind::armor1 ||
         it.kind == ItemKind::armor2 || it.kind == ItemKind::armor3;
}

float strength(const Stats& st) { return st.health + protection(st.armortype, st.armor); }

sh::Vec3f at_of(const ai::Memory& m) {
  return {static_cast<float>(m.x), static_cast<float>(m.y), static_cast<float>(m.z)};
}

void set_mode(Fighter& f, FightMode next, ai::Why* why, const char* because) {
  if (f.mode == next) return;
  f.stats.modes++;
  ai::enter(f.mode, f.held, next, why, fight_mode_name, because);
}

void go_to(Fighter& f, int32_t node) {
  if (node < 0) return;
  const sh::PilotMode pm = f.bot.pilot.pilot.mode;
  if (node == f.goal_node && pm != sh::PilotMode::idle && pm != sh::PilotMode::arrived) return;
  if (node == f.goal_node && pm == sh::PilotMode::arrived) return;  // standing on it
  f.goal_node = node;
  sh::bot_goal(f.bot, node);
}

// The best item for the player at `me` with `st`: worth over time to have it, `away` from a known
// enemy (when `heal`, health and armour alone, and never nearer the enemy than this player is).
int32_t best_item(const Fighter& f, const Stats& st, sh::Vec3f me, double t, bool heal,
                  const sh::Vec3f* enemy, ai::Why* why) {
  const auto score = [&](int32_t i) {
    const ItemSpot& it = f.items[static_cast<size_t>(i)];
    if (it.node < 0 || (heal && !heals(it))) return 0.0;
    const double v = item_value(it, st);
    if (v <= 0) return 0.0;
    const double run = dist(me, it.at) / kRun;
    const double wait = it.present ? 0 : it.back_at - t;
    if (wait > (heal ? kHealWait_s : kWait_s)) return 0.0;
    double s = v / (1 + std::fmax(run, wait));
    if (i == f.goal_item) s *= kKeepGoal;  // the goal held unless another is clearly better
    if (enemy && heal && dist(*enemy, it.at) < dist(me, it.at)) s *= 0.2;
    return s;
  };
  const auto name = [&](int32_t i) { return f.items[static_cast<size_t>(i)].name.c_str(); };
  return ai::pick_best(static_cast<int32_t>(f.items.size()), score, why,
                       heal ? "heal_goal" : "item_goal", name, "worth over the time to have it");
}

void think(Fighter& f, const server::QcServer& s, int me, const Stats& st, ai::Why* why) {
  const double t = s.sv.time;
  const sh::Edict& self = s.sv.edicts[static_cast<size_t>(me)];
  const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};

  // The items: one no longer sent was taken (timed from now), one sent again is back.
  for (ItemSpot& it : f.items) {
    if (it.ent < 0) continue;
    bool sent = false;
    for (const sh::ViewEntity& v : f.bot.view.entities) sent |= v.ent == it.ent;
    if (it.present && !sent) {
      it.back_at = t + it.respawn_s;
      f.stats.taken++;
      if (dist(it.at, self.origin) < 64 && st.health > 0) f.stats.pickups++;
    }
    it.present = sent;
  }

  const sh::Contact* c = sh::freshest(f.bot.senses, eye);
  const bool seen_now = c && c->visible;
  const bool recent = c && ai::known(c->seen) && ai::age(c->seen, t) <= kChase_s;
  const sh::Vec3f enemy = c ? at_of(ai::known(c->seen) ? c->seen : c->heard) : sh::Vec3f{};
  const float range = c ? dist(eye, enemy) : 400;

  // The mode.
  const float strong = strength(st);
  const bool weak = f.mode == FightMode::retreat ? strong < kStrongAgain : strong < kRetreatBelow;
  const int32_t heal_at = c && weak ? best_item(f, st, self.origin, t, true, &enemy, nullptr) : -1;
  if (heal_at >= 0 && item_value(f.items[static_cast<size_t>(heal_at)], st) > 0) {
    set_mode(f, FightMode::retreat, why, "weak with someone about: health or armour first");
  } else if (seen_now) {
    set_mode(f, FightMode::fight, why, "a contact in sight");
  } else if (recent) {
    set_mode(f, FightMode::chase, why, "lost from sight a moment ago");
  } else {
    set_mode(f, FightMode::roam, why, "no one known");
  }

  // The goal.
  if (f.mode == FightMode::chase && c) {
    go_to(f, sh::nav_nearest(*f.bot.pilot.pilot.g, at_of(c->seen)));
  } else {
    const bool heal = f.mode == FightMode::retreat;
    const int32_t i = best_item(f, st, self.origin, t, heal, c ? &enemy : nullptr, why);
    if (i >= 0) {
      f.goal_item = i;
      go_to(f, f.items[static_cast<size_t>(i)].node);
    }
  }

  // The weapon.
  const auto score = [&](int32_t i) {
    const WeaponRow& w = kWeapons[i];
    return loaded(st, w) ? weapon_score(w, range) : -1.0;
  };
  const auto name = [](int32_t i) { return kWeapons[i].name; };
  const int32_t w = ai::pick_best(8, score, why, "weapon", name, "the best loaded at the range");
  if (w >= 0 && kWeapons[w].bit != st.weapon && loaded(st, kWeapons[w]) &&
      t - f.impulse_at >= kImpulseGap_s)
    f.want_weapon = kWeapons[w].number;
}

}  // namespace

const char* fight_mode_name(FightMode m) {
  switch (m) {
    case FightMode::roam:
      return "roam";
    case FightMode::fight:
      return "fight";
    case FightMode::chase:
      return "chase";
    case FightMode::retreat:
      return "retreat";
  }
  return "?";
}

Stats read_stats(const server::QcServer& s, int ent) {
  const pg::LayoutFields& f = s.q.layout->f;
  const int kHealth = f.health, kItems = f.items, kArmor = f.armorvalue, kType = f.armortype;
  const int kWeapon = f.weapon, kShells = f.ammo_shells, kNails = f.ammo_nails;
  const int kRockets = f.ammo_rockets, kCells = f.ammo_cells;
  Stats st;
  st.health = field(s, ent, kHealth);
  st.armor = field(s, ent, kArmor);
  st.armortype = field(s, ent, kType);
  st.items = static_cast<int32_t>(field(s, ent, kItems));
  st.weapon = static_cast<int32_t>(field(s, ent, kWeapon));
  st.shells = field(s, ent, kShells);
  st.nails = field(s, ent, kNails);
  st.rockets = field(s, ent, kRockets);
  st.cells = field(s, ent, kCells);
  return st;
}

void fighter_init(Fighter& f, const sh::NavGraph& g, const server::QcServer& s, int slot,
                  const char* entities, sh::AimSkill skill, double hz, uint32_t seed) {
  sh::bot_init(f.bot, g, sh::PilotSettings{}, sh::PilotHands::human, skill, hz, seed);
  const int me = slot + 1;
  sh::client_view(s.sv, me, f.bot.view);
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  f.items.clear();
  if (!pg::parse_entity_text(entities, blocks, error)) return;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    if (!cn) continue;
    const char* flags = pg::block_value(b, "spawnflags");
    ItemSpot it;
    if (!item_of(cn, flags ? std::atoi(flags) : 0, it)) continue;
    sh::Vec3f origin;
    parse_vec(pg::block_value(b, "origin"), origin);
    // The entity sent nearest the map's place, across the floor.
    float best = 48;
    for (const sh::ViewEntity& v : f.bot.view.entities) {
      if (v.ent <= s.sv.set.maxclients) continue;
      const sh::Edict& e = s.sv.edicts[static_cast<size_t>(v.ent)];
      const sh::Vec3f mid{(e.absmin.x + e.absmax.x) * 0.5f, (e.absmin.y + e.absmax.y) * 0.5f,
                          e.absmin.z};
      const float dx = v.origin.x - origin.x, dy = v.origin.y - origin.y;
      const float d = std::sqrt(dx * dx + dy * dy);
      if (d < best) {
        best = d;
        it.ent = v.ent;
        it.at = mid;
      }
    }
    if (it.ent < 0) continue;
    it.node = sh::nav_nearest(g, {it.at.x, it.at.y, it.at.z + 24});
    f.items.push_back(it);
  }
}

sh::MoveCmd fighter_command(Fighter& f, const server::QcServer& s, int slot, int msec) {
  const int me = slot + 1;
  const Stats st = read_stats(s, me);
  const double t = s.sv.time;
  ai::Why* why = f.bot.pilot.why ? f.bot.pilot.why->follow(slot, f.bot.pilot.command) : nullptr;
  f.stats.commands++;
  const bool dead = st.health <= 0;
  if (f.was_dead && !dead) {
    f.stats.respawns++;
    f.goal_node = -1;
    f.next_think = 0;
  }
  f.was_dead = dead;

  if (!dead && t >= f.next_think) {
    f.next_think = t + kThink_s;
    think(f, s, me, st, why);
  }
  if (!dead && f.bot.pilot.pilot.mode == sh::PilotMode::arrived) f.next_think = 0;

  const WeaponRow* held = row_of(st.weapon);
  f.bot.shot_speed = held ? held->shot_speed : 0;
  f.bot.aim_dz = held ? held->aim_dz : 0;

  sh::MoveCmd cmd = sh::bot_source(f.bot).next(&f.bot, s.sv, slot, msec);
  f.stats.mode_commands[static_cast<int>(f.mode)]++;
  if (held) f.stats.weapon_commands[held->number]++;

  if (dead) {
    // Back in once the mod allows: attack pressed every other command (a press is an edge).
    cmd.buttons = (f.stats.commands & 1) ? sh::kButtonAttack : 0;
    return cmd;
  }
  if (f.want_weapon > 0) {
    cmd.impulse = static_cast<uint8_t>(f.want_weapon);
    f.want_weapon = 0;
    f.impulse_at = t;
    f.stats.switches++;
  }

  // The trigger: a contact in sight, the view on the aim within the weapon's width at the range.
  const sh::Contact* c = nullptr;
  for (const sh::Contact& k : f.bot.senses.contacts)
    if (k.ent == f.bot.target && k.visible) c = &k;
  if (held && c && f.bot.aiming) {
    const sh::Edict& self = s.sv.edicts[static_cast<size_t>(me)];
    const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};
    const float d = dist(eye, at_of(c->seen));
    const float tol = static_cast<float>(
        ge::core::libm::atan2(held->width, std::fmax(d, 1.f)) * (180.0 / ge::core::kPi) + 1.5);
    const bool fire = d <= held->range && sh::bot_aim_off(f.bot) <= tol;
    if (fire) {
      cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
      f.stats.shots++;
    }
    if (why) {
      why->decide("trigger");
      why->fact("range", d);
      why->fact("off", sh::bot_aim_off(f.bot));
      why->fact("tolerance", tol);
      why->choose(fire ? "fire" : "hold", fire ? "on the target within its width" : "off it");
    }
  }
  return cmd;
}

}  // namespace shooters_edge::bots
