// THE LAMPS' SHADOW ATLAS's clear (render/lamp_shadows.hpp): one triangle over the viewport (a
// tile, set by the pass) writing depth 0, nothing cast, before the tile's casters are drawn. The
// atlas is loaded, not cleared, so the tiles that are not redrawn keep their depth.
//
// ge:summary The lamps' shadow atlas's clear: a tile's depth back to nothing cast
// ge:keywords render lamp-shadows atlas clear tile depth
// ge:skill render

@vertex
fn vs_clear(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
  let x = f32(i32(i & 1u) * 4 - 1);
  let y = f32(i32(i >> 1u) * 4 - 1);
  return vec4f(x, y, 0.0, 1.0);
}
