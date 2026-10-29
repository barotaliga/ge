// AMBIENT OCCLUSION'S SMOOTHING (render/ao.hpp), the pass's second dispatch, after the search
// (ao.wgsl): each texel the mean of the 3 x 3 round it whose depth is like its own (within a
// tenth). The search's noise is laid out so any 3 x 3 window holds its turns evenly, so the mean
// is the pattern averaged; the depth keeps a figure's shade off the slope behind it.
@group(0) @binding(0) var raw: texture_2d<f32>;
@group(0) @binding(1) var smoothed: texture_storage_2d<rgba16float, write>;

@compute @workgroup_size(8, 8)
fn blur(@builtin(global_invocation_id) id: vec3u) {
  let size = vec2i(textureDimensions(raw));
  let t = vec2i(id.xy);
  if (any(t >= size)) {
    return;
  }
  let c = textureLoad(raw, t, 0);
  if (c.g <= 0.0) {
    textureStore(smoothed, t, c);
    return;
  }
  var sum = 0.0;
  var weights = 0.0;
  for (var y = -1; y <= 1; y++) {
    for (var x = -1; x <= 1; x++) {
      let q = textureLoad(raw, clamp(t + vec2i(x, y), vec2i(0), size - 1), 0);
      let w = max(1.0 - 10.0 * abs(q.g - c.g) / c.g, 0.0);
      sum += q.r * w;
      weights += w;
    }
  }
  textureStore(smoothed, t, vec4f(sum / weights, c.g, 0.0, 1.0));
}
