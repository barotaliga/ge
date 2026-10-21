// THE TERRAIN'S NORMALS, baked once at load (render/terrain.hpp): each sample's normal from the
// central differences of its neighbours' heights (one-sided at the grid's edge), into a
// filterable texture the draw samples per pixel.
#include "render/wgsl/terrain_common.wgsl"

@group(0) @binding(0) var<uniform> params: TerrainParams;
@group(0) @binding(1) var heights: texture_2d<f32>;
@group(0) @binding(2) var normals: texture_storage_2d<rgba16float, write>;

@compute @workgroup_size(8, 8)
fn bake(@builtin(global_invocation_id) id: vec3u) {
  let size = params.field_size.xy;
  if (id.x >= size.x || id.y >= size.y) {
    return;
  }
  let c = vec2i(id.xy);
  let m = vec2i(size) - 1;
  let l = textureLoad(heights, vec2i(max(c.x - 1, 0), c.y), 0).r;
  let r = textureLoad(heights, vec2i(min(c.x + 1, m.x), c.y), 0).r;
  let d = textureLoad(heights, vec2i(c.x, max(c.y - 1, 0)), 0).r;
  let u = textureLoad(heights, vec2i(c.x, min(c.y + 1, m.y)), 0).r;
  let dx = f32(min(c.x + 1, m.x) - max(c.x - 1, 0)) * params.field.z;
  let dz = f32(min(c.y + 1, m.y) - max(c.y - 1, 0)) * params.field.z;
  let n = normalize(vec3f(-(r - l) / dx, 1.0, -(u - d) / dz));
  textureStore(normals, id.xy, vec4f(n, 1.0));
}
