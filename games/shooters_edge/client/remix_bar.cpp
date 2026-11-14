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

#include "shooters_edge/client/classic_pictures.hpp"
#include "shooters_edge/server/sequel_bar.hpp"
#include "shooters_edge/server/sequel_server.hpp"

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

// The classic gun each of the sequel's weapons wears, in its weapon order (as kSequelAsClassic's
// view models): the blaster the shotgun, the shotguns the double, the machinegun and chaingun the
// nailguns, the launchers theirs, the hyperblaster, railgun and BFG the beam, the hand grenades
// the grenade launcher.
constexpr hud::Weapon kWorn[server::kSqWeaponCount] = {
    hud::Weapon::shotgun,         hud::Weapon::double_shotgun,  hud::Weapon::double_shotgun,
    hud::Weapon::nailgun,         hud::Weapon::heavy_nailgun,   hud::Weapon::grenade_launcher,
    hud::Weapon::rocket_launcher, hud::Weapon::beam_gun,        hud::Weapon::beam_gun,
    hud::Weapon::beam_gun,        hud::Weapon::grenade_launcher};

}  // namespace

void worn_hud_reset(WornHud& h, const RemixFiles& remix) {
  h.bar.pictures.clear();
  h.art = {};
  if (remix.looks == nullptr) return;
  h.art = load_classic_pictures(remix.files, false).hud;
  h.art.finish();
}

hud::Status worn_status(const server::SequelServer& sq, double t) {
  using server::SqAmmo;
  hud::Status s;
  s.t = t;
  const server::SequelPlayer& p = sq.players[0];
  const server::SqBody& b = p.body;
  s.health = static_cast<int>(p.health);
  s.dead = !p.alive;
  s.armour = b.armour_count;
  s.armour_class = static_cast<hud::ArmourClass>(b.armour);  // jacket, combat, body: light up
  const auto ammo = [&b](SqAmmo a) { return b.ammo[static_cast<int>(a)]; };
  const int w = std::clamp(p.hand.weapon, 0, server::kSqWeaponCount - 1);
  const server::SqAmmo held = server::kSqWeapons[size_t(w)].ammo;
  s.ammo = {ammo(SqAmmo::shells), ammo(SqAmmo::bullets),
            ammo(held == SqAmmo::grenades ? SqAmmo::grenades : SqAmmo::rockets),
            ammo(held == SqAmmo::slugs ? SqAmmo::slugs : SqAmmo::cells)};
  for (int k = 0; k < server::kSqWeaponCount; k++)
    if (server::sq_holds(b, k))
      s.owned = static_cast<uint8_t>(s.owned | (1u << (static_cast<int>(kWorn[k]) - 1)));
  s.weapon = kWorn[w];
  const double now = sq.level_time;
  const auto left = [now](double until) { return until > now ? until - now : 0.0; };
  s.power_s[size_t(hud::Power::damage)] = left(p.quad_until);
  s.power_s[size_t(hud::Power::shield)] = left(p.invulnerable_until);
  s.power_s[size_t(hud::Power::suit)] = left(p.suit_until);
  s.frags = static_cast<int>(p.frags);
  return s;
}

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
