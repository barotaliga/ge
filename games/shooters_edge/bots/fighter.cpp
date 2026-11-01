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
#include "shooter/trace.hpp"
#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/team.hpp"

namespace shooters_edge::bots {

namespace {

namespace pg = ge::progs;
namespace ai = ge::ai;

// The baseline mod's weapon bits (games/shooters_edge/qc/common/defs.qc) and ammunition maxima
// (items.qc).
constexpr int32_t kWScatter = 1, kWDouble = 2, kWSpiker = 4, kWHeavy = 8, kWGrenades = 16;
constexpr int32_t kWRockets = 32, kWArc = 64, kWBlade = 4096;
constexpr float kAmmoMax = 100;
constexpr int32_t kItQuad = 4194304;  // the quad's IT_ bit
constexpr int32_t kItPent = 1048576;  // the shield's

constexpr float kRetreatBelow = 50;  // health plus armour's protection: weak under this
constexpr float kStrongAgain = 80;   // and strong again at this
constexpr double kChase_s = 3;       // a contact lost this long ago is still chased
constexpr double kImpulseGap_s = 0.5;
constexpr double kWait_s = 20;     // an item back within this is worth going to wait for
constexpr double kHealWait_s = 3;  // but a retreat goes for health that is there
constexpr double kKeepGoal = 1.5;  // the goal's score is held up by this against the others
constexpr float kRun = 320;        // units/s, the run to an item
constexpr double kAir_s = 6;  // under this long, the shorter way out of the water (drowning at 12)
constexpr double kDiveMost_s = 8;   // no dive whose swim is longer than this
constexpr double kCallAgain_s = 4;  // an enemy in sight is called out again after this
constexpr double kHelp_s = 6;       // a call for help holds this long
constexpr float kCallNear = 600;    // an enemy is called by an item this near it, units

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
  if (std::strcmp(cn, "item_artifact_invulnerability") == 0) {
    it.kind = ItemKind::pent;
    it.respawn_s = 60;
    it.name = "pent";
    return true;
  }
  if (std::strcmp(cn, "item_artifact_super_damage") == 0) {
    it.kind = ItemKind::quad;
    it.respawn_s = 60;
    it.name = "quad";
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
    case ItemKind::quad:
      return (st.items & kItQuad) ? 0 : 150;
    case ItemKind::pent:
      return (st.items & kItPent) ? 0 : 130;
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

bool alive_slot(const server::QcServer& s, int slot) {
  return s.sv.clients[static_cast<size_t>(slot)].active &&
         s.sv.edicts[static_cast<size_t>(slot) + 1].health > 0;
}

// A living teammate's slot that has claimed item `i` (-1: none).
int claimed_by_mate(const Fighter& f, const server::QcServer& s, int32_t i) {
  if (!f.team || i < 0 || static_cast<size_t>(i) >= f.team->claimed.size()) return -1;
  const int c = f.team->claimed[static_cast<size_t>(i)];
  return c >= 0 && c != f.slot && alive_slot(s, c) ? c : -1;
}

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
  if (f.reach_at >= 0 && !f.reach.reached(node)) return;  // no way there from here
  const sh::PilotMode pm = f.bot.pilot.pilot.mode;
  if (node == f.goal_node && pm != sh::PilotMode::idle && pm != sh::PilotMode::arrived) return;
  if (node == f.goal_node && pm == sh::PilotMode::arrived) return;  // standing on it
  f.goal_node = node;
  sh::bot_goal(f.bot, node);
}

bool door_shut(const server::QcServer& s, const ItemSpot& it) {
  if (it.door_ent < 0) return false;
  const sh::Vec3f d = s.sv.edicts[static_cast<size_t>(it.door_ent)].origin - it.door_rest;
  return d.x * d.x + d.y * d.y + d.z * d.z < 1;
}

// Goes for item `i`: through its button first while its door is shut.
void go_item(Fighter& f, const server::QcServer& s, int32_t i) {
  const ItemSpot& it = f.items[static_cast<size_t>(i)];
  f.goal_item = i;
  if (it.dive && f.reach_at >= 0 && !f.reach.reached(it.node)) {
    // Through the water: to the dive's node, then the field (fighter_command) takes it in.
    f.unlocking = false;
    if (f.last_dive != i) f.dives++;
    f.last_dive = i;
    f.diving = i;
    go_to(f, it.dive_node);
    return;
  }
  f.diving = -1;
  const bool unlock = door_shut(s, it) && it.button_node >= 0;
  if (unlock && !f.unlocking) f.buttons++;
  f.unlocking = unlock;
  go_to(f, unlock ? it.button_node : it.node);
}

// The best item for the player at `me` with `st`: worth over time to have it, `away` from a known
// enemy (when `heal`, health and armour alone, and never nearer the enemy than this player is).
int32_t best_item(const Fighter& f, const server::QcServer& s, const Stats& st, sh::Vec3f me,
                  double t, bool heal, const sh::Vec3f* enemy, ai::Why* why) {
  const auto score = [&](int32_t i) {
    const ItemSpot& it = f.items[static_cast<size_t>(i)];
    if (it.node < 0 || (heal && !heals(it))) return 0.0;
    // Only what it knows of (or sees). One gone is waited for at its time back as the player
    // knows it: exact when timed, a guess (late) when not (knowledge.hpp).
    if (!item_in_mind(f, i)) return 0.0;
    // No way there from here, unless by its dive (from a node there is a way to).
    const bool by_dive = f.reach_at >= 0 && !f.reach.reached(it.node);
    if (by_dive && (!it.dive || !f.reach.reached(it.dive_node) || it.dive_s > kDiveMost_s))
      return 0.0;
    if (door_shut(s, it) &&
        (it.button_node < 0 || (f.reach_at >= 0 && !f.reach.reached(it.button_node))))
      return 0.0;  // shut, and its button out of reach
    const double v = item_value(it, st);
    if (v <= 0) return 0.0;
    const double run = dist(me, it.at) / kRun + (by_dive ? it.dive_s : 0);
    const double wait = it.present ? 0 : it.back_at - t;
    // Only the items a game turns on are waited for; the rest are taken when they are there.
    const bool key = key_item(it);
    if (wait > (heal || (!key && f.team) ? kHealWait_s : kWait_s)) return 0.0;
    double sc = v / (1 + std::fmax(run, wait));
    if (i == f.goal_item) sc *= kKeepGoal;  // the goal held unless another is clearly better
    if (enemy && heal && dist(*enemy, it.at) < dist(me, it.at)) sc *= 0.2;
    // A teammate going for it has it; a strong player leaves health and armour to the others.
    if (claimed_by_mate(f, s, i) >= 0) sc *= 0.15;
    if (f.team && heals(it) && it.kind != ItemKind::mega && strength(st) >= 150) sc *= 0.3;
    // A weak player falls back to his own: health near the held area is worth more.
    if (f.team && heal && f.team->hold_item >= 0 &&
        dist(it.at, f.items[static_cast<size_t>(f.team->hold_item)].at) < kLeash)
      sc *= 1.5;
    return sc;
  };
  const auto name = [&](int32_t i) { return f.items[static_cast<size_t>(i)].name.c_str(); };
  return ai::pick_best(static_cast<int32_t>(f.items.size()), score, why,
                       heal ? "heal_goal" : "item_goal", name, "worth over the time to have it");
}

// A backpack in view within `reach` worth going for (ammunition short or a weapon lacking), or -1.
int32_t pack_near(const Fighter& f, const server::QcServer& s, const Stats& st, sh::Vec3f me,
                  float reach) {
  bool short_of = st.rockets < 20 || st.shells < 40 || st.nails < 60 || st.cells < 30;
  short_of |= (st.items & (kWRockets | kWGrenades)) == 0;
  if (!short_of) return -1;
  int32_t best = -1;
  float best_d = reach;
  for (const sh::ViewEntity& v : f.bot.view.entities) {
    if (v.modelindex <= 0 || static_cast<size_t>(v.modelindex) >= s.models.size() ||
        s.models[static_cast<size_t>(v.modelindex)] != "progs/backpack.mdl")
      continue;
    const float d = dist(v.origin, me);
    if (d < best_d) {
      best_d = d;
      best = v.ent;
    }
  }
  return best;
}

// The callout for an enemy at `there`: by the landmark nearest it that the player knows (a key item
// or a weapon: "enemy at RA", "enemy at RL"), the key items counted nearer; empty with none near.
std::string enemy_call(const Fighter& f, sh::Vec3f there) {
  int32_t best = -1;
  float best_d = kCallNear;
  for (size_t i = 0; i < f.items.size(); i++) {
    const ItemSpot& it = f.items[i];
    if (!it.known || (!key_item(it) && it.kind != ItemKind::weapon)) continue;
    const float d = dist(it.at, there) * (key_item(it) ? 0.5f : 1.f);
    if (d < best_d) {
      best_d = d;
      best = static_cast<int32_t>(i);
    }
  }
  return best < 0 ? std::string()
                  : std::string("enemy at ") + item_call(f.items[static_cast<size_t>(best)]);
}

// The role's goal (team.hpp), true when it set one; false leaves the goal to the items.
bool team_goal(Fighter& f, const server::QcServer& s, const Stats& st, sh::Vec3f me, double t,
               const sh::Contact* c, ai::Why* why) {
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  // A backpack close by is had first, unless retreating with someone about.
  if (f.mode != FightMode::retreat) {
    const int32_t pack = pack_near(f, s, st, me, 400);
    if (pack >= 0) {
      if (pack != f.pack_ent) f.stats.packs++;
      f.pack_ent = pack;
      go_to(f, sh::nav_nearest(g, s.sv.edicts[static_cast<size_t>(pack)].origin));
      return true;
    }
  }
  if (!f.team || f.mode == FightMode::retreat) return false;
  const Team& tm = *f.team;
  const auto node_of = [&](int32_t i) { return f.items[static_cast<size_t>(i)].node; };
  const auto can_reach = [&](int32_t i) {
    return i >= 0 && (f.reach_at < 0 || f.reach.reached(node_of(i)));
  };
  if (!can_reach(tm.hold_item) && f.role != Role::rush) return false;
  const auto at = [&](int32_t i) { return f.items[static_cast<size_t>(i)].at; };
  const auto note = [&](const char* role, const char* because) {
    if (!why) return;
    why->decide("role_goal");
    why->choose(role, because);
  };
  switch (f.role) {
    case Role::hold: {
      if (tm.hold_item < 0 || strength(st) < kHoldStrong) return false;
      // He chases out of his area no further than the leash; else he stands on it.
      if (f.mode == FightMode::chase && c && dist(at_of(c->seen), at(tm.hold_item)) < kLeash) {
        go_to(f, sh::nav_nearest(g, at_of(c->seen)));
        note("hold", "chasing within the leash");
        return true;
      }
      // The armour gone and back soon, or there: wait on it; else the items near it.
      const int32_t i = best_item(f, s, st, me, t, false, nullptr, nullptr);
      if (i >= 0 && dist(at(i), at(tm.hold_item)) < kLeash * 0.6f) {
        go_item(f, s, i);
      } else {
        go_item(f, s, tm.hold_item);
      }
      note("hold", "on the held area");
      return true;
    }
    case Role::rush:
      if (!can_reach(tm.quad_item)) return false;
      go_item(f, s, tm.quad_item);
      note("rush", "the quad is due: waiting on it");
      return true;
    case Role::carry:
      if (tm.enemy_seen >= 0 && t - tm.enemy_seen < 6) {
        go_to(f, sh::nav_nearest(g, tm.enemy_at));
        note("carry", "the quad to the enemy last called out");
        return true;
      }
      if (tm.hold_item < 0) return false;
      go_to(f, node_of(tm.hold_item));
      note("carry", "no enemy called out: through the held area");
      return true;
    case Role::free:
      // A teammate's call for help is answered by a player strong enough to give it.
      if (f.mode != FightMode::retreat && t < tm.help_until && strength(st) >= 120 &&
          dist(tm.help_at, me) < 1500) {
        go_to(f, sh::nav_nearest(g, tm.help_at));
        note("free", "a teammate called for help");
        return true;
      }
      // A teammate's callout is chased when this player is strong and has nothing in sight.
      if (f.mode == FightMode::roam && tm.enemy_seen >= 0 && t - tm.enemy_seen < 3 &&
          strength(st) >= 150 && (st.items & kWRockets) != 0 && dist(tm.enemy_at, me) < 1200) {
        go_to(f, sh::nav_nearest(g, tm.enemy_at));
        note("free", "a teammate's callout, strong enough to help");
        return true;
      }
      return false;
  }
  return false;
}

void think(Fighter& f, const server::QcServer& s, int me, const Stats& st, ai::Why* why) {
  const double t = s.sv.time;
  const sh::Edict& self = s.sv.edicts[static_cast<size_t>(me)];
  const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};

  // Where it can get to from here, worked out once a second (a teleporter or a drop is one way).
  if (f.reach_at < 0 || t - f.reach_at >= 1) {
    const sh::NavGraph& g = *f.bot.pilot.pilot.g;
    f.reach_n = sh::nav_reach(g, sh::nav_nearest(g, self.origin), f.reach);
    f.reach_at = t;
  }
  bool escaping = false;  // on the way out of a pocket: no other goal this think
  // A dive is chosen again every think (go_item); one under way keeps the escape out of it.
  const bool was_diving = f.diving >= 0;
  // A dive once in the water is swum to its end: no other goal turns it round under water.
  const int32_t committed = was_diving && self.waterlevel >= 1 ? f.diving : -1;
  f.diving = -1;
  // Off the graph (knocked onto a ledge no proof stands on): back to the nearest node by sight
  // of the plan, below rather than above, walked straight at with a hop now and then.
  {
    const sh::NavGraph& g = *f.bot.pilot.pilot.g;
    f.lost_to = -1;
    // Where it stands reaches only a small share of the map (a pool or a pit the links prove no
    // swim or climb out of): it heads for the nearest node outside what it can reach, swimming up
    // as well as across.
    const bool pool =
        !was_diving && f.reach_at >= 0 && static_cast<size_t>(f.reach_n) * 4 < g.nodes.size();
    const bool swim = self.waterlevel >= 2;
    sh::Vec3f way;
    f.swimming = !was_diving && f.swim && (self.waterlevel >= 1 || pool) &&
                 (pool || sh::nav_nearest(g, self.origin) < 0) &&
                 sh::swim_next(*f.swim, g, self.origin, way);
    if (f.swimming) {
      f.lost++;
    } else {
      // Dry in a pocket whose way out is under water: into the water at its nearest way out, by a
      // route when one leads there.
      const auto wet_node = [&](size_t n) {
        sh::Vec3f to;
        return sh::swim_next(*f.swim, g, g.nodes[n].origin, to);
      };
      if (pool && f.swim) {
        int32_t pick = -1;
        float best = 0;
        for (size_t n = 0; n < g.nodes.size(); n++) {
          if (!f.reach.reached(static_cast<int32_t>(n)) || !wet_node(n)) continue;
          const float d = dist(g.nodes[n].origin, self.origin);
          if (pick < 0 || d < best) {
            pick = static_cast<int32_t>(n);
            best = d;
          }
        }
        if (pick >= 0) {
          f.lost++;
          f.goal_item = -1;
          go_to(f, pick);
          escaping = true;
        }
      }
      if (!escaping && !was_diving && (pool || sh::nav_nearest(g, self.origin) < 0)) {
        f.lost++;
        // The nearest few by distance; the first of them in a clear line is walked or swum at (out
        // of a pocket with water: the nearest nodes over water).
        constexpr int kLook = 64;
        int32_t near[kLook];
        float near_d[kLook];
        int nn = 0;
        for (size_t n = 0; n < g.nodes.size(); n++) {
          const sh::Vec3f d = g.nodes[n].origin - self.origin;
          if (pool && f.reach.reached(static_cast<int32_t>(n))) continue;
          if (pool && f.swim && !wet_node(n)) continue;
          if (d.z > (swim || pool ? 240 : 48)) continue;
          const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
          if (nn == kLook && dd >= near_d[kLook - 1]) continue;
          int k = nn < kLook ? nn++ : kLook - 1;
          for (; k > 0 && near_d[k - 1] > dd; k--) {
            near[k] = near[k - 1];
            near_d[k] = near_d[k - 1];
          }
          near[k] = static_cast<int32_t>(n);
          near_d[k] = dd;
        }
        for (int k = 0; k < nn && f.lost_to < 0; k++) {
          const sh::HullTrace tr = sh::trace_move(s.sv, self.origin, {-16, -16, -24}, {16, 16, 32},
                                                  g.nodes[static_cast<size_t>(near[k])].origin,
                                                  sh::kTraceNoMonsters, me);
          if (tr.fraction >= 1) f.lost_to = near[k];
        }
        if (f.lost_to < 0 && nn > 0) f.lost_to = near[0];
        if (!pool) f.reach_at = -1;  // worked out again once back on
      }
    }
  }

  // The items, as far as it can see them (knowledge.hpp); a key item taken is called out.
  const ItemNews news = knowledge_update(f, s, me, eye, t);
  if (news.taken >= 0) {
    const ItemSpot& it = f.items[static_cast<size_t>(news.taken)];
    if (key_item(it) && news.by_me) {
      team_say(f, t, std::string("took ") + item_call(it));
    } else if (key_item(it) && it.timed) {
      team_say(
          f, t,
          std::string(item_call(it)) + " in " + std::to_string(static_cast<int>(it.respawn_s)));
    }
  }

  const sh::Contact* c = sh::freshest(f.bot.senses, eye);
  const bool seen_now = c && c->visible;
  const bool recent = c && ai::known(c->seen) && ai::age(c->seen, t) <= kChase_s;
  const sh::Vec3f enemy = c ? at_of(ai::known(c->seen) ? c->seen : c->heard) : sh::Vec3f{};
  const float range = c ? dist(eye, enemy) : 400;

  // The mode.
  const float strong = strength(st);
  const bool weak = f.mode == FightMode::retreat ? strong < kStrongAgain : strong < kRetreatBelow;
  const int32_t heal_at =
      c && weak ? best_item(f, s, st, self.origin, t, true, &enemy, nullptr) : -1;
  if (heal_at >= 0 && item_value(f.items[static_cast<size_t>(heal_at)], st) > 0) {
    if (f.mode != FightMode::retreat && f.team && team_say(f, t, "need help")) {
      f.team->help_at = self.origin;
      f.team->help_until = t + kHelp_s;
    }
    set_mode(f, FightMode::retreat, why, "weak with someone about: health or armour first");
  } else if (seen_now) {
    set_mode(f, FightMode::fight, why, "a contact in sight");
  } else if (recent) {
    set_mode(f, FightMode::chase, why, "lost from sight a moment ago");
  } else {
    set_mode(f, FightMode::roam, why, "no one known");
  }

  // A sighting is the team's once it is called out (where, by the item it is nearest).
  if (f.team && seen_now) {
    const sh::Vec3f there = at_of(c->seen);
    // Called out unless a teammate just called the same place.
    const bool called = f.team->enemy_seen >= 0 && t - f.team->enemy_seen < kCallAgain_s &&
                        dist(f.team->enemy_at, there) < 400;
    const std::string words = enemy_call(f, there);
    if (!called && !words.empty() && t - f.called_at >= kCallAgain_s && team_say(f, t, words))
      f.called_at = t;
    if (t - f.called_at < kCallAgain_s) {
      if (f.team->enemy_seen < t) f.team->stats.callouts++;
      f.team->enemy_at = there;
      f.team->enemy_seen = t;
    }
  }

  // The goal: the role's first, then the items.
  if (committed >= 0) {
    // Kept on the dive's field whatever the reach from the water says (the nearest node under
    // water may stand in the pocket already).
    f.diving = f.last_dive = committed;
  } else if (!escaping && !team_goal(f, s, st, self.origin, t, c, why)) {
    if (f.mode == FightMode::chase && c) {
      go_to(f, sh::nav_nearest(*f.bot.pilot.pilot.g, at_of(c->seen)));
    } else {
      const bool heal = f.mode == FightMode::retreat;
      const int32_t i = best_item(f, s, st, self.origin, t, heal, c ? &enemy : nullptr, why);
      if (i >= 0) {
        if (f.team) f.team->claimed[static_cast<size_t>(i)] = f.slot;
        go_item(f, s, i);
      } else if (f.sense == MapSense::novice && (!f.team || f.team->hold_item < 0)) {
        // Nothing in mind: a novice walks the map to learn it.
        go_to(f, explore_node(f, self.origin));
      } else if (f.team && f.team->hold_item >= 0 &&
                 f.reach.reached(f.items[static_cast<size_t>(f.team->hold_item)].node)) {
        // Nothing worth having: back to the held area, beside the holder.
        go_to(f, f.items[static_cast<size_t>(f.team->hold_item)].node);
      }
    }
  }

  // A goal no way leads to any more (the way there was a drop, or a teleporter): the nearest item
  // there is a way to and that is not underfoot, so the player never stands planning in vain.
  if (!escaping && f.lost_to < 0 && f.reach_at >= 0 &&
      (f.goal_node < 0 || !f.reach.reached(f.goal_node))) {
    int32_t pick = -1;
    float best = 0;
    for (size_t i = 0; i < f.items.size(); i++) {
      const ItemSpot& it = f.items[i];
      if (it.node < 0 || !f.reach.reached(it.node) || !item_in_mind(f, static_cast<int32_t>(i)))
        continue;
      const float d = dist(it.at, self.origin);
      if (d < 200) continue;
      if (pick < 0 || d < best) {
        pick = static_cast<int32_t>(i);
        best = d;
      }
    }
    if (pick >= 0) {
      go_item(f, s, pick);
    } else {
      go_to(f, explore_node(f, self.origin));  // nothing it knows of: it walks to learn the map
    }
  }

  if (f.diving < 0) f.last_dive = -1;

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

// A living teammate within kFireClearance of the line from `eye` to `target`, nearer than it.
bool mate_in_line(const Fighter& f, const server::QcServer& s, sh::Vec3f eye, sh::Vec3f target) {
  if (!f.team) return false;
  const sh::Vec3f line = target - eye;
  const float len2 = line.x * line.x + line.y * line.y + line.z * line.z;
  if (len2 < 1) return false;
  for (const Fighter* m : f.team->members) {
    if (m == &f || m->slot < 0 || !alive_slot(s, m->slot)) continue;
    const sh::Vec3f p = s.sv.edicts[static_cast<size_t>(m->slot) + 1].origin;
    const sh::Vec3f to = p - eye;
    const float u = (to.x * line.x + to.y * line.y + to.z * line.z) / len2;
    if (u <= 0 || u >= 1) continue;
    const sh::Vec3f off = to - line * u;
    if (off.x * off.x + off.y * off.y + off.z * off.z < kFireClearance * kFireClearance)
      return true;
  }
  return false;
}

}  // namespace

float fighter_strength(const Stats& st) {
  return st.health + protection(st.armortype, st.armor) + ((st.items & kWRockets) ? 50.f : 0.f);
}

const char* role_name(Role r) {
  switch (r) {
    case Role::free:
      return "free";
    case Role::hold:
      return "hold";
    case Role::rush:
      return "rush";
    case Role::carry:
      return "carry";
  }
  return "?";
}

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

// The node a button is pressed from: the nearest within reach whose line to the button meets the
// button itself, so a node behind the wall the button is set in is never chosen.
static int button_node(const sh::NavGraph& g, const server::QcServer& s, int button, sh::Vec3f at,
                       float reach) {
  int best = -1;
  float best_d = reach;
  for (size_t n = 0; n < g.nodes.size(); n++) {
    const sh::Vec3f o = g.nodes[n].origin;
    const float d = dist(o, at);
    if (d >= best_d) continue;
    const sh::Vec3f eye{o.x, o.y, o.z + 22};
    const sh::HullTrace tr = sh::trace_move(s.sv, eye, {}, {}, at, sh::kTraceNoMonsters, -1);
    if (tr.fraction < 1 && tr.ent != button) continue;
    best = static_cast<int>(n);
    best_d = d;
  }
  return best >= 0 ? best : sh::nav_nearest(g, at);
}

void fighter_init(Fighter& f, const sh::NavGraph& g, const server::QcServer& s, int slot,
                  const char* entities, sh::AimSkill skill, double hz, uint32_t seed) {
  sh::bot_init(f.bot, g, sh::PilotSettings{}, sh::PilotHands::human, skill, hz, seed);
  f.slot = slot;
  f.sense = map_sense_of(skill);
  f.rng = seed * 2654435761u + 12345u;
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
  // Doors a button opens, and the items behind them.
  const auto entity_on = [&](const char* model) {
    for (int e = s.sv.set.maxclients + 1; e < s.sv.num_edicts; e++) {
      const sh::Edict& d = s.sv.edicts[static_cast<size_t>(e)];
      if (!d.free && d.modelindex > 0 && static_cast<size_t>(d.modelindex) < s.models.size() &&
          s.models[static_cast<size_t>(d.modelindex)] == model)
        return e;
    }
    return -1;
  };
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    const char* target = pg::block_value(b, "target");
    const char* model = pg::block_value(b, "model");
    if (!cn || std::strcmp(cn, "func_button") != 0 || !target || !model) continue;
    const int button = entity_on(model);
    if (button < 0) continue;
    for (const pg::EntityBlock& d : blocks) {
      const char* dc = pg::block_value(d, "classname");
      const char* dn = pg::block_value(d, "targetname");
      const char* dm = pg::block_value(d, "model");
      if (!dc || std::strcmp(dc, "func_door") != 0 || !dn || std::strcmp(dn, target) != 0 || !dm)
        continue;
      const int door = entity_on(dm);
      if (door < 0) continue;
      const sh::Edict& de = s.sv.edicts[static_cast<size_t>(door)];
      const sh::Edict& be = s.sv.edicts[static_cast<size_t>(button)];
      const sh::Vec3f mid{(de.absmin.x + de.absmax.x) * 0.5f, (de.absmin.y + de.absmax.y) * 0.5f,
                          (de.absmin.z + de.absmax.z) * 0.5f};
      for (ItemSpot& it : f.items) {
        if (dist(it.at, mid) > 160) continue;
        it.door_ent = door;
        it.door_rest = de.origin;
        it.button_at = {(be.absmin.x + be.absmax.x) * 0.5f, (be.absmin.y + be.absmax.y) * 0.5f,
                        (be.absmin.z + be.absmax.z) * 0.5f};
        it.button_shoot = pg::block_value(b, "health") != nullptr;
        it.button_ent = button;
        it.button_node = button_node(g, s, button, it.button_at, it.button_shoot ? 900.f : 256.f);
      }
    }
  }
  knowledge_init(f);
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

  // The last step onto an item: the nav's node beside it is not the item, so once the pilot has
  // arrived the fighter walks the rest of the way straight at it (the axes under the view).
  if (!dead && f.bot.pilot.pilot.mode == sh::PilotMode::arrived && f.goal_item >= 0) {
    const ItemSpot& it = f.items[static_cast<size_t>(f.goal_item)];
    const sh::Vec3f me_at = s.sv.edicts[static_cast<size_t>(me)].origin;
    const sh::Vec3f to = f.unlocking ? it.button_at : it.at;
    const double dx = to.x - me_at.x, dy = to.y - me_at.y;
    const double reach = f.unlocking ? 160 : 96;
    const bool on_it = f.goal_node == (f.unlocking ? it.button_node : it.node);
    if (on_it && (it.present || f.unlocking) && dx * dx + dy * dy > 12 * 12 &&
        dx * dx + dy * dy < reach * reach) {
      const double rel = ge::core::libm::atan2(dy, dx) - cmd.angles.y * (ge::core::kPi / 180.0);
      cmd.forward = sh::quantize_axis(static_cast<float>(ge::core::libm::cos(rel) * 320));
      cmd.side = sh::quantize_axis(static_cast<float>(-ge::core::libm::sin(rel) * 320));
    }
  }

  // A button opened by a shot: from where the line meets it, the view on it and the trigger.
  if (!dead && f.unlocking && f.goal_item >= 0) {
    const ItemSpot& it = f.items[static_cast<size_t>(f.goal_item)];
    const sh::Vec3f me_at = s.sv.edicts[static_cast<size_t>(me)].origin;
    const sh::Vec3f eye{me_at.x, me_at.y, me_at.z + 22};
    const sh::Vec3f d = it.button_at - eye;
    const double flat = std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y);
    if (it.button_shoot && flat < 900) {
      const sh::HullTrace tr =
          sh::trace_move(s.sv, eye, {}, {}, it.button_at, sh::kTraceNoMonsters, me);
      if (tr.fraction >= 1 || tr.ent == it.button_ent) {
        cmd.angles.y =
            static_cast<float>(ge::core::libm::atan2(d.y, d.x) * (180.0 / ge::core::kPi));
        cmd.angles.x =
            static_cast<float>(-ge::core::libm::atan2(d.z, flat) * (180.0 / ge::core::kPi));
        cmd.forward = 0;
        cmd.side = 0;
        if (f.stats.commands & 1) cmd.buttons |= sh::kButtonAttack;
      }
    }
  }

  sh::Vec3f way;
  const sh::Edict& body = s.sv.edicts[static_cast<size_t>(me)];
  // Its air: the head under this long, it takes whichever way out of the water is shorter, the
  // dive's on or the way back, so it never drowns on a swim.
  if (body.waterlevel >= 3) {
    if (f.under_since < 0) f.under_since = t;
  } else {
    f.under_since = -1;
  }
  const bool short_of_air = f.under_since >= 0 && t - f.under_since > kAir_s;
  const ItemSpot* dive_for = f.diving >= 0 ? &f.items[static_cast<size_t>(f.diving)] : nullptr;
  // A walk along a pool's floor turns up and out by the field at kAir_s. A dive is left to finish:
  // none is planned longer than kDiveMost_s, so its way on is the way to air.
  const bool on_dive = dive_for && dive_for->dive;
  if (short_of_air && f.swim && !on_dive && sh::swim_steps(*f.swim, body.origin) >= 0)
    f.swimming = true;
  // Diving: in the water, or at the dive's node (arrived, or within a step of it).
  const bool dive_now =
      dive_for && dive_for->dive &&
      (body.waterlevel >= 1 ||
       (f.goal_node == dive_for->dive_node &&
        (f.bot.pilot.pilot.mode == sh::PilotMode::arrived ||
         dist(body.origin,
              f.bot.pilot.pilot.g->nodes[static_cast<size_t>(dive_for->dive_node)].origin) <
             48))) &&
      (body.waterlevel >= 1
           ? sh::swim_next(*dive_for->dive, *f.bot.pilot.pilot.g, body.origin, way)
           : dive_for->dive_entry >= 0 &&
                 (way =
                      f.bot.pilot.pilot.g->nodes[static_cast<size_t>(dive_for->dive_entry)].origin,
                  true));
  if (!dead && (dive_now ||
                (f.swimming && sh::swim_next(*f.swim, *f.bot.pilot.pilot.g, body.origin, way)))) {
    // Swimming the field's way out: the view on the next cell (pitch down positive), forward,
    // and jump held, which rises in water and climbs out over the edge at the top.
    const sh::Vec3f me_at = s.sv.edicts[static_cast<size_t>(me)].origin;
    const double dx = way.x - me_at.x, dy = way.y - me_at.y, dz = way.z - me_at.z;
    const double flat = std::sqrt(dx * dx + dy * dy);
    cmd.angles.y = static_cast<float>(ge::core::libm::atan2(dy, dx) * (180.0 / ge::core::kPi));
    cmd.angles.x = static_cast<float>(-ge::core::libm::atan2(dz, flat) * (180.0 / ge::core::kPi));
    cmd.forward = sh::quantize_axis(320);
    cmd.side = 0;
    if (dz > 8) cmd.buttons |= sh::kButtonJump;
  } else if (!dead && f.lost_to >= 0) {
    const sh::Vec3f to = f.bot.pilot.pilot.g->nodes[static_cast<size_t>(f.lost_to)].origin;
    const sh::Vec3f me_at = s.sv.edicts[static_cast<size_t>(me)].origin;
    const double rel = ge::core::libm::atan2(to.y - me_at.y, to.x - me_at.x) -
                       cmd.angles.y * (ge::core::kPi / 180.0);
    cmd.forward = sh::quantize_axis(static_cast<float>(ge::core::libm::cos(rel) * 320));
    cmd.side = sh::quantize_axis(static_cast<float>(-ge::core::libm::sin(rel) * 320));
    const sh::Edict& self = s.sv.edicts[static_cast<size_t>(me)];
    if (self.waterlevel >= 2) {
      // Swimming: the view pitched at it (down is positive) and jump held to rise.
      const double flat =
          std::sqrt((to.x - me_at.x) * (to.x - me_at.x) + (to.y - me_at.y) * (to.y - me_at.y));
      const double up = to.z + 16 - me_at.z;
      cmd.angles.x = static_cast<float>(-ge::core::libm::atan2(up, flat) * (180.0 / ge::core::kPi));
      cmd.buttons |= sh::kButtonJump;
    } else if ((f.stats.commands / 48) % 3 == 0) {
      cmd.buttons |= sh::kButtonJump;
    }
  }

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
    bool fire = d <= held->range && sh::bot_aim_off(f.bot) <= tol;
    if (fire && mate_in_line(f, s, eye, at_of(c->seen))) {
      fire = false;
      f.stats.held_fire++;
      if (f.team) f.team->stats.held_shots++;
    }
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
