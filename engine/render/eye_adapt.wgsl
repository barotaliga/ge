// THE EYE'S STEP (render/eye.hpp; the meter is eye.wgsl), one workgroup: the histogram read and
// cleared, the bins between the `low` and `high` shares of the weight averaged (the darkest specks
// and the brightest lights left out), and the eye stepped toward the stops that average sits
// under the key, outside a band of `tolerance` round it, scaled by `strength` and clamped to
// [min_ev, max_ev]. Opening to the dark takes `to_dark_s` a half of the way, closing to the light
// `to_light_s`. A module of its own: it binds the sky's light buffer for writing, which every
// pass that includes frame.wgsl binds for reading.

const kBins = 64u;
const kBinsPerEv = 4.0;
const kLowEv = -12.0;

// frame.wgsl's SkyLight: the sky's four rows (the sky's tables write them), then the eye's.
struct EyeLight {
  irradiance_up: vec4f,
  sun_at_eye: vec4f,
  moon_up: vec4f,
  moon_at_eye: vec4f,
  eye: vec4f,
}

struct EyeStep {
  timing: vec4f,  // dt (s), to_light_s, to_dark_s, 1 to jump to the goal (a cut)
  aim: vec4f,     // the key (log2), the tolerance (stops), min_ev, max_ev
  band: vec4f,    // the low share, the high share, the strength, 0
}

@group(0) @binding(0) var<storage, read_write> adapt_bins: array<atomic<u32>, 64>;
@group(0) @binding(1) var<storage, read_write> eye_light: EyeLight;
@group(0) @binding(2) var<uniform> eye_step: EyeStep;

var<workgroup> counts: array<f32, 64>;

@compute @workgroup_size(64)
fn adapt(@builtin(local_invocation_index) li: u32) {
  counts[li] = f32(atomicExchange(&adapt_bins[li], 0u));
  workgroupBarrier();
  if (li != 0u) {
    return;
  }
  var total = 0.0;
  for (var i = 0u; i < kBins; i++) {
    total += counts[i];
  }
  let was = eye_light.eye;
  if (total <= 0.0) {
    return;
  }
  // The weight between the low and the high share, each bin's clipped to that band.
  let lo = eye_step.band.x * total;
  let hi = eye_step.band.y * total;
  var below = 0.0;
  var sum = 0.0;
  var kept = 0.0;
  for (var i = 0u; i < kBins; i++) {
    let a = max(lo, below);
    let b = min(hi, below + counts[i]);
    if (b > a) {
      sum += (b - a) * (kLowEv + (f32(i) + 0.5) / kBinsPerEv);
      kept += b - a;
    }
    below += counts[i];
  }
  let average = select(was.z, sum / kept, kept > 0.0);
  var d = eye_step.aim.x - average;
  d = select(select(d + eye_step.aim.y, 0.0, d > -eye_step.aim.y), d - eye_step.aim.y, d > eye_step.aim.y);
  let goal = clamp(d * eye_step.band.z, eye_step.aim.z, eye_step.aim.w);
  var ev = goal;
  if (eye_step.timing.w < 0.5) {
    let half = select(eye_step.timing.y, eye_step.timing.z, goal > was.x);
    let k = 1.0 - exp2(-eye_step.timing.x / max(half, 1e-3));
    ev = was.x + (goal - was.x) * k;
  }
  eye_light.eye = vec4f(ev, goal, average, total);
}
