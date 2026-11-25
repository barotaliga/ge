// AMBIENT OCCLUSION APPLIED AFTER THE FACT (render/ao.hpp's `encode_ao_apply`): for a renderer
// that draws its colour and depth in one pass (no depth prepass for the search to read first), the
// occlusion is worked out from the finished depth and the colour already drawn is multiplied by
// the sky each pixel sees (the blend takes the fragment's colour as its factor on what is there).
// The half-size texels are read as frame.wgsl's `sky_seen` reads them: the nearest four, each by
// how like its depth is to the pixel's, so an edge does not bleed one surface's shade onto the
// other. A pixel nothing was drawn at (the sky's, drawn later) is left as it is.
//
// ge:summary Ambient occlusion multiplied into a drawn frame, for a renderer with no depth prepass
// ge:keywords render ambient-occlusion ao ao-apply multiply post depth-weighted
// ge:skill render

@group(0) @binding(0) var ao_map: texture_2d<f32>;
@group(0) @binding(1) var depth: texture_depth_2d;  // reversed-z: 0 where nothing was drawn

struct ApplyOut {
  @builtin(position) clip: vec4f,
};

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> ApplyOut {
  var o: ApplyOut;
  let xy = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
  o.clip = vec4f(xy * 2.0 - 1.0, 0.0, 1.0);
  return o;
}

@fragment
fn fs_main(in: ApplyOut) -> @location(0) vec4f {
  let z = textureLoad(depth, vec2i(in.clip.xy), 0);
  if (z <= 0.0) {
    return vec4f(1.0);
  }
  let dims = vec2i(textureDimensions(ao_map));
  let q = in.clip.xy * 0.5 - 0.5;
  let b = vec2i(floor(q));
  let f = q - floor(q);
  var sum = 0.0;
  var weights = 0.0;
  for (var k = 0; k < 4; k++) {
    let o = vec2i(k & 1, k >> 1);
    let t = textureLoad(ao_map, clamp(b + o, vec2i(0), dims - 1), 0);
    let near = select(1.0 - f, f, vec2<bool>(o.x == 1, o.y == 1));
    let like = max(1.0 - 10.0 * abs(t.g - z) / max(z, 1e-6), 1e-3);
    let w = near.x * near.y * like;
    sum += t.r * w;
    weights += w;
  }
  return vec4f(vec3f(1.0 - sum / max(weights, 1e-6)), 1.0);
}
