// engine/render/impact.hpp: every surface class has its look, found by its id and by the surface's
// word (the sky none, an unknown word stone's); a knob set by name (the dust's as "dust.<knob>")
// lands clamped; the rounds are found; the burst's axis leans from the normal toward the round's
// glance by `ricochet` and never into the surface; the dust is a blast on the surface carried out
// of it; a hit throws the same things from the same seed, all out of the surface, metal sparks and
// wood none, a liquid only beads, and a pellet less than a nail; the pool takes each kind with its
// own flight and a chip with its surface's shade.
//
// ge:summary Tests for impacts: per-surface looks, knobs, rounds, the axis, the dust, the throw
// ge:keywords test render impact hit surface material ricochet dust chips sparks drops knobs
// ge:skill render testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "render/explosions.hpp"
#include "render/impact.hpp"

namespace {

namespace r = ge::render;

// A nail into a wall facing -z, the round going +z and down a little.
r::Impact wall_hit(const char* surface, const char* round = "round.nail", uint32_t seed = 7) {
  r::Impact h;
  h.at[2] = 10;
  h.normal[1] = 0;
  h.normal[2] = -1;
  const double d = std::sqrt(0.6 * 0.6 + 0.8 * 0.8);
  h.dir[0] = 0.6 / d;
  h.dir[1] = 0;
  h.dir[2] = 0.8 / d;
  h.look = r::impact_look_for(surface);
  h.round = r::impact_round_by_id(round);
  h.seed = seed;
  h.born_s = 2;
  return h;
}

std::vector<r::Spark> thrown(const r::Impact& h) {
  std::vector<r::Spark> out(r::impact_births_max(r::look_of(h), r::round_of(h)) + 4);
  out.resize(r::impact_births(h, out));
  return out;
}

int count(const std::vector<r::Spark>& s, r::SparkKind k) {
  int n = 0;
  for (const r::Spark& x : s) n += x.kind == k ? 1 : 0;
  return n;
}

}  // namespace

TEST(every_surface_class_has_its_look) {
  for (const char* s :
       {"stone", "dirt", "grass", "sand", "wood", "metal", "tile", "liquid", "unknown"}) {
    const r::ImpactLook* l = r::impact_look_for(s);
    CHECK(l != nullptr);
    if (l == nullptr) continue;
    CHECK(l->id == std::string("impact.") + s);
    CHECK(r::impact_look_by_id(l->id) == l);
  }
  CHECK(r::impact_look_for("sky") == nullptr);
  CHECK(r::impact_look_for("cheese") == &r::default_impact());
  CHECK(r::default_impact().id == "impact.stone");
  CHECK(&r::look_of(r::Impact{}) == &r::default_impact());
  CHECK(r::impact_round_by_id("round.pellet") != nullptr);
  CHECK(&r::round_of(r::Impact{}) == &r::default_round());
}

TEST(an_impact_knob_set_by_name_lands_clamped) {
  r::ImpactLook l = r::default_impact();
  CHECK(r::set_impact_knob(l, "chips", 12.6) && l.chips == 13);
  CHECK(r::set_impact_knob(l, "ricochet", 4) && l.ricochet == 1);  // clamped
  CHECK(r::set_impact_knob(l, "dust.smoke_time", 3) && l.dust.smoke_s == 3);
  CHECK(!r::set_impact_knob(l, "dust.colour", 1));
  CHECK(!r::set_impact_knob(l, "colour", 1));
  for (const r::ImpactKnob& k : r::impact_knobs()) {
    CHECK((k.number != nullptr) != (k.count != nullptr));
    CHECK(k.lo < k.hi && !k.what.empty());
    for (const r::ImpactLook& p : r::impact_looks()) {
      const double v = k.number ? p.*k.number : p.*k.count;
      CHECK(v >= k.lo && v <= k.hi);
    }
  }
}

TEST(the_burst_leans_toward_the_glance_and_stays_out_of_the_surface) {
  const r::Impact h = wall_hit("stone");
  double a[3];
  r::impact_axis(h, 0, a);
  CHECK(std::abs(a[2] + 1) < 1e-12);  // the normal
  r::impact_axis(h, 1, a);
  CHECK(a[0] > 0.5 && a[2] < 0);  // on along the round's glance, still out of the wall
  // A round straight in: every lean is the normal.
  r::Impact square = h;
  square.dir[0] = 0;
  square.dir[2] = 1;
  r::impact_axis(square, 0.8, a);
  CHECK(std::abs(a[2] + 1) < 1e-9);
  // A grazing round: lifted off the surface.
  r::Impact graze = h;
  graze.dir[0] = 1;
  graze.dir[2] = 0;
  r::impact_axis(graze, 1, a);
  CHECK(-a[2] >= 0.14);
}

TEST(the_dust_is_a_blast_on_the_surface_carried_out_of_it) {
  const r::Impact h = wall_hit("dirt");
  r::Blast b;
  CHECK(r::dust_of(h, b));
  CHECK(b.on_surface && b.look == &r::impact_look_for("dirt")->dust);
  CHECK(b.at[2] < 10 && b.v[2] < 0 && b.born_s == 2 && b.seed == 7);
  CHECK(r::fireball_at(b, 2.1).alive && r::fireball_at(b, 2.1).fire == 0);  // dust, not fire
  r::Impact pellet = wall_hit("dirt", "round.pellet");
  r::Blast pb;
  CHECK(r::dust_of(pellet, pb) && pb.scale < b.scale);
  r::ImpactLook none = r::default_impact();
  none.dust.smoke = 0;
  pellet.look = &none;
  CHECK(!r::dust_of(pellet, pb));
}

TEST(a_hit_throws_its_surfaces_burst_the_same_from_the_same_seed) {
  const std::vector<r::Spark> a = thrown(wall_hit("stone")), b = thrown(wall_hit("stone"));
  CHECK(a.size() == b.size() && !a.empty());
  for (size_t i = 0; i < a.size(); ++i) {
    CHECK_BITS(a[i].v[0], b[i].v[0]);
    CHECK_BITS(a[i].life_s, b[i].life_s);
  }
  const std::vector<r::Spark> c = thrown(wall_hit("stone", "round.nail", 8));
  CHECK(c.size() == a.size() && c[0].v[0] != a[0].v[0]);
  // Everything leaves the wall.
  for (const char* s : {"stone", "metal", "wood", "dirt", "sand", "tile", "liquid"}) {
    for (const r::Spark& x : thrown(wall_hit(s))) CHECK(x.v[2] < 0);
  }
  // Metal sparks and throws no dust of its own; wood splinters and strikes none; a liquid only
  // beads; a pellet throws less than a nail and strikes fewer sparks.
  const std::vector<r::Spark> metal = thrown(wall_hit("metal")), wood = thrown(wall_hit("wood"));
  const std::vector<r::Spark> liquid = thrown(wall_hit("liquid"));
  CHECK(count(metal, r::SparkKind::spark) > 3 * count(a, r::SparkKind::spark));
  CHECK(count(wood, r::SparkKind::spark) == 0 && count(wood, r::SparkKind::chip) > 0);
  CHECK(count(liquid, r::SparkKind::drop) == static_cast<int>(liquid.size()));
  const std::vector<r::Spark> pm = thrown(wall_hit("metal", "round.pellet"));
  CHECK(pm.size() < metal.size());
  CHECK(count(pm, r::SparkKind::spark) < count(metal, r::SparkKind::spark) / 2);
  // Metal's sparks go on along the glance (+x), dirt's clods straight out.
  double mx = 0;
  for (const r::Spark& x : metal) mx += x.kind == r::SparkKind::spark ? x.v[0] : 0;
  CHECK(mx > 0);
  // A span too small is filled and no more.
  std::vector<r::Spark> two(2);
  CHECK(r::impact_births(wall_hit("stone"), two) == 2);
}

TEST(the_pool_flies_each_kind_its_own_way) {
  r::Spark chip{r::SparkKind::chip, {0, 0, 0}, {1, 0, 0}, 1, 0.01, 0, 1, 0.8};
  CHECK(std::abs(r::spark_particle(chip).albedo - 0.8f) < 1e-6f);
  chip.albedo = -1;
  CHECK(r::spark_particle(chip).albedo == 0.4f);  // its kind's
  const r::Spark grit{r::SparkKind::grit, {}, {}, 1, 0.006, 0, 1};
  const r::Spark drop{r::SparkKind::drop, {}, {}, 1, 0.01, 0, 1};
  CHECK(r::spark_particle(grit).fall < 0.5f && r::spark_particle(grit).drag > 1);
  CHECK(r::spark_particle(drop).albedo > 0.7f && r::spark_particle(drop).fall == 1);
}
