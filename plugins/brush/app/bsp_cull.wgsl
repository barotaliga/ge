// THE WORLD'S CULL (bsp_cull.hpp): which of a map's faces a frame draws, on the GPU, in two
// phases. A face (a CHUNK: its run of triangles, its box, its kind) is drawn when the eye's
// potentially visible set holds it, its box meets the frustum, and the depth pyramid does not
// hide it. The EARLY phase tests against LAST frame's pyramid, reprojected; what that hides it
// marks for the LATE phase, which tests against this frame's pyramid after the early draws. Each
// phase compacts the drawn faces' triangles in the faces' own order (a scan, not an atomic
// append), so a frame's pixels never depend on the order threads ran in, and writes each kind's
// indexed draw's arguments.
//
// ge:summary The world's cull: PVS, frustum and Hi-Z in two phases, compacted in order by a scan
// ge:keywords bsp cull pvs hi-z occlusion two-phase compaction scan indirect draw chunk
// ge:skill brush render
#include "render/wgsl/frame.wgsl"

struct Chunk {
  lo: vec3f,
  first: u32,  // its first index in the source
  hi: vec3f,
  count: u32,  // its indices
  info: vec4u,  // x: its kind
};

struct Params {
  info: vec4u,  // chunks, indices in the source, the pyramid usable (1), 0
};

@group(1) @binding(0) var<uniform> params: Params;
@group(1) @binding(1) var<storage, read> chunks: array<Chunk>;
@group(1) @binding(2) var<storage, read> pvs: array<u32>;  // a bit a chunk
@group(1) @binding(3) var<storage, read> src: array<u32>;
@group(1) @binding(4) var<storage, read_write> dst: array<u32>;  // the early phase's, the late's
@group(1) @binding(5) var<storage, read_write> sizes: array<u32>;  // a chunk's indices drawn
// 0 not drawn, 1 hidden early (for the late phase), 2 drawn early, 3 drawn late.
@group(1) @binding(6) var<storage, read_write> state: array<u32>;
// Two phases of eight kinds' indexed draws (five words each), then the counters.
@group(1) @binding(7) var<storage, read_write> args: array<atomic<u32>>;
@group(1) @binding(8) var hiz: texture_2d<f32>;

const kKinds = 8u;
const kStats = 80u;  // 2 x 8 x 5
const kStatPvs = 0u;
const kStatFrustum = 1u;
const kStatOccluded = 2u;
const kStatHidden = 3u;
const kStatEarly = 4u;
const kStatLate = 5u;

// True when the camera-relative box (lo, hi) lies wholly behind the pyramid's depth seen
// through `vp`: its corners' rectangle on screen, read at the mip where it spans at most three
// texels a side, its nearest corner against the farthest depth there (render/hiz.hpp).
fn hidden(vp: mat4x4f, lo: vec3f, hi: vec3f) -> bool {
  var a = vec2f(1e9);
  var b = vec2f(-1e9);
  var nearest = 0.0;
  for (var k = 0u; k < 8u; k++) {
    let corner = select(lo, hi, vec3<bool>((k & 1u) != 0u, (k & 2u) != 0u, (k & 4u) != 0u));
    let clip = vp * vec4f(corner, 1.0);
    if (clip.w <= 1e-3) {
      return false;  // the box reaches the camera's plane
    }
    let ndc = clip.xyz / clip.w;
    let uv = vec2f(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    a = min(a, uv);
    b = max(b, uv);
    nearest = max(nearest, ndc.z);
  }
  // A render pixel's margin: the last pyramid was built under a jitter of up to half a pixel.
  a -= frame.size.zw;
  b += frame.size.zw;
  if (any(b < vec2f(0.0)) || any(a > vec2f(1.0))) {
    return false;
  }
  a = clamp(a, vec2f(0.0), vec2f(1.0));
  b = clamp(b, vec2f(0.0), vec2f(1.0));
  let mips = textureNumLevels(hiz);
  let size0 = vec2f(textureDimensions(hiz, 0u));
  let span = max((b.x - a.x) * size0.x, (b.y - a.y) * size0.y);
  let mip = min(u32(max(ceil(log2(max(span, 1.0) / 2.0)), 0.0)), mips - 1u);
  let size = max(vec2i(size0) >> vec2u(mip), vec2i(1));
  let i0 = clamp(vec2i(floor(a * vec2f(size))), vec2i(0), size - 1);
  let i1 = clamp(vec2i(floor(b * vec2f(size))), vec2i(0), size - 1);
  var farthest = 1.0;
  for (var y = i0.y; y <= i1.y; y++) {
    for (var x = i0.x; x <= i1.x; x++) {
      farthest = min(farthest, textureLoad(hiz, vec2i(x, y), i32(mip)).r);
    }
  }
  return nearest < farthest;
}

fn outside(lo: vec3f, hi: vec3f) -> bool {
  let c = 0.5 * (lo + hi);
  let e = 0.5 * (hi - lo);
  for (var k = 0; k < 5; k++) {
    let p = frame.planes[k];
    if (dot(p.xyz, c) + dot(abs(p.xyz), e) + p.w < 0.0) {
      return true;
    }
  }
  return false;
}

@compute @workgroup_size(64)
fn test_early(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i >= params.info.x) {
    return;
  }
  state[i] = 0u;
  sizes[i] = 0u;
  let ch = chunks[i];
  if ((pvs[i >> 5u] & (1u << (i & 31u))) == 0u) {
    atomicAdd(&args[kStats + kStatPvs], 1u);
    return;
  }
  let lo = ch.lo - frame.eye.xyz;
  let hi = ch.hi - frame.eye.xyz;
  if (outside(lo, hi)) {
    atomicAdd(&args[kStats + kStatFrustum], 1u);
    return;
  }
  // The last frame's camera saw this box where it is plus how far the eye has moved since.
  let d = frame.eye_delta.xyz;
  if (params.info.z != 0u && hidden(frame.prev_view_proj, lo + d, hi + d)) {
    state[i] = 1u;
    atomicAdd(&args[kStats + kStatOccluded], 1u);
    return;
  }
  atomicAdd(&args[kStats + kStatEarly], 1u);
  state[i] = 2u;
  sizes[i] = ch.count;
}

@compute @workgroup_size(64)
fn test_late(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i >= params.info.x) {
    return;
  }
  sizes[i] = 0u;
  if (state[i] != 1u) {
    return;
  }
  let ch = chunks[i];
  if (hidden(frame.view_proj, ch.lo - frame.eye.xyz, ch.hi - frame.eye.xyz)) {
    atomicAdd(&args[kStats + kStatHidden], 1u);
    return;
  }
  atomicAdd(&args[kStats + kStatLate], 1u);
  state[i] = 3u;
  sizes[i] = ch.count;
}

// THE SCAN: one workgroup. Each thread sums a block of chunks, the block sums are scanned, and
// each thread turns its block's sizes into offsets (in place); then each kind's draw is the run
// between its first chunk's offset and the next kind's.
const kThreads = 256u;
var<workgroup> sums: array<u32, 256>;
var<workgroup> kind_at: array<u32, 9>;  // each kind's first offset, and the end

fn scan(lid: u32, phase: u32) {
  let n = params.info.x;
  let per = (n + kThreads - 1u) / kThreads;
  let a = min(lid * per, n);
  let b = min(a + per, n);
  var s = 0u;
  for (var i = a; i < b; i++) {
    s += sizes[i];
  }
  sums[lid] = s;
  workgroupBarrier();
  // Hillis-Steele over the 256 sums (inclusive), then exclusive by the thread's own.
  for (var step = 1u; step < kThreads; step <<= 1u) {
    var v = 0u;
    if (lid >= step) {
      v = sums[lid - step];
    }
    workgroupBarrier();
    sums[lid] += v;
    workgroupBarrier();
  }
  var at = sums[lid] - s;
  for (var i = a; i < b; i++) {
    let size = sizes[i];
    sizes[i] = at;  // the chunk's offset from here on
    // A chunk that starts a kind (the chunks are sorted by kind) names where the kind starts.
    let k = chunks[i].info.x;
    var j0 = 0u;
    if (i > 0u) {
      j0 = chunks[i - 1u].info.x + 1u;
    }
    for (var j = j0; j <= k; j++) {
      kind_at[j] = at;
    }
    at += size;
  }
  storageBarrier();
  workgroupBarrier();
  if (lid == kThreads - 1u) {
    var last = 0u;
    if (n > 0u) {
      last = chunks[n - 1u].info.x + 1u;
    }
    for (var j = last; j <= kKinds; j++) {
      kind_at[j] = sums[kThreads - 1u];
    }
  }
  workgroupBarrier();
  if (lid < kKinds) {
    let w = (phase * kKinds + lid) * 5u;
    atomicStore(&args[w], kind_at[lid + 1u] - kind_at[lid]);
    atomicStore(&args[w + 1u], 1u);
    atomicStore(&args[w + 2u], phase * params.info.y + kind_at[lid]);
    atomicStore(&args[w + 3u], 0u);
    atomicStore(&args[w + 4u], 0u);
  }
}

@compute @workgroup_size(256)
fn scan_early(@builtin(local_invocation_index) lid: u32) {
  scan(lid, 0u);
}

@compute @workgroup_size(256)
fn scan_late(@builtin(local_invocation_index) lid: u32) {
  scan(lid, 1u);
}

// THE COPY: each drawn chunk's indices to its offset in the phase's region.
fn copy(i: u32, phase: u32, drawn: bool) {
  if (!drawn) {
    return;
  }
  let ch = chunks[i];
  let to = phase * params.info.y + sizes[i];
  for (var j = 0u; j < ch.count; j++) {
    dst[to + j] = src[ch.first + j];
  }
}

@compute @workgroup_size(64)
fn copy_early(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i < params.info.x) {
    copy(i, 0u, state[i] == 2u);
  }
}

@compute @workgroup_size(64)
fn copy_late(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i < params.info.x) {
    copy(i, 1u, state[i] == 3u);
  }
}
