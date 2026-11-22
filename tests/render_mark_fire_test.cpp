// engine/render/mark_fire.hpp: a hot mark burns in a few small gas flames at once, standing on the
// mark and staying where they were lit, shorter and fewer as it cools (the weak ones out first)
// and out by 70 % of its glow; a beam's burnt line burns along it, a big mark in more and
// taller flames up to a cap, a mark on a ceiling in none; smoke starts as the flames die, is
// thickest late in the glow and gone a little after it; a scorch, a hole or blood does neither;
// the smoke lies on the mark, rises and stays a handful of wisps; the same frames throw the same.
//
// ge:summary Tests for a hot mark's flames and smoke as it cools
// ge:keywords test render marks burn smoke flames mark_fire_births mark_flames smoulders
// ge:skill render testing
#include <cstring>
#include <vector>

#include "check.hpp"
#include "render/mark_fire.hpp"
#include "render/marks.hpp"

namespace {

namespace r = ge::render;
namespace gd = ge::detail;

// A wall's face at z = 10 facing -z, 20 m wide and 4 m tall, and a ceiling at y = 4 facing down.
gd::TriMesh room() {
  gd::TriMesh m;
  m.pos = {{-10, 0, 10}, {-10, 4, 10}, {10, 4, 10}, {10, 0, 10},
           {-10, 4, 0},  {10, 4, 0},   {10, 4, 10}, {-10, 4, 10}};
  m.idx = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
  m.tag = {0, 0, 1, 1};
  return m;
}

struct Wall {
  gd::TriMesh mesh = room();
  gd::Receivers rec = gd::bin_receivers(mesh, 2);
  gd::DecalPool pool = gd::make_pool(16);
};

int glow(Wall& w, double y, double z, double ny, double nz, r::GlowMark mark) {
  const double at[3] = {0, y, z}, n[3] = {0, ny, nz};
  return r::press_glow(w.pool, w.rec, at, n, mark, 0, 1);
}

// The smoke over [from, to) at 60 frames a second off one slug's mark pressed at t = 0.
std::vector<r::Particle> smoke(double from, double to) {
  Wall w;
  CHECK(glow(w, 2, 10, 0, -1, r::GlowMark{0.32, 3.0}) >= 0);
  r::MarkFire s;
  std::vector<r::Particle> out;
  constexpr double dt = 1.0 / 60;
  for (int i = 1; i * dt < to; ++i) {
    const double t = i * dt;
    if (t < from) continue;
    r::mark_fire_births(s, r::mark_fire_look(), w.pool, t, dt, out);
  }
  return out;
}

std::vector<r::Flame> flames_at(const gd::DecalPool& pool, double t) {
  std::vector<r::Flame> out;
  r::mark_flames(r::mark_fire_look(), pool, t, out);
  return out;
}

}  // namespace

TEST(the_flames_come_first_and_the_smoke_after) {
  const r::MarkFireLook& look = r::mark_fire_look();
  const r::MarkFireLevel fresh = r::mark_fire_level(look, 0, 3);
  CHECK(fresh.fire == 1);
  CHECK(fresh.smoke == 0);
  const r::MarkFireLevel late = r::mark_fire_level(look, 0.75 * 3, 3);
  CHECK(late.fire == 0);
  CHECK(late.smoke > 0.99);
  CHECK(r::mark_fire_level(look, 1.2 * 3, 3).smoke > 0);   // on a little past the glow
  CHECK(r::mark_fire_level(look, 1.7 * 3, 3).smoke == 0);  // and gone
}

TEST(a_slugs_mark_burns_in_a_few_small_flames_that_die_down) {
  Wall w;
  CHECK(glow(w, 2, 10, 0, -1, r::GlowMark{0.32, 3.0}) >= 0);
  const std::vector<r::Flame> fresh = flames_at(w.pool, 0.05);
  CHECK(!fresh.empty() && fresh.size() <= 3);
  double tallest = 0;
  for (const r::Flame& f : fresh) {
    CHECK(f.height_m > 0.03f && f.height_m < 0.15f);  // ten centimetres or so
    CHECK(f.base[0] > -0.25 && f.base[0] < 0.25 && f.base[1] > 1.75 && f.base[1] < 2.25);
    CHECK(f.base[2] < 10 && f.base[2] > 9.97);  // just off the wall
    tallest = std::max(tallest, static_cast<double>(f.height_m));
  }
  // Cooling: no more of them and shorter, cooler gas; out by half its glow.
  const std::vector<r::Flame> later = flames_at(w.pool, 1.0);
  CHECK(later.size() <= fresh.size());
  CHECK(!later.empty());
  CHECK(later[0].height_m < tallest);
  CHECK(later[0].temperature_k < fresh[0].temperature_k);
  CHECK(flames_at(w.pool, 2.2).empty());
}

TEST(a_flame_stays_where_it_was_lit) {
  Wall w;
  CHECK(glow(w, 2, 10, 0, -1, r::GlowMark{0.6, 3.0}) >= 0);
  const std::vector<r::Flame> a = flames_at(w.pool, 0.1);
  const std::vector<r::Flame> b = flames_at(w.pool, 0.2);
  CHECK(a.size() == b.size() && !a.empty());
  for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
    CHECK(a[i].base[0] == b[i].base[0] && a[i].base[1] == b[i].base[1]);
    CHECK(a[i].seed == b[i].seed);
  }
}

TEST(a_big_mark_burns_in_more_and_taller_flames_up_to_a_cap) {
  Wall small, big;
  CHECK(glow(small, 2, 10, 0, -1, r::GlowMark{0.32, 3.0}) >= 0);
  CHECK(glow(big, 2, 10, 0, -1, r::GlowMark{1.9, 7.0}) >= 0);
  const std::vector<r::Flame> s = flames_at(small.pool, 0.05), b = flames_at(big.pool, 0.05);
  CHECK(b.size() > s.size());
  CHECK(static_cast<int>(b.size()) <= r::mark_fire_look().flames_per_mark);
  CHECK(b[0].height_m > s[0].height_m);
  CHECK(b[0].height_m <= static_cast<float>(r::mark_fire_look().flame_tallest_m) + 1e-6f);
}

TEST(a_mark_on_a_ceiling_burns_in_none) {
  Wall w;
  CHECK(glow(w, 4, 5, -1, 0, r::GlowMark{0.32, 3.0}) >= 0);
  CHECK(flames_at(w.pool, 0.05).empty());
}

TEST(a_beams_burnt_line_burns_along_it) {
  Wall w;
  // A line of pieces 0.18 m wide and 0.6 m long, laid end to end at t = 0.
  for (int i = 0; i < 6; ++i) {
    gd::DecalAt d;
    d.at = {-1.5 + 0.6 * i, 2, 10};
    d.normal = {0, 0, -1};
    d.size_m = 0.18;
    d.length_m = 0.6;
    d.along = {1, 0, 0};
    d.kind = gd::DecalKind::burn_line;
    d.variant = static_cast<uint32_t>(i);
    CHECK(gd::place_decal(w.pool, w.rec, d) >= 0);
  }
  const std::vector<r::Flame> f = flames_at(w.pool, 0.1);
  CHECK(f.size() >= 9);  // two a piece, a few with no glowing spot found
  double lo = 1e9, hi = -1e9;
  for (const r::Flame& q : f) lo = std::min(lo, q.base[0]), hi = std::max(hi, q.base[0]);
  CHECK(lo < -1 && hi > 1);  // along its length
}

TEST(a_slugs_mark_smokes_after_and_stops) {
  CHECK(smoke(0, 0.8).empty());  // the flames' time
  const std::vector<r::Particle> mid = smoke(1.5, 3.0);
  CHECK(!mid.empty());
  CHECK(smoke(5.0, 6.0).empty());
  // Subtle: a handful of wisps over its whole life.
  const std::vector<r::Particle> all = smoke(0, 6.0);
  CHECK(all.size() < 20);
  for (const r::Particle& p : all) {
    CHECK(p.kelvin == 0);
    CHECK(p.p[2] < 10 && p.p[2] > 9.9);  // a centimetre out of the wall
    CHECK(p.p[0] > -0.3 && p.p[0] < 0.3 && p.p[1] > 1.7 && p.p[1] < 2.3);
    CHECK(p.v[1] > 0);  // up
    CHECK(p.v[2] < 0);  // and out of the wall
  }
}

TEST(only_the_hot_kinds_smoulder) {
  CHECK(r::smoulders(gd::DecalKind::melt));
  CHECK(r::smoulders(gd::DecalKind::burn_line));
  CHECK(!r::smoulders(gd::DecalKind::scorch));
  CHECK(!r::smoulders(gd::DecalKind::bullet_hole));
  CHECK(!r::smoulders(gd::DecalKind::blood_pool));
  Wall w;
  gd::DecalAt d;
  d.at = {0, 2, 10};
  d.normal = {0, 0, -1};
  d.kind = gd::DecalKind::bullet_hole;
  CHECK(gd::place_decal(w.pool, w.rec, d) >= 0);
  r::MarkFire s;
  std::vector<r::Particle> out;
  for (int i = 1; i < 240; ++i)
    r::mark_fire_births(s, r::mark_fire_look(), w.pool, i / 60.0, 1 / 60.0, out);
  CHECK(out.empty());
  CHECK(flames_at(w.pool, 0.05).empty());
}

TEST(a_long_frame_throws_nothing) {
  Wall w;
  CHECK(glow(w, 2, 10, 0, -1, r::GlowMark{1.9, 7.0}) >= 0);
  r::MarkFire s;
  std::vector<r::Particle> out;
  CHECK(r::mark_fire_births(s, r::mark_fire_look(), w.pool, 5.0, 2.0, out) == 0);
}

TEST(the_same_frames_throw_the_same) {
  const std::vector<r::Particle> a = smoke(0, 4.0), b = smoke(0, 4.0);
  CHECK(a.size() == b.size());
  CHECK(!a.empty() && std::memcmp(a.data(), b.data(), a.size() * sizeof(r::Particle)) == 0);
}
