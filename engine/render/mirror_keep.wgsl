// THE SCENE KEPT FOR THE NEXT FRAME'S MIRRORS (render/mirror.hpp's "mirror.keep"): the frame's
// colour before TAA-U, and in alpha how far from the eye each pixel's surface lies (m; 0 where
// nothing was drawn: the sky). The next frame's surfaces march their reflected rays through it
// (render/wgsl/mirror.wgsl).
#include "render/wgsl/frame_types.wgsl"

@group(0) @binding(0) var hdr: texture_2d<f32>;
@group(0) @binding(1) var depth: texture_depth_2d;
@group(0) @binding(2) var<uniform> frame: Frame;

struct VertexOut {
  @builtin(position) position: vec4f,
}

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOut {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u)) * 2.0 - 1.0;
  var out: VertexOut;
  out.position = vec4f(p, 0.0, 1.0);
  return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  let px = vec2i(in.position.xy);
  let z = textureLoad(depth, px, 0);
  var dist = 0.0;
  if (z > 0.0) {
    let uv = in.position.xy * frame.size.zw;
    let w = frame.inv_view_proj * vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, z, 1.0);
    dist = length(w.xyz / w.w);
  }
  return vec4f(textureLoad(hdr, px, 0).rgb, dist);
}
