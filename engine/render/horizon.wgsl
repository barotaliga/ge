// THE HORIZON MAP'S BAKE (render/horizon.hpp): for each texel, march toward one sun bearing in
// steps that grow geometrically from one cell to the range, and keep the tangent of the highest
// elevation the ground reaches, the planet's curvature taken off each sample's height.
#include "render/wgsl/terrain_common.wgsl"

struct HorizonBake {
  dir: vec4f,    // toward the bearing (x, z), the first step (m), the step ratio
  grid: vec4f,   // the map's origin x, z (world m), its cell (m)
  size: vec4u,   // width, height
}

const kSteps = 96u;               // render/horizon.hpp's kHorizonSteps
const kEarthRadiusM = 6360000.0;  // render/horizon.hpp's kEarthRadiusM

@group(0) @binding(0) var<uniform> params: TerrainParams;
@group(0) @binding(1) var heights: texture_2d<f32>;
@group(0) @binding(2) var<uniform> bake_in: HorizonBake;
@group(0) @binding(3) var horizon_out: texture_storage_2d<rgba16float, write>;

@compute @workgroup_size(8, 8)
fn bake(@builtin(global_invocation_id) id: vec3u) {
  if (id.x >= bake_in.size.x || id.y >= bake_in.size.y) {
    return;
  }
  let p = bake_in.grid.xy + vec2f(id.xy) * bake_in.grid.z;
  let h0 = field_height(heights, params, p);
  var best = -1.0;
  var t = bake_in.dir.z;
  for (var i = 0u; i < kSteps; i++) {
    let drop = t * t / (2.0 * kEarthRadiusM);
    best = max(best, (field_height(heights, params, p + bake_in.dir.xy * t) - drop - h0) / t);
    t *= bake_in.dir.w;
  }
  textureStore(horizon_out, id.xy, vec4f(best, 0.0, 0.0, 1.0));
}
