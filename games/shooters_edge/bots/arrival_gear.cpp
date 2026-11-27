// Gear on arrival (arrival_gear.hpp): Quake's earlier maps walked on the campaign host, Quake II's
// on a scratch server, the pickups taken by each game's own rule and the monsters paid for.
//
// ge:summary Gear on arrival: earlier maps' pickups taken by the game's rule, monsters paid for
// ge:keywords arrival-gear q1_take_map_gear q2_arrival_body q2_campaign_order kept-share
// ge:skill shooter bots
// ge:test tests/shooters_edge_arrival_gear_test.cpp
#include "shooters_edge/bots/arrival_gear.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <set>
#include <string_view>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "core/num_text.hpp"
#include "progs/file.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::bots {

namespace sv = server;
namespace sh = ge::shooter;
namespace pg = ge::progs;
using ge::core::num::to_fixed;

double gear_kept_share(std::span<const int> have, std::span<const double> damage, double health) {
  double capacity = 0;
  for (size_t k = 0; k < have.size() && k < damage.size(); k++)
    capacity += static_cast<double>(std::max(have[k], 0)) * damage[k];
  if (health <= 0) return std::max(0.0, 1 - kGearMargin);
  if (capacity <= 0) return 0;
  return std::max(0.0, 1 - health / capacity - kGearMargin);
}

namespace {

bool has(std::string_view s, std::string_view part) {
  return s.find(part) != std::string_view::npos;
}

// A pickup that is gear: a weapon, ammunition, armour; never health, a power, a key or a rune.
bool q1_gear_item(std::string_view cn) {
  if (cn.rfind("weapon_", 0) != 0 && cn.rfind("item_", 0) != 0) return false;
  for (std::string_view no : {"health", "artifact", "key", "sigil", "rune", "powerup", "sphere",
                              "horn", "wetsuit", "empathy"})
    if (has(cn, no)) return false;
  return true;
}

float get_f(const sv::QcServer& q, int e, int f) {
  return pg::as_float(*pg::entity_field(q.q.vm, e, f));
}
void put_f(sv::QcServer& q, int e, int f, float v) {
  *pg::entity_field(q.q.vm, e, f) = pg::as_word(v);
}

}  // namespace

GearLeg q1_take_map_gear(sv::Campaign& c, int slot) {
  GearLeg leg;
  leg.map = c.map.name;
  sv::QcServer* q = sv::qc_of(c.server);
  if (q == nullptr) return leg;
  sh::Server& s = q->sv;
  const int me = slot + 1;
  // Every pickup the map placed for this skill, touched by him.
  for (int e = s.set.maxclients + 1; e < s.num_edicts; e++) {
    if (s.edicts[static_cast<size_t>(e)].free) continue;
    if (static_cast<size_t>(e) >= q->spawned_as.size()) break;
    const std::string& cn = q->spawned_as[static_cast<size_t>(e)];
    if (!q1_gear_item(cn)) continue;
    const sh::Func touch = s.edicts[static_cast<size_t>(e)].touch;
    if (touch == sh::kNoFunc) continue;
    const int old_self = s.g.self, old_other = s.g.other;
    s.g.self = e;
    s.g.other = me;
    s.g.time = static_cast<float>(s.time);
    sh::call_program(s, touch);
    s.g.self = old_self;
    s.g.other = old_other;
    leg.pickups++;
  }
  // The monsters' health.
  for (int e = s.set.maxclients + 1; e < s.num_edicts; e++) {
    const sh::Edict& d = s.edicts[static_cast<size_t>(e)];
    if (d.free || static_cast<size_t>(e) >= q->spawned_as.size()) continue;
    if (q->spawned_as[static_cast<size_t>(e)].rfind("monster_", 0) != 0) continue;
    if (d.health > 0) leg.monster_health += d.health;
  }
  // Paid out of every kind by one share: the true stock where the program keeps one apart from
  // the four the status bar shows (Dissolution of Eternity), else the four.
  const pg::LayoutFields& f = q->q.layout->f;
  const int shown[4] = {f.ammo_shells, f.ammo_nails, f.ammo_rockets, f.ammo_cells};
  const int* stock = q->q.fields.stock;
  const bool stocked = stock[0] >= 0;
  std::vector<int> have;
  std::vector<double> damage;
  for (int k = 0; k < 4; k++) {
    have.push_back(static_cast<int>(get_f(*q, me, stocked ? stock[k] : shown[k])));
    damage.push_back(kQ1Damage[k]);
  }
  for (int k = 4; stocked && k < 7; k++) {
    if (stock[k] < 0) continue;
    have.push_back(static_cast<int>(get_f(*q, me, stock[k])));
    damage.push_back(kQ1PackDamage[k - 4]);
  }
  for (size_t k = 0; k < have.size(); k++)
    leg.capacity += static_cast<double>(std::max(have[k], 0)) * damage[k];
  leg.kept = gear_kept_share(have, damage, leg.monster_health);
  auto scale = [&](int field) {
    if (field < 0) return;
    const float v = get_f(*q, me, field);
    put_f(*q, me, field, static_cast<float>(static_cast<int>(v * leg.kept)));
  };
  for (int k = 0; k < 4; k++) scale(shown[k]);
  for (int k = 0; stocked && k < 7; k++) scale(stock[k]);
  put_f(*q, me, f.health, 100);
  s.edicts[static_cast<size_t>(me)].health = 100;
  // The ammunition in hand read again for the weapon held.
  if (const int fn = pg::find_function(*q->progs, "W_SetCurrentAmmo"); fn > 0) {
    const int old_self = s.g.self;
    s.g.self = me;
    sh::call_program(s, fn);
    s.g.self = old_self;
  }
  leg.after = q1_gear_text(c, slot);
  return leg;
}

std::string q1_gear_text(const sv::Campaign& c, int slot) {
  const sv::QcServer* q = sv::qc_of(c.server);
  if (q == nullptr) return "";
  const int me = slot + 1;
  const pg::LayoutFields& f = q->q.layout->f;
  const int items = static_cast<int>(get_f(*q, me, f.items));
  static constexpr const char* kNames[7] = {"SG", "SSG", "NG", "SNG", "GL", "RL", "LG"};
  std::string out;
  for (int b = 0; b < 7; b++)
    if (items & (1 << b)) out += std::string(out.empty() ? "" : " ") + kNames[b];
  const int* stock = q->q.fields.stock;
  const bool stocked = stock[0] >= 0;
  const int shown[4] = {f.ammo_shells, f.ammo_nails, f.ammo_rockets, f.ammo_cells};
  static constexpr const char* kAmmo[7] = {"s", "n", "r", "c", "lava", "multi", "plasma"};
  out += " |";
  for (int k = 0; k < 7; k++) {
    const int field = stocked ? stock[k] : (k < 4 ? shown[k] : -1);
    if (field < 0) continue;
    out += " " + std::to_string(static_cast<int>(get_f(*q, me, field))) + kAmmo[k];
  }
  const float type = get_f(*q, me, f.armortype), value = get_f(*q, me, f.armorvalue);
  if (value > 0) {
    out += type >= 0.75f ? " | RA " : type >= 0.55f ? " | YA " : " | GA ";
    out += std::to_string(static_cast<int>(value));
  }
  return out;
}

bool q2_arrival_body(sv::MapLoader load, void* ctx, sv::Q2Pack pack, int skill,
                     const std::vector<std::string>& before, sv::SqBody& out,
                     std::vector<GearLeg>& legs, std::string& error) {
  out = sv::sq_new_body(pack);
  if (before.empty()) return true;
  sv::Q2ServerOptions so;
  so.pack = pack;
  sv::RulesRunner rules = sv::q2_runner(sv::make_q2_server(so));
  rules.console("deathmatch 0");
  rules.console("coop 0");
  rules.console("skill " + std::to_string(skill));
  for (const std::string& name : before) {
    sv::MapSpec map;
    if (!load(ctx, name, map, error)) return false;
    if (!rules.start(map, error)) return false;
    const sv::Q2Server& s = *sv::q2_of(rules);
    GearLeg leg;
    leg.map = name;
    for (const sv::Q2Entity& e : s.entities) {
      if (e.item < 0 || e.gone) continue;
      const sv::SqItem& it = sv::kSqItems[e.item];
      if (it.kind == sv::SqKind::health || it.kind == sv::SqKind::powerup ||
          it.kind == sv::SqKind::key)
        continue;
      int count = 0;
      if (it.kind == sv::SqKind::ammo && e.block >= 0 && s.map != nullptr &&
          static_cast<size_t>(e.block) < s.map->entities.size())
        count =
            std::atoi(s.map->entities[static_cast<size_t>(e.block)].get_any_case("count").c_str());
      if (sv::sq_pick_up(e.item, out, false, skill, 0, count)) leg.pickups++;
    }
    for (const sv::Q2Monster& m : s.monsters) {
      if (m.spare || m.entity < 0) continue;
      const sv::Q2Entity& e = s.entities[static_cast<size_t>(m.entity)];
      if (!e.gone && e.health > 0) leg.monster_health += e.health;
    }
    const std::span<const int> have(out.ammo, sv::kSqAmmoKinds);
    for (int k = 0; k < sv::kSqAmmoKinds; k++)
      leg.capacity += static_cast<double>(std::max(out.ammo[k], 0)) * kQ2Damage[k];
    leg.kept = gear_kept_share(have, kQ2Damage, leg.monster_health);
    for (int k = 0; k < sv::kSqAmmoKinds; k++)
      if (kQ2Damage[k] > 0) out.ammo[k] = static_cast<int>(out.ammo[k] * leg.kept);
    out.health = out.max_health;
    leg.after = q2_gear_text(out, pack);
    legs.push_back(std::move(leg));
  }
  return true;
}

std::string q2_gear_text(const sv::SqBody& body, sv::Q2Pack pack) {
  std::string out;
  for (int i = 0; i < sv::kSqItemCount; i++) {
    const sv::SqItem& it = sv::kSqItems[i];
    if (it.kind != sv::SqKind::weapon || body.counts[i] <= 0) continue;
    if (it.pack != sv::Q2Pack::base && it.pack != pack) continue;
    out += std::string(out.empty() ? "" : ", ") + it.pickup;
  }
  if (out.empty()) out = "Blaster";
  static constexpr const char* kAmmo[sv::kSqAmmoKinds] = {
      "",         "shells", "bullets",    "grenades", "rockets", "cells", "slugs",
      "magslugs", "traps",  "flechettes", "prox",     "tesla",   "rounds"};
  out += " |";
  for (int k = 1; k < sv::kSqAmmoKinds; k++)
    if (body.ammo[k] > 0) out += " " + std::to_string(body.ammo[k]) + " " + kAmmo[k];
  if (body.armour != sv::SqArmour::none) {
    static constexpr const char* kArmour[4] = {"", "jacket", "combat", "body"};
    out += " | " + std::string(kArmour[static_cast<int>(body.armour)]) + " " +
           std::to_string(body.armour_count);
  }
  return out;
}

namespace {

// A level change's map string ("eou1_.cin+*bunk1$start"): the map it enters ("" none: a picture
// or a film alone), and whether it begins a new unit.
std::string change_target(const std::string& text, bool& new_unit) {
  std::string part = text;
  if (const size_t plus = part.rfind('+'); plus != std::string::npos) part = part.substr(plus + 1);
  if (const size_t d = part.find('$'); d != std::string::npos) part = part.substr(0, d);
  new_unit = !part.empty() && part[0] == '*';
  if (new_unit) part = part.substr(1);
  const size_t dot = part.rfind('.');
  if (dot != std::string::npos) {
    const std::string ext = part.substr(dot);
    if (ext == ".cin" || ext == ".pcx") return "";
  }
  return part;
}

}  // namespace

std::vector<std::string> q2_campaign_order(sv::MapLoader load, void* ctx,
                                           const std::string& first) {
  std::vector<std::string> order;
  std::set<std::string> seen{first};
  std::deque<std::string> unit{first}, next_units;
  while (!unit.empty() || !next_units.empty()) {
    if (unit.empty()) {
      unit.push_back(next_units.front());
      next_units.pop_front();
      continue;
    }
    const std::string name = unit.front();
    unit.pop_front();
    sv::MapSpec map;
    std::string error;
    if (!load(ctx, name, map, error)) continue;
    order.push_back(name);
    std::vector<ge::brush::Entity> ents;
    if (map.brushes != nullptr) {
      ents = map.brushes->entities;
    } else if (!ge::brush::parse_entities(map.entities, &ents, &error)) {
      continue;
    }
    for (const ge::brush::Entity& e : ents) {
      if (e.get("classname") != "target_changelevel") continue;
      bool new_unit = false;
      const std::string to = change_target(e.get("map"), new_unit);
      if (to.empty() || !seen.insert(to).second) continue;
      (new_unit ? next_units : unit).push_back(to);
    }
  }
  return order;
}

}  // namespace shooters_edge::bots
