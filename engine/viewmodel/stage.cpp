// The labs' stage: the corridor built from quads, the named lights placed round the player, and
// the first-person frame drawn in two passes (the room, then the gun in its own depth range).
//
// ge:summary The labs' stage: the corridor's quads, the named lights, the two-pass frame
// ge:keywords viewmodel stage corridor_stage stage_lights draw_stage flare
// ge:skill render labs
// ge:test tests/viewmodel_raster_test.cpp
#include "viewmodel/stage.hpp"

#include <cmath>

#include "viewmodel/raster.hpp"

namespace ge::viewmodel {

namespace {

constexpr double kHalfWidth = 1.75, kHeight = 3.2, kBack = -3, kFar = 24;

// A quad a, b, c, d (in order round it) facing `normal`, one colour.
void quad(Stage& s, const double a[3], const double b[3], const double c[3], const double d[3],
          const float normal[3], const float rgb[3]) {
  const uint32_t base = static_cast<uint32_t>(s.positions.size() / 3);
  for (const double* p : {a, b, c, d}) {
    for (int k = 0; k < 3; ++k) {
      s.positions.push_back(static_cast<float>(p[k]));
      s.normals.push_back(normal[k]);
      s.colour.push_back(rgb[k]);
    }
  }
  // Wound so (b - a) x (c - a) points along the normal.
  const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
  const double cr[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                        e1[0] * e2[1] - e1[1] * e2[0]};
  if (cr[0] * normal[0] + cr[1] * normal[1] + cr[2] * normal[2] >= 0)
    s.indices.insert(s.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  else
    s.indices.insert(s.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
}

// A box's five faces that show from inside the corridor or round it: all six, outward.
void box(Stage& s, const double lo[3], const double hi[3], const float rgb[3]) {
  const double x0 = lo[0], y0 = lo[1], z0 = lo[2], x1 = hi[0], y1 = hi[1], z1 = hi[2];
  const float px[3] = {1, 0, 0}, nx[3] = {-1, 0, 0}, py[3] = {0, 1, 0}, ny[3] = {0, -1, 0},
              pz[3] = {0, 0, 1}, nz[3] = {0, 0, -1};
  const double v[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                          {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
  quad(s, v[1], v[2], v[6], v[5], px, rgb);
  quad(s, v[0], v[4], v[7], v[3], nx, rgb);
  quad(s, v[3], v[7], v[6], v[2], py, rgb);
  quad(s, v[0], v[1], v[5], v[4], ny, rgb);
  quad(s, v[4], v[5], v[6], v[7], pz, rgb);
  quad(s, v[0], v[3], v[2], v[1], nz, rgb);
}

}  // namespace

Stage corridor_stage() {
  Stage s;
  const float up[3] = {0, 1, 0}, down[3] = {0, -1, 0}, right[3] = {1, 0, 0}, left[3] = {-1, 0, 0};
  // The floor in metre flags, two greys of stone.
  for (int iz = static_cast<int>(kBack); iz < static_cast<int>(kFar); ++iz) {
    for (int ix = -2; ix < 2; ++ix) {
      const double x0 = ix < -1 ? -kHalfWidth : ix, x1 = ix > 0 ? kHalfWidth : ix + 1;
      const double a[3] = {x0, 0, double(iz)}, b[3] = {x0, 0, iz + 1.0}, c[3] = {x1, 0, iz + 1.0},
                   d[3] = {x1, 0, double(iz)};
      const bool odd = ((ix + iz) & 1) != 0;
      const float rgb[3] = {odd ? 0.30f : 0.36f, odd ? 0.29f : 0.34f, odd ? 0.27f : 0.31f};
      quad(s, a, b, c, d, up, rgb);
    }
  }
  // The walls in two-metre panels, the ceiling over them.
  const float wall[3] = {0.42f, 0.38f, 0.33f}, wall2[3] = {0.36f, 0.33f, 0.29f},
              ceil[3] = {0.25f, 0.24f, 0.23f};
  for (int i = 0; kBack + 2.0 * i < kFar; ++i) {
    const double z = kBack + 2.0 * i;
    const bool odd = i % 2 != 0;
    const double l0[3] = {-kHalfWidth, 0, z}, l1[3] = {-kHalfWidth, kHeight, z},
                 l2[3] = {-kHalfWidth, kHeight, z + 2}, l3[3] = {-kHalfWidth, 0, z + 2};
    quad(s, l0, l1, l2, l3, right, odd ? wall2 : wall);
    const double r0[3] = {kHalfWidth, 0, z}, r1[3] = {kHalfWidth, kHeight, z},
                 r2[3] = {kHalfWidth, kHeight, z + 2}, r3[3] = {kHalfWidth, 0, z + 2};
    quad(s, r0, r1, r2, r3, left, odd ? wall : wall2);
    const double c0[3] = {-kHalfWidth, kHeight, z}, c1[3] = {kHalfWidth, kHeight, z},
                 c2[3] = {kHalfWidth, kHeight, z + 2}, c3[3] = {-kHalfWidth, kHeight, z + 2};
    quad(s, c0, c1, c2, c3, down, ceil);
  }
  // Pillars against the walls every four metres, and a wall closing the far end.
  const float pillar[3] = {0.48f, 0.44f, 0.38f};
  for (int i = 0; 2.0 + 4.0 * i < kFar; ++i) {
    const double z = 2.0 + 4.0 * i;
    const double a0[3] = {-kHalfWidth, 0, z - 0.25},
                 a1[3] = {-kHalfWidth + 0.35, kHeight, z + 0.25};
    box(s, a0, a1, pillar);
    const double b0[3] = {kHalfWidth - 0.35, 0, z - 0.25}, b1[3] = {kHalfWidth, kHeight, z + 0.25};
    box(s, b0, b1, pillar);
  }
  const float back[3] = {0, 0, -1};
  const double e0[3] = {-kHalfWidth, 0, kFar}, e1[3] = {-kHalfWidth, kHeight, kFar},
               e2[3] = {kHalfWidth, kHeight, kFar}, e3[3] = {kHalfWidth, 0, kFar};
  quad(s, e0, e1, e2, e3, back, wall2);
  return s;
}

std::string_view stage_light_name(StageLight kind) {
  switch (kind) {
    case StageLight::day:
      return "day";
    case StageLight::dim:
      return "dim";
    case StageLight::dark:
      return "dark";
    case StageLight::torch:
      return "torch";
    case StageLight::flash:
      return "flash";
    case StageLight::coloured:
      return "coloured";
  }
  return "?";
}

StageLights stage_lights(StageLight kind, const camera::Pose& view, const float muzzle_view[3]) {
  StageLights l;
  auto set3 = [](float* d, float a, float b, float c) {
    d[0] = a;
    d[1] = b;
    d[2] = c;
  };
  auto lamp = [&](double x, double y, double dz, float r, float g, float b, float intensity,
                  float range, bool show) {
    PointLight p;
    p.position[0] = x;
    p.position[1] = y;
    p.position[2] = view.eye.z + dz;
    set3(p.colour, r, g, b);
    p.intensity = intensity;
    p.range_m = range;
    p.radius_m = 0.1f;
    l.lights.push_back(p);
    if (show)
      for (int k = 0; k < 3; ++k) l.sources.push_back(static_cast<float>(p.position[k]));
  };
  switch (kind) {
    case StageLight::day:
      set3(l.probe.ambient, 0.42f, 0.45f, 0.52f);
      set3(l.probe.direct, 0.95f, 0.88f, 0.76f);
      set3(l.probe.toward, -0.37f, 0.89f, 0.27f);
      break;
    case StageLight::dim:
      set3(l.probe.ambient, 0.12f, 0.12f, 0.13f);
      break;
    case StageLight::dark:
      set3(l.probe.ambient, 0, 0, 0);
      break;
    case StageLight::torch:
      set3(l.probe.ambient, 0.04f, 0.04f, 0.05f);
      lamp(-1.55, 2.1, 3.0, 1.0f, 0.55f, 0.22f, 3.5f, 10.0f, true);
      break;
    case StageLight::flash: {
      set3(l.probe.ambient, 0.03f, 0.03f, 0.035f);
      double at[3];
      view_to_world(view, muzzle_view, at);
      lamp(at[0], at[1], at[2] - view.eye.z, 1.0f, 0.78f, 0.42f, 0.35f, 6.0f, false);
      l.lights.back().radius_m = 0.2f;
      l.flare = true;
      for (int k = 0; k < 3; ++k) l.flare_at[k] = muzzle_view[k];
      break;
    }
    case StageLight::coloured:
      set3(l.probe.ambient, 0.05f, 0.05f, 0.06f);
      lamp(-1.5, 1.9, 3.4, 1.0f, 0.12f, 0.08f, 2.4f, 8.0f, true);
      lamp(1.5, 1.9, 3.4, 0.12f, 0.3f, 1.0f, 2.4f, 8.0f, true);
      break;
  }
  if (kind == StageLight::dark) l.floor = 0.06f;
  return l;
}

std::vector<uint8_t> draw_stage(const Stage& stage, const StageLights& lights,
                                const camera::Pose& view, const HeldModel& gun,
                                const Placement& place, const StageShot& shot) {
  const float black[3] = {0, 0, 0};
  ViewTarget t = make_target(shot.width, shot.height, shot.supersample, black);

  // The room, in the view's frame, lit with no floor and no gloss.
  std::vector<float> pos(stage.positions.size()), nrm(stage.normals.size());
  for (size_t i = 0; i + 2 < stage.positions.size(); i += 3) {
    const double w[3] = {stage.positions[i], stage.positions[i + 1], stage.positions[i + 2]};
    world_to_view(view, w, &pos[i]);
    world_to_view_dir(view, &stage.normals[i], &nrm[i]);
  }
  const float origin[3] = {0, 0, 2};
  Lighting room = light_held(lights.probe, lights.lights, view, origin, 0.0f);
  room.gloss = 0;
  DrawLit d;
  d.positions = pos;
  d.normals = nrm;
  d.indices = stage.indices;
  d.colour = stage.colour;
  d.fov_y = view.fov_y;
  d.near_m = 0.05;
  draw_lit(t, d, room);

  // The torches and lamps the room shows: small boxes lit by themselves.
  for (size_t i = 0; i + 2 < lights.sources.size(); i += 3) {
    Stage s;
    const double lo[3] = {lights.sources[i] - 0.06, lights.sources[i + 1] - 0.1,
                          lights.sources[i + 2] - 0.06};
    const double hi[3] = {lights.sources[i] + 0.06, lights.sources[i + 1] + 0.1,
                          lights.sources[i + 2] + 0.06};
    const PointLight& src = lights.lights[i / 3];
    const float rgb[3] = {src.colour[0], src.colour[1], src.colour[2]};
    box(s, lo, hi, rgb);
    std::vector<float> sp(s.positions.size()), sn(s.normals.size());
    for (size_t j = 0; j + 2 < s.positions.size(); j += 3) {
      const double w[3] = {s.positions[j], s.positions[j + 1], s.positions[j + 2]};
      world_to_view(view, w, &sp[j]);
      world_to_view_dir(view, &s.normals[j], &sn[j]);
    }
    Lighting self;
    for (int k = 0; k < 3; ++k) self.ambient[k] = 2.5f;
    self.gloss = 0;
    DrawLit ds = d;
    ds.positions = sp;
    ds.normals = sn;
    ds.indices = s.indices;
    ds.colour = s.colour;
    draw_lit(t, ds, self);
  }

  // The gun, in its own depth range and through its own lens.
  clear_depth(t);
  std::vector<float> gp, gn;
  place_held(gun, place, shot.frame_a, shot.frame_b, shot.frame_t, gp, gn);
  const float grip[3] = {static_cast<float>(place.m[3]), static_cast<float>(place.m[7]),
                         static_cast<float>(place.m[11])};
  const Lighting lit = light_held(lights.probe, lights.lights, view, grip, lights.floor);
  DrawLit g;
  g.positions = gp;
  g.normals = gn;
  g.indices = gun.indices;
  g.colour = gun.colour;
  g.uv = gun.uv;
  g.skin = gun.skin.image.rgba.empty() ? nullptr : &gun.skin;
  g.fov_y = place.fov_y > 0 ? place.fov_y : view.fov_y;
  g.near_m = 0.01;
  draw_lit(t, g, lit);

  // A muzzle flash: a small star of bright quads facing the eye at the muzzle.
  if (lights.flare) {
    Stage s;
    const float* c = lights.flare_at;
    const float facing[3] = {0, 0, -1};
    const float hot[3] = {1.0f, 0.85f, 0.5f};
    for (int k = 0; k < 2; ++k) {
      const double r = k == 0 ? 0.05 : 0.03, w = k == 0 ? 0.012 : 0.03;
      const double a[3] = {c[0] - r, c[1] - w, c[2]}, b[3] = {c[0] - r, c[1] + w, c[2]},
                   e[3] = {c[0] + r, c[1] + w, c[2]}, f[3] = {c[0] + r, c[1] - w, c[2]};
      quad(s, a, b, e, f, facing, hot);
      const double a2[3] = {c[0] - w, c[1] - r, c[2]}, b2[3] = {c[0] - w, c[1] + r, c[2]},
                   e2[3] = {c[0] + w, c[1] + r, c[2]}, f2[3] = {c[0] + w, c[1] - r, c[2]};
      quad(s, a2, b2, e2, f2, facing, hot);
    }
    Lighting self;
    for (int k = 0; k < 3; ++k) self.ambient[k] = 3.0f;
    self.gloss = 0;
    clear_depth(t);
    DrawLit df = g;
    df.positions = s.positions;
    df.normals = s.normals;
    df.indices = s.indices;
    df.colour = s.colour;
    df.uv = {};
    df.skin = nullptr;
    df.cull_back = false;
    draw_lit(t, df, self);
  }
  return resolve(t, shot.exposure);
}

}  // namespace ge::viewmodel
