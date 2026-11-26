// Shooter's Edge's arsenals as its bots read them (arsenals.hpp): Quake's, Scourge of Armagon's,
// Dissolution of Eternity's and Quake II's.
//
// ge:summary Quake's and Quake II's arsenals as the bots read them
// ge:keywords bot-arsenal first-arsenal q2-arsenal stats-reader arsenals hipnotic-arsenal
// ge:keywords arsenal-for-mod laser-cannon mjolnir proximity-gun mission-pack rogue-arsenal
// ge:keywords lava-nailgun multi-rockets plasma-gun
// ge:skill bots shooter
// ge:test tests/shooters_edge_q2_nav_test.cpp
#include "shooters_edge/bots/arsenals.hpp"
#include "shooters_edge/bots/quake_maps.hpp"
#include "shooters_edge/bots/talk.hpp"

#include <array>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/profiles/arsenal.hpp"
#include "shooters_edge/profiles/items.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"

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

void first_read(const ge::shooter::PlayerNumbers& n, Stats& st) {
  st.items = static_cast<int32_t>(n.items);
  st.weapon = static_cast<int32_t>(n.weapon);
  for (int k = 0; k < 4; k++) st.ammo[k] = n.ammo[k];
}

int first_monster(std::string_view model) {
  namespace pf = shooters_edge::profiles;
  for (std::string_view name : pf::kQ1NoWeapon)
    if (model == name) return 3;
  for (std::string_view name : pf::kQ1BlastOnly)
    if (model == name) return 2;
  for (std::string_view name : pf::kQ1Monsters)
    if (model == name) return 1;
  return 0;
}

// How far a Quake player keeps from a monster in a fight, by the reach of its bite or leap: the
// dog's and the fiend's leaps, the knights' swords, the ogre's chainsaw, the shambler's claws, the
// spawn's bounce; none for the ones that shoot (a soldier, an enforcer, a scrag, a vore).
float first_keep_off(std::string_view model) {
  struct Reach {
    std::string_view model;
    float keep;
  };
  static constexpr Reach kReach[] = {
      {"progs/dog.mdl", 192},     {"progs/demon.mdl", 320},  {"progs/knight.mdl", 160},
      {"progs/hknight.mdl", 192}, {"progs/ogre.mdl", 160},   {"progs/shambler.mdl", 192},
      {"progs/tarbaby.mdl", 224}, {"progs/zombie.mdl", 128},
  };
  for (const Reach& r : kReach)
    if (model == r.model) return r.keep;
  return 0;
}

// Quake's zombie: only a blast puts it down for good (a lesser hit knocks it down to rise again).
bool first_blast_only(std::string_view model) { return model == "progs/zombie.mdl"; }

// Quake's grenade and rocket launchers as they lie on a map.
bool first_blast_pickup(std::string_view model) {
  return model == "progs/g_rock.mdl" || model == "progs/g_rock2.mdl";
}

// Quake II's roster: every kind that hunts (not a good guy, not a figure that only answers a
// use) is one any weapon puts down.
int q2_monster(std::string_view model) {
  for (const server::roster::Kind& k : server::roster::kKinds)
    if (!k.good && !k.prop && model == k.model) return 1;
  return 0;
}

// How long after its last fire sound a weapon is surely still in hand: a weapon select waits until
// the refire is over (porting/shooters-edge/weapon-switch.md). A single shot's refire; a stream
// (the nailguns, the arc) ends 0.2 s after its last shot, and the arc sounds at most every 0.6 s
// while it fires, so its last bolt may come that much after the last sound heard.
double held_after(const shooters_edge::profiles::WeaponRow& w) {
  if (w.fire == shooters_edge::profiles::Fire::spike) return 0.2;
  if (w.fire == shooters_edge::profiles::Fire::beam) return 0.6 + 0.2;
  return w.refire;
}

// Quake's fire sounds, by its weapon table, and the arc's start where a game plays one.
std::span<const BotFireSound> first_fire_sounds() {
  static const std::vector<BotFireSound> all = [] {
    std::vector<BotFireSound> out;
    for (const shooters_edge::profiles::WeaponRow& w : shooters_edge::profiles::kQ1Weapons)
      out.push_back({w.fire_sound, w.slot, held_after(w)});
    out.push_back({"weapons/lstart.wav", 8, held_after(shooters_edge::profiles::kQ1Weapons[7])});
    return out;
  }();
  return all;
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
  a.tables = &shooters_edge::profiles::kQ1Tables;
  a.monster = first_monster;
  a.keep_off = first_keep_off;
  a.blast_only = first_blast_only;
  a.blast_pickup = first_blast_pickup;
  a.short_of[0] = 40;  // shells
  a.short_of[1] = 60;  // nails
  a.short_of[2] = 20;  // rockets
  a.short_of[3] = 30;  // cells
  a.read = first_read;
  a.heard = &shooters_edge::profiles::kQ1Tables;
  a.fire_sounds = first_fire_sounds();
  a.maps = &quake_maps();
  a.talk = &team_words();
  return a;
}

// SCOURGE OF ARMAGON (the `hipnotic` mission pack, porting/shooters-edge/q1-pack-weapons.md): the
// base game's eight and the pack's three. The laser cannon (bit 2^23, impulse 225) streams bolts at
// 1000 units/s, a cell a tenth of a second; Mjolnir (bit 128, impulse 226) is a blow at arm's
// length that spends no ammunition; the proximity gun (bit 2^16) throws the grenade launcher's lob
// on rockets, asked for by key 6 as the launcher is (the key toggles the two), and is carried but
// never chosen to fight with. Each says its aim slot: its impulse lies past the base numbers.
constexpr int32_t kWLaser = 8388608, kWHammer = 128, kWProximity = 65536;

const std::array<BotWeapon, 11> kHipWeapons = [] {
  std::array<BotWeapon, 11> w{};
  for (size_t i = 0; i < 8; i++) w[i] = kFirstWeapons[i];
  w[8] = row(kWLaser, 225, "laser cannon", 3, 1, 1000, 0, 16, 1500, 0.1f, true, false,
             WeaponCurve::spiker, 60, 1.6f);
  w[8].aim_slot = 1;
  w[9] = row(kWHammer, 226, "mjolnir", -1, 0, 0, 0, 24, 72, 0.7f, false, false, WeaponCurve::blade,
             40, 1.2f);
  w[9].aim_slot = 0;
  // Kept, not fought with: its bombs lie in wait for anyone who comes near, their thrower too,
  // and it shares key 6 with the grenade launcher (the key toggles the two). Thrown in a fight it
  // cost hipdm1's bots 20 frags in five minutes, most of them their own.
  w[10] = row(kWProximity, 6, "proximity gun", 2, 1, 600, -20, 40, 500, 0.6f, false, true,
              WeaponCurve::kept, 10, 1.2f);
  w[10].aim_slot = 2;
  return w;
}();

// The base game's item rows, then the pack's three guns.
const std::vector<shooters_edge::profiles::ItemRow>& hip_items() {
  static const std::vector<shooters_edge::profiles::ItemRow> rows = [] {
    std::vector<shooters_edge::profiles::ItemRow> r(std::begin(shooters_edge::profiles::kQ1Items),
                                                    std::end(shooters_edge::profiles::kQ1Items));
    r.push_back({.class_name = "weapon_laser_gun",
                 .kind = ItemKind::weapon,
                 .respawn_s = 30,
                 .weapon = kWLaser,
                 .name = "laser cannon"});
    r.push_back({.class_name = "weapon_mjolnir",
                 .kind = ItemKind::weapon,
                 .respawn_s = 30,
                 .weapon = kWHammer,
                 .name = "mjolnir"});
    // No row for the proximity gun: a bot never fights with it, and as a weapon to fetch it
    // drew the bots to it five to seven times a match on hipdm1, a fifth of their kills lost
    // (se-pack-dm, six seeds); taken in passing it is still known by its bit.
    return r;
  }();
  return rows;
}

const shooters_edge::profiles::ItemTables& hip_tables() {
  static const shooters_edge::profiles::ItemTables t{hip_items(), shooters_edge::profiles::kQ1Words,
                                                     shooters_edge::profiles::kQ1Sounds,
                                                     shooters_edge::profiles::kQ1Names};
  return t;
}

// The grenade and rocket launchers and the proximity gun as they lie on a map: a blast for the
// monsters only a blast keeps down.
bool hip_blast_pickup(std::string_view model) {
  return first_blast_pickup(model) || model == "progs/g_prox.mdl";
}

// The packs' own monsters a player keeps his distance from, by the reach of what they do close
// (rough reaches, read off how each fights, not measured): Scourge's scourge (its sting), gremlin
// (its leap) and Armagon (its stomp), and the spike mine that floats at a player to burst;
// Dissolution's phantom swordsman, mummy, eel (its shock) and dragon (its breath).
struct Reach {
  std::string_view model;
  float keep;
};
constexpr Reach kPackReach[] = {
    {"progs/scor.mdl", 192},     {"progs/grem.mdl", 224},   {"progs/armabody.mdl", 256},
    {"progs/spikmine.mdl", 320}, {"progs/sword.mdl", 160},  {"progs/mummy.mdl", 160},
    {"progs/eel2.mdl", 128},     {"progs/dragon.mdl", 256},
};

float pack_keep_off(std::string_view model) {
  for (const Reach& r : kPackReach)
    if (model == r.model) return r.keep;
  return first_keep_off(model);
}

// Quake's monsters (the packs' own among them), and the two a pack fields that the shared list
// does not name: Scourge's spike mine and Dissolution's phantom swordsman.
int pack_monster(std::string_view model) {
  if (model == "progs/spikmine.mdl" || model == "progs/sword.mdl") return 1;
  return first_monster(model);
}

BotArsenal make_hipnotic() {
  BotArsenal a = make_first();
  a.keep_off = pack_keep_off;
  a.monster = pack_monster;
  a.weapons = kHipWeapons;
  a.tables = &hip_tables();
  a.blast_pickup = hip_blast_pickup;
  return a;
}

// DISSOLUTION OF ETERNITY (the `rogue` mission pack, porting/shooters-edge/q1-pack-weapons.md): the
// base game's eight with the blade moved to bit 2048 (its 4096 is the lava nailgun's here), and the
// pack's five, each on an ammunition of its own: lava nails (4), multi rockets (5), plasma (6). The
// status bar's four ammunitions switch meaning with the gun held, so the bots read the program's
// true stock (`PlayerNumbers::stock`, filled by the server from the program's own fields). Each
// pack gun has its own impulse (60..64) and says its aim slot.
constexpr int32_t kRogueBlade = 2048, kWLava = 4096, kWSuperLava = 8192, kWMultiGrenades = 16384;
constexpr int32_t kWMultiRockets = 32768, kWPlasma = 65536;

const std::array<BotWeapon, 13> kRogueWeapons = [] {
  std::array<BotWeapon, 13> w{};
  for (size_t i = 0; i < 8; i++) w[i] = kFirstWeapons[i];
  w[0].bit = kRogueBlade;
  w[8] = row(kWLava, 60, "lava spiker", 4, 1, 1000, 0, 16, 1500, 0.1f, true, false,
             WeaponCurve::spiker, 40, 1.4f);
  w[8].aim_slot = 1;
  w[9] = row(kWSuperLava, 61, "heavy lava spiker", 4, 2, 1000, 0, 20, 1500, 0.1f, true, false,
             WeaponCurve::heavy, 70, 1.7f);
  w[9].aim_slot = 1;
  w[10] = row(kWMultiGrenades, 62, "multi grenades", 5, 1, 600, -20, 48, 500, 0.6f, false, true,
              WeaponCurve::lob, 50, 1.7f);
  w[10].aim_slot = 2;
  // Four rockets in a fan: loosely aimed, as the launcher is, and wider.
  w[11] = row(kWMultiRockets, 63, "multi rockets", 5, 1, 1000, -20, 128, 1200, 0.8f, false, true,
              WeaponCurve::rocket, 50, 2.2f);
  w[11].aim_slot = 3;
  // A slow-starting bolt at 1250 units/s that bursts (70 units) and arcs to those near it.
  w[12] = row(kWPlasma, 64, "plasma", 6, 1, 1250, 0, 48, 1200, 1.0f, false, true,
              WeaponCurve::rocket, 70, 2);
  w[12].aim_slot = 3;
  return w;
}();

void rogue_read(const ge::shooter::PlayerNumbers& n, Stats& st) {
  st.items = static_cast<int32_t>(n.items);
  st.weapon = static_cast<int32_t>(n.weapon);
  for (int k = 0; k < 7; k++) st.ammo[k] = n.stock[k];
}

const std::vector<shooters_edge::profiles::ItemRow>& rogue_items() {
  static const std::vector<shooters_edge::profiles::ItemRow> rows = [] {
    std::vector<shooters_edge::profiles::ItemRow> r(std::begin(shooters_edge::profiles::kQ1Items),
                                                    std::end(shooters_edge::profiles::kQ1Items));
    for (shooters_edge::profiles::ItemRow& it : r)
      if (it.weapon == kWBlade) it.weapon = kRogueBlade;
    // No rows for the pack's three ammunitions (lava nails, multi rockets, plasma): the bot reads
    // its stock of each and fights with the guns, but as boxes to fetch they drew the bots away
    // from the fight, on ctf1 twenty to forty boxes a match and a third of the frags (se-pack-dm,
    // four seeds; rating them with the base ammunition their key shares did not help). Taken in
    // passing, they still feed the guns.
    return r;
  }();
  return rows;
}

const shooters_edge::profiles::ItemTables& rogue_tables() {
  static const shooters_edge::profiles::ItemTables t{
      rogue_items(), shooters_edge::profiles::kQ1Words, shooters_edge::profiles::kQ1Sounds,
      shooters_edge::profiles::kQ1Names};
  return t;
}

BotArsenal make_rogue() {
  BotArsenal a = make_first();
  a.weapons = kRogueWeapons;
  a.ammo_count = 7;
  a.ammo_max[4] = 200;  // lava nails
  a.ammo_max[5] = 100;  // multi rockets
  a.ammo_max[6] = 100;  // plasma
  a.tables = &rogue_tables();
  a.read = rogue_read;
  a.keep_off = pack_keep_off;
  a.monster = pack_monster;
  return a;
}

// Quake II's ammunitions by their index in its rules, less one: shells 0, bullets 1, grenades 2,
// rockets 3, cells 4, slugs 5.
int q2_ammo(server::SqAmmo a) { return static_cast<int>(a) - 1; }

int q2_item_ammo(ItemKind kind) {
  switch (kind) {
    case ItemKind::shells:
      return 0;
    case ItemKind::nails:  // the second ammunition: Quake II's bullets
      return 1;
    case ItemKind::grenades:
      return 2;
    case ItemKind::rockets:
      return 3;
    case ItemKind::cells:
      return 4;
    case ItemKind::slugs:
      return 5;
    case ItemKind::ammo_more1:  // a pack's first own: The Reckoning's mag slugs
      return 6;
    case ItemKind::ammo_more2:  // its second: The Reckoning's traps
      return 7;
    default:
      return -1;
  }
}

// Ground Zero's: its flechettes, prox mines and disruptor rounds, by their place in its rules (the
// tesla, thrown and never aimed, is left out).
int q2_gz_item_ammo(ItemKind kind) {
  switch (kind) {
    case ItemKind::ammo_more1:  // flechettes
      return 8;
    case ItemKind::ammo_more2:  // prox mines
      return 9;
    case ItemKind::ammo_more3:  // disruptor rounds
      return 11;
    default:
      return q2_item_ammo(kind);
  }
}

// The bots' kind of an ammunition of the rules (`health` for one they have no index for). A pack's
// own are its `ammo_more` kinds in its own order: no two packs are loaded at once.
ItemKind q2_ammo_kind(server::SqAmmo a) {
  const ItemKind kinds[] = {ItemKind::health,     ItemKind::shells,     ItemKind::nails,
                            ItemKind::grenades,   ItemKind::rockets,    ItemKind::cells,
                            ItemKind::slugs,      ItemKind::ammo_more1, ItemKind::ammo_more2,
                            ItemKind::ammo_more1, ItemKind::ammo_more2, ItemKind::health,
                            ItemKind::ammo_more3};
  const auto i = static_cast<size_t>(a);
  return i < std::size(kinds) ? kinds[i] : ItemKind::health;
}

void q2_read(const ge::shooter::PlayerNumbers& n, Stats& st) {
  st.items = n.carried;
  st.weapon = n.held;
  for (int k = 0; k < kMaxAmmo; k++) st.ammo[k] = n.stock[k];
}

// Quake II's items as the bots' rows, off its rules' own item table (server/q2_items.hpp):
// armour by its tier, health (the mega health apart), the weapons by their bit, the ammunitions,
// the quad and the shield; the rest (keys, the other powers, packs) left out. A mission pack's own
// rows come in with `pack` (its weapons, its ammunitions; its other items as the rest).
std::vector<shooters_edge::profiles::ItemRow> q2_rows(server::Q2Pack pack) {
  std::vector<shooters_edge::profiles::ItemRow> out;
  {
    for (int i = 0; i < server::kSqItemCount; i++) {
      const server::SqItem& it = server::kSqItems[i];
      if (!server::q2_pack_has(pack, it.pack)) continue;
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
        case server::SqKind::ammo:
          r.kind = q2_ammo_kind(it.ammo);
          keep = it.ammo != server::SqAmmo::none && r.kind != ItemKind::health;
          break;
        case server::SqKind::powerup: {
          // Ground Zero's double damage is sought as the quad is: more damage for a while.
          const std::string_view c = it.classname;
          keep = c == "item_quad" || c == "item_double" || c == "item_invulnerability";
          r.kind = c == "item_invulnerability" ? ItemKind::pent : ItemKind::quad;
          break;
        }
        default:
          keep = false;
          break;
      }
      if (keep) out.push_back(r);
    }
  }
  return out;
}

struct Q2Arsenal {
  std::vector<BotWeapon> weapons;
  std::vector<shooters_edge::profiles::ItemRow> rows;
  shooters_edge::profiles::ItemTables tables;
  BotArsenal arsenal;
};

// How each of Quake II's weapons is aimed and scored: its shot's speed, its curve, how wide the
// trigger takes the target, the range it is fired within, whether it streams, and its worth. By
// the rules' weapon index; the hand grenade and the trap (thrown, not aimed) are left out.
struct Q2Use {
  int weapon;
  float speed, aim_dz, width, range;
  bool stream, blast;
  WeaponCurve curve;
  float worth, arms;
  int aim_slot;  // the aim trait it is held by (BotWeapon::aim_slot)
};
constexpr Q2Use kQ2Uses[] = {
    {0, 1000, 0, 20, 1000, false, false, WeaponCurve::spiker, 0, 1, 0},        // blaster
    {1, 0, 0, 24, 1200, false, false, WeaponCurve::scatter, 30, 1.1f, 0},      // shotgun
    {2, 0, 0, 32, 350, false, false, WeaponCurve::double_, 45, 1.3f, 0},       // super shotgun
    {3, 0, 0, 20, 1500, true, false, WeaponCurve::hitscan, 40, 1.2f, 1},       // machinegun
    {4, 0, 0, 24, 1500, true, false, WeaponCurve::heavy, 70, 1.5f, 1},         // chaingun
    {5, 600, -20, 40, 500, false, true, WeaponCurve::lob, 40, 1.5f, 2},        // grenade launcher
    {6, 650, -20, 96, 1200, false, true, WeaponCurve::rocket, 30, 2, 3},       // rocket launcher
    {7, 1000, 0, 24, 1000, true, false, WeaponCurve::beam, 70, 1.6f, 4},       // hyperblaster
    {8, 0, 0, 12, 4000, false, false, WeaponCurve::rail, 90, 2, 0},            // railgun
    {9, 400, -20, 128, 1500, false, true, WeaponCurve::big_blast, 100, 2, 3},  // BFG
    // The Reckoning's: the ion ripper's bolt (500 a second, it bounces: best in rooms), the
    // phalanx's two balls (725 a second, each a blast).
    {11, 500, 0, 32, 700, false, false, WeaponCurve::spiker, 50, 1.4f, 1},    // ion ripper
    {12, 725, -20, 80, 1200, false, true, WeaponCurve::rocket, 60, 1.8f, 3},  // phalanx
    // Ground Zero's: the chainfist at arm's length, the ETF rifle's stream of flechettes (1150 a
    // second), the prox launcher's mines lobbed, the plasma beam's heat beam (it hits at once),
    // the disruptor's tracker that homes on its target. The tesla is thrown, not aimed.
    {14, 0, 0, 32, 64, true, false, WeaponCurve::blade, 10, 1, 0},            // chainfist
    {15, 1150, 0, 20, 1000, true, false, WeaponCurve::spiker, 45, 1.2f, 1},   // ETF rifle
    {16, 600, -20, 64, 500, false, true, WeaponCurve::lob, 35, 1.5f, 2},      // prox launcher
    {17, 0, 0, 24, 1000, true, false, WeaponCurve::beam, 65, 1.6f, 4},        // plasma beam
    {18, 1000, 0, 48, 1500, false, false, WeaponCurve::spiker, 60, 1.6f, 1},  // disruptor
};

Q2Arsenal make_q2(server::Q2Pack pack) {
  Q2Arsenal out;
  for (const Q2Use& u : kQ2Uses) {
    const server::SqWeaponRow& r = server::kSqWeapons[u.weapon];
    if (!server::q2_pack_has(pack, r.pack)) continue;
    // From one shot to the next: the frames from its first firing frame to its last, a tenth of a
    // second each (a stream fires every frame).
    const float refire = u.stream ? 0.1f : 0.1f * static_cast<float>(r.last_fire - r.last_activate);
    out.weapons.push_back(row(1 << u.weapon, u.weapon + 1, r.name,
                              r.ammo == server::SqAmmo::none ? -1 : q2_ammo(r.ammo),
                              static_cast<float>(r.per_shot), u.speed, u.aim_dz, u.width, u.range,
                              refire, u.stream, u.blast, u.curve, u.worth, u.arms));
    out.weapons.back().aim_slot = u.aim_slot;
  }
  out.rows = q2_rows(pack);  // its tables are pointed at them where it rests (q2_arsenal)
  BotArsenal& a = out.arsenal;
  a.ammo_count = 6;
  a.item_ammo = q2_item_ammo;
  a.launcher = 1 << 6;
  a.lob = 1 << 5;
  a.beam = 1 << 7;
  a.heavy = 1 << 4;
  a.launcher_ammo = 3;
  const float most[6] = {100, 200, 50, 50, 200, 50};
  for (int k = 0; k < 6; k++) a.ammo_max[k] = most[k];
  const float shorts[6] = {20, 100, 10, 20, 100, 10};
  for (int k = 0; k < 6; k++) a.short_of[k] = shorts[k];
  if (server::q2_pack_has(pack, server::Q2Pack::reckoning)) {  // mag slugs and traps
    a.ammo_count = 8;
    a.ammo_max[6] = 50;
    a.ammo_max[7] = 5;
    a.short_of[6] = 10;
    a.short_of[7] = 1;
  }
  if (server::q2_pack_has(pack, server::Q2Pack::ground_zero)) {  // flechettes, prox, tesla, rounds
    a.ammo_count = 12;
    a.item_ammo = q2_gz_item_ammo;
    const float gz_most[4] = {200, 50, 50, 100};
    const float gz_short[4] = {50, 5, 0, 5};
    for (int k = 0; k < 4; k++) {
      a.ammo_max[8 + k] = gz_most[k];
      a.short_of[8 + k] = gz_short[k];
    }
  }
  a.read = q2_read;
  a.monster = q2_monster;
  a.heard = &shooters_edge::profiles::kQ1Tables;  // as before the seam; its own words next
  a.fire_sounds = first_fire_sounds();
  a.maps = &quake_maps();  // Quake's maps' rows, by name: none on Quake II's own
  a.talk = &team_words();
  return out;
}

}  // namespace

const BotArsenal& first_arsenal() {
  static const BotArsenal a = make_first();
  return a;
}

const BotArsenal& hipnotic_arsenal() {
  static const BotArsenal a = make_hipnotic();
  return a;
}

const BotArsenal& rogue_arsenal() {
  static const BotArsenal a = make_rogue();
  return a;
}

const BotArsenal& arsenal_for_mod(std::string_view mod) {
  std::string m(mod);
  for (char& c : m) c = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
  if (m == "hipnotic") return hipnotic_arsenal();
  if (m == "rogue") return rogue_arsenal();
  return first_arsenal();
}

const BotArsenal& q2_arsenal(server::Q2Pack pack) {
  // One a pack, built once and never moved: its spans point into it.
  static std::array<Q2Arsenal, 3> all = {make_q2(server::Q2Pack::base),
                                         make_q2(server::Q2Pack::reckoning),
                                         make_q2(server::Q2Pack::ground_zero)};
  static const std::array<BotArsenal, 3> arsenals = [] {
    std::array<BotArsenal, 3> out;
    for (size_t k = 0; k < out.size(); k++) {
      all[k].tables = shooters_edge::profiles::ItemTables{all[k].rows, {}, {}, {}};
      out[k] = all[k].arsenal;
      out[k].weapons = all[k].weapons;
      out[k].tables = &all[k].tables;
    }
    return out;
  }();
  const auto k = static_cast<size_t>(pack);
  return arsenals[k < arsenals.size() ? k : 0];
}

}  // namespace shooters_edge::bots
