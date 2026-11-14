// The sequel's status bar, server side (sequel_bar.hpp).
//
// ge:summary The sequel's status bar from the server: its program, stats, pictures and item names
// ge:keywords sequel status-bar stats bar_rows bar_start
// ge:skill shooter ui
// ge:test tests/shooters_edge_sequel_bar_test.cpp
#include "shooters_edge/server/sequel_bar.hpp"

#include <algorithm>
#include <string>

#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::server {

namespace {

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
constexpr int kHealthNumber = 41;

// The bars (sequel_bar.hpp). The bottom row 24 px from the screen's foot in the 320 px field; the
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
int image(const SequelServer& s, std::string_view name) {
  const auto& im = s.world.images;
  for (size_t i = 1; i < im.size(); i++)
    if (im[i] == name) return static_cast<int>(i);
  return 0;
}

int ammo_number(SqAmmo a) {
  switch (a) {
    case SqAmmo::shells:
      return 18;
    case SqAmmo::bullets:
      return 19;
    case SqAmmo::grenades:
      return 12;
    case SqAmmo::rockets:
      return 21;
    case SqAmmo::cells:
      return 20;
    case SqAmmo::slugs:
      return 22;
    case SqAmmo::none:
      break;
  }
  return 0;
}

}  // namespace

int sq_item_number(int row) {
  if (row < 0 || row >= kSqItemCount) return 0;
  const std::string_view cn = kSqItems[row].classname;
  if (kSqItems[row].kind == SqKind::health && cn.rfind("item_health", 0) == 0) return kHealthNumber;
  for (int n = 1; n < kSqItemNumbers; n++)
    if (cn == kNumbered[n].classname) return n;
  return 0;
}

int sq_weapon_number(int weapon) {
  if (weapon == static_cast<int>(SqWeapon::blaster)) return kSqBlasterNumber;
  if (weapon == static_cast<int>(SqWeapon::hand_grenade)) return 12;
  if (weapon < 0 || weapon >= kSqWeaponCount || kSqWeapons[weapon].item == nullptr) return 0;
  return sq_item_number(sq_item(kSqWeapons[weapon].item));
}

bool sq_selects(int n) {
  // The power armours, every weapon and the grenades, and the five power-ups with a use.
  return (n >= 5 && n <= 17) || (n >= 23 && n <= 27);
}

std::string_view sq_icon(int n) {
  return n > 0 && n < kSqItemNumbers ? kNumbered[n].icon : std::string_view();
}

std::string sq_bar_program(bool match) {
  return std::string(kBottom) + kHelp + (match ? kTimerMatch : kTimerSingle) +
         (match ? kMatch : "");
}

void bar_start(SequelServer& s) {
  RulesWorld& w = s.world;
  w.status_bar = sq_bar_program(s.cvars.find("deathmatch")->on());
  w.images.assign(1, std::string());
  const auto add = [&](std::string_view name) {
    if (name.empty() || std::find(w.images.begin(), w.images.end(), name) != w.images.end()) return;
    w.images.emplace_back(name);
  };
  add("i_health");
  for (int n = 1; n < kSqItemNumbers; n++) add(kNumbered[n].icon);
  add("i_help");
  w.item_names.assign(kSqItemNumbers, std::string());
  for (int n = 1; n < kSqItemNumbers; n++) w.item_names[size_t(n)] = kNumbered[n].name;
}

void bar_rows(SequelServer& s) {
  RulesWorld& w = s.world;
  if (w.images.empty()) return;
  const bool flash_on = (s.frame_number & 8) != 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    const SequelPlayer& p = s.players[i];
    int16_t* st = w.clients[i].numbers.stats;
    std::fill(st, st + 32, int16_t(0));
    st[0] = static_cast<int16_t>(image(s, "i_health"));
    st[1] = clamp16(p.health);
    const SqAmmo ammo = kSqWeapons[p.hand.weapon].ammo;
    if (ammo != SqAmmo::none) {
      st[2] = static_cast<int16_t>(image(s, sq_icon(ammo_number(ammo))));
      st[3] = clamp16(p.body.ammo[static_cast<int>(ammo)]);
    }
    const int cells = p.body.ammo[static_cast<int>(SqAmmo::cells)];
    const bool worn = p.body.armour != SqArmour::none && p.body.armour_count > 0;
    if (p.power_on && cells > 0 && (!worn || flash_on)) {
      st[4] = static_cast<int16_t>(image(s, "i_powershield"));
      st[5] = clamp16(cells);
    } else if (worn) {
      st[4] = static_cast<int16_t>(image(s, sq_icon(static_cast<int>(p.body.armour) == 3   ? 1
                                                    : static_cast<int>(p.body.armour) == 2 ? 2
                                                                                           : 3)));
      st[5] = clamp16(p.body.armour_count);
    }
    if (p.body.selected > 0) st[6] = static_cast<int16_t>(image(s, sq_icon(p.body.selected)));
    st[12] = static_cast<int16_t>(p.body.selected > 0 ? p.body.selected : -1);
    if (p.picked > 0 && s.level_time <= p.picked_at + 3.0) {
      st[7] = static_cast<int16_t>(image(s, sq_icon(p.picked)));
      st[8] = static_cast<int16_t>(1056 + p.picked);
    }
    const struct {
      double until;
      int number;
    } timers[4] = {
        {p.quad_until, 23}, {p.invulnerable_until, 24}, {p.suit_until, 27}, {p.breather_until, 26}};
    for (const auto& t : timers) {
      if (t.until <= s.level_time) continue;
      st[9] = static_cast<int16_t>(image(s, sq_icon(t.number)));
      // Whole seconds left, rounded down, from the frames left.
      const auto frames = static_cast<int64_t>((t.until - s.level_time) * 10.0 + 1e-6);
      st[10] = static_cast<int16_t>(frames / 10);
      break;
    }
    if (p.help_unread >= 1 && flash_on) st[11] = static_cast<int16_t>(image(s, "i_help"));
    st[14] = clamp16(p.frags);
    // A blow in this frame, or between the last frame and this one (a move's fall), shows once a
    // frame's rows are sent.
    if (p.hit_frame >= s.frame_number - 1) st[15] = static_cast<int16_t>(p.hit_bits);
  }
}

}  // namespace shooters_edge::server
