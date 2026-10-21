// THE TERRAIN'S DRAW (render/terrain.hpp): a tile of the clipmap, its vertices pulled from the
// vertex index and placed by its instance's level and tile; the odd vertices morphed onto the
// coarser grid toward the level's edge, the vertices a finer level covers sunk under it; lit by
// the sun and the sky through the shared functions of frame.wgsl, and seen through the haze.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/terrain_common.wgsl"

override kTile: u32 = 32u;
// The morph runs from params.info.w to this share of a level's half side.
const kMorphEnd = 0.95;

@group(1) @binding(0) var<uniform> params: TerrainParams;
@group(1) @binding(1) var heights: texture_2d<f32>;
@group(1) @binding(2) var normals: texture_2d<f32>;
@group(1) @binding(3) var albedo: texture_2d<f32>;
@group(1) @binding(4) var<storage, read> tiles: array<u32>;

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) rel: vec3f,  // camera-relative world position, m
  @location(1) xz: vec2f,   // world plan position, m
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> VertexOut {
  let packed = tiles[iid];
  let level = packed & 0xffu;
  let tx = (packed >> 8u) & 0xffu;
  let ty = (packed >> 16u) & 0xffu;
  let n = kTile + 1u;
  let g = vec2f(f32(tx * kTile + vid % n), f32(ty * kTile + vid / n));
  let lv = params.levels[level];
  let s = lv.z;
  let at = lv.xy + g * s;
  let half = f32(kTile * 2u) * s;
  let d = max(abs(at.x - frame.eye.x), abs(at.y - frame.eye.z)) / half;
  var m = clamp((d - params.info.w) / (kMorphEnd - params.info.w), 0.0, 1.0);
  if (level + 1u >= u32(params.info.x)) {
    m = 0.0;
  }
  let xz = lv.xy + (g - fract(g * 0.5) * 2.0 * m) * s;
  var y = field_height(heights, params, xz);
  if (level > 0u) {
    let f = params.levels[level - 1u];
    let lo = f.xy + vec2f(0.25 * s);
    let hi = f.xy + vec2f(f32(kTile * 4u) * f.z) - vec2f(0.25 * s);
    if (all(xz > lo) && all(xz < hi)) {
      y -= params.info.z * s;
    }
  }
  var out: VertexOut;
  out.rel = vec3f(xz.x, y, xz.y) - frame.eye.xyz;
  out.position = frame.view_proj * vec4f(out.rel, 1.0);
  out.xz = xz;
  return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  let maxc = grid_max(params);
  let g = (in.xz - params.field.xy) / params.field.z;
  let uv = (mirror_grid(g, maxc) + 0.5) / vec2f(params.field_size.xy);
  let flip = mirror_sign(g, maxc);
  let n = normalize(textureSampleLevel(normals, linear_clamp, uv, 0.0).xyz * vec3f(flip.x, 1.0, flip.y));
  let a = textureSampleLevel(albedo, linear_clamp, uv, 0.0).rgb;
  let y = in.rel.y + frame.eye.y;
  let sun = sun_light(y) * max(dot(n, frame.sun.xyz), 0.0);
  let c = a / PI * (sun + sky_irradiance(n));
  return vec4f(haze(c, in.position.xy * frame.size.zw, length(in.rel)), 1.0);
}
