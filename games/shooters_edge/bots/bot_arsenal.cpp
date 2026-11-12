// A game's weapons as a bot sees them (bot_arsenal.hpp).
//
// ge:summary The first game's and the sequel's arsenals as the bots read them
// ge:keywords bot-arsenal first-arsenal sequel-arsenal weapon-score stats-reader
// ge:skill bots shooter
// ge:test tests/shooters_edge_sequel_nav_test.cpp
#include "shooters_edge/bots/bot_arsenal.hpp"

#include <array>
#include <string_view>
#include <vector>

#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_items.hpp"

namespace shooters_edge::bots {

namespace {

using shooters_edge::profiles::ItemKind;

constexpr int32_t kWScatter = 1, kWDouble = 2, kWSpiker = 4, kWHeavy = 8, kWGrenades = 16;
constexpr int32_t kWRockets = 32, kWArc = 64, kWBlade = 4096;

BotWeapon row(int32_t bit, int number, const char* name, int ammo, float cost, float shot_speed,
              float aim_dz, float width, float range, float refire_s, bool stream, bool blast,
              WeaponCurve curve, float worth, float arms = 1) {
  BotWeapon w;
  w.bit = bit;
  w.number = number;
  w.name = name;
  w.ammo = ammo;
  w.cost = cost;
  w.shot_speed = shot_speed;
  w.aim_dz = aim_dz;
  w.width = width;
  w.range = range;
  w.refire_s = refire_s;
  w.stream = stream;
  w.blast = blast;
  w.curve = curve;
  w.worth = worth;
  w.arms = arms;
  return w;
}

// Shells 0, nails 1, rockets 2, cells 3.
const BotWeapon kFirstWeapons[8] = {
    row(kWBlade, 1, "blade", -1, 0, 0, 0, 24, 72, 0.5f, false, false, WeaponCurve::blade, 30),
    row(kWScatter, 2, "scattergun", 0, 1, 0, 0, 24, 1200, 0.5f, false, false, WeaponCurve::scatter,
        30),
    row(kWDouble, 3, "double", 0, 2, 0, 0, 32, 350, 0.7f, false, false, WeaponCurve::double_, 45,
        1.3f),
    row(kWSpiker, 4, "spiker", 1, 1, 1000, 0, 16, 1500, 0.1f, true, false, WeaponCurve::spiker, 30,
        1.2f),
    row(kWHeavy, 5, "heavy spiker", 1, 2, 1000, 0, 20, 1500, 0.1f, true, false, WeaponCurve::heavy,
        60, 1.5f),
    row(kWGrenades, 6, "grenades", 2, 1, 600, -20, 40, 500, 0.6f, false, true, WeaponCurve::lob, 40,
        1.5f),
    // Fired as people fire it, loosely: near him is enough, the blast reaches the rest.
    row(kWRockets, 7, "rockets", 2, 1, 1000, -20, 96, 1200, 0.8f, false, true, WeaponCurve::rocket,
        30, 2),
    row(kWArc, 8, "arc", 3, 1, 0, 0, 20, 600, 0.1f, true, false, WeaponCurve::beam, 90, 2),
};

int first_item_ammo(ItemKind kind) {
  switch (kind) {
    case ItemKind::shells:
      return 0;
    case ItemKind::nails:
      return 1;
    case ItemKind::rockets:
      return 2;
    case ItemKind::cells:
      return 3;
    default:
      return -1;
  }
}

void first_read(const server::PlayerNumbers& n, Stats& st) {
  st.items = static_cast<int32_t>(n.items);
  st.weapon = static_cast<int32_t>(n.weapon);
  for (int k = 0; k < 4; k++) st.ammo[k] = n.ammo[k];
}

BotArsenal make_first() {
  BotArsenal a;
  a.weapons = kFirstWeapons;
  a.ammo_count = 4;
  a.item_ammo = first_item_ammo;
  a.launcher = kWRockets;
  a.lob = kWGrenades;
  a.beam = kWArc;
  a.heavy = kWHeavy;
  a.launcher_ammo = 2;
  a.ammo_max[0] = 100;  // shells
  a.ammo_max[1] = 200;  // nails
  a.ammo_max[2] = 100;  // rockets
  a.ammo_max[3] = 100;  // cells
  a.tables = &shooters_edge::profiles::kClassicTables;
  a.short_of[0] = 40;  // shells
  a.short_of[1] = 60;  // nails
  a.short_of[2] = 20;  // rockets
  a.short_of[3] = 30;  // cells
  a.read = first_read;
  return a;
}

// The sequel's ammunitions by their index in its rules, less one: shells 0, bullets 1, grenades 2,
// rockets 3, cells 4, slugs 5.
int sequel_ammo(server::SqAmmo a) { return static_cast<int>(a) - 1; }

int sequel_item_ammo(ItemKind kind) {
  switch (kind) {
    case ItemKind::shells:
      return 0;
    case ItemKind::nails:  // the second ammunition: the sequel's bullets
      return 1;
    case ItemKind::grenades:
      return 2;
    case ItemKind::rockets:
      return 3;
    case ItemKind::cells:
      return 4;
    case ItemKind::slugs:
      return 5;
    default:
      return -1;
  }
}

void sequel_read(const server::PlayerNumbers& n, Stats& st) {
  st.items = n.carried;
  st.weapon = n.held;
  for (int k = 0; k < 6; k++) st.ammo[k] = n.stock[k];
}

// The sequel's items as the bots' rows, off its rules' own item table (server/sequel_items.hpp):
// armour by its tier, health (the mega health apart), the weapons by their bit, the ammunitions,
// the quad and the shield; the rest (keys, the other powers, packs) left out.
const shooters_edge::profiles::ItemTables& sequel_tables() {
  static const std::vector<shooters_edge::profiles::ItemRow> rows = [] {
    std::vector<shooters_edge::profiles::ItemRow> out;
    for (int i = 0; i < server::kSqItemCount; i++) {
      const server::SqItem& it = server::kSqItems[i];
      shooters_edge::profiles::ItemRow r;
      r.class_name = it.classname;
      r.name = it.pickup;
      r.respawn_s = it.respawn_s > 0 ? static_cast<float>(it.respawn_s) : -1.f;
      r.amount = static_cast<float>(it.amount);
      bool keep = true;
      switch (it.kind) {
        case server::SqKind::armour:
          r.kind = it.amount == int(server::SqArmour::body)     ? ItemKind::armor3
                   : it.amount == int(server::SqArmour::combat) ? ItemKind::armor2
                                                                : ItemKind::armor1;
          r.amount = -1;
          break;
        case server::SqKind::health:
          r.kind = it.amount >= 100 ? ItemKind::mega : ItemKind::health;
          keep = it.amount > 0;
          break;
        case server::SqKind::weapon: {
          const int w = server::sq_weapon_of_item(i);
          keep = w >= 0;
          r.kind = ItemKind::weapon;
          r.weapon = w >= 0 ? 1 << w : 0;
          break;
        }
        case server::SqKind::ammo: {
          const ItemKind kinds[] = {ItemKind::health,   ItemKind::shells,  ItemKind::nails,
                                    ItemKind::grenades, ItemKind::rockets, ItemKind::cells,
                                    ItemKind::slugs};
          r.kind = kinds[static_cast<int>(it.ammo)];
          keep = it.ammo != server::SqAmmo::none;
          break;
        }
        case server::SqKind::powerup:
          keep = std::string_view(it.classname) == "item_quad" ||
                 std::string_view(it.classname) == "item_invulnerability";
          r.kind = std::string_view(it.classname) == "item_quad" ? ItemKind::quad : ItemKind::pent;
          break;
        default:
          keep = false;
          break;
      }
      if (keep) out.push_back(r);
    }
    return out;
  }();
  static const shooters_edge::profiles::ItemTables tables{rows, {}, {}, {}};
  return tables;
}

struct SequelArsenal {
  std::array<BotWeapon, 10> weapons;
  BotArsenal arsenal;
};

// How each of the sequel's weapons is aimed and scored: its shot's speed, its curve, how wide the
// trigger takes the target, the range it is fired within, whether it streams, and its worth.
SequelArsenal make_sequel() {
  struct Use {
    float speed, aim_dz, width, range;
    bool stream, blast;
    WeaponCurve curve;
    float worth, arms;
  };
  const Use uses[10] = {
      {1000, 0, 20, 1000, false, false, WeaponCurve::spiker, 0, 1},        // blaster
      {0, 0, 24, 1200, false, false, WeaponCurve::scatter, 30, 1.1f},      // shotgun
      {0, 0, 32, 350, false, false, WeaponCurve::double_, 45, 1.3f},       // super shotgun
      {0, 0, 20, 1500, true, false, WeaponCurve::hitscan, 40, 1.2f},       // machinegun
      {0, 0, 24, 1500, true, false, WeaponCurve::heavy, 70, 1.5f},         // chaingun
      {600, -20, 40, 500, false, true, WeaponCurve::lob, 40, 1.5f},        // grenade launcher
      {650, -20, 96, 1200, false, true, WeaponCurve::rocket, 30, 2},       // rocket launcher
      {1000, 0, 24, 1000, true, false, WeaponCurve::beam, 70, 1.6f},       // hyperblaster
      {0, 0, 12, 4000, false, false, WeaponCurve::rail, 90, 2},            // railgun
      {400, -20, 128, 1500, false, true, WeaponCurve::big_blast, 100, 2},  // BFG
  };
  SequelArsenal out;
  for (int i = 0; i < 10; i++) {
    const server::SqWeaponRow& r = server::kSqWeapons[i];
    const Use& u = uses[i];
    // From one shot to the next: the frames from its first firing frame to its last, a tenth of a
    // second each (a stream fires every frame).
    const float refire = u.stream ? 0.1f : 0.1f * static_cast<float>(r.last_fire - r.last_activate);
    out.weapons[static_cast<size_t>(i)] =
        row(1 << i, i + 1, r.name, r.ammo == server::SqAmmo::none ? -1 : sequel_ammo(r.ammo),
            static_cast<float>(r.per_shot), u.speed, u.aim_dz, u.width, u.range, refire, u.stream,
            u.blast, u.curve, u.worth, u.arms);
  }
  BotArsenal& a = out.arsenal;
  a.ammo_count = 6;
  a.item_ammo = sequel_item_ammo;
  a.launcher = 1 << 6;
  a.lob = 1 << 5;
  a.beam = 1 << 7;
  a.heavy = 1 << 4;
  a.launcher_ammo = 3;
  const float most[6] = {100, 200, 50, 50, 200, 50};
  for (int k = 0; k < 6; k++) a.ammo_max[k] = most[k];
  a.tables = &sequel_tables();
  const float shorts[6] = {20, 100, 10, 20, 100, 10};
  for (int k = 0; k < 6; k++) a.short_of[k] = shorts[k];
  a.read = sequel_read;
  return out;
}

}  // namespace

const BotArsenal& first_arsenal() {
  static const BotArsenal a = make_first();
  return a;
}

const BotArsenal& sequel_arsenal() {
  static const SequelArsenal s = make_sequel();
  static const BotArsenal a = [] {
    BotArsenal x = s.arsenal;
    x.weapons = s.weapons;
    return x;
  }();
  return a;
}

WeaponRole role_of(const BotArsenal& a, int32_t bit) {
  if (bit == 0) return WeaponRole::none;
  if (bit == a.launcher) return WeaponRole::launcher;
  if (bit == a.lob) return WeaponRole::lob;
  if (bit == a.beam) return WeaponRole::beam;
  if (bit == a.heavy) return WeaponRole::heavy;
  return WeaponRole::none;
}

const BotWeapon* weapon_row(const BotArsenal& a, int32_t bit) {
  for (const BotWeapon& w : a.weapons)
    if (w.bit == bit) return &w;
  return nullptr;
}

float ammo_of(const Stats& st, int ammo) {
  return ammo >= 0 && ammo < kMaxAmmo ? st.ammo[ammo] : 1.f;
}

bool loaded(const Stats& st, const BotWeapon& w) {
  return (st.items & w.bit) != 0 && ammo_of(st, w.ammo) >= w.cost;
}

double weapon_score(const BotWeapon& w, float d) {
  switch (w.curve) {
    case WeaponCurve::blade:
      return d < 72 ? 30 : 0.1;
    case WeaponCurve::scatter:
      return 20;
    case WeaponCurve::double_:  // its spread is wider than a man past a few hundred units
      return d < 250 ? 60 : d < 350 ? 30 : 3;
    case WeaponCurve::spiker:
      return 35;
    case WeaponCurve::heavy:
      return d < 600 ? 65 : 40;
    case WeaponCurve::lob:  // indoors at a room's range the blast beats the spikes
      return d > 120 && d < 450 ? 70 : d > 100 && d < 550 ? 40 : 5;
    case WeaponCurve::rocket:
      return d > 130 ? 90 : 15;
    case WeaponCurve::beam:
      return d < 560 ? 85 : 5;
    case WeaponCurve::hitscan:
      return 45;
    case WeaponCurve::rail:
      return d > 300 ? 95 : 60;
    case WeaponCurve::big_blast:
      return d > 400 ? 100 : 2;
  }
  return 0;
}

float launcher_rounds(const Stats& st) {
  return st.arsenal->launcher != 0 ? ammo_of(st, st.arsenal->launcher_ammo) : 0.f;
}

bool holds_launcher(const Stats& st) {
  return st.arsenal->launcher != 0 && st.weapon == st.arsenal->launcher;
}

bool has_launcher(const Stats& st) { return (st.items & st.arsenal->launcher) != 0; }

int launcher_number(const BotArsenal& a) {
  const BotWeapon* w = weapon_row(a, a.launcher);
  return w != nullptr ? w->number : 0;
}

}  // namespace shooters_edge::bots
