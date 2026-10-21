// The GPU test scene (test_scene.hpp): a sky gradient over the whole target and one triangle
// with a red, a green and a blue corner, in linear light, encoded for an 8-bit target.
#include "gpu/wgsl/colour.wgsl"

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) colour: vec3f,
}

// Vertices 0-2: a triangle covering the target (the sky); 3-5: the coloured triangle.
const kPositions = array<vec2f, 6>(
  vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0),
  vec2f(-0.6, -0.6), vec2f(0.6, -0.6), vec2f(0.0, 0.7),
);
// Linear sky: a pale horizon at the bottom of the target, a deep blue towards the top.
const kHorizon = vec3f(0.62, 0.76, 0.92);
const kZenith = vec3f(0.10, 0.27, 0.62);
const kCorners = array<vec3f, 3>(vec3f(0.9, 0.05, 0.05), vec3f(0.05, 0.8, 0.1), vec3f(0.05, 0.1, 0.9));

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOut {
  var out: VertexOut;
  let p = kPositions[i];
  out.position = vec4f(p, 0.0, 1.0);
  if (i < 3u) {
    out.colour = mix(kHorizon, kZenith, (p.y + 1.0) * 0.5);
  } else {
    out.colour = kCorners[i - 3u];
  }
  return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  return vec4f(linear_to_srgb(in.colour), 1.0);
}
