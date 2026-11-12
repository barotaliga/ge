// A map's goals (goals.hpp): generated from its items, its plan and its tricks.
//
// ge:summary A map's goals from the map: kinds, targets, horizons, needs and worths, any map
// ge:keywords map_goals goals_text goal-catalogue
// ge:skill bots
// ge:test tests/shooters_edge_map_plan_test.cpp
#include "shooters_edge/bots/goals.hpp"

#include <algorithm>

#include "core/num_text.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/rocket_tricks.hpp"

namespace shooters_edge::bots {

namespace {

constexpr float kPowerHorizon_s = 30;  // a power's run, about
constexpr float kSpam_s = 5;           // a way in spammed this long at a time
constexpr float kPack_s = 20;          // a pointed pack lies about this long
constexpr float kShort_s = 10;         // a surface, a heal, a hunt: short goals

bool goal_item(const ItemSpot& it) {
  switch (it.kind) {
    case ItemKind::armor1:
    case ItemKind::armor2:
    case ItemKind::armor3:
    case ItemKind::mega:
    case ItemKind::quad:
    case ItemKind::pent:
      return true;
    case ItemKind::weapon:
      return it.role != WeaponRole::none;
    default:
      return false;
  }
}

bool timed_item(const ItemSpot& it) {
  return it.kind == ItemKind::armor2 || it.kind == ItemKind::armor3 || it.kind == ItemKind::mega ||
         it.kind == ItemKind::quad || it.kind == ItemKind::pent ||
         (it.kind == ItemKind::weapon && it.role == WeaponRole::launcher);
}

// An area's name: its place's, numbered when another area has the same ("ya", "ya 2").
std::string area_name(const MapPlan& plan, int32_t index) {
  const PlanArea& a = plan.areas[static_cast<size_t>(index)];
  if (a.name.empty()) return "area " + std::to_string(index);
  int same = 0, before = 0;
  for (size_t k = 0; k < plan.areas.size(); k++)
    if (plan.areas[k].name == a.name) {
      same++;
      if (static_cast<int32_t>(k) < index) before++;
    }
  return same > 1 && before > 0 ? a.name + " " + std::to_string(before + 1) : a.name;
}

}  // namespace

const char* goal_kind_name(GoalKind k) {
  static const char* const kNames[kGoalKinds] = {"take",  "time",   "hold",    "retake", "clear",
                                                 "rush",  "escort", "arm",     "spam",   "trick",
                                                 "fetch", "drop",   "surface", "heal",   "hunt"};
  return kNames[static_cast<int>(k)];
}

const char* horizon_name(Horizon h) {
  switch (h) {
    case Horizon::match:
      return "match";
    case Horizon::until_back:
      return "until back";
    case Horizon::power:
      return "a power's run";
    case Horizon::seconds:
      return "seconds";
  }
  return "match";
}

std::string needs_text(uint32_t needs) {
  static const char* const kNames[kNeeds] = {"present",    "taken",    "lost",      "power",
                                             "mate-power", "due",      "lacking",   "enemy-near",
                                             "blast",      "pointed",  "launcher",  "weak",
                                             "under",      "stronger", "enemy-seen"};
  std::string out;
  for (int k = 0; k < kNeeds; k++)
    if ((needs >> k) & 1u) out += (out.empty() ? "" : "+") + std::string(kNames[k]);
  return out.empty() ? "-" : out;
}

std::vector<Goal> map_goals(const MapPlan& plan, const std::vector<ItemSpot>& items,
                            const std::string& map) {
  std::vector<Goal> out;
  const auto add = [&](Goal g) { out.push_back(std::move(g)); };
  // The items' own: take, time, arm.
  bool power = false;
  for (size_t i = 0; i < items.size(); i++) {
    const ItemSpot& it = items[i];
    if (it.node < 0 || !goal_item(it)) continue;
    const std::string call = item_call(it);
    const double worth = plan_worth(it);
    power = power || it.kind == ItemKind::quad || it.kind == ItemKind::pent;
    const bool weapon = it.kind == ItemKind::weapon;
    if (!weapon)  // a weapon's taking is its arming (below)
      add({GoalKind::take, static_cast<int32_t>(i), -1, -1, -1, Horizon::until_back, 0,
           kNeedPresent, worth, "take " + call});
    if (timed_item(it))
      add({GoalKind::time, static_cast<int32_t>(i), -1, -1, -1, Horizon::until_back, 0, kNeedTaken,
           worth * 0.8, "time " + call});
    if (weapon)
      add({GoalKind::arm, static_cast<int32_t>(i), -1, -1, -1, Horizon::until_back, 0,
           kNeedLacking | kNeedPresent, worth, "arm " + call});
  }
  // The areas': hold, retake; with a power on the map, clear; the powers' rush and escort.
  for (size_t a = 0; a < plan.areas.size(); a++) {
    const PlanArea& ar = plan.areas[a];
    const std::string name = area_name(plan, static_cast<int32_t>(a));
    if (ar.power) {
      add({GoalKind::rush, ar.items.empty() ? -1 : ar.items[0], static_cast<int32_t>(a), -1, -1,
           Horizon::until_back, 0, kNeedDue, ar.worth, "rush " + name});
      add({GoalKind::escort, ar.items.empty() ? -1 : ar.items[0], static_cast<int32_t>(a), -1, -1,
           Horizon::power, kPowerHorizon_s, kNeedMatePower, ar.worth * 0.3, "escort " + name});
      continue;
    }
    if (!ar.reached) continue;  // no way there from most spawns: taken on the way, never held
    add({GoalKind::hold, -1, static_cast<int32_t>(a), -1, -1, Horizon::match, 0, 0, ar.hold,
         "hold " + name});
    add({GoalKind::retake, -1, static_cast<int32_t>(a), -1, -1, Horizon::seconds, 30, kNeedLost,
         ar.hold * 1.2, "retake " + name});
    if (power)
      add({GoalKind::clear, -1, static_cast<int32_t>(a), -1, -1, Horizon::power, kPowerHorizon_s,
           kNeedPower | kNeedLost, ar.worth * 1.5, "clear " + name});
  }
  // The base's ways in: spam each.
  if (plan.base >= 0) {
    const PlanArea& base = plan.areas[static_cast<size_t>(plan.base)];
    for (size_t e = 0; e < base.entries.size(); e++)
      add({GoalKind::spam, -1, plan.base, base.entries[e], -1, Horizon::seconds, kSpam_s,
           kNeedEnemyNear | kNeedBlast, base.hold * 0.2,
           "spam " + area_name(plan, plan.base) + " #" + std::to_string(e + 1)});
  }
  // The map's tricks, as means a route may ask for.
  const auto tricks = rocket_tricks(map);
  for (size_t k = 0; k < tricks.size(); k++)
    add({GoalKind::trick, -1, -1, -1, static_cast<int32_t>(k), Horizon::seconds, 3, kNeedLauncher,
         0, "trick " + std::to_string(k + 1) + (tricks[k].flip ? " (flip)" : " (rocket jump)")});
  // Every map's short goals.
  add({GoalKind::fetch, -1, -1, -1, -1, Horizon::seconds, kPack_s, kNeedPointed, 150,
       "fetch pack"});
  if (power)
    add({GoalKind::drop, -1, -1, -1, -1, Horizon::seconds, 3,
         kNeedMatePower | kNeedLauncher | kNeedWeak, 120, "drop rl for the quad"});
  add({GoalKind::surface, -1, -1, -1, -1, Horizon::seconds, kShort_s, kNeedUnder, 1000, "surface"});
  add({GoalKind::heal, -1, -1, -1, -1, Horizon::seconds, kShort_s, kNeedWeak, 200, "heal"});
  add({GoalKind::hunt, -1, -1, -1, -1, Horizon::seconds, kShort_s, kNeedEnemySeen | kNeedStronger,
       100, "hunt"});
  std::stable_sort(out.begin(), out.end(), [](const Goal& a, const Goal& b) {
    return a.kind != b.kind ? a.kind < b.kind : a.worth > b.worth;
  });
  return out;
}

std::string goals_text(const std::vector<Goal>& goals) {
  using ge::core::num::to_fixed;
  std::string out;
  for (const Goal& g : goals)
    out += g.name + ": " + horizon_name(g.horizon) +
           (g.horizon == Horizon::seconds ? " " + to_fixed(g.seconds, 0) + " s" : "") + ", needs " +
           needs_text(g.needs) + ", worth " + to_fixed(g.worth, 0) + "\n";
  return out;
}

}  // namespace shooters_edge::bots
