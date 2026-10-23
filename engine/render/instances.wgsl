// THE INSTANCES' DRAW (render/instances.hpp): one indirect draw a bucket, its bucket told by the
// base vertex (the vertex index's bits 20 and up), its instance read from the bucket's list and
// its vertex pulled from the shared buffer and decoded. Lit by the sun past the ground's own
// horizon and past what stands in the sun (wgsl/sun_shadow.wgsl), and the sky through
// frame.wgsl's functions, seen through the haze, with its motion
// from the last frame's place (and a skinned vertex's own last place in its mesh). A palette
// LOD's vertex is posed here, by its instance's palette this frame and the last (`skinned`). Drawn twice from one `@invariant` vertex stage: depth only
// (`fs_depth`, which keeps a LOD band's dithered share of the pixels), then shaded where the
// depth is `equal` (`fs_main`, which never discards).
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/terrain_common.wgsl"
#include "render/wgsl/instances_common.wgsl"
#include "render/wgsl/sun_shadow.wgsl"

@group(1) @binding(0) var<uniform> ground: TerrainParams;
@group(1) @binding(1) var horizon0: texture_2d<f32>;
@group(1) @binding(2) var horizon1: texture_2d<f32>;

@group(2) @binding(0) var<uniform> fp: FamilyParams;
@group(2) @binding(1) var<storage, read> vertices: array<u32>;
@group(2) @binding(2) var<storage, read> lods: array<Lod>;
@group(2) @binding(3) var<storage, read> streams: array<vec4f>;
@group(2) @binding(4) var<storage, read> lists: array<u32>;
@group(2) @binding(5) var<storage, read> materials: array<vec4f>;
@group(2) @binding(6) var<storage, read> palettes: array<vec4f>;  // three rows a joint

@group(3) @binding(0) var<uniform> shadow: SunShadow;
@group(3) @binding(1) var shadow_maps: texture_depth_2d_array;
@group(3) @binding(2) var shadow_cmp: sampler_comparison;

// The last frame's mesh-space position of vertex `index` of `lod`, from the two words a vertex
// the skinning pass keeps (render/skinning.hpp); a LOD that keeps none moves with its instance.
fn last_position(index: u32, lod: Lod, now: vec3f) -> vec3f {
  if (lod.motion.x == 0u) {
    return now;
  }
  let at = lod.motion.x + index * 2u;
  let w0 = vertices[at];
  let w1 = vertices[at + 1u];
  let q = vec3f(f32(w0 & 0xffffu), f32(w0 >> 16u), f32(w1 & 0xffffu)) / 65535.0;
  return lod.box_min.xyz + q * lod.box_size.xyz;
}

// A palette LOD's vertex posed by its instance's palette (rows from `first`, three a joint): the
// linear blend of its four joints, the weights as 16-bit unorms (render/skinning.hpp's
// `palette_source`), its position and normal in mesh space.
struct Posed {
  position: vec3f,
  normal: vec3f,
}

fn skinned(skin: u32, first: u32, p: vec3f, n: vec3f) -> Posed {
  let joints = vertices[skin];
  let w01 = vertices[skin + 1u];
  let w23 = vertices[skin + 2u];
  let w = vec4f(f32(w01 & 0xffffu), f32(w01 >> 16u), f32(w23 & 0xffffu), f32(w23 >> 16u));
  let weights = w / max(w.x + w.y + w.z + w.w, 1.0);
  let last = fp.skin.x - 1u;
  var r0 = vec4f(0.0);
  var r1 = vec4f(0.0);
  var r2 = vec4f(0.0);
  for (var k = 0u; k < 4u; k++) {
    let j = first + min((joints >> (8u * k)) & 0xffu, last) * 3u;
    r0 += weights[k] * palettes[j];
    r1 += weights[k] * palettes[j + 1u];
    r2 += weights[k] * palettes[j + 2u];
  }
  let h = vec4f(p, 1.0);
  var out: Posed;
  out.position = vec3f(dot(r0, h), dot(r1, h), dot(r2, h));
  out.normal = vec3f(dot(r0.xyz, n), dot(r1.xyz, n), dot(r2.xyz, n));
  return out;
}

struct VertexOut {
  @invariant @builtin(position) position: vec4f,
  @location(0) rel: vec3f,       // camera-relative world position, m
  @location(1) last_rel: vec3f,  // the last frame's place of this point, from this eye
  @location(2) normal: vec3f,
  @location(3) @interpolate(flat) material: u32,
  @location(4) @interpolate(flat) band: u32,  // the entry's band bits (0: no band)
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> VertexOut {
  let bucket = vid >> 20u;
  let lod = lods[bucket % fp.info.y];
  let entry = lists[bucket * fp.info.z + iid];
  let i = entry & kEntryIndex;
  let at = (lod.info.z + (vid & 0xfffffu)) * 3u;
  let v = decode_vertex(vertices[at], vertices[at + 1u], vertices[at + 2u], lod);
  let cap = fp.info.z;
  let p = streams[kStreamPlace * cap + i];
  let q = streams[kStreamRotation * cap + i];
  let s = streams[kStreamScale * cap + i];
  let lp = streams[kStreamLastPlace * cap + i];
  let lq = streams[kStreamLastRotation * cap + i];
  var position = v.position;
  var normal = v.normal;
  var last = last_position(vid & 0xfffffu, lod, v.position);
  if (lod.motion.y != 0u) {
    let skin = lod.motion.y + (vid & 0xfffffu) * 3u;
    let rows = i * fp.skin.x * 3u;
    let now = skinned(skin, fp.skin.y + rows, v.position, v.normal);
    position = now.position;
    normal = now.normal;
    last = skinned(skin, fp.skin.z + rows, v.position, v.normal).position;
  }
  let local = position * s.xyz;
  let last_local = last * s.xyz;
  var out: VertexOut;
  out.rel = p.xyz + quat_rotate(q, local) - frame.eye.xyz;
  out.last_rel = lp.xyz + quat_rotate(lq, last_local) - frame.eye.xyz;
  out.position = frame.view_proj * vec4f(out.rel, 1.0);
  // A normal under a scale by s is scaled by 1 / s.
  out.normal = quat_rotate(q, normalize(normal / s.xyz));
  out.material = u32(p.w) + v.part;
  out.band = entry >> 23u;
  return out;
}

// Interleaved gradient noise over the pixel, stepped each frame: the LOD band's dither.
fn dither(pixel: vec2f) -> f32 {
  let p = pixel + 5.588238 * f32(u32(frame.time.y) % 64u);
  return fract(52.9829189 * fract(dot(p, vec2f(0.06711056, 0.00583715))));
}

// In a band the finer LOD keeps the pixels whose dither is under its share, the coarser the rest.
fn band_keeps(band: u32, pixel: vec2f) -> bool {
  if ((band & 1u) == 0u) {
    return true;
  }
  let share = f32((band >> 1u) & 0x7fu) / 127.0;
  let coarse = (band & 0x100u) != 0u;
  return (dither(pixel) < share) != coarse;
}

@fragment
fn fs_depth(in: VertexOut) {
  if (!band_keeps(in.band, in.position.xy)) {
    discard;
  }
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // rg: the screen motion (uv), b: reactive (0: a solid surface)
}

@fragment
fn fs_main(in: VertexOut, @builtin(front_facing) front: bool) -> FragmentOut {
  var n = normalize(in.normal);
  if (!front) {
    n = -n;
  }
  let a = materials[in.material].rgb;
  let y = in.rel.y + frame.eye.y;
  let xz = in.rel.xz + frame.eye.xz;
  let lit = max(dot(n, frame.sun.xyz), 0.0) *
            ground_shadow(ground, horizon0, horizon1, linear_clamp, frame.sun.xyz, xz) *
            sun_shadow(shadow, shadow_maps, shadow_cmp, in.rel, n, in.position.xy, frame.time.y);
  let c = a / PI * (sun_light(y) * lit + sky_irradiance(n));
  var out: FragmentOut;
  out.colour = vec4f(haze(c, in.position.xy * frame.size.zw, length(in.rel)), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.last_rel, 1.0), in.position.xy), 0.0, 0.0);
  return out;
}

// The overdraw probe (labs only): one per kept fragment that passes the depth test.
@fragment
fn fs_count(in: VertexOut) -> @location(0) vec4f {
  if (!band_keeps(in.band, in.position.xy)) {
    discard;
  }
  return vec4f(1.0, 0.0, 0.0, 0.0);
}

// The wire probe (labs only): the triangles' edges as lines, a hair nearer than the surface under
// them (reversed-z: a larger depth is nearer), in one warm colour so the trees and the figures
// stand apart from the ground's distance bands.
struct WireOut {
  @location(0) colour: vec4f,
  @builtin(frag_depth) depth: f32,
}

@fragment
fn fs_wire(in: VertexOut) -> WireOut {
  if (!band_keeps(in.band, in.position.xy)) {
    discard;
  }
  var out: WireOut;
  out.colour = vec4f(1.0, 0.62, 0.2, 1.0);
  out.depth = in.position.z * 1.002;
  return out;
}
