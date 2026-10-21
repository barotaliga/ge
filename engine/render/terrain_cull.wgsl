// THE TERRAIN'S CULL (render/terrain.hpp): one thread a tile, its box (its plan square and its
// height range from the min/max pyramid, sunk cells included) against the frustum; a visible
// tile is appended to the instance list and counted into the draw's indirect arguments.
#include "render/wgsl/frame_types.wgsl"
#include "render/wgsl/terrain_common.wgsl"

@group(0) @binding(0) var<uniform> frame: Frame;

@group(1) @binding(0) var<uniform> params: TerrainParams;
@group(1) @binding(1) var pyramid: texture_2d<f32>;
@group(1) @binding(2) var<storage, read_write> tiles: array<u32>;

struct DrawArgs {
  index_count: u32,
  instance_count: atomic<u32>,
  first_index: u32,
  base_vertex: i32,
  first_instance: u32,
}
@group(1) @binding(3) var<storage, read_write> args: DrawArgs;

// The height range over a plan box, from the coarsest mip whose texel covers the box's side; a
// box reaching past the map (where the grid is read mirrored) takes the whole map's range.
fn height_range(lo: vec2f, hi: vec2f) -> vec2f {
  let mips = params.field_size.z;
  let map_hi = params.field.xy + vec2f(params.field_size.xy - 1u) * params.field.z;
  if (any(lo < params.field.xy) || any(hi > map_hi)) {
    return textureLoad(pyramid, vec2i(0), i32(mips - 1u)).rg;
  }
  let need = max(max(hi.x - lo.x, hi.y - lo.y) / params.field.z, 1.0);
  let mip = min(u32(ceil(log2(need))), mips - 1u);
  let span = params.field.z * f32(1u << mip);
  let size = vec2i(textureDimensions(pyramid, mip));
  let i0 = clamp(vec2i(floor((lo - params.field.xy) / span)), vec2i(0), size - 1);
  let i1 = clamp(vec2i(floor((hi - params.field.xy) / span)), vec2i(0), size - 1);
  var r = vec2f(1e30, -1e30);
  for (var y = i0.y; y <= i1.y; y++) {
    for (var x = i0.x; x <= i1.x; x++) {
      let t = textureLoad(pyramid, vec2i(x, y), i32(mip)).rg;
      r = vec2f(min(r.x, t.x), max(r.y, t.y));
    }
  }
  return r;
}

@compute @workgroup_size(16)
fn cull(@builtin(global_invocation_id) id: vec3u) {
  let level = id.x / 16u;
  if (level >= u32(params.info.x)) {
    return;
  }
  let tx = (id.x % 16u) % 4u;
  let ty = (id.x % 16u) / 4u;
  let lv = params.levels[level];
  let side = params.info.y * lv.z;
  let lo = lv.xy + vec2f(f32(tx), f32(ty)) * side;
  let hi = lo + side;
  let h = height_range(lo, hi);
  let bmin = vec3f(lo.x, h.x - params.info.z * lv.z, lo.y) - frame.eye.xyz;
  let bmax = vec3f(hi.x, h.y, hi.y) - frame.eye.xyz;
  for (var i = 0; i < 5; i++) {
    let p = frame.planes[i];
    let far = select(bmin, bmax, p.xyz >= vec3f(0.0));
    if (dot(p.xyz, far) + p.w < 0.0) {
      return;
    }
  }
  let slot = atomicAdd(&args.instance_count, 1u);
  tiles[slot] = level | (tx << 8u) | (ty << 16u);
}
