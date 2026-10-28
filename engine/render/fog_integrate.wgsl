// THE FOG'S COLUMNS (render/fog.hpp): each froxel column gathered front to back. A slice's light
// over its depth is dimmed by the slice itself (the light scattered at its far side has the
// slice's own medium to cross), its transmittance multiplied on, and both are written at the
// slice's far edge: the in-scatter toward the eye (rgb) and the share of what lies behind that is
// seen (a).

// render/fog.cpp's Params (fog.wgsl's FogParams).
struct FogParams {
  grid: vec4u,
  look: vec4f,
}

@group(0) @binding(0) var<uniform> fp: FogParams;
@group(0) @binding(1) var media: texture_3d<f32>;
@group(0) @binding(2) var volume_out: texture_storage_3d<rgba16float, write>;

@compute @workgroup_size(8, 8, 1)
fn integrate(@builtin(global_invocation_id) id: vec3u) {
  if (any(id.xy >= fp.grid.xy)) {
    return;
  }
  let slices = fp.grid.z;
  var light = vec3f(0.0);
  var seen = 1.0;
  var near = 0.0;
  for (var z = 0u; z < slices; z++) {
    let s = f32(z + 1u) / f32(slices);
    let far = fp.look.x * s * s;
    let depth = far - near;
    near = far;
    let m = textureLoad(media, vec3u(id.xy, z), 0);
    let pass_share = exp(-m.a * depth);
    // Over a clear slice the light is its source times its depth (the limit of the share taken
    // over the extinction); the transmittance stays exactly 1.
    light += seen * m.rgb * select(depth, (1.0 - pass_share) / m.a, m.a > 1e-7);
    seen *= pass_share;
    textureStore(volume_out, vec3u(id.xy, z), vec4f(light, seen));
  }
}
