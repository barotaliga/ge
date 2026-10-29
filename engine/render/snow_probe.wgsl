// THE SNOW MIRROR'S PROBE (render/snow_mirror.hpp, `read_snow_raw` and `read_snow_columns`): what
// the GPU reads from a mirrored layer, written out for a lab or a test to compare with the CPU.
// `raw` writes `snow_raw` at every sample of a rectangle; `columns` writes `snow_column` at a list
// of plan points.

#include "render/wgsl/snow_mirror.wgsl"

struct ProbeParams {
  rect: vec4i,  // raw: c0, r0, width, height (samples); columns: 0, 0, the points, 1
}

@group(0) @binding(0) var<uniform> probe: ProbeParams;
@group(0) @binding(1) var<storage, read> probe_xz: array<f32>;
@group(0) @binding(2) var<storage, read_write> probe_out: array<u32>;

@compute @workgroup_size(64)
fn raw(@builtin(global_invocation_id) id: vec3u) {
  let n = u32(probe.rect.z * probe.rect.w);
  if (id.x >= n) {
    return;
  }
  let c = probe.rect.x + i32(id.x % u32(probe.rect.z));
  let r = probe.rect.y + i32(id.x / u32(probe.rect.z));
  let s = snow_raw(c, r);
  probe_out[5u * id.x] = s.depth;
  probe_out[5u * id.x + 1u] = s.mass;
  probe_out[5u * id.x + 2u] = s.bits;
  probe_out[5u * id.x + 3u] = s.source;
  probe_out[5u * id.x + 4u] = s.samples;
}

@compute @workgroup_size(64)
fn columns(@builtin(global_invocation_id) id: vec3u) {
  if (id.x >= u32(probe.rect.z)) {
    return;
  }
  let col = snow_column(probe_xz[2u * id.x], probe_xz[2u * id.x + 1u]);
  let o = 6u * id.x;
  probe_out[o] = bitcast<u32>(col.depth);
  probe_out[o + 1u] = bitcast<u32>(col.drop);
  probe_out[o + 2u] = bitcast<u32>(col.density);
  probe_out[o + 3u] = bitcast<u32>(col.packed);
  probe_out[o + 4u] = bitcast<u32>(col.touched);
  probe_out[o + 5u] = bitcast<u32>(col.pressed);
}
