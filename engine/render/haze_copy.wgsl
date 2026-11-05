// THE HEAT HAZE'S COPY (render/haze.hpp's "haze.copy"): the frame's colour before the haze reads
// it, so a ribbon can read what is behind it pushed off.
@group(0) @binding(0) var copy_from: texture_2d<f32>;

struct CopyOut {
  @builtin(position) position: vec4f,
}

@vertex
fn copy_vs(@builtin(vertex_index) i: u32) -> CopyOut {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u)) * 2.0 - 1.0;
  var out: CopyOut;
  out.position = vec4f(p, 0.0, 1.0);
  return out;
}

@fragment
fn copy_fs(in: CopyOut) -> @location(0) vec4f {
  return textureLoad(copy_from, vec2i(in.position.xy), 0);
}
