// What the instance families' shaders share (render/instances.hpp): the family's parameters,
// its mesh and LOD tables, the instance streams' layout, the list entry's bits, the rotation
// and the packed vertex's decode (render/mesh.hpp is its CPU twin).

struct FamilyParams {
  info: vec4u,  // count, LODs, capacity, occlusion on (1)
  cull: vec4f,  // px a unit of tan on screen, the least radius on screen (px), the LOD band
}

struct Mesh {
  sphere: vec4f,  // the bounding sphere in mesh space: centre xyz, radius
  info: vec4u,    // first LOD, LOD count
}

struct Lod {
  box_min: vec4f,   // the packed positions' box; w: the least radius on screen it is drawn at
  box_size: vec4f,
  info: vec4u,      // first index, index count, first vertex, mesh
  motion: vec4u,    // x: the first word of the last frame's positions (two a vertex), 0: none
}

// The instance streams, each `capacity` vec4s long, in this order.
const kStreamPlace = 0u;      // xyz world position (m); w: material (an exact integer)
const kStreamRotation = 1u;   // a unit quaternion, body to world
const kStreamScale = 2u;      // xyz scale; w: mesh (an exact integer)
const kStreamLastPlace = 3u;  // the last frame's position
const kStreamLastRotation = 4u;

// A list entry: the instance's index, then (bit 23) whether it is in a LOD band, then (bits
// 24..31) its side of the band (bit 31: the coarser) and the finer LOD's share (7 bits).
const kEntryIndex = 0x7fffffu;
const kEntryBand = 0x800000u;

// v rotated by the unit quaternion q (body to world), as engine/core/quat.hpp's `rotate`.
fn quat_rotate(q: vec4f, v: vec3f) -> vec3f {
  let c = cross(q.xyz, v);
  return v + 2.0 * (q.w * c + cross(q.xyz, c));
}

struct PackedVertex {
  position: vec3f,  // mesh space, m
  normal: vec3f,
  part: u32,
}

fn snorm16(bits: u32) -> f32 {
  let s = i32(bits << 16u) >> 16u;
  return max(f32(s) / 32767.0, -1.0);
}

fn decode_vertex(w0: u32, w1: u32, w2: u32, lod: Lod) -> PackedVertex {
  var v: PackedVertex;
  let q = vec3f(f32(w0 & 0xffffu), f32(w0 >> 16u), f32(w1 & 0xffffu)) / 65535.0;
  v.position = lod.box_min.xyz + q * lod.box_size.xyz;
  v.part = w1 >> 16u;
  let o = vec2f(snorm16(w2 & 0xffffu), snorm16(w2 >> 16u));
  var n = vec3f(o.x, 1.0 - abs(o.x) - abs(o.y), o.y);
  if (n.y < 0.0) {
    let fx = (1.0 - abs(n.z)) * select(-1.0, 1.0, n.x >= 0.0);
    let fz = (1.0 - abs(n.x)) * select(-1.0, 1.0, n.z >= 0.0);
    n = vec3f(fx, n.y, fz);
  }
  v.normal = normalize(n);
  return v;
}
