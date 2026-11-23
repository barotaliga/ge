// Quake II's status bar, server side (q2_bar.hpp).
//
// ge:summary Quake II's status bar from the server: its program, stats, pictures and item names
// ge:keywords q2 status-bar stats bar_rows bar_start
// ge:skill shooter ui
// ge:test tests/shooters_edge_q2_bar_test.cpp
#include "shooters_edge/server/q2_bar.hpp"
#include "shooters_edge/server/q2_screens.hpp"

#include <algorithm>
#include <cstdlib>
#include <span>
#include <string>

#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_powers.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_spectate.hpp"

namespace shooters_edge::server {

namespace {

namespace con = ge::console;

// The game's item list in its own order: the class name (the one health row stands for the
// four), the icon, the name a player reads.
struct Numbered {
  const char* classname;
  const char* icon;
  const char* name;
};
constexpr Numbered kNumbered[kSqItemNumbers] = {
    {"", "", ""},
    {"item_armor_body", "i_bodyarmor", "Body Armor"},
    {"item_armor_combat", "i_combatarmor", "Combat Armor"},
    {"item_armor_jacket", "i_jacketarmor", "Jacket Armor"},
    {"item_armor_shard", "i_jacketarmor", "Armor Shard"},
    {"item_power_screen", "i_powerscreen", "Power Screen"},
    {"item_power_shield", "i_powershield", "Power Shield"},
    {"weapon_blaster", "w_blaster", "Blaster"},
    {"weapon_shotgun", "w_shotgun", "Shotgun"},
    {"weapon_supershotgun", "w_sshotgun", "Super Shotgun"},
    {"weapon_machinegun", "w_machinegun", "Machinegun"},
    {"weapon_chaingun", "w_chaingun", "Chaingun"},
    {"ammo_grenades", "a_grenades", "Grenades"},
    {"weapon_grenadelauncher", "w_glauncher", "Grenade Launcher"},
    {"weapon_rocketlauncher", "w_rlauncher", "Rocket Launcher"},
    {"weapon_hyperblaster", "w_hyperblaster", "HyperBlaster"},
    {"weapon_railgun", "w_railgun", "Railgun"},
    {"weapon_bfg", "w_bfg", "BFG10K"},
    {"ammo_shells", "a_shells", "Shells"},
    {"ammo_bullets", "a_bullets", "Bullets"},
    {"ammo_cells", "a_cells", "Cells"},
    {"ammo_rockets", "a_rockets", "Rockets"},
    {"ammo_slugs", "a_slugs", "Slugs"},
    {"item_quad", "p_quad", "Quad Damage"},
    {"item_invulnerability", "p_invulnerability", "Invulnerability"},
    {"item_silencer", "p_silencer", "Silencer"},
    {"item_breather", "p_rebreather", "Rebreather"},
    {"item_enviro", "p_envirosuit", "Environment Suit"},
    {"item_ancient_head", "i_fixme", "Ancient Head"},
    {"item_adrenaline", "p_adrenaline", "Adrenaline"},
    {"item_bandolier", "p_bandolier", "Bandolier"},
    {"item_pack", "i_pack", "Ammo Pack"},
    {"key_data_cd", "k_datacd", "Data CD"},
    {"key_power_cube", "k_powercube", "Power Cube"},
    {"key_pyramid", "k_pyramid", "Pyramid Key"},
    {"key_data_spinner", "k_dataspin", "Data Spinner"},
    {"key_pass", "k_security", "Security Pass"},
    {"key_blue_key", "k_bluekey", "Blue Key"},
    {"key_red_key", "k_redkey", "Red Key"},
    {"key_commander_head", "k_comhead", "Commander's Head"},
    {"key_airstrike_target", "i_airstrike", "Airstrike Marker"},
    {"item_health", "i_health", "Health"},
};
// The Reckoning's (porting note: q2-reckoning-rules.md § 12): its six among the base game's.
constexpr Numbered kReckoning[kSqItemNumbers + 6] = {
    {"", "", ""},
    {"item_armor_body", "i_bodyarmor", "Body Armor"},
    {"item_armor_combat", "i_combatarmor", "Combat Armor"},
    {"item_armor_jacket", "i_jacketarmor", "Jacket Armor"},
    {"item_armor_shard", "i_jacketarmor", "Armor Shard"},
    {"item_power_screen", "i_powerscreen", "Power Screen"},
    {"item_power_shield", "i_powershield", "Power Shield"},
    {"weapon_blaster", "w_blaster", "Blaster"},
    {"weapon_shotgun", "w_shotgun", "Shotgun"},
    {"weapon_supershotgun", "w_sshotgun", "Super Shotgun"},
    {"weapon_machinegun", "w_machinegun", "Machinegun"},
    {"weapon_chaingun", "w_chaingun", "Chaingun"},
    {"ammo_grenades", "a_grenades", "Grenades"},
    {"ammo_trap", "a_trap", "Trap"},
    {"weapon_grenadelauncher", "w_glauncher", "Grenade Launcher"},
    {"weapon_rocketlauncher", "w_rlauncher", "Rocket Launcher"},
    {"weapon_hyperblaster", "w_hyperblaster", "HyperBlaster"},
    {"weapon_boomer", "w_ripper", "Ionripper"},
    {"weapon_railgun", "w_railgun", "Railgun"},
    {"weapon_phalanx", "w_phallanx", "Phalanx"},
    {"weapon_bfg", "w_bfg", "BFG10K"},
    {"ammo_shells", "a_shells", "Shells"},
    {"ammo_bullets", "a_bullets", "Bullets"},
    {"ammo_cells", "a_cells", "Cells"},
    {"ammo_rockets", "a_rockets", "Rockets"},
    {"ammo_slugs", "a_slugs", "Slugs"},
    {"ammo_magslug", "a_mslugs", "Mag Slug"},
    {"item_quad", "p_quad", "Quad Damage"},
    {"item_quadfire", "p_quadfire", "DualFire Damage"},
    {"item_invulnerability", "p_invulnerability", "Invulnerability"},
    {"item_silencer", "p_silencer", "Silencer"},
    {"item_breather", "p_rebreather", "Rebreather"},
    {"item_enviro", "p_envirosuit", "Environment Suit"},
    {"item_ancient_head", "i_fixme", "Ancient Head"},
    {"item_adrenaline", "p_adrenaline", "Adrenaline"},
    {"item_bandolier", "p_bandolier", "Bandolier"},
    {"item_pack", "i_pack", "Ammo Pack"},
    {"key_data_cd", "k_datacd", "Data CD"},
    {"key_power_cube", "k_powercube", "Power Cube"},
    {"key_pyramid", "k_pyramid", "Pyramid Key"},
    {"key_data_spinner", "k_dataspin", "Data Spinner"},
    {"key_pass", "k_security", "Security Pass"},
    {"key_blue_key", "k_bluekey", "Blue Key"},
    {"key_red_key", "k_redkey", "Red Key"},
    {"key_green_key", "k_green", "Green Key"},
    {"key_commander_head", "k_comhead", "Commander's Head"},
    {"key_airstrike_target", "i_airstrike", "Airstrike Marker"},
    {"item_health", "i_health", "Health"},
};

// Ground Zero's list: the base list with the pack's rows put in among them, in the order its
// clients number them.
constexpr Numbered kGroundZero[] = {
    {"", "", ""},
    {"item_armor_body", "i_bodyarmor", "Body Armor"},
    {"item_armor_combat", "i_combatarmor", "Combat Armor"},
    {"item_armor_jacket", "i_jacketarmor", "Jacket Armor"},
    {"item_armor_shard", "i_jacketarmor", "Armor Shard"},
    {"item_power_screen", "i_powerscreen", "Power Screen"},
    {"item_power_shield", "i_powershield", "Power Shield"},
    {"weapon_blaster", "w_blaster", "Blaster"},
    {"weapon_shotgun", "w_shotgun", "Shotgun"},
    {"weapon_supershotgun", "w_sshotgun", "Super Shotgun"},
    {"weapon_machinegun", "w_machinegun", "Machinegun"},
    {"weapon_chaingun", "w_chaingun", "Chaingun"},
    {"weapon_etf_rifle", "w_etf_rifle", "ETF Rifle"},
    {"ammo_grenades", "a_grenades", "Grenades"},
    {"weapon_grenadelauncher", "w_glauncher", "Grenade Launcher"},
    {"weapon_proxlauncher", "w_proxlaunch", "Prox Launcher"},
    {"weapon_rocketlauncher", "w_rlauncher", "Rocket Launcher"},
    {"weapon_hyperblaster", "w_hyperblaster", "HyperBlaster"},
    {"weapon_plasmabeam", "w_heatbeam", "Plasma Beam"},
    {"weapon_railgun", "w_railgun", "Railgun"},
    {"weapon_bfg", "w_bfg", "BFG10K"},
    {"weapon_chainfist", "w_chainfist", "Chainfist"},
    {"weapon_disintegrator", "w_disintegrator", "Disruptor"},
    {"ammo_shells", "a_shells", "Shells"},
    {"ammo_bullets", "a_bullets", "Bullets"},
    {"ammo_cells", "a_cells", "Cells"},
    {"ammo_rockets", "a_rockets", "Rockets"},
    {"ammo_slugs", "a_slugs", "Slugs"},
    {"ammo_flechettes", "a_flechettes", "Flechettes"},
    {"ammo_prox", "a_prox", "Prox"},
    {"ammo_tesla", "a_tesla", "Tesla"},
    {"ammo_nuke", "p_nuke", "A-M Bomb"},
    {"ammo_disruptor", "a_disruptor", "Rounds"},
    {"item_quad", "p_quad", "Quad Damage"},
    {"item_invulnerability", "p_invulnerability", "Invulnerability"},
    {"item_silencer", "p_silencer", "Silencer"},
    {"item_breather", "p_rebreather", "Rebreather"},
    {"item_enviro", "p_envirosuit", "Environment Suit"},
    {"item_ancient_head", "i_fixme", "Ancient Head"},
    {"item_adrenaline", "p_adrenaline", "Adrenaline"},
    {"item_bandolier", "p_bandolier", "Bandolier"},
    {"item_pack", "i_pack", "Ammo Pack"},
    {"item_ir_goggles", "p_ir", "IR Goggles"},
    {"item_double", "p_double", "Double Damage"},
    {"item_compass", "p_compass", "compass"},
    {"item_sphere_vengeance", "p_vengeance", "vengeance sphere"},
    {"item_sphere_hunter", "p_hunter", "hunter sphere"},
    {"item_sphere_defender", "p_defender", "defender sphere"},
    {"item_doppleganger", "p_doppleganger", "Doppleganger"},
    {"dm_tag_token", "i_tagtoken", "Tag Token"},
    {"key_data_cd", "k_datacd", "Data CD"},
    {"key_power_cube", "k_powercube", "Power Cube"},
    {"key_pyramid", "k_pyramid", "Pyramid Key"},
    {"key_data_spinner", "k_dataspin", "Data Spinner"},
    {"key_pass", "k_security", "Security Pass"},
    {"key_blue_key", "k_bluekey", "Blue Key"},
    {"key_red_key", "k_redkey", "Red Key"},
    {"key_commander_head", "k_comhead", "Commander's Head"},
    {"key_airstrike_target", "i_airstrike", "Airstrike Marker"},
    {"key_nuke_container", "i_contain", "Antimatter Pod"},
    {"key_nuke", "i_nuke", "Antimatter Bomb"},
    {"item_health", "i_health", "Health"},
};

// Each pack's list (q2_pack.hpp), in its own order: the base game's until a pack's rows land.
std::span<const Numbered> numbered(Q2Pack pack) {
  switch (pack) {
    case Q2Pack::reckoning:
      return kReckoning;
    case Q2Pack::ground_zero:
      return kGroundZero;
    case Q2Pack::base:
      break;
  }
  return kNumbered;
}

const Numbered* row_of(int n, Q2Pack pack) {
  const auto list = numbered(pack);
  return n > 0 && n < static_cast<int>(list.size()) ? &list[size_t(n)] : nullptr;
}

// The bars (q2_bar.hpp). The bottom row 24 px from the screen's foot in the 320 px field; the
// row above at 50 px, a pickup's name level with the middle of its icon.
constexpr const char* kBottom =
    "yb -24 xv 0 hnum xv 50 pic 0 "
    "if 2 xv 100 anum xv 150 pic 2 endif "
    "if 4 xv 200 rnum xv 250 pic 4 endif "
    "if 6 xv 296 pic 6 endif "
    "yb -50 "
    "if 7 xv 0 pic 7 xv 26 yb -42 stat_string 8 yb -50 endif ";
constexpr const char* kHelp = "if 11 xv 148 pic 11 endif ";
constexpr const char* kTimerSingle = "if 9 xv 262 num 2 10 xv 296 pic 9 endif ";
constexpr const char* kTimerMatch = "if 9 xv 246 num 2 10 xv 296 pic 9 endif ";
constexpr const char* kMatch =
    "xr -50 yt 2 num 3 14 "
    "if 17 xv 0 yb -58 string2 \"SPECTATOR MODE\" endif "
    "if 16 xv 0 yb -68 string \"Chasing\" xv 64 stat_string 16 endif ";

int16_t clamp16(double v) { return static_cast<int16_t>(std::clamp(v, -32768.0, 32767.0)); }

// The image index of `name`, registered at the map's start; 0 when it is not.
int image(const Q2Server& s, std::string_view name) {
  const auto& im = s.world.images;
  for (size_t i = 1; i < im.size(); i++)
    if (im[i] == name) return static_cast<int>(i);
  return 0;
}

// The classname of each ammo kind's item, by `SqAmmo`.
constexpr const char* kAmmoClass[kSqAmmoKinds] = {
    "",           "ammo_shells",     "ammo_bullets", "ammo_grenades",  "ammo_rockets",
    "ammo_cells", "ammo_slugs",      "ammo_magslug", "ammo_trap",      "ammo_flechettes",
    "ammo_prox",  "ammo_tesla",      "ammo_disruptor"};

int ammo_number(SqAmmo a, Q2Pack pack) {
  return a == SqAmmo::none ? 0 : sq_number_of(kAmmoClass[static_cast<int>(a)], pack);
}

// Player `slot`'s `hand` setting: 0 right, 1 left, 2 centre.
int hand_of(const Q2Server& s, int slot) {
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == "hand") return std::atoi(v.c_str());
  return 0;
}

}  // namespace

int sq_numbers(Q2Pack pack) { return static_cast<int>(numbered(pack).size()); }

int sq_number_of(std::string_view classname, Q2Pack pack) {
  const auto list = numbered(pack);
  for (size_t n = 1; n < list.size(); n++)
    if (classname == list[n].classname) return static_cast<int>(n);
  return 0;
}

int sq_item_number(int row, Q2Pack pack) {
  if (row < 0 || row >= kSqItemCount) return 0;
  const std::string_view cn = kSqItems[row].classname;
  if (kSqItems[row].kind == SqKind::health && cn.rfind("item_health", 0) == 0)
    return sq_number_of("item_health", pack);
  return sq_number_of(cn, pack);
}

int sq_weapon_number(int weapon, Q2Pack pack) {
  if (weapon == static_cast<int>(SqWeapon::blaster)) return sq_number_of("weapon_blaster", pack);
  if (weapon == static_cast<int>(SqWeapon::hand_grenade))
    return sq_number_of("ammo_grenades", pack);
  if (weapon < 0 || weapon >= kSqWeaponCount || kSqWeapons[weapon].item == nullptr) return 0;
  return sq_item_number(sq_item(kSqWeapons[weapon].item, pack), pack);
}

bool sq_weapon_numbered(int n, Q2Pack pack) {
  const Numbered* r = row_of(n, pack);
  if (r == nullptr) return false;
  const std::string_view cn = r->classname;
  if (cn == "weapon_blaster" || cn == "ammo_grenades" || cn == "ammo_tesla") return true;
  const int row = sq_item(cn, pack);
  return row >= 0 && kSqItems[row].kind == SqKind::weapon;
}

bool sq_powerup_numbered(int n, Q2Pack pack) {
  const Numbered* r = row_of(n, pack);
  if (r == nullptr) return false;
  const int row = sq_item(r->classname, pack);
  return row >= 0 && kSqItems[row].kind == SqKind::powerup;
}

bool sq_selects(int n, Q2Pack pack) {
  // The power armours, every weapon and the grenades, and the power-ups with a use.
  if (sq_weapon_numbered(n, pack) || sq_powerup_numbered(n, pack)) return true;
  const Numbered* r = row_of(n, pack);
  if (r == nullptr) return false;
  const int row = sq_item(r->classname, pack);
  return row >= 0 && kSqItems[row].kind == SqKind::power_armour;
}

int sq_count(const SqBody& b, int n, Q2Pack pack) {
  const Numbered* r = row_of(n, pack);
  if (r == nullptr) return 0;
  const std::string_view cn = r->classname;
  if (cn == "item_armor_shard" || cn == "item_health") return 0;
  if (cn == "weapon_blaster") return b.blaster ? 1 : 0;
  for (int a = 1; a < kSqAmmoKinds; a++)
    if (cn == kAmmoClass[a]) return b.ammo[a];
  const int row = sq_item(cn, pack);
  if (row < 0) return 0;
  if (kSqItems[row].kind == SqKind::armour)  // the armour worn: its points
    return static_cast<int>(b.armour) == kSqItems[row].amount ? b.armour_count : 0;
  return b.counts[row];
}

std::string_view sq_item_name(int n, Q2Pack pack) {
  const Numbered* r = row_of(n, pack);
  return r != nullptr ? r->name : std::string_view();
}

std::string_view sq_icon(int n, Q2Pack pack) {
  const Numbered* r = row_of(n, pack);
  return r != nullptr ? r->icon : std::string_view();
}

std::string sq_bar_program(bool match) {
  return std::string(kBottom) + kHelp + (match ? kTimerMatch : kTimerSingle) +
         (match ? kMatch : "");
}

void bar_start(Q2Server& s) {
  RulesWorld& w = s.world;
  w.status_bar = sq_bar_program(s.cvars.find("deathmatch")->on());
  w.images.assign(1, std::string());
  const auto add = [&](std::string_view name) {
    if (name.empty() || std::find(w.images.begin(), w.images.end(), name) != w.images.end()) return;
    w.images.emplace_back(name);
  };
  const auto list = numbered(s.opt.pack);
  add("i_health");
  for (size_t n = 1; n < list.size(); n++) add(list[n].icon);
  add("i_help");
  w.item_names.assign(list.size(), std::string());
  for (size_t n = 1; n < list.size(); n++) w.item_names[n] = list[n].name;
}

void bar_rows(Q2Server& s) {
  RulesWorld& w = s.world;
  if (w.images.empty()) return;
  const bool flash_on = (s.frame_number & 8) != 0;
  const bool match = s.cvars.find("deathmatch")->on();
  const Q2Pack pack = s.opt.pack;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    const Q2Player& p = s.players[i];
    int16_t* st = w.clients[i].numbers.stats;
    std::fill(st, st + 32, int16_t(0));
    st[0] = static_cast<int16_t>(image(s, "i_health"));
    st[1] = clamp16(std::max(p.health, -99.0f));  // never below −99
    const SqAmmo ammo = kSqWeapons[p.hand.weapon].ammo;
    if (ammo != SqAmmo::none) {
      st[2] = static_cast<int16_t>(image(s, sq_icon(ammo_number(ammo, pack), pack)));
      st[3] = clamp16(p.body.ammo[static_cast<int>(ammo)]);
    }
    const int cells = p.body.ammo[static_cast<int>(SqAmmo::cells)];
    const bool worn = p.body.armour != SqArmour::none && p.body.armour_count > 0;
    if (p.power_on && cells > 0 && (!worn || flash_on)) {
      st[4] = static_cast<int16_t>(image(s, "i_powershield"));
      st[5] = clamp16(cells);
    } else if (worn) {
      constexpr const char* kWorn[] = {"", "item_armor_jacket", "item_armor_combat",
                                       "item_armor_body"};
      st[4] = static_cast<int16_t>(
          image(s, sq_icon(sq_number_of(kWorn[static_cast<int>(p.body.armour)], pack), pack)));
      st[5] = clamp16(p.body.armour_count);
    }
    if (p.body.selected > 0) st[6] = static_cast<int16_t>(image(s, sq_icon(p.body.selected, pack)));
    st[12] = static_cast<int16_t>(p.body.selected > 0 ? p.body.selected : -1);
    if (p.picked > 0 && s.level_time <= p.picked_at + 3.0) {
      st[7] = static_cast<int16_t>(image(s, sq_icon(p.picked, pack)));
      st[8] = static_cast<int16_t>(1056 + p.picked);
    }
    if (const SqPower* pw = sq_first_power(p, s.level_time)) {
      st[9] = static_cast<int16_t>(image(s, sq_icon(sq_number_of(pw->item, pack), pack)));
      // Whole seconds left, rounded down, from the frames left.
      const auto frames = static_cast<int64_t>((p.*pw->until - s.level_time) * 10.0 + 1e-6);
      st[10] = static_cast<int16_t>(frames / 10);
    }
    // The help slot: the help computer's icon blinking while unread; else, with the gun not drawn
    // in the view (`hand` 2, or a field of view over 91) and the host's own `cl_gun` not 2, the
    // weapon in hand.
    const con::Cvar* gun = s.cvars.find("cl_gun");
    if (p.help_unread >= 1 && flash_on) {
      st[11] = static_cast<int16_t>(image(s, "i_help"));
    } else if (p.help_unread == 0 && (gun == nullptr ? 2 : gun->number) != 2 &&
               (hand_of(s, i) == 2 || fight_fov(s, i) > 91)) {
      st[11] = static_cast<int16_t>(image(s, sq_icon(sq_weapon_number(p.hand.weapon, pack), pack)));
    }
    // A screen over the bar; the scoreboard always while dead or between levels in deathmatch.
    const bool dead = !p.alive && !s.clients[i].spectator;  // a spectator is not dead
    if (p.screen != kScreenNone || (match && (dead || s.intermission_at > 0))) st[13] |= 1;
    if (p.inven_open && p.alive) st[13] |= 2;  // a screen over the bar (q2_screens.hpp)
    st[14] = clamp16(p.frags);
    // A blow in this frame, or between the last frame and this one (a move's fall), shows once a
    // frame's rows are sent.
    if (p.hit_frame >= s.frame_number - 1) st[15] = static_cast<int16_t>(p.hit_bits);
  }
  // A spectator's flag, and a chaser's view of his target's bar (every player's filled first).
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active) spectate_stats(s, i, w.clients[i].numbers.stats);
}

}  // namespace shooters_edge::server
