// THE INSTANCES' CULL (render/instances.hpp): one thread an instance. The EARLY phase tests its
// bounding sphere against the frustum, its radius on screen against the least, and (when the
// last frame exists) its box against the last frame's depth pyramid seen through the last
// frame's camera; what that hides is marked for the LATE phase, which tests it against this
// frame's pyramid. A visible instance is appended to its LOD's bucket (to two inside a LOD band)
// and counted into that bucket's indirect draw. The SHADOW entry culls a shadow map's casters
// (render/shadows.hpp) into a caster slot's lists: the map's box (its view's planes), the radius
// in the map's texels (`fp.cull.x` texels a metre: an orthographic view has no distance), no
// pyramid and no band.
#include "render/wgsl/frame_types.wgsl"
#include "render/wgsl/instances_common.wgsl"

@group(0) @binding(0) var<uniform> frame: Frame;

@group(1) @binding(0) var<uniform> fp: FamilyParams;
@group(1) @binding(1) var<storage, read> meshes: array<Mesh>;
@group(1) @binding(2) var<storage, read> lods: array<Lod>;
@group(1) @binding(3) var<storage, read> streams: array<vec4f>;
@group(1) @binding(4) var<storage, read_write> lists: array<u32>;

struct DrawArgs {
  index_count: u32,
  instance_count: atomic<u32>,
  first_index: u32,
  base_vertex: i32,
  first_instance: u32,
}
// Two phases of kMaxBuckets draws, then the stats (render/instances.hpp's InstanceStats).
struct Counters {
  draws: array<DrawArgs, 128>,
  stats: array<atomic<u32>, 8>,
}
@group(1) @binding(5) var<storage, read_write> counters: Counters;
// Per instance: 1 when the early phase's pyramid hid it, for the late phase.
@group(1) @binding(6) var<storage, read_write> state: array<u32>;
@group(1) @binding(7) var hiz: texture_2d<f32>;

const kStatEarly = 0u;
const kStatLate = 1u;
const kStatFrustum = 2u;
const kStatSmall = 3u;
const kStatOccluded = 4u;
const kStatHidden = 5u;

// A render pixel in uv: the margin round a box's rectangle (the last frame's pyramid was built
// under a jitter of up to half a pixel the unjittered camera does not have).
fn margin() -> vec2f {
  return frame.size.zw * 1.0;
}

// True when the box round the camera-relative sphere (c, r) lies wholly behind the pyramid's
// depth, seen through `vp`. Its rectangle on screen is read at the mip where it spans at most
// three texels a side; the box's nearest corner against the farthest depth there.
fn hidden(vp: mat4x4f, c: vec3f, r: f32) -> bool {
  var lo = vec2f(1e9);
  var hi = vec2f(-1e9);
  var nearest = 0.0;
  for (var k = 0u; k < 8u; k++) {
    let corner = c + r * vec3f(select(-1.0, 1.0, (k & 1u) != 0u), select(-1.0, 1.0, (k & 2u) != 0u),
                               select(-1.0, 1.0, (k & 4u) != 0u));
    let clip = vp * vec4f(corner, 1.0);
    if (clip.w <= 1e-3) {
      return false;  // the box reaches the camera's plane
    }
    let ndc = clip.xyz / clip.w;
    let uv = vec2f(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    lo = min(lo, uv);
    hi = max(hi, uv);
    nearest = max(nearest, ndc.z);
  }
  lo -= margin();
  hi += margin();
  if (any(hi < vec2f(0.0)) || any(lo > vec2f(1.0))) {
    return false;  // off that camera's screen: it cannot say
  }
  lo = clamp(lo, vec2f(0.0), vec2f(1.0));
  hi = clamp(hi, vec2f(0.0), vec2f(1.0));
  let mips = textureNumLevels(hiz);
  let size0 = vec2f(textureDimensions(hiz, 0u));
  let span = max((hi.x - lo.x) * size0.x, (hi.y - lo.y) * size0.y);
  let mip = min(u32(max(ceil(log2(max(span, 1.0) / 2.0)), 0.0)), mips - 1u);
  // A mip's size by the pyramid's halving (render/hiz.hpp), not textureDimensions(hiz, mip):
  // the software adapter answers that wrong when the level differs between threads.
  let size = max(vec2i(size0) >> vec2u(mip), vec2i(1));
  let i0 = clamp(vec2i(floor(lo * vec2f(size))), vec2i(0), size - 1);
  let i1 = clamp(vec2i(floor(hi * vec2f(size))), vec2i(0), size - 1);
  var farthest = 1.0;
  for (var y = i0.y; y <= i1.y; y++) {
    for (var x = i0.x; x <= i1.x; x++) {
      farthest = min(farthest, textureLoad(hiz, vec2i(x, y), i32(mip)).r);
    }
  }
  return nearest < farthest;
}

fn append(bucket: u32, entry: u32) {
  let slot = atomicAdd(&counters.draws[bucket].instance_count, 1u);
  lists[bucket * fp.info.z + slot] = entry;
}

// Append instance i (radius `px` on screen) to its mesh's LOD in `phase`, to both LODs inside a
// band (render/instances.hpp's pick_lod is the CPU twin).
fn place(i: u32, mesh: Mesh, px: f32, phase: u32) {
  let first = mesh.info.x;
  let n = mesh.info.y;
  let base = phase * fp.info.y;
  let band = fp.cull.z;
  for (var j = 0u; j + 1u < n; j++) {
    let t = lods[first + j].box_min.w;
    if (band > 0.0 && px >= t * (1.0 - band) && px <= t * (1.0 + band)) {
      let share = clamp((px - t * (1.0 - band)) / (2.0 * band * t), 0.0, 1.0);
      let q = u32(round(share * 127.0));
      append(base + first + j, i | kEntryBand | (q << 24u));
      append(base + first + j + 1u, i | kEntryBand | ((q | 0x80u) << 24u));
      return;
    }
  }
  var k = 0u;
  while (k + 1u < n && px < lods[first + k].box_min.w) {
    k++;
  }
  append(base + first + k, i);
}

struct Seen {
  centre: vec3f,  // camera-relative, m
  radius: f32,
  mesh: Mesh,
}

fn seen(i: u32) -> Seen {
  let cap = fp.info.z;
  let p = streams[kStreamPlace * cap + i];
  let q = streams[kStreamRotation * cap + i];
  let s = streams[kStreamScale * cap + i];
  var out: Seen;
  out.mesh = meshes[u32(s.w)];
  out.centre = p.xyz + quat_rotate(q, out.mesh.sphere.xyz * s.xyz) - frame.eye.xyz;
  out.radius = out.mesh.sphere.w * max(s.x, max(s.y, s.z));
  return out;
}

// The radius on screen, px.
fn screen_px(v: Seen) -> f32 {
  let d = length(v.centre);
  if (d <= v.radius) {
    return 1e9;
  }
  return v.radius / d * fp.cull.x;
}

// Past the view's reach (FrameView::reach_m), where the mist has closed it: counted with the
// frustum's.
fn past_reach(v: Seen) -> bool {
  return frame.mist.y > 0.0 && length(v.centre) - v.radius > frame.mist.y;
}

@compute @workgroup_size(64)
fn early(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i >= fp.info.x) {
    return;
  }
  state[i] = 0u;
  let v = seen(i);
  if (past_reach(v)) {
    atomicAdd(&counters.stats[kStatFrustum], 1u);
    return;
  }
  for (var k = 0; k < 5; k++) {
    let p = frame.planes[k];
    if (dot(p.xyz, v.centre) + p.w < -v.radius) {
      atomicAdd(&counters.stats[kStatFrustum], 1u);
      return;
    }
  }
  let px = screen_px(v);
  if (px < fp.cull.y) {
    atomicAdd(&counters.stats[kStatSmall], 1u);
    return;
  }
  // The last frame's camera saw this point at `centre` plus how far the eye has moved since.
  if (fp.info.w != 0u && hidden(frame.prev_view_proj, v.centre + frame.eye_delta.xyz, v.radius)) {
    state[i] = 1u;
    atomicAdd(&counters.stats[kStatOccluded], 1u);
    return;
  }
  atomicAdd(&counters.stats[kStatEarly], 1u);
  place(i, v.mesh, px, 0u);
}

@compute @workgroup_size(64)
fn late(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i >= fp.info.x || state[i] == 0u) {
    return;
  }
  let v = seen(i);
  if (hidden(frame.view_proj, v.centre, v.radius)) {
    atomicAdd(&counters.stats[kStatHidden], 1u);
    return;
  }
  atomicAdd(&counters.stats[kStatLate], 1u);
  place(i, v.mesh, screen_px(v), 1u);
}

@compute @workgroup_size(64)
fn shadow(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i >= fp.info.x) {
    return;
  }
  let v = seen(i);
  if (past_reach(v)) {
    atomicAdd(&counters.stats[kStatFrustum], 1u);
    return;
  }
  for (var k = 0; k < 5; k++) {
    let p = frame.planes[k];
    if (dot(p.xyz, v.centre) + p.w < -v.radius) {
      atomicAdd(&counters.stats[kStatFrustum], 1u);
      return;
    }
  }
  let px = v.radius * fp.cull.x;
  if (px < fp.cull.y) {
    atomicAdd(&counters.stats[kStatSmall], 1u);
    return;
  }
  atomicAdd(&counters.stats[kStatEarly], 1u);
  place(i, v.mesh, px, 0u);
}
