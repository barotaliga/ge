// WHAT AN ITEM IS WORTH AND WHICH ONE IS GONE FOR (fighter_parts.hpp): an item's worth to a
// player holding what he holds, what he holds once he has taken it, a door shut on it, the best
// item over the time to have it, the goal after it on a chain, a short goal on the way and a
// backpack near.
//
// ge:summary The fighter's items: worth, the best over the time to have it, chains, packs near
// ge:keywords fighter item-worth raw_value item_value heals after_taking best_item chain_after
// ge:keywords on_the_way pack_near door_shut item-goal heal-goal short-goal backpack
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#include <cmath>
#include <limits>

#include "ai/choose.hpp"
#include "ai/memory.hpp"
#include "shooter/nav_field.hpp"
#include "shooter/senses.hpp"
#include "shooters_edge/bots/bot_arsenal.hpp"
#include "shooters_edge/bots/fighter_parts.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/map_plan.hpp"
#include "shooters_edge/bots/mind.hpp"
#include "shooters_edge/bots/team.hpp"
#include "shooters_edge/bots/water.hpp"

namespace shooters_edge::bots::fighter_parts {

namespace {

namespace ai = ge::ai;

// The baseline mod's weapon bits (games/shooters_edge/rules/common_decl.hpp) and ammunition maxima
// (common_items.cpp).
constexpr float kAmmoMax = 100;
constexpr int32_t kItQuad = 4194304;  // the quad's IT_ bit
constexpr int32_t kItPent = 1048576;  // the shield's
constexpr double kTired = 0.2;        // its worth meanwhile, as a share
constexpr double kWait_s = 20;        // an item back within this is worth going to wait for
constexpr double kHealWait_s = 3;     // but a retreat goes for health that is there
constexpr double kKeepGoal = 1.5;     // the goal's score is held up by this against the others
constexpr float kRun = 320;           // units/s, the run to an item
constexpr double kTold_s = 10;        // a point at an item is followed this long
// A hurt player does not walk back into a fight his side is losing: health within this of a danger
// spot (where he died, where a mate died, an area called lost, a call for help: mind.hpp) is worth
// this share while he heals, and health an enemy he knows of now stands nearer to than he does is
// worth half.
constexpr double kDangerMemory_s = 40;
constexpr float kDangerReach = 384;
constexpr double kDangerScale = 0.25;
constexpr double kEnemyNearScale = 0.5;
constexpr double kEnemyKnown_s = 3;  // an enemy seen or heard this lately is known of now
constexpr double kHotSpot_s = 60;    // two danger spots this lately near an item: no healing there
constexpr double kMegaHealBonus = 10;   // the mega's worth to a healer over the health it gives
constexpr double kBaseHeal = 1.4;       // health in the team's base is worth this more to a healer
constexpr double kLauncherWorth = 240;  // the launcher lacked: worth a long run
constexpr double kLobWorth = 80;        // (`lobtake`) the lob lacked, the launcher carried
constexpr float kRocketsLow = 25;       // a launcher with fewer rockets than this wants more
constexpr float kRocketsStock = 50;     // rockets stocked with no launcher yet, up to this
constexpr double kRocketsStockWorth = 15;  // and worth this from none
constexpr float kOnTheWayRise = 40;        // a short goal's floor within this of the player's
constexpr float kFeet = 24;                // a player's origin over the floor
constexpr float kBeside = 96;              // nearer than this, any way it lies
constexpr float kAhead = 0.34f;            // else the cosine off its motion, at least

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

sh::Vec3f at_of(const ai::Memory& m) {
  return {static_cast<float>(m.x), static_cast<float>(m.y), static_cast<float>(m.z)};
}

// What item `it` is worth to a player with `st` now (0: nothing).
double raw_value(const ItemSpot& it, const Stats& st) {
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
      // The launcher wins a team game (people take it 60 times in 20 min): worth a long run
      // while it is there, not a long wait while it is away.
      if (it.weapon == st.arsenal->launcher) return it.present ? kLauncherWorth : 120;
      const BotWeapon* w = weapon_row(*st.arsenal, it.weapon);
      return w ? w->worth : 30;
    }
    default:
      break;
  }
  // Ammunition: worth what it fills of a weapon carried that uses it.
  const BotArsenal& arsenal = *st.arsenal;
  int a = arsenal.item_ammo(it.kind);
  if (a < 0) a = 0;
  bool uses = false;
  for (const BotWeapon& w : arsenal.weapons) uses |= w.ammo == a && (st.items & w.bit) != 0;
  const float have = ammo_of(st, a);
  // Rockets are stocked for the launcher still to come, as people stock them: worth something to
  // a player who has neither launcher yet.
  if (a == arsenal.launcher_ammo && !uses)
    return have >= kRocketsStock ? 1
                                 : std::fmax(1.0, kRocketsStockWorth * (1 - have / kRocketsStock));
  if (!uses || have >= kAmmoMax) return 1;
  const double fill = 30.0 * (kAmmoMax - have) / kAmmoMax;
  // Rockets for a launcher in hand running dry are worth half the launcher: it is no use without.
  if (a == arsenal.launcher_ammo && (st.items & arsenal.launcher) != 0 && have < kRocketsLow)
    return std::fmax(fill, 0.5 * kLauncherWorth * (1.0 - have / kRocketsLow));
  return fill;
}

bool heals(const ItemSpot& it) {
  return it.kind == ItemKind::health || it.kind == ItemKind::mega || it.kind == ItemKind::armor1 ||
         it.kind == ItemKind::armor2 || it.kind == ItemKind::armor3;
}

// What the player holds once it has taken `it` (`chains`: the goal after it chosen as if taken).
Stats after_taking(Stats st, const ItemSpot& it) {
  switch (it.kind) {
    case ItemKind::health:
      if (st.health < 100) st.health = std::fmin(100.f, st.health + it.amount);
      break;
    case ItemKind::mega:
      st.health = std::fmin(250.f, st.health + 100);
      break;
    case ItemKind::armor1:
    case ItemKind::armor2:
    case ItemKind::armor3: {
      const float type = it.kind == ItemKind::armor1   ? 0.3f
                         : it.kind == ItemKind::armor2 ? 0.6f
                                                       : 0.8f;
      const float value = it.kind == ItemKind::armor1   ? 100.f
                          : it.kind == ItemKind::armor2 ? 150.f
                                                        : 200.f;
      if (protection(type, value) > protection(st.armortype, st.armor)) {
        st.armortype = type;
        st.armor = value;
      }
      break;
    }
    case ItemKind::quad:
      st.items |= kItQuad;
      break;
    case ItemKind::pent:
      st.items |= kItPent;
      break;
    case ItemKind::weapon:
      st.items |= it.weapon;
      break;
    default: {
      // Ammunition: the arsenal's index of it.
      const int a = st.arsenal->item_ammo ? st.arsenal->item_ammo(it.kind) : -1;
      if (a >= 0 && a < kMaxAmmo) st.ammo[a] += it.amount;
      break;
    }
  }
  return st;
}

}  // namespace

// What an item is worth to it now, as it rates the map.
double item_value(const ItemSpot& it, const Stats& st) { return raw_value(it, st) * it.weight; }

bool door_shut(const ge::shooter::BotView& s, const ItemSpot& it) {
  if (it.door_ent < 0) return false;
  const sh::Vec3f d = s.world.entities[static_cast<size_t>(it.door_ent)].origin - it.door_rest;
  return d.x * d.x + d.y * d.y + d.z * d.z < 1;
}

// The best item for the player at `me` with `st`: worth over time to have it, `away` from a known
// enemy (when `heal`, health and armour alone, and never nearer the enemy than this player is).
int32_t best_item(const Fighter& f, const ge::shooter::BotView& s, const Stats& st, sh::Vec3f me,
                  double t, bool heal, const sh::Vec3f* enemy, ai::Why* why, int32_t skip) {
  const auto score = [&](int32_t i) {
    const ItemSpot& it = f.items[static_cast<size_t>(i)];
    if (i == skip || it.node < 0 || (heal && !heals(it)) || route_lost(f, i, t)) return 0.0;
    // Only what it knows of (or sees). One gone is waited for at its time back as the player
    // knows it: exact when timed, a guess (late) when not (knowledge.hpp).
    if (!item_in_mind(f, i)) return 0.0;
    if (f.trap && (*f.trap)[static_cast<size_t>(it.node)]) return 0.0;  // no way back out of it
    // No way there from here, unless by its dive (from a node there is a way to).
    // (`fields`) Its way read off the item's field: a door only a button opens is on it as an
    // errand, the run's time the field's.
    const bool fields = fields_on(f);
    const double field_run = fields ? node_run_s(f, it.node) : ai::kUnreached;
    const bool by_dive =
        fields ? field_run == ai::kUnreached : f.reach_at >= 0 && !f.reach.reached(it.node);
    if (by_dive && (!it.dive || !node_reached(f, it.dive_node) || it.dive_s > kDiveMost_s ||
                    (i == f.dive_off && t < f.dive_off_until)))
      return 0.0;
    if (!fields && door_shut(s, it) &&
        (it.button_node < 0 || (f.reach_at >= 0 && !f.reach.reached(it.button_node))))
      return 0.0;  // shut, and its button out of reach
    if (it.after >= 0 && f.features[sh::Feature::routes] && item_in_mind(f, it.after)) {
      // An item rule: not while the one to be had first is believed up or due.
      const ItemSpot& first = f.items[static_cast<size_t>(it.after)];
      if (first.present || first.back_at - t < it.after_due_s) return 0.0;
    }
    // The map's weights are a team's plan (what to hold); a weak player, or one alone, wants
    // what does him good soonest.
    double v = heal || !f.team ? raw_value(it, st) : item_value(it, st);
    // (`lobtake`) The lob, the launcher's second (people carry it 29 % of the time they carry the
    // launcher, 4 % without): worth kLobWorth to one with the launcher and not the lob.
    if (f.features[sh::Feature::lobtake] && !heal && it.kind == ItemKind::weapon &&
        it.weapon == f.arsenal->lob && f.arsenal->lob != 0 && (st.items & it.weapon) == 0 &&
        (st.items & f.arsenal->launcher) != 0)
      v = std::fmax(v, kLobWorth);
    // Healing, the mega health is worth the health it gives back to 100, as a box is: a hurt player
    // takes the health near him, not the room every hurt player of both teams runs to.
    if (heal && it.kind == ItemKind::mega) v = std::fmax(0.f, 100 - st.health) + kMegaHealBonus;
    if (v <= 0) return 0.0;
    // The team's base (the map plan's) is where a hurt player falls back to.
    if (heal && f.plan && f.plan->base >= 0 && plan_area_of(*f.plan, it.node) == f.plan->base)
      v *= kBaseHeal;
    if (heal) {  // where two or more of its side fell lately is not healed at at all
      int fell = 0;
      for (const auto& [where, when] : f.danger)
        fell += t - when < kHotSpot_s && dist(where, it.at) < kDangerReach ? 1 : 0;
      if (fell >= 2) return 0.0;
    }
    // (`routetime`, M18r.md 3) The run by the route's time (the reach's, worked out each second),
    // not the straight line: e1m2's YA is near in a line from below and 13 s by its stairs, and
    // as the route went round other items came nearer in a line: 6 of 371 trips took it.
    const bool by_route = f.features[sh::Feature::routetime] && f.reach_at >= 0 && it.node >= 0 &&
                          f.reach.reached(it.node);
    const double run = (fields && !by_dive ? field_run
                        : by_route         ? f.reach.cost(it.node)
                                           : dist(me, it.at) / kRun) +
                       (by_dive ? it.dive_s : 0);
    const double wait = it.present ? 0 : it.back_at - t;
    // Only the items a game turns on are waited for; the rest are taken when they are there.
    const bool key = key_item(it);
    if (wait > (heal || (!key && f.team) ? kHealWait_s : kWait_s)) return 0.0;
    double sc = v / (1 + std::fmax(run, wait));
    if (i == f.goal_item) sc *= kKeepGoal;  // the goal held unless another is clearly better
    if (i == f.tired_item && t < f.tired_until) sc *= kTired;  // gone for in vain lately
    if (enemy && (heal || f.outmatched) && dist(*enemy, it.at) < dist(me, it.at)) sc *= 0.2;
    // A teammate who said he goes for it has it; one in sight it is no use to, or the quad's
    // carrier, gets it; a strong player leaves health to the others; a point is followed.
    if (mate_going(f, i, t)) sc *= 0.15;
    if (gives_up(f, s, f.slot + 1, i, st, t)) return 0.0;
    if (f.team && heals(it) && it.kind != ItemKind::mega && strength(st) >= 150) sc *= 0.3;
    if (f.mind.told == i && t - f.mind.told_t < kTold_s) sc *= 4;
    // A weak player falls back to his own: health near his area is worth more.
    if (f.team && heal && f.mind.claim >= 0 &&
        dist(it.at, f.items[static_cast<size_t>(f.mind.claim)].at) < kLeash)
      sc *= 1.5;
    if (heal) {
      for (const auto& [where, when] : f.danger)
        if (t - when < kDangerMemory_s && dist(where, it.at) < kDangerReach) sc *= kDangerScale;
      for (const sh::Contact& k : f.bot.senses.contacts) {
        if (k.ent < 0) continue;
        const bool seen = ai::known(k.seen) && ai::age(k.seen, t) <= kEnemyKnown_s;
        const bool heard = ai::known(k.heard) && ai::age(k.heard, t) <= kEnemyKnown_s;
        if (!seen && !heard) continue;
        if (dist(at_of(seen ? k.seen : k.heard), it.at) < dist(me, it.at)) sc *= kEnemyNearScale;
      }
    }
    return sc;
  };
  const auto name = [&](int32_t i) {
    const ItemSpot& it = f.items[static_cast<size_t>(i)];
    return it.label.empty() ? it.name.c_str() : it.label.c_str();
  };
  return ai::pick_best(static_cast<int32_t>(f.items.size()), score, why,
                       heal ? "heal_goal" : "item_goal", name, "worth over the time to have it", 0);
}

// THE GOAL AFTER (`chains`): the item the player would go for once it has item `i`, chosen as
// it would choose then (from where `i` stands, holding what `i` gives); -1 with none, or `i` not
// there to take now (it waits for it there).
int32_t chain_after(const Fighter& f, const ge::shooter::BotView& s, const Stats& st, int32_t i,
                    double t, bool heal, const sh::Vec3f* enemy) {
  if (!f.features[sh::Feature::chains] || i < 0) return -1;
  const ItemSpot& it = f.items[static_cast<size_t>(i)];
  if (!it.present) return -1;
  return best_item(f, s, after_taking(st, it), it.at, t, heal, enemy, nullptr, i);
}

// A SHORT GOAL ON THE WAY: an item there now that a person turns aside for, on its own floor within
// kOnTheWay and ahead of where it runs (or beside it), that a way reaches, nobody of its side said
// he takes, and not the goal itself; the nearest, or -1. Taken before the long goal, as a person
// picks up what lies by his way: the goal is chosen again once it has it (a pickup is news).
const char* on_the_way_bar(const Fighter& f, const ge::shooter::BotView& s, const Stats& st,
                           sh::Vec3f me, sh::Vec3f velocity, double t, int32_t i) {
  const ItemSpot& it = f.items[static_cast<size_t>(i)];
  if (i == f.goal_item) return "the goal";
  if (!it.present || it.node < 0 || !item_in_mind(f, i)) return "not known there";
  if (route_lost(f, i, t)) return "its route lost";
  if (f.reach_at >= 0 && !f.reach.reached(it.node)) return "not reached";
  if (std::fabs(it.at.z - (me.z - kFeet)) > kOnTheWayRise) return "another floor";
  if (door_shut(s, it)) return "its door shut";
  // What a person turns aside for: health it lacks, better armour, a weapon it has not got,
  // rockets; shells and nails are taken as they lie on the way, not gone for.
  const bool ammo =
      it.kind == ItemKind::shells || it.kind == ItemKind::nails || it.kind == ItemKind::cells;
  if (ammo || (it.kind == ItemKind::weapon && (st.items & it.weapon) != 0)) return "not wanted";
  if (raw_value(it, st) <= 0) return "worth nothing now";
  if (mate_going(f, i, t)) return "a mate's";
  if (gives_up(f, s, f.slot + 1, i, st, t)) return "given up";
  const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
  const float d = dist(me, it.at);
  // Ahead of where it runs (within about 70 degrees of its motion), or beside it.
  if (d > kBeside && speed > 100) {
    const float dx = it.at.x - me.x, dy = it.at.y - me.y;
    const float flat = std::sqrt(dx * dx + dy * dy);
    if (flat > 1 && (dx * velocity.x + dy * velocity.y) / (flat * speed) < kAhead)
      return "behind its run";
  }
  return nullptr;
}

int32_t on_the_way(const Fighter& f, const ge::shooter::BotView& s, const Stats& st, sh::Vec3f me,
                   sh::Vec3f velocity, double t) {
  int32_t best = -1;
  float best_d = kOnTheWay;
  for (size_t k = 0; k < f.items.size(); k++) {
    const auto i = static_cast<int32_t>(k);
    if (on_the_way_bar(f, s, st, me, velocity, t, i) != nullptr) continue;
    const float d = dist(me, f.items[k].at);
    if (d < best_d) {
      best = i;
      best_d = d;
    }
  }
  return best;
}

// A backpack within `reach` worth going for (ammunition short or a weapon lacking), or -1: one
// in sight on its screen (no wall, door or water's surface between), or the one already gone for
// while it lies there.
int32_t pack_near(const Fighter& f, const ge::shooter::BotView& s, const Stats& st, sh::Vec3f me,
                  float reach) {
  const BotArsenal& arsenal = *f.arsenal;
  bool short_of = false;
  for (int k = 0; k < arsenal.ammo_count; k++) short_of |= st.ammo[k] < arsenal.short_of[k];
  short_of |= (st.items & (arsenal.launcher | arsenal.lob)) == 0;
  // A novice wants every pack he sees (`packinfer`), whatever he carries.
  short_of |= f.sense == MapSense::novice && f.features[sh::Feature::packinfer];
  if (!short_of) return -1;
  sh::SenseSettings look = f.bot.senses.set;
  look.range = reach;
  const sh::Vec3f eye{me.x, me.y, me.z + look.view_height};
  int32_t best = -1;
  float best_d = reach;
  for (const sh::ViewEntity& v : f.bot.view.entities) {
    if (v.modelindex <= 0 || static_cast<size_t>(v.modelindex) >= s.models.size() ||
        s.models[static_cast<size_t>(v.modelindex)] != "progs/backpack.mdl")
      continue;
    const float d = dist(v.origin, me);
    if (d >= best_d) continue;
    // A pack lies on the floor: seen at a few units over it (as a player's origin less his eye).
    if (v.ent != f.pack_ent &&
        !sh::sees_player(s.sv, look, f.slot + 1, eye, {f.bot.pitch, f.bot.yaw, 0},
                         {v.origin.x, v.origin.y, v.origin.z + 8 - look.view_height}))
      continue;
    best_d = d;
    best = v.ent;
  }
  return best;
}

}  // namespace shooters_edge::bots::fighter_parts

namespace shooters_edge::bots {

bool fields_on(const Fighter& f) {
  return f.features[sh::Feature::fields] && f.fields != nullptr && f.field_node >= 0;
}

bool node_reached(const Fighter& f, int32_t node) {
  if (node < 0) return false;
  if (fields_on(f)) return node_run_s(f, node) < ge::ai::kUnreached;
  return f.reach_at < 0 || f.reach.reached(node);
}

double node_run_s(const Fighter& f, int32_t node) {
  if (node < 0) return ge::ai::kUnreached;
  if (fields_on(f)) {
    const sh::NavField& fl = sh::nav_field(*f.fields, node, f.bot.pilot.pilot.set.jumper);
    const float s = sh::nav_field_time(*f.fields, fl, f.field_node);
    return s < std::numeric_limits<float>::infinity() ? static_cast<double>(s) : ge::ai::kUnreached;
  }
  return f.reach_at >= 0 && f.reach.reached(node) ? f.reach.cost(node) : ge::ai::kUnreached;
}

}  // namespace shooters_edge::bots
