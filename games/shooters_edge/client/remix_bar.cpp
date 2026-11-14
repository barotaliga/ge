// Classic remix's worn bar (remix_bar.hpp).
//
// ge:summary Classic remix's worn bar: the classic status as the sequel's stats, its bar run
// ge:keywords remix worn-bar worn_stats run_worn_bar sequel-hud
// ge:skill ui shooter
// ge:test tests/shooters_edge_remix_test.cpp
#include "shooters_edge/client/remix_bar.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "shooters_edge/server/sequel_bar.hpp"

namespace shooters_edge::client {

namespace {

// The pictures the stats name, by index (0 none), and the weapons' names a pickup shows.
constexpr std::string_view kImages[] = {"",
                                        "i_health",
                                        "a_shells",
                                        "a_bullets",
                                        "a_grenades",
                                        "a_rockets",
                                        "a_slugs",
                                        "i_jacketarmor",
                                        "i_combatarmor",
                                        "i_bodyarmor",
                                        "w_shotgun",
                                        "w_sshotgun",
                                        "w_machinegun",
                                        "w_chaingun",
                                        "w_glauncher",
                                        "w_rlauncher",
                                        "w_railgun",
                                        "p_quad",
                                        "p_invulnerability",
                                        "p_envirosuit"};
enum Image : uint8_t {
  kNone,
  kHealth,
  kShells,
  kBullets,
  kGrenades,
  kRockets,
  kSlugs,
  kJacket,
  kCombat,
  kBody,
  kGunFirst,  // the guns' icons, in hud::Weapon's order from the shotgun
  kQuad = kGunFirst + hud::kGuns,
  kInvulnerable,
  kSuit,
};
constexpr std::string_view kGunNames[hud::kGuns] = {
    "Shotgun",          "Super Shotgun",   "Machinegun", "Chaingun",
    "Grenade Launcher", "Rocket Launcher", "Railgun"};
constexpr int kItemNames = 1056;  // the item name strings' first index
constexpr double kPickupS = 3.0;  // how long a pickup shows
constexpr double kBlowS = 0.1;    // how long the blow just taken lights its field

int16_t clamp16(int v) { return static_cast<int16_t>(std::clamp(v, -32768, 32767)); }

int16_t ammo_icon(hud::Weapon w) {
  switch (w) {
    case hud::Weapon::shotgun:
    case hud::Weapon::double_shotgun:
      return kShells;
    case hud::Weapon::nailgun:
    case hud::Weapon::heavy_nailgun:
      return kBullets;
    case hud::Weapon::grenade_launcher:
      return kGrenades;
    case hud::Weapon::rocket_launcher:
      return kRockets;
    case hud::Weapon::beam_gun:
      return kSlugs;
    case hud::Weapon::melee:
      break;
  }
  return kNone;
}

}  // namespace

std::array<int16_t, hud::kLayoutStats> worn_stats(const hud::Status& st, bool match) {
  std::array<int16_t, hud::kLayoutStats> s{};
  s[0] = kHealth;
  s[1] = clamp16(st.health);
  if (const int16_t icon = ammo_icon(st.weapon); icon != kNone) {
    s[2] = icon;
    s[3] = clamp16(st.ammo_for(st.weapon));
  }
  if (st.armour > 0 && st.armour_class != hud::ArmourClass::none) {
    s[4] = static_cast<int16_t>(kJacket + static_cast<int>(st.armour_class) - 1);
    s[5] = clamp16(st.armour);
  }
  int latest = -1;
  for (int g = 0; g < hud::kGuns; g++)
    if (st.t - st.picked_at[size_t(g)] < kPickupS &&
        (latest < 0 || st.picked_at[size_t(g)] > st.picked_at[size_t(latest)]))
      latest = g;
  if (latest >= 0) {
    s[7] = static_cast<int16_t>(kGunFirst + latest);
    s[8] = static_cast<int16_t>(kItemNames + latest);
  }
  constexpr std::pair<hud::Power, int16_t> kPowers[] = {
      {hud::Power::damage, kQuad}, {hud::Power::shield, kInvulnerable}, {hud::Power::suit, kSuit}};
  for (const auto& [p, icon] : kPowers)
    if (st.powered(p)) {
      s[9] = icon;
      s[10] = clamp16(static_cast<int>(st.power_s[size_t(p)]));
      break;
    }
  if (match) s[14] = clamp16(st.frags);
  if (st.t - st.hurt_at < kBlowS) s[15] = 1;
  return s;
}

std::string_view worn_image(int index) {
  return index > 0 && index < static_cast<int>(std::size(kImages)) ? kImages[size_t(index)]
                                                                   : std::string_view();
}

void run_worn_bar(SequelBar& bar, const hud::Status& st, bool match, int64_t frame, int width,
                  int height) {
  bar.ops.clear();
  static const std::string kProgram[2] = {server::sq_bar_program(false),
                                          server::sq_bar_program(true)};
  const std::array<int16_t, hud::kLayoutStats> stats = worn_stats(st, match);
  const int scale = sequel_bar_scale(height);
  hud::LayoutInput in;
  in.program = kProgram[match ? 1 : 0];
  in.stats = std::span<const int16_t>(stats);
  in.width = width / scale;
  in.height = height / scale;
  in.frame = frame;
  in.self = 0;
  in.string = [](const void*, int index) -> std::string_view {
    if (index >= hud::kLayoutImages && index < hud::kLayoutImages + 256)
      return worn_image(index - hud::kLayoutImages);
    if (index >= kItemNames && index < kItemNames + hud::kGuns)
      return kGunNames[index - kItemNames];
    return {};
  };
  hud::run_layout(in, bar.ops);
}

bool draw_worn_bar(ge::ui::DrawList& dl, SequelBar& bar, const profiles::RemixArsenal* worn,
                   const hud::Status& st, bool match, int width, int height) {
  if (worn == nullptr || worn->items != "sequel") return false;
  run_worn_bar(bar, st, match, static_cast<int64_t>(st.t * 10), width, height);
  draw_sequel_bar(dl, bar, sequel_bar_scale(height));
  return true;
}

}  // namespace shooters_edge::client
