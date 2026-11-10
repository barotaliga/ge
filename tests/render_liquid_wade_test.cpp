// engine/render/liquid_wade.hpp: a thicker liquid's waves are slower, longer and die sooner and its
// furrow closes more slowly, by the rules' powers (slime twice water, lava four times); a body
// wading is stamped onto the body it stands in a stamp a spacing, its newest the head, standing
// still a small one now and then, leaving it ends its head, a jump back clears; the surface's CPU
// twin has a trough on the track with its lips raised, closing over the furrow's time, a bow ahead,
// rings that run out at the waves' speed, and lava's crust torn along the furrow; a plunge heaves
// going out and craters coming down, its splash's drops fewer and lower the thicker the liquid.
//
// ge:summary Tests for wading: the viscosity's rules, the stamps, the furrow, bow and rings
// ge:keywords test render liquid wade wading viscosity furrow bow rings stamps wade_liquids
// ge:skill render water testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "render/liquid.hpp"
#include "render/liquid_wade.hpp"
#include "render/particles.hpp"

namespace {

namespace r = ge::render;

bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

// A body of `look` 10 m across round the origin, its level at 1 m and its floor at 0.
r::Liquids pool(const r::LiquidLook& look) {
  r::Liquids liquids;
  r::Liquids::Slot slot;
  slot.staged.info[0] = 1;
  slot.staged.info[1] = look.kind == r::LiquidKind::lava ? 0 : 1;
  slot.staged.box[0] = -5;
  slot.staged.box[1] = -5;
  slot.staged.box[2] = 5;
  slot.staged.box[3] = 5;
  slot.floor = 0;
  slot.wade = look.wade;
  const r::WadeWaves w = r::wade_waves(look.wade, look.kind == r::LiquidKind::lava);
  slot.staged.wade2[1] = static_cast<float>(w.life_s);
  liquids.bodies.push_back(slot);
  return liquids;
}

r::LiquidWader at(double x, double z, double y = 0.3) {
  r::LiquidWader w;
  w.id = 7;
  w.feet[0] = x;
  w.feet[1] = y;
  w.feet[2] = z;
  return w;
}

}  // namespace

TEST(a_thicker_liquid_moves_slower_and_holds_its_furrow_longer) {
  const r::LiquidLook& water_like = *r::find_liquid_look("slime-mucus");  // 2, as the slime
  const double v_slime = r::find_liquid_look("slime")->wade.viscosity;
  const double v_lava = r::find_liquid_look("lava")->wade.viscosity;
  CHECK(v_slime == 2);
  CHECK(v_lava == 4);
  CHECK(water_like.wade.viscosity == 2);
  r::WadeLook water;
  r::WadeLook lava;
  lava.viscosity = 4;
  const r::WadeWaves a = r::wade_waves(water), b = r::wade_waves(lava, true);
  CHECK(near(b.speed_mps, a.speed_mps / 2, 1e-12));
  CHECK(near(b.wavelength_m, a.wavelength_m * 2, 1e-12));
  CHECK(near(b.furrow_s, a.furrow_s * 4, 1e-12));
  CHECK(b.ring_s < a.ring_s);
  CHECK(b.speed_mps * b.ring_s < a.speed_mps * a.ring_s / 2);  // the rings reach under half as far
  CHECK(b.depth_m > a.depth_m);
  CHECK(a.heal_s == 0 && b.heal_s > b.furrow_s);
  CHECK(b.life_s >= 3 * b.heal_s && a.life_s >= 4 * a.furrow_s);
}

TEST(a_wader_is_stamped_a_spacing_apart_its_newest_the_head) {
  const r::LiquidLook& look = *r::find_liquid_look("slime");
  r::Liquids liquids = pool(look);
  r::LiquidWading wading;
  // Walking +x at 2 m/s from -3 m, 30 frames a second.
  for (int i = 0; i <= 60; ++i) {
    const double t = i / 30.0;
    const r::LiquidWader w = at(-3 + 2 * t, 0);
    r::wade_liquids(liquids, wading, {&w, 1}, t, true);
  }
  const std::vector<r::WadeStamp>& s = wading.stamps[0];
  CHECK(s.size() >= 8 && s.size() <= 10);  // 4 m at 0.45 m and the first
  int heads = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    heads += s[i].head ? 1 : 0;
    if (i > 0) CHECK(s[i].x - s[i - 1].x >= look.wade.spacing_m - 1e-9);
  }
  CHECK(heads == 1 && s.back().head);
  CHECK(near(s.back().vx, 2, 0.05));
  CHECK(s.back().strength == 1);  // in to the waist: feet 0.7 m under, a body 1.4 m tall
  CHECK(liquids.bodies[0].staged.wade0[0] == static_cast<float>(s.size()));
  // Standing still: a small stamp every kIdleS.
  const size_t was = s.size();
  for (int i = 61; i <= 90; ++i) {
    const r::LiquidWader w = at(1, 0);
    r::wade_liquids(liquids, wading, {&w, 1}, i / 30.0, true);
  }
  CHECK(s.size() == was + 1);
  CHECK(s.back().strength < 0.9);
  // Out of it: no head; a jump back clears.
  const r::LiquidWader dry = at(1, 0, 2);
  r::wade_liquids(liquids, wading, {&dry, 1}, 3.1, true);
  CHECK(!s.back().head);
  r::wade_liquids(liquids, wading, {&dry, 1}, 1.0, true);
  CHECK(wading.stamps[0].empty());
  CHECK(liquids.bodies[0].staged.wade0[0] == 0);
}

TEST(the_surface_has_a_furrow_with_lips_a_bow_and_rings) {
  r::WadeLook look;
  look.viscosity = 2;
  const r::WadeWaves w = r::wade_waves(look);
  // A body that walked +x from -2 to 0 at 2 m/s, stamped every 0.25 s, the last its head.
  std::vector<r::WadeStamp> s;
  for (int i = 0; i <= 4; ++i) {
    r::WadeStamp k;
    k.x = -2 + 0.5 * i;
    k.t = 0.25 * i;
    k.strength = 1;
    k.vx = i > 0 ? 2 : 0;
    k.span_s = i > 0 ? 0.25 : 0;
    k.head = i == 4;
    s.push_back(k);
  }
  const double t = 1.05;
  const r::WadeAt mid = r::wade_height(s, look, false, -0.6, 0, t, false);
  const r::WadeAt lip = r::wade_height(s, look, false, -0.6, look.leg_m * 1.4, t, false);
  CHECK(mid.height_m < -0.5 * w.depth_m * std::exp(-0.6 / w.furrow_s));
  CHECK(lip.height_m > 0);
  CHECK(mid.furrow > 0.3 && mid.open == 0);  // a slime: no crust to tear
  // Older track has closed more.
  const r::WadeAt old = r::wade_height(s, look, false, -1.9, 0, t, false);
  CHECK(old.height_m > mid.height_m);
  // The bow ahead of the head (drawn on 0.05 s to x = 0.1).
  const r::WadeAt bow = r::wade_height(s, look, false, 0.1 + 1.2 * look.leg_m, 0, t, false);
  CHECK(bow.height_m > 0.2 * w.depth_m);
  // A ring: the first stamp's packet at its front, nothing well past it.
  std::vector<r::WadeStamp> one{s[0]};
  one[0].head = false;
  const double front = w.speed_mps * 1.0;
  double most = 0;
  for (int i = -10; i <= 10; ++i) {
    const double x = -2 + front + i * w.wavelength_m / 20;
    most = std::fmax(most, std::fabs(r::wade_height(one, look, false, x, 0, 1.0).height_m));
  }
  CHECK(most > 0.1 * w.amp_m);
  CHECK(std::fabs(
            r::wade_height(one, look, false, -2 + front + 4 * w.wavelength_m, 0, 1.0).height_m) <
        1e-4);
  // Lava: the furrow torn open, healing slower than it closes.
  r::WadeLook thick;
  thick.viscosity = 4;
  const r::WadeAt torn = r::wade_height(s, thick, true, -0.6, 0, t, false);
  CHECK(torn.open > 0.5);
  const r::WadeAt healed = r::wade_height(s, thick, true, -0.6, 0, t + 30, false);
  CHECK(healed.open == 0 && healed.height_m == 0);
}

TEST(a_plunge_heaves_going_out_and_craters_and_splashes_coming_down) {
  const r::LiquidLook& look = *r::find_liquid_look("lava");
  r::Liquids liquids = pool(look);
  r::LiquidWading wading;
  CHECK(r::plunge_liquid(liquids, wading, 9, 1, 0, 1.0, true, 1) == -1);   // out of the box
  CHECK(r::plunge_liquid(liquids, wading, 0, 3, 0, 1.0, true, 1) == -1);   // far over the level
  CHECK(r::plunge_liquid(liquids, wading, 0, 1, 0, 1.0, true, 0.7) == 0);  // out: a heave
  CHECK(r::plunge_liquid(liquids, wading, 2, 1, 0, 1.0, false, 9) == 0);   // down: a crater
  r::wade_liquids(liquids, wading, {}, 1.0, true);
  CHECK(liquids.bodies[0].staged.wade0[0] == 2);
  CHECK(liquids.bodies[0].staged.wade_v[0][3] == 2 && liquids.bodies[0].staged.wade_v[1][3] == 0);
  const auto& s = wading.stamps[0];
  const r::WadeAt up = r::wade_height(s, look.wade, true, 0, 0, 1.05, false);
  const r::WadeAt down = r::wade_height(s, look.wade, true, 2, 0, 1.05, false);
  CHECK(up.height_m > 0.05);
  CHECK(down.height_m < -0.08);
  CHECK(down.open > 0.5 && down.open <= 1 && down.furrow <= 1);
  CHECK(s[1].strength == r::kPlungeMost);  // held to the strongest
  // Filled back in over the furrow's time; the rings run out at lava's slow pace.
  CHECK(std::fabs(r::wade_height(s, look.wade, true, 2, 0, 20, false).height_m) < 1e-6);
  // The splash: drops thrown a little way up, fewer and lower in the thicker liquid.
  std::vector<r::Particle> lava, slime;
  const double at[3] = {2, 1, 0};
  r::liquid_splash(liquids, 0, at, 1, 5, lava);
  r::Liquids thin = pool(*r::find_liquid_look("slime"));
  r::liquid_splash(thin, 0, at, 1, 5, slime);
  CHECK(!lava.empty() && lava.size() < slime.size());
  for (const r::Particle& d : lava) {
    CHECK(d.v[1] > 0 && d.v[1] <= 2.0f);  // up at most 0.2 m
    CHECK(d.kelvin < 0 && d.floor_y == 1.0f);
  }
  std::vector<r::Particle> again;
  r::liquid_splash(liquids, 0, at, 1, 5, again);
  CHECK(again.size() == lava.size() && again[0].v[1] == lava[0].v[1]);
}
