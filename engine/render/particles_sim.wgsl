// THE PARTICLES' FLIGHT (render/particles.hpp): one thread a slot of the ring. A live particle
// falls by its share of gravity, drags toward the air (the game's wind), moves and ages; at the ground (the
// terrain's heights) a grain is gone and a fine one comes to rest on the snow. `fly` is
// step_particle's twin in particles.cpp. In a pool that collides, a particle that falls meets what
// the last frame drew (`off_surface`, collide_particle's twin): the depth target still holds it.
#include "render/wgsl/terrain_common.wgsl"
#include "render/wgsl/particle_types.wgsl"

@group(0) @binding(0) var<uniform> pp: ParticleParams;
@group(0) @binding(1) var<storage, read_write> slots: array<Particle>;
@group(0) @binding(2) var<uniform> ground: TerrainParams;
@group(0) @binding(3) var heights: texture_2d<f32>;
@group(0) @binding(4) var seen_depth: texture_depth_2d;

const kSurfaceBounce = 0.35;  // render/particles.hpp's
const kSurfaceSkin = 0.12;
const kNearestSurface = 0.8;  // m: nearer than this is the eye's own (a held weapon), never met

fn fly(q: Particle, dt: f32, air: vec3f) -> Particle {
  var p = q;
  p.v.y -= kGravity * p.fall * dt;
  p.v = air + (p.v - air) * exp(-p.drag * dt);
  p.p += p.v * dt;
  p.age += dt;
  return p;
}

// The point the last frame drew at texel `q`, relative to its eye (w 0: nothing drawn there).
fn seen_at(q: vec2i, dims: vec2i) -> vec4f {
  let c = clamp(q, vec2i(0), dims - vec2i(1));
  let d = textureLoad(seen_depth, c, 0);
  if (d <= 0.0) {
    return vec4f(0.0);
  }
  let uv = (vec2f(c) + 0.5) / vec2f(dims);
  let h = pp.seen_inv * vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, d, 1.0);
  return vec4f(h.xyz / h.w, 1.0);
}

// Off what the last frame drew: behind the surface there by no more than the skin (and a step's
// travel), it is put back on the face and bounced (collide_particle's twin).
fn off_surface(q: Particle) -> Particle {
  var p = q;
  let rel = p.p - pp.seen_eye.xyz;
  let clip = pp.seen_vp * vec4f(rel, 1.0);
  if (clip.w <= pp.step.y) {
    return p;  // at the eye or behind it: nothing seen there
  }
  let ndc = clip.xy / clip.w;
  if (any(abs(ndc) >= vec2f(1.0))) {
    return p;  // off the last frame's picture
  }
  let dims = vec2i(textureDimensions(seen_depth));
  let px = vec2i((vec2f(ndc.x, -ndc.y) * 0.5 + 0.5) * vec2f(dims));
  let c = seen_at(px, dims);
  if (c.w == 0.0 || length(c.xyz) < kNearestSurface) {
    return p;
  }
  // How far behind the drawn surface along the view: its distance against the particle's.
  let behind = length(rel) - length(c.xyz);
  if (behind < 0.0 || behind > kSurfaceSkin + length(p.v) * pp.step.x * 2.0) {
    return p;
  }
  // The face's normal from the drawn points beside it, the nearer side each way (an edge's far
  // side is another surface), turned toward the eye.
  let r = seen_at(px + vec2i(1, 0), dims);
  let l = seen_at(px - vec2i(1, 0), dims);
  let d = seen_at(px + vec2i(0, 1), dims);
  let u = seen_at(px - vec2i(0, 1), dims);
  let dr = r.xyz - c.xyz;
  let dl = c.xyz - l.xyz;
  let dd = d.xyz - c.xyz;
  let du = c.xyz - u.xyz;
  let ax = select(dl, dr, r.w > 0.0 && (l.w == 0.0 || dot(dr, dr) < dot(dl, dl)));
  let ay = select(du, dd, d.w > 0.0 && (u.w == 0.0 || dot(dd, dd) < dot(du, du)));
  var n = cross(ax, ay);
  if (dot(n, n) < 1e-12) {
    return p;
  }
  n = normalize(n);
  if (dot(n, c.xyz) > 0.0) {
    n = -n;
  }
  let at = c.xyz + pp.seen_eye.xyz;
  p.p = p.p + n * (dot(at - p.p, n) + 0.01);
  let vn = dot(p.v, n);
  if (vn >= 0.0) {
    return p;
  }
  let b = select(kSurfaceBounce, p.bounce, p.bounce > 0.0);
  let along = p.v - vn * n;
  if (-vn * b > kFloorRest && p.hops < max_hops(p)) {
    p.v = along * kFloorSlide - vn * b * n;
    p.hops += 1.0;
  } else if (n.y > 0.6) {
    p.v = vec3f(0.0);
    p.age = max(p.age, p.life * 0.7);
  } else {
    p.v = along * kFloorSlide;
  }
  return p;
}

@compute @workgroup_size(64)
fn sim(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i >= pp.info.x) {
    return;
  }
  var p = slots[i];
  if (p.age >= p.life || pp.step.x <= 0.0) {
    return;
  }
  p = on_floor(fly(p, pp.step.x, pp.air.xyz));
  if (pp.seen_eye.w > 0.5 && p.fall > 0.5) {
    p = off_surface(p);
  }
  let h = field_height(heights, ground, p.p.xz);
  // Under the snow and falling: it has come down on it. (One under it and rising is still coming
  // up out of the snow it was thrown from: a ski sinks into soft snow.)
  if (p.p.y < h && p.v.y < 0.0) {
    if (p.fall > 0.5) {
      // A grain buries itself in the snow it lands on.
      p.age = p.life;
    } else {
      // A fine one settles and fades where it lies.
      p.p.y = h;
      p.v = vec3f(p.v.x * 0.5, 0.0, p.v.z * 0.5);
      p.age = max(p.age, p.life * 0.7);
    }
  }
  slots[i] = p;
}
