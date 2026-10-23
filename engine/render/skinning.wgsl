// THE SKINNING PASS (render/skinning.hpp): one thread a vertex of a skinned mesh. The bind
// vertex is moved by the linear blend of its four joints' palette rows, then packed as
// render/mesh.hpp packs a vertex (the position as 16-bit unorms inside the mesh's reach box, the
// normal octahedral in two 16-bit snorms, the part) into the instance family's vertex buffer,
// where every view's draw pulls it. `skin_packed` in skinning.cpp is its CPU twin. Before it
// writes, it keeps the position it replaces as the vertex's last frame's (the LOD's two words a
// vertex after the family's vertices, render/instances.hpp), so the colour pass draws the
// vertex's own motion; settled, the last frame's is this one's.

struct SkinParams {
  info: vec4u,     // vertices, the first vertex in the family's buffer, joints, 0
  box_min: vec4f,  // the reach box the positions are packed into
  box_size: vec4f,
  motion: vec4u,   // the first word of the last frame's positions (0: none), settled (1)
}

// A bind vertex: position and its four joints (bytes), normal and its part, the four weights.
struct BindVertex {
  a: vec4u,
  b: vec4u,
  w: vec4f,
}

@group(0) @binding(0) var<uniform> sp: SkinParams;
@group(0) @binding(1) var<storage, read> bind: array<BindVertex>;
@group(0) @binding(2) var<storage, read> palette: array<vec4f>;  // three rows a joint
@group(0) @binding(3) var<storage, read_write> packed: array<u32>;

fn unorm16(v: f32) -> u32 {
  return u32(round(clamp(v, 0.0, 1.0) * 65535.0));
}

fn snorm16(v: f32) -> u32 {
  return u32(i32(round(clamp(v, -1.0, 1.0) * 32767.0))) & 0xffffu;
}

fn sign_not_zero(v: f32) -> f32 {
  return select(-1.0, 1.0, v >= 0.0);
}

@compute @workgroup_size(64)
fn skin(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i >= sp.info.x) {
    return;
  }
  let v = bind[i];
  let p = vec4f(bitcast<vec3f>(v.a.xyz), 1.0);
  let n = bitcast<vec3f>(v.b.xyz);
  let joints = v.a.w;
  let last = sp.info.z - 1u;
  var r0 = vec4f(0.0);
  var r1 = vec4f(0.0);
  var r2 = vec4f(0.0);
  for (var k = 0u; k < 4u; k++) {
    let w = v.w[k];
    let j = min((joints >> (8u * k)) & 0xffu, last);
    r0 += w * palette[j * 3u];
    r1 += w * palette[j * 3u + 1u];
    r2 += w * palette[j * 3u + 2u];
  }
  let pos = vec3f(dot(r0, p), dot(r1, p), dot(r2, p));
  var nrm = vec3f(dot(r0.xyz, n), dot(r1.xyz, n), dot(r2.xyz, n));
  nrm = nrm / max(length(nrm), 1e-20);

  let q = (pos - sp.box_min.xyz) / sp.box_size.xyz;
  let l1 = abs(nrm.x) + abs(nrm.y) + abs(nrm.z);
  var ox = nrm.x / l1;
  var oy = nrm.z / l1;
  if (nrm.y < 0.0) {
    let fx = (1.0 - abs(oy)) * sign_not_zero(ox);
    let fy = (1.0 - abs(ox)) * sign_not_zero(oy);
    ox = fx;
    oy = fy;
  }
  let o = (sp.info.y + i) * 3u;
  let w0 = unorm16(q.x) | (unorm16(q.y) << 16u);
  let w1 = unorm16(q.z) | (v.b.w << 16u);
  if (sp.motion.x != 0u) {
    let l = sp.motion.x + i * 2u;
    if (sp.motion.y != 0u) {
      packed[l] = w0;
      packed[l + 1u] = w1 & 0xffffu;
    } else {
      packed[l] = packed[o];
      packed[l + 1u] = packed[o + 1u] & 0xffffu;
    }
  }
  packed[o] = w0;
  packed[o + 1u] = w1;
  packed[o + 2u] = snorm16(ox) | (snorm16(oy) << 16u);
}
