// engine/render/beam.hpp and beams.hpp: every preset is named and found; the knobs clamp and an
// unknown one is refused; the level is 0 before a shot, swells, holds near 1, flashes in a
// strike's first moments and dies after it is let go; the channel leaves the source and lands on
// the hit, stays near the axis by its pull (the arc closer than the bolt), is the same within a
// strike and new in the next, and is a pure function of the shot; the bolt forks and the braid
// winds three strands; the lights lie along the beam with the hit's at the surface and sum to
// the look's light; a held beam burns once a spot and again as it moves or dwells, never after
// it is let go; an ember cools to nothing; the staging is camera-relative and counts what it
// drops.
//
// ge:summary Tests for lightning beams: presets, knobs, level, channel walk, lights, burns, staging
// ge:keywords test render beam lightning strike channel fork braid lights burns ember staging
// ge:skill render testing
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "check.hpp"
#include "render/beam.hpp"
#include "render/beams.hpp"

namespace {

namespace r = ge::render;

r::BeamLook look_of(const char* name) {
  r::BeamLook l;
  CHECK(r::beam_look(name, l));
  return l;
}

r::BeamShot a_shot() {
  r::BeamShot s;
  s.from[0] = 1;
  s.from[1] = 1.4;
  s.from[2] = 2;
  s.to[0] = 3;
  s.to[1] = 1.0;
  s.to[2] = 14;
  s.normal[2] = -1;
  s.born_s = 1;
  s.seed = 9;
  return s;
}

// The largest distance of any main-channel point from the shot's axis, m.
double off_axis(const r::BeamShape& b, const r::BeamShot& s) {
  double d[3], len = 0;
  for (int k = 0; k < 3; ++k) {
    d[k] = s.to[k] - s.from[k];
    len += d[k] * d[k];
  }
  len = std::sqrt(len);
  double most = 0;
  for (int i = 0; i < b.count; ++i) {
    if (b.seg[i].glow < 1) continue;
    const float* p = b.seg[i].b;
    const double along = (p[0] * d[0] + p[1] * d[1] + p[2] * d[2]) / len;
    double perp = 0;
    for (int k = 0; k < 3; ++k) {
      const double q = p[k] - d[k] / len * along;
      perp += q * q;
    }
    most = std::fmax(most, std::sqrt(perp));
  }
  return most;
}

TEST(beam_presets_are_named_and_knobs_clamp) {
  for (int i = 0; i < r::kBeamLooks; ++i) {
    r::BeamLook l;
    CHECK(r::beam_look(r::beam_look_name(i), l));
  }
  r::BeamLook l;
  CHECK(!r::beam_look("nothing", l));
  CHECK(r::beam_look_name(r::kBeamLooks).empty());
  CHECK(r::set_beam_knob(l, "pull", 7));
  CHECK(l.pull == 1);
  CHECK(r::set_beam_knob(l, "filaments", 9));
  CHECK(l.filaments == r::kBeamFilaments);
  CHECK(!r::set_beam_knob(l, "colour", 1));
  const r::BeamKnob* k = r::beam_knobs();
  for (int i = 0; i < r::kBeamKnobs; ++i) CHECK(k[i].lo < k[i].hi && !k[i].name.empty());
}

TEST(beam_level_swells_holds_flashes_and_dies) {
  const r::BeamLook l = look_of("arc");
  r::BeamShot s = a_shot();
  s.end_s = 3;
  CHECK(r::beam_level(l, s, 0.5) == 0);
  CHECK(r::beam_level(l, s, 1.001) < r::beam_level(l, s, 1.2));
  // Held: within the flicker and the flash of 1.
  for (int i = 0; i < 131; ++i) {
    const double t = 1.2 + i * 0.0137;
    const float v = r::beam_level(l, s, t);
    CHECK(v > 1 - l.flicker - 1e-3 && v < (1 + l.flicker) * (1 + l.flash) + 1e-3);
  }
  // A strike's first moment is brighter than its last.
  const double strike = 1.0 / l.strike_hz;
  const double t0 = 1.5 + strike * 0.01, t1 = 1.5 + strike * 0.99;
  if (r::beam_strike(l, s, t0) == r::beam_strike(l, s, t1)) {
    CHECK(r::beam_level(l, s, t0) > r::beam_level(l, s, t1));
  }
  CHECK(r::beam_level(l, s, 3 + l.release_s * 0.5) > 0);
  CHECK(r::beam_level(l, s, 3 + l.release_s * 1.01) == 0);
}

TEST(beam_channel_runs_source_to_hit_and_strikes_anew) {
  const r::BeamShot s = a_shot();
  auto a = std::make_unique<r::BeamShape>();
  auto b = std::make_unique<r::BeamShape>();
  const r::BeamLook arc = look_of("arc");
  r::build_beam(arc, s, 0.5, *a);
  CHECK(a->count == 0);  // before the shot
  const double t = 1.5;
  r::build_beam(arc, s, t, *a);
  CHECK(a->count > 4);
  CHECK(a->seg[0].a[0] == 0 && a->seg[0].a[1] == 0 && a->seg[0].a[2] == 0);
  const r::BeamSegment& last = a->seg[a->count - 1];
  CHECK(std::fabs(last.b[0] - 2) < 1e-4 && std::fabs(last.b[1] + 0.4) < 1e-4 &&
        std::fabs(last.b[2] - 12) < 1e-4);
  // Pure, the same inside one strike, new in the next.
  r::build_beam(arc, s, t, *b);
  CHECK(b->count == a->count &&
        std::memcmp(a->seg, b->seg, sizeof(r::BeamSegment) * a->count) == 0);
  const double strike = 1.0 / arc.strike_hz;
  const double in = 1 + std::floor((t - 1) / strike) * strike + strike * 0.9;
  r::build_beam(arc, s, in, *b);
  CHECK(std::memcmp(a->seg, b->seg, sizeof(r::BeamSegment) * a->count) == 0);
  r::build_beam(arc, s, t + strike, *b);
  CHECK(std::memcmp(a->seg, b->seg, sizeof(r::BeamSegment) * a->count) != 0);
  // The arc keeps close to its axis; the bolt wanders further and forks.
  const r::BeamLook bolt = look_of("bolt");
  double arc_most = 0, bolt_most = 0;
  int forks = 0;
  for (int k = 0; k < 20; ++k) {
    const double tk = 1.2 + k * 0.071;
    r::build_beam(arc, s, tk, *a);
    r::build_beam(bolt, s, tk, *b);
    arc_most = std::fmax(arc_most, off_axis(*a, s));
    bolt_most = std::fmax(bolt_most, off_axis(*b, s));
    for (int i = 0; i < b->count; ++i) forks += b->seg[i].glow < 1 ? 1 : 0;
  }
  CHECK(arc_most < 0.35);
  CHECK(bolt_most > arc_most);
  CHECK(forks > 0);
  // The braid: three strands, each from the source to the hit.
  r::build_beam(look_of("braid"), s, t, *a);
  int starts = 0;
  for (int i = 0; i < a->count; ++i) {
    const float* p = a->seg[i].a;
    starts += (p[0] == 0 && p[1] == 0 && p[2] == 0) ? 1 : 0;
  }
  CHECK(starts == 3);
}

TEST(beam_lights_lie_along_it_and_at_the_hit) {
  const r::BeamLook l = look_of("arc");
  const r::BeamShot s = a_shot();
  r::LocalLight out[r::kBeamLights];
  CHECK(r::beam_lights(l, s, 0.2, out) == 0);
  const double t = 1.5;
  const int n = r::beam_lights(l, s, t, out);
  CHECK(n >= 3 && n <= r::kBeamLights);
  const float level = r::beam_level(l, s, t);
  CHECK(std::fabs(out[0].candela - l.root_cd * level) < 1e-3f * l.root_cd);
  CHECK(std::fabs(out[0].position[0] - s.from[0]) < 0.1);
  float along = 0;
  for (int i = 1; i + 1 < n; ++i) along += out[i].candela;
  CHECK(std::fabs(along - l.light_cd * level) < 1e-2f * l.light_cd);
  const r::LocalLight& hit = out[n - 1];
  CHECK(std::fabs(hit.position[2] - (s.to[2] - 0.15)) < 1e-9);
  CHECK(std::fabs(hit.candela - l.hit_cd * level) < 1e-3f * l.hit_cd);
}

TEST(beam_burns_trace_a_line_melt_where_held_and_stop_after_release) {
  const r::BeamLook l = look_of("arc");
  r::BeamShot s = a_shot();
  s.end_s = 10;
  r::BurnTrail trail;
  r::BeamBurn burn;
  CHECK(!r::beam_burns(l, s, 0.9, trail, burn));  // not fired yet
  CHECK(!r::beam_burns(l, s, 1.0, trail, burn));  // first landing: where the line starts
  // Swept 0.6 m along the wall (x) in one frame: a piece from the start to the hit, its ends
  // reaching half a width past each point.
  s.to[0] += 0.6;
  CHECK(r::beam_burns(l, s, 1.01, trail, burn));
  CHECK(burn.melt == 0 && burn.along[0] > 0.999);
  CHECK(std::fabs(burn.at[0] - (s.to[0] - 0.3)) < 1e-9);
  CHECK(std::fabs(burn.length_m - (0.6 + l.line_m)) < 1e-6);
  CHECK(!r::beam_burns(l, s, 1.01, trail, burn));  // the line has reached the hit
  // A sweep too fast for one piece: pieces of `line_most_m` until it reaches the hit.
  s.to[0] += 2;
  int pieces = 0;
  while (pieces < 8 && r::beam_burns(l, s, 1.02, trail, burn)) ++pieces;
  CHECK(pieces == 3);
  // Held on the spot: nothing until `melt_after_s`, then a melt that grows, pressed again every
  // `melt_every_s` under the same number.
  CHECK(!r::beam_burns(l, s, 1.02 + l.melt_after_s * 0.5, trail, burn));
  const double t0 = 1.02 + l.melt_after_s + 0.001;
  CHECK(r::beam_burns(l, s, t0, trail, burn));
  const uint32_t melt = burn.melt;
  const double small = burn.size_m;
  CHECK(melt > 0 && small >= l.melt_m && burn.along[0] == 0);
  CHECK(!r::beam_burns(l, s, t0 + l.melt_every_s * 0.5, trail, burn));
  CHECK(r::beam_burns(l, s, t0 + l.melt_grow_s * 0.5, trail, burn));
  CHECK(burn.melt == melt && burn.size_m > small);
  CHECK(r::beam_burns(l, s, t0 + l.melt_grow_s * 2, trail, burn));
  CHECK(std::fabs(burn.size_m - l.melt_most_m) < 1e-6);
  // A jump to another face starts a new line there, drawing nothing through the air.
  s.to[2] -= 1;
  s.normal[0] = 1, s.normal[2] = 0;
  CHECK(!r::beam_burns(l, s, 9.9, trail, burn));
  CHECK(!r::beam_burns(l, s, 10.1, trail, burn));  // let go
  CHECK(r::ember_level(l, 0) == l.ember_cd);
  CHECK(r::ember_level(l, l.ember_s * 0.5) < l.ember_cd * 0.2f);
  CHECK(r::ember_level(l, l.ember_s) == 0);
}

TEST(beams_stage_camera_relative_and_count_drops) {
  r::Beams beams;
  beams.settings.max_segments = 8;
  beams.settings.max_burn_verts = 6;
  beams.seg_staging.assign(8, r::GpuBeamSeg{});
  beams.burn_staging.assign(6, r::GpuBurnVert{});
  const r::BeamLook l = look_of("arc");
  const r::BeamShot s = a_shot();
  auto shape = std::make_unique<r::BeamShape>();
  r::build_beam(l, s, 1.5, *shape);
  const r::BeamDraw d{shape.get(), &l, 3};
  std::vector<r::BurnVertex> burns(7);
  burns[0].at[0] = 5;
  r::FrameView v;
  v.eye[0] = 1;
  v.eye[1] = 1;
  v.eye[2] = 1;
  r::stage_beams(beams, std::span(&d, 1), burns, v, 400);
  CHECK(beams.seg_count == 8);
  CHECK(beams.burn_count == 6);
  CHECK(beams.dropped == static_cast<uint32_t>(shape->count - 8) + 1);
  CHECK(beams.seg_staging[0].a[0] == 0 && beams.seg_staging[0].a[1] == 0.4f &&
        beams.seg_staging[0].a[2] == 1);
  CHECK(beams.burn_staging[0].at[0] == 4);
  CHECK(beams.staged.info[0] == 1 && beams.staged.looks[0].core[3] == l.core_cd * shape->level);
}

}  // namespace
