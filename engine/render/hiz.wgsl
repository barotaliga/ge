// THE HI-Z REDUCTION (render/hiz.hpp): each texel of a mip is the minimum (reversed-z: the
// farthest) of every texel of the level below that overlaps it, two or three a side. `first`
// reads the depth target, `next` the mip before.

@group(0) @binding(0) var src_depth: texture_depth_2d;
@group(0) @binding(1) var dst_first: texture_storage_2d<r32float, write>;

@group(1) @binding(0) var src_mip: texture_2d<f32>;
@group(1) @binding(1) var dst_next: texture_storage_2d<r32float, write>;

// The span of source texels output texel `i` of `dst` covers along one axis, inclusive.
fn span(i: u32, src: u32, dst: u32) -> vec2u {
  let lo = (i * src) / dst;
  let hi = ((i + 1u) * src + dst - 1u) / dst - 1u;
  return vec2u(lo, min(hi, src - 1u));
}

@compute @workgroup_size(8, 8)
fn first(@builtin(global_invocation_id) id: vec3u) {
  let dst = textureDimensions(dst_first);
  if (id.x >= dst.x || id.y >= dst.y) {
    return;
  }
  let src = textureDimensions(src_depth);
  let sx = span(id.x, src.x, dst.x);
  let sy = span(id.y, src.y, dst.y);
  var m = 1.0;
  for (var y = sy.x; y <= sy.y; y++) {
    for (var x = sx.x; x <= sx.y; x++) {
      m = min(m, textureLoad(src_depth, vec2u(x, y), 0));
    }
  }
  textureStore(dst_first, id.xy, vec4f(m, 0.0, 0.0, 0.0));
}

@compute @workgroup_size(8, 8)
fn next(@builtin(global_invocation_id) id: vec3u) {
  let dst = textureDimensions(dst_next);
  if (id.x >= dst.x || id.y >= dst.y) {
    return;
  }
  let src = textureDimensions(src_mip);
  let sx = span(id.x, src.x, dst.x);
  let sy = span(id.y, src.y, dst.y);
  var m = 1.0;
  for (var y = sy.x; y <= sy.y; y++) {
    for (var x = sx.x; x <= sx.y; x++) {
      m = min(m, textureLoad(src_mip, vec2u(x, y), 0).r);
    }
  }
  textureStore(dst_next, id.xy, vec4f(m, 0.0, 0.0, 0.0));
}
