// THE PARTICLES DRAWN (render/particles.hpp): one quad a slot, instanced. A live particle is a
// streak along its motion over `streak` s, at least `size` long and `size` wide, laid in the plane
// facing the eye; a dead one folds to a point off the screen. Lit as the instances are (the sun
// past the ground's horizon and the shadow maps, the sky) with a forward lobe toward the sun,
// hazed, faded in and out over its life, thinned by its length, and softened against the depth
// behind it. Premultiplied over the picture; the motion target's reactive channel only.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/terrain_common.wgsl"
#include "render/wgsl/sun_shadow.wgsl"
#include "render/wgsl/particle_types.wgsl"

@group(1) @binding(0) var<uniform> ground: TerrainParams;
@group(1) @binding(1) var horizon0: texture_2d<f32>;
@group(1) @binding(2) var horizon1: texture_2d<f32>;

@group(2) @binding(0) var<uniform> pp: ParticleParams;
@group(2) @binding(1) var<storage, read> slots: array<Particle>;
@group(2) @binding(2) var scene_depth: texture_depth_2d;

@group(3) @binding(0) var<uniform> shadow: SunShadow;
@group(3) @binding(1) var shadow_maps: texture_depth_2d_array;
@group(3) @binding(2) var shadow_cmp: sampler_comparison;

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) quad: vec2f,                    // -1..1 along the streak, -1..1 across it
  @location(1) rel: vec3f,                     // camera-relative, m
  @location(2) @interpolate(flat) look: vec4f, // albedo, opacity at this age, thinning, size
}

const kCorners = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0),
                                 vec2f(-1.0, -1.0), vec2f(1.0, 1.0), vec2f(-1.0, 1.0));

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> VertexOut {
  var out: VertexOut;
  let p = slots[iid];
  if (p.age >= p.life || p.age < 0.0) {
    out.position = vec4f(2.0, 2.0, 2.0, 1.0);
    return out;
  }
  let c = kCorners[vid];
  let rel = p.p - frame.eye.xyz;
  let to_eye = -normalize(rel);
  // The streak: the motion over the shutter, laid flat in the plane facing the eye.
  let motion = p.v * p.streak;
  let flat_motion = motion - dot(motion, to_eye) * to_eye;
  let len = length(flat_motion);
  var along = vec3f(0.0);
  if (len > p.size) {
    along = flat_motion / len;
  } else {
    // Too slow to streak: a round speck, its along any line in the plane.
    let side = cross(vec3f(0.0, 1.0, 0.0), to_eye);
    along = side / max(length(side), 1e-4);
  }
  let across = normalize(cross(to_eye, along));
  let half_len = max(len, p.size) * 0.5;
  // The streak trails behind the particle: its head where it is now.
  let centre = rel - flat_motion * 0.5 * select(0.0, 1.0, len > p.size);
  let corner = centre + along * (c.x * half_len) + across * (c.y * p.size * 0.5);
  out.position = frame.view_proj * vec4f(corner, 1.0);
  out.quad = c;
  out.rel = corner;
  let t = p.age / max(p.life, 1e-4);
  let fade = smoothstep(0.0, 0.1, t) * (1.0 - smoothstep(0.66, 1.0, t));
  // A streak spreads the same grain over its length: thinner the longer it is.
  let thin = p.size / max(len, p.size);
  out.look = vec4f(p.albedo, p.opacity * fade, thin, p.size);
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // b only: the reactive mask
}

// Henyey–Greenstein, normalised to 1 over the sphere times 4 pi: light scattered by cos `c`
// between the ray to the eye's continuation and the sun.
fn forward_lobe(c: f32, g: f32) -> f32 {
  let d = 1.0 + g * g - 2.0 * g * c;
  return (1.0 - g * g) / (d * sqrt(d));
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  // A soft capsule: full on its axis, gone at its rim and its ends.
  let across = 1.0 - in.quad.y * in.quad.y;
  let ends = 1.0 - smoothstep(0.6, 1.0, abs(in.quad.x));
  var a = in.look.y * in.look.z * across * ends;
  // Softened where it nears what is behind it.
  let d = textureLoad(scene_depth, vec2i(in.position.xy), 0);
  let behind = pp.step.y / max(d, 1e-7);
  let here = pp.step.y / max(in.position.z, 1e-7);
  a *= clamp((behind - here) / max(pp.step.z, 1e-3), 0.0, 1.0);
  if (a <= 0.002) {
    discard;
  }
  let y = in.rel.y + frame.eye.y;
  let xz = in.rel.xz + frame.eye.xz;
  let to_eye = -normalize(in.rel);
  let lit = ground_shadow(ground, horizon0, horizon1, linear_clamp, frame.sun.xyz, xz) *
            sun_shadow(shadow, shadow_maps, shadow_cmp, in.rel, to_eye, in.position.xy,
                       frame.time.y);
  // Snow scatters most of the light forward: spray between the eye and a low sun is silver. A
  // cloud of grains is lit through, not on one face: the sun's whole light round the lobe, as
  // bright as the snow it came off facing the sun.
  let lobe = mix(1.0, forward_lobe(dot(-to_eye, frame.sun.xyz), 0.6), 0.6);
  let c = in.look.x / PI * (sun_light(y) * lit * lobe + sky_irradiance(vec3f(0.0, 1.0, 0.0)));
  let hazed = haze(c, in.position.xy * frame.size.zw, length(in.rel));
  var out: FragmentOut;
  out.colour = vec4f(hazed * a, a);
  out.motion = vec4f(0.0, 0.0, a, 0.0);
  return out;
}
