// games/shooters_edge/app/muzzle.hpp: every weapon's fire in the table is a look the engine knows,
// scaled with the gun; the melee weapon and the beam gun fire none; a shot's flame lives its life
// on the frame's pose and its sparks go into the births once; no gun leaves smoke, so the shot is
// gone with its flame; a shot of a weapon that throws nothing leaves nothing.
//
// ge:summary Tests for Shooter's Edge's muzzle fire: the looks, a shot's flame and sparks, no smoke
// ge:keywords test shooters-edge muzzle fire_looks step_muzzle flame no-smoke sparks
// ge:skill render shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/app/muzzle.hpp"
#include "shooters_edge/view/weapons.hpp"

namespace {

namespace fx = shooters_edge::effects;
namespace gr = ge::render;
namespace se = shooters_edge::view;

std::vector<gr::MuzzleLook> looks() {
  std::vector<fx::FireRow> rows;
  for (const se::WeaponLook& l : se::weapon_looks()) rows.push_back({l.muzzle, l.muzzle_knobs});
  std::vector<gr::MuzzleLook> out;
  std::string error;
  CHECK(fx::fire_looks(rows, se::kBuiltScale, out, &error));
  return out;
}

}  // namespace

TEST(every_weapons_fire_is_a_known_look_scaled_with_the_gun) {
  const std::vector<gr::MuzzleLook> l = looks();
  CHECK(l.size() == static_cast<size_t>(se::kWeapons));
  CHECK(!fx::fires(l[static_cast<size_t>(se::Weapon::melee)]));
  CHECK(!fx::fires(l[static_cast<size_t>(se::Weapon::beam_gun)]));
  const gr::MuzzleLook& shotgun = l[static_cast<size_t>(se::Weapon::shotgun)];
  CHECK(fx::fires(shotgun) && shotgun.length_m > 0.1 && shotgun.length_m < 0.2);
  const gr::MuzzleLook& rocket = l[static_cast<size_t>(se::Weapon::rocket_launcher)];
  CHECK(rocket.length_m > shotgun.length_m);
  std::vector<gr::MuzzleLook> out;
  std::string error;
  const fx::FireRow bad[] = {{"muzzle.shotgun", "nosuch=1"}};
  CHECK(!fx::fire_looks(bad, 1, out, &error) && !error.empty());
}

TEST(a_shot_lives_its_flame_and_leaves_no_smoke) {
  const std::vector<gr::MuzzleLook> l = looks();
  for (const gr::MuzzleLook& k : l) CHECK(k.smoke.smoke == 0);  // no gun smokes
  const gr::MuzzleLook& k = l[static_cast<size_t>(se::Weapon::shotgun)];
  fx::MuzzleShots s;
  gr::MuzzlePose pose;
  std::vector<gr::Particle> births;
  std::vector<gr::Blast> puffs;
  std::vector<gr::MuzzleFire> flames;
  fx::step_muzzle(s, &k, true, pose, 10.0, &births, &puffs, &flames);
  CHECK(flames.size() == 1 && puffs.empty() && births.size() == k.sparks);
  for (const gr::Particle& p : births) CHECK(p.grow == 0);  // sparks, never a puff
  // The flame follows this frame's pose.
  pose.at[0] = 0.5;
  births.clear();
  flames.clear();
  fx::step_muzzle(s, &k, false, pose, 10.0 + 0.5 * k.life_s, &births, &puffs, &flames);
  CHECK(flames.size() == 1 && flames[0].pose.at[0] == 0.5 && births.empty() && puffs.empty());
  flames.clear();
  // Its flame out, the shot is gone: nothing lingers where it went off.
  fx::step_muzzle(s, &k, false, pose, 10.0 + 2 * k.life_s, &births, &puffs, &flames);
  CHECK(flames.empty() && puffs.empty() && s.live.empty());
}

TEST(a_weapon_that_throws_nothing_leaves_nothing) {
  const std::vector<gr::MuzzleLook> l = looks();
  fx::MuzzleShots s;
  std::vector<gr::Particle> births;
  std::vector<gr::Blast> puffs;
  std::vector<gr::MuzzleFire> flames;
  fx::step_muzzle(s, &l[static_cast<size_t>(se::Weapon::melee)], true, {}, 1, &births, &puffs,
                  &flames);
  fx::step_muzzle(s, nullptr, true, {}, 1, &births, &puffs, &flames);
  CHECK(births.empty() && puffs.empty() && flames.empty() && s.live.empty());
}
