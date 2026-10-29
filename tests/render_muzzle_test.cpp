// engine/render/muzzle.hpp: the weapons' presets are found by their ids, and a knob set by name
// (the smoke's as "smoke.<knob>") lands clamped; the flame rides the muzzle's pose, swells along
// the barrel and burns out within its life, a rocket's long and a shotgun's pronged; the puff is
// born at the muzzle and carried down the barrel's line; the flash dies with the flame; a shot
// throws the same sparks from the same seed, inside its cone; and staging draws a flame
// stretched along its barrel beside the blasts.
//
// ge:summary Tests for muzzle fire: presets, knobs, the flame on its pose, smoke, flash, sparks
// ge:keywords test render muzzle muzzle-flash flame smoke sparks pose knobs preset stage
// ge:skill render testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "render/explosions.hpp"
#include "render/muzzle.hpp"

namespace {

namespace r = ge::render;

r::MuzzlePose pose_at(double x, double y, double z) {
  r::MuzzlePose p;
  p.at[0] = x;
  p.at[1] = y;
  p.at[2] = z;
  return p;  // pointing along +z, up +y
}

}  // namespace

TEST(weapons_are_found_by_their_ids) {
  for (const char* id : {"muzzle.shotgun", "muzzle.nailgun", "muzzle.grenade", "muzzle.rocket"}) {
    const r::MuzzleLook* l = r::muzzle_look_by_id(id);
    CHECK(l != nullptr && l->id == id);
  }
  CHECK(r::muzzle_look_by_id("explosion") == nullptr);
  CHECK(&r::look_of(r::Shot{}) == &r::default_muzzle());
}

TEST(a_muzzle_knob_set_by_name_lands_clamped) {
  r::MuzzleLook l = r::default_muzzle();
  CHECK(r::set_muzzle_knob(l, "length", 0.5) && l.length_m == 0.5);
  CHECK(r::set_muzzle_knob(l, "prongs", 3.4) && l.prongs == 3);
  CHECK(r::set_muzzle_knob(l, "life", 9) && l.life_s == 0.5);  // clamped
  CHECK(r::set_muzzle_knob(l, "smoke.smoke_time", 3) && l.smoke.smoke_s == 3);
  CHECK(!r::set_muzzle_knob(l, "smoke.colour", 1));
  CHECK(!r::set_muzzle_knob(l, "colour", 1));
  for (const r::MuzzleKnob& k : r::muzzle_knobs()) {
    CHECK((k.number != nullptr) != (k.count != nullptr));
    CHECK(k.lo < k.hi && !k.what.empty());
    for (const r::MuzzleLook& p : r::muzzle_looks()) {
      const double v = k.number ? p.*k.number : p.*k.count;
      CHECK(v >= k.lo && v <= k.hi);
    }
  }
}

TEST(the_flame_rides_the_muzzle_and_burns_out) {
  const r::Shot s{r::muzzle_look_by_id("muzzle.rocket"), 4, 1.0};
  const r::MuzzlePose a = pose_at(0, 1.5, 0), b = pose_at(0.1, 1.6, -0.05);
  CHECK(!r::flame_at(s, a, 0.99).alive);
  const r::Flame f0 = r::flame_at(s, a, 1.004), f1 = r::flame_at(s, a, 1.05);
  CHECK(f0.alive && f1.alive);
  CHECK(f1.stretch > 2);                              // a rocket's jet is long
  CHECK(f1.centre[2] > f0.centre[2]);                 // it shoots out down the barrel
  CHECK(f1.centre[2] - a.at[2] < 0.55 * 0.9 + 1e-9);  // its base at the muzzle
  CHECK(f0.fire > f1.fire && f0.core_k > f1.core_k);
  CHECK(!r::flame_at(s, a, 1.0 + r::look_of(s).life_s + 1e-3).alive);
  // The same moment on another pose: the flame is where the gun is now.
  const r::Flame moved = r::flame_at(s, b, 1.05);
  for (int i = 0; i < 3; ++i) {
    CHECK(std::fabs((moved.centre[i] - f1.centre[i]) - (b.at[i] - a.at[i])) < 1e-12);
  }
  // A shotgun's is wide and pronged, its first prong square to the barrel.
  const r::Shot g{r::muzzle_look_by_id("muzzle.shotgun"), 4, 0};
  const r::Flame fg = r::flame_at(g, a, 0.03);
  CHECK(fg.stretch < f1.stretch);
  CHECK(std::fabs(fg.side[2]) < 1e-12);
  CHECK_BITS(r::flame_at(g, a, 0.03).core_k, fg.core_k);  // pure
}

TEST(the_puff_is_carried_down_the_barrels_line) {
  const r::Shot s{r::muzzle_look_by_id("muzzle.shotgun"), 7, 2.0};
  const r::Blast puff = r::smoke_of(s, pose_at(0, 1.5, 0));
  CHECK(puff.born_s == 2.0 && !puff.on_surface);
  CHECK(puff.v[2] > 0 && puff.v[0] == 0);
  const r::Fireball early = r::fireball_at(puff, 2.05), late = r::fireball_at(puff, 2.8);
  CHECK(early.alive && late.alive);
  CHECK(late.centre[2] > early.centre[2] + 0.3);                   // carried on
  CHECK(late.centre[2] < 0.3 * 0.38 + 4 * r::kBlastDragS + 1e-9);  // and slowed by the air
  CHECK(early.fire == 0);                                          // smoke only
  r::Scorch mark{};
  CHECK(!r::scorch_of(puff, mark));  // in the open air, and its look leaves none
}

TEST(the_flash_dies_with_the_flame) {
  const r::Shot s{r::muzzle_look_by_id("muzzle.shotgun"), 1, 0};
  const r::MuzzlePose p = pose_at(0, 1.5, 0);
  const r::Flash hot = r::muzzle_flash_at(s, p, 0.01), late = r::muzzle_flash_at(s, p, 0.055);
  CHECK(hot.candela > 1e3);
  CHECK(late.candela < 0.2 * hot.candela);
  CHECK(r::muzzle_flash_at(s, p, 1).candela == 0);
  CHECK(hot.range_m == r::look_of(s).light_range_m);
}

TEST(a_shot_throws_the_same_sparks_inside_its_cone) {
  const r::Shot s{r::muzzle_look_by_id("muzzle.rocket"), 21, 0};
  const r::MuzzlePose p = pose_at(0, 1.5, 0);
  std::vector<r::Spark> a(r::shot_births_max(r::look_of(s))), b(a.size());
  const size_t n = r::shot_births(s, p, a);
  CHECK(n == r::look_of(s).sparks && r::shot_births(s, p, b) == n);
  for (size_t i = 0; i < n; ++i) {
    CHECK_BITS(a[i].v[1], b[i].v[1]);
    const double speed =
        std::sqrt(a[i].v[0] * a[i].v[0] + a[i].v[1] * a[i].v[1] + a[i].v[2] * a[i].v[2]);
    CHECK(a[i].v[2] / speed >= std::cos(r::look_of(s).spread) - 1e-9);
  }
}

TEST(staging_draws_a_flame_stretched_along_its_barrel) {
  r::Explosions ex;
  ex.staged.assign(r::kMaxExplosions, r::GpuBlast{});
  ex.order.reserve(256);
  r::FrameView v;
  v.eye[1] = 1.6;
  v.time_s = 0.02;
  const r::MuzzleFire fire{{r::muzzle_look_by_id("muzzle.rocket"), 3, 0}, pose_at(0.2, 1.4, 0.6)};
  r::Blast blast;
  blast.at[2] = 12;
  const std::vector<r::Blast> blasts = {blast};
  r::stage_explosions(ex, blasts, v, 160, 90, std::span<const r::MuzzleFire>(&fire, 1));
  CHECK(ex.drawn == 2);
  const r::GpuBlast& near = ex.staged[1];  // the flame is the nearer, drawn last
  CHECK(near.axis[2] == 1.0f && near.axis[3] > 2);
  CHECK(near.look[3] > near.centre[3] * near.axis[3]);               // its bound holds its length
  CHECK(ex.staged[0].axis[3] == 1.0f && ex.staged[0].side[3] == 0);  // the blast is a ball
  // Once its life is over nothing is staged for it.
  v.time_s = 0.5;
  r::stage_explosions(ex, {}, v, 160, 90, std::span<const r::MuzzleFire>(&fire, 1));
  CHECK(ex.drawn == 0);
}
