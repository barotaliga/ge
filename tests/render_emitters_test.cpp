// engine/render/emitters.hpp: a drip falls from its ceiling at its rate, once a slot, and two spans
// that meet give each birth once, bit for bit as one span; embers are born over the floor, rise,
// glow and cool; motes fill the box; steam lifts off the floor; a long pause keeps its latest
// births only; the embers over a glowing volume stand on its floor at a rate its glow gives.
//
// ge:summary Tests for the area emitters: the cadence, each kind's births, the embers over a glow
// ge:keywords test render emitter embers motes drips steam emit_area embers_over cadence
// ge:skill render testing
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "render/emitters.hpp"

namespace {

namespace r = ge::render;

r::AreaEmitter box(r::AreaEmitter::Kind kind, double rate) {
  r::AreaEmitter e;
  e.kind = kind;
  e.centre[0] = 5;
  e.centre[1] = 2;
  e.centre[2] = -4;
  e.half[0] = 3;
  e.half[1] = 2;
  e.half[2] = 1;
  e.rate = rate;
  e.seed = 9;
  return e;
}

// The same bits, field by field (every field is a float).
bool same(const r::Particle& a, const r::Particle& b) {
  using Bits = std::array<uint32_t, sizeof(r::Particle) / 4>;
  return std::bit_cast<Bits>(a) == std::bit_cast<Bits>(b);
}

}  // namespace

TEST(drips_fall_from_the_ceiling_at_their_rate) {
  const r::AreaEmitter e = box(r::AreaEmitter::Kind::drips, 4);
  std::vector<r::Particle> out;
  r::emit_area(e, 10, 20, out);
  CHECK(out.size() == 40);
  for (const r::Particle& p : out) {
    CHECK(p.p[1] <= 4.0f && p.v[1] <= 0.0f);  // from the ceiling (y 4), falling
    CHECK(p.p[0] >= 2.0f && p.p[0] <= 8.0f && p.p[2] >= -5.0f && p.p[2] <= -3.0f);
    CHECK(p.fall == 1.0f && p.kelvin == 0.0f);
    // It lives long enough to reach the floor (y 0) from the ceiling: 4 m in about 0.9 s.
    CHECK(p.life > 0.9f && p.life < 1.0f);
  }
  // The one born last, at 19.75 s, has fallen a quarter second (in the pool's sixtieths, a few
  // centimetres past the exact fall).
  const r::Particle& young = out.back();
  CHECK(std::fabs(young.age - 0.25f) < 1e-5f);
  CHECK(std::fabs(young.p[1] - (4.0f - 0.5f * 9.81f * 0.0625f)) < 0.03f);
}

TEST(two_spans_that_meet_give_each_birth_once_as_one_span) {
  for (auto kind : {r::AreaEmitter::Kind::embers, r::AreaEmitter::Kind::motes,
                    r::AreaEmitter::Kind::drips, r::AreaEmitter::Kind::steam}) {
    const r::AreaEmitter e = box(kind, 13);
    std::vector<r::Particle> whole, a, b;
    r::emit_area(e, 3, 7, whole);
    r::emit_area(e, 3, 5.5, a);
    r::emit_area(e, 5.5, 7, b);
    CHECK(a.size() + b.size() == whole.size());
    CHECK(whole.size() >= 50 && whole.size() <= 54);  // 13 a second for 4 s
    // The births themselves are the same: placed and launched alike, flown to their own ends.
    std::vector<r::Particle> again;
    r::emit_area(e, 3, 5.5, again);
    CHECK(again.size() == a.size());
    for (size_t i = 0; i < a.size(); ++i) CHECK(same(a[i], again[i]));
  }
}

TEST(embers_rise_from_the_floor_glowing_and_cooling) {
  const r::AreaEmitter e = box(r::AreaEmitter::Kind::embers, 30);
  std::vector<r::Particle> out;
  r::emit_area(e, 0, 0.0001, out);  // none yet at this rate
  CHECK(out.empty());
  r::emit_area(e, 1, 2, out);
  CHECK(out.size() >= 29 && out.size() <= 31);
  for (const r::Particle& p : out) {
    CHECK(p.v[1] > 0.0f && p.fall < 0.0f);   // rising on the hot air
    CHECK(p.p[1] >= 0.0f && p.p[1] < 3.0f);  // off the floor's lowest third, at most a second up
    CHECK(p.kelvin >= 1300.0f && p.kelvin <= 1700.0f && p.cool_s > 0.0f);
  }
}

TEST(motes_fill_the_box_and_steam_lifts_off_its_floor) {
  std::vector<r::Particle> motes, steam;
  r::emit_area(box(r::AreaEmitter::Kind::motes, 200), 0, 1, motes);
  double lo = 1e9, hi = -1e9;
  for (const r::Particle& p : motes) {
    lo = std::min(lo, static_cast<double>(p.p[1]));
    hi = std::max(hi, static_cast<double>(p.p[1]));
  }
  CHECK(lo < 0.5 && hi > 3.5);
  r::emit_area(box(r::AreaEmitter::Kind::steam, 50), 0, 1, steam);
  for (const r::Particle& p : steam) CHECK(p.p[1] < 0.5f && p.v[1] > 0.0f);
}

TEST(a_long_pause_keeps_its_latest_births) {
  const r::AreaEmitter e = box(r::AreaEmitter::Kind::drips, 100);
  std::vector<r::Particle> out;
  r::emit_area(e, 0, 1000, out);
  CHECK(out.size() == r::kMaxAreaBirths);
  CHECK(out.back().age < 0.02f);
  out.clear();
  r::emit_area(e, 5, 5, out);  // an empty span
  r::emit_area(e, 6, 5, out);  // and a backward one
  CHECK(out.empty());
}

TEST(the_embers_over_a_glow_stand_on_its_floor_at_its_rate) {
  r::FogVolume v;
  v.centre[0] = 1;
  v.centre[2] = 2;
  v.half[0] = v.half[2] = 9;
  v.half[1] = 5;
  v.density = 0.12;
  v.glow[0] = 0.9;
  v.glow[1] = 0.32;
  v.glow[2] = 0.06;
  v.hug_ground = true;
  const r::AreaEmitter e = r::embers_over(v, 100, 3);
  CHECK(e.kind == r::AreaEmitter::Kind::embers && e.seed == 3u);
  CHECK(e.centre[1] == 105 && e.centre[0] == 1 && e.half[1] == 5);
  CHECK(e.rate >= 4 && e.rate <= 60);
  v.glow[0] = v.glow[1] = v.glow[2] = 0;
  CHECK(r::embers_over(v, 100).rate == 4);
}
