// THE EYE'S METER (render/eye.hpp; the step is eye_adapt.wgsl): the frame metered for the eye.
//
// `meter` reads the frame the post pass reads on a fixed grid of kGridW x kGridH bilinear taps
//   and bins each tap's luminance, exposed at the SKY'S exposure (frame.wgsl's `sky_exposure`, the
//   eye at rest), in log2 steps of 1 / kBinsPerEv from kLowEv up over kBins / kBinsPerEv stops,
//   weighed toward the middle of the picture where the eye looks. Each workgroup bins in its
//   own memory and adds its bins to the global histogram once.
//
// What the eye holds (sky_light.eye, frame.wgsl's SkyLight): x the stops it has opened past the
// sky's exposure (frame.wgsl's `exposure` reads it), y the target it steps toward, z the
// metered average (log2, at the sky's exposure), w the weight metered. A zeroed buffer is the
// eye at rest, so a frame that never runs this pass is exposed by the sky alone.
#include "render/wgsl/frame.wgsl"

const kGridW = 128u;
const kGridH = 72u;
const kBins = 64u;
const kBinsPerEv = 4.0;
const kLowEv = -12.0;

@group(1) @binding(0) var hdr: texture_2d<f32>;
@group(1) @binding(1) var lin: sampler;
@group(1) @binding(2) var<storage, read_write> bins: array<atomic<u32>, 64>;

var<workgroup> local_bins: array<atomic<u32>, 64>;

@compute @workgroup_size(16, 8)
fn meter(@builtin(global_invocation_id) id: vec3u, @builtin(local_invocation_index) li: u32) {
  if (li < kBins) {
    atomicStore(&local_bins[li], 0u);
  }
  workgroupBarrier();
  if (id.x < kGridW && id.y < kGridH) {
    let uv = (vec2f(id.xy) + 0.5) / vec2f(f32(kGridW), f32(kGridH));
    let c = textureSampleLevel(hdr, lin, uv, 0.0).rgb;
    let y = max(dot(c, kLuma) * sky_exposure(), 1e-8);
    let ev = log2(y);
    let bin = u32(clamp((ev - kLowEv) * kBinsPerEv, 0.0, f32(kBins - 1u)));
    // The middle counts four times the corners: the weight falls off over the picture's
    // half-diagonal.
    let r = length((uv - 0.5) * vec2f(2.0, 2.0)) * 0.70710678;
    let w = u32(4.0 + 12.0 * clamp(1.0 - r * r, 0.0, 1.0));
    atomicAdd(&local_bins[bin], w);
  }
  workgroupBarrier();
  if (li < kBins) {
    let n = atomicLoad(&local_bins[li]);
    if (n > 0u) {
      atomicAdd(&bins[li], n);
    }
  }
}
